/*
 * qsrv - a small line-based job queue and key/value broker.
 *
 * MIT License
 *
 * Copyright (c) 2026 The qsrv author
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define QSRV_VERSION "2.3.1"
#define MAX_LINE 4096
#define MAX_ARGS 64
#define MAX_CONNS 32
#define MAX_TX 16
#define MAX_KEYLEN 64
#define MAX_VALLEN 512
#define MAX_QNAME 32
#define MAX_NSNAME 16
#define MAX_QUEUES_PER_NS 8
#define MAX_KEYS_PER_NS 64
#define DEFAULT_MAXLEN 100
#define DEFAULT_MAXNACK 3
#define DEFAULT_PRI 5
#define DEFAULT_TTR 30
#define INT_LIMIT 1000000000000000LL

typedef long long i64;

/* ------------------------------------------------------------------ */
/* growable byte buffer                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    char *p;
    size_t n, cap;
} Buf;

static void buf_reserve(Buf *b, size_t extra) {
    if (b->n + extra + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 64;
        while (nc < b->n + extra + 1) nc *= 2;
        b->p = realloc(b->p, nc);
        if (!b->p) { perror("realloc"); exit(1); }
        b->cap = nc;
    }
}
static void buf_add(Buf *b, const void *d, size_t n) {
    buf_reserve(b, n);
    memcpy(b->p + b->n, d, n);
    b->n += n;
    b->p[b->n] = 0;
}
static void buf_adds(Buf *b, const char *s) { buf_add(b, s, strlen(s)); }
static void buf_printf(Buf *b, const char *fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof tmp) n = sizeof tmp - 1;
    buf_add(b, tmp, (size_t)n);
}
static void buf_free(Buf *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }
static void buf_consume(Buf *b, size_t n) {
    if (n >= b->n) { b->n = 0; if (b->p) b->p[0] = 0; return; }
    memmove(b->p, b->p + n, b->n - n);
    b->n -= n;
    b->p[b->n] = 0;
}

/* ------------------------------------------------------------------ */
/* reply helpers                                                       */
/* ------------------------------------------------------------------ */

static void r_simple(Buf *o, const char *s) { buf_printf(o, "+%s\r\n", s); }
static void r_int(Buf *o, i64 v) { buf_printf(o, ":%lld\r\n", v); }
static void r_bulk(Buf *o, const char *p, size_t n) {
    buf_printf(o, "$%zu\r\n", n);
    buf_add(o, p, n);
    buf_adds(o, "\r\n");
}
static void r_nilbulk(Buf *o) { buf_adds(o, "$-1\r\n"); }
static void r_nilarr(Buf *o) { buf_adds(o, "*-1\r\n"); }
static void r_arr(Buf *o, i64 n) { buf_printf(o, "*%lld\r\n", n); }
static void r_err(Buf *o, const char *code, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    buf_printf(o, "-%s %s\r\n", code, tmp);
}

/* printable, upper-cased, truncated echo of a client supplied word */
static void sanitize_word(const char *p, size_t n, char *out, size_t outsz) {
    size_t lim = n > 24 ? 24 : n;
    size_t j = 0;
    for (size_t i = 0; i < lim && j + 1 < outsz; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c < 0x20 || c >= 0x7f) c = '?';
        else if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 32);
        out[j++] = (char)c;
    }
    out[j] = 0;
}

/* ------------------------------------------------------------------ */
/* data model                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    char *p;
    size_t n;
} Tok;

typedef struct {
    int argc;
    Tok argv[MAX_ARGS];
} Cmd;

enum { JS_READY = 1, JS_DELAYED, JS_RESERVED, JS_DEAD };

typedef struct {
    i64 id;
    char ns[MAX_NSNAME + 1];
    char q[MAX_QNAME + 1];
    char *payload;
    size_t plen;
    int pri;
    int ttr;
    int state;
    i64 ready_at;
    i64 deadline;
    int owner;
    int nacks, reserves, timeouts;
    i64 seq; /* ready order or dead order */
    int alive;
} Job;

typedef struct {
    char ns[MAX_NSNAME + 1];
    char name[MAX_QNAME + 1];
    int maxlen, maxnack;
    i64 pushed, acked, nacked, timeouts;
    int alive;
} Queue;

typedef struct {
    char ns[MAX_NSNAME + 1];
    char *k;
    size_t kn;
    char *v;
    size_t vn;
    i64 version;
    i64 expire_at; /* 0 = never */
    int alive;
} Key;

typedef struct {
    int used;
    int fd;
    int id;
    char ns[MAX_NSNAME + 1];
    Buf in;
    int blocked;
    int woken;
    char wq[MAX_QNAME + 1];
    char wns[MAX_NSNAME + 1];
    i64 wdeadline; /* -1 forever */
    i64 wseq;
    int inmulti;
    int txabort;
    int ntx;
    Cmd *tx[MAX_TX];
    int closing;
} Conn;

static Job **jobs;
static int njobs, capjobs;
static Queue **queues;
static int nqueues, capqueues;
static Key **keys;
static int nkeys, capkeys;
static Conn conns[MAX_CONNS];

static i64 g_clock = 0;
static i64 g_next_job = 1;
static i64 g_seq = 0;
static i64 g_wseq = 0;
static int g_next_conn = 1;
static i64 g_commands = 0;
static i64 g_total_conns = 0;
static i64 g_pushed = 0, g_acked = 0, g_expired_keys = 0;

/* ------------------------------------------------------------------ */
/* lookup helpers                                                      */
/* ------------------------------------------------------------------ */

static Queue *find_queue(const char *ns, const char *name) {
    for (int i = 0; i < nqueues; i++) {
        Queue *q = queues[i];
        if (q->alive && !strcmp(q->ns, ns) && !strcmp(q->name, name)) return q;
    }
    return NULL;
}
static int count_queues(const char *ns) {
    int c = 0;
    for (int i = 0; i < nqueues; i++)
        if (queues[i]->alive && !strcmp(queues[i]->ns, ns)) c++;
    return c;
}
static Queue *new_queue(const char *ns, const char *name, int maxlen, int maxnack) {
    if (nqueues == capqueues) {
        capqueues = capqueues ? capqueues * 2 : 16;
        queues = realloc(queues, sizeof(Queue *) * (size_t)capqueues);
    }
    Queue *q = calloc(1, sizeof *q);
    snprintf(q->ns, sizeof q->ns, "%s", ns);
    snprintf(q->name, sizeof q->name, "%s", name);
    q->maxlen = maxlen;
    q->maxnack = maxnack;
    q->alive = 1;
    queues[nqueues++] = q;
    return q;
}
static Job *find_job(i64 id) {
    for (int i = 0; i < njobs; i++)
        if (jobs[i]->alive && jobs[i]->id == id) return jobs[i];
    return NULL;
}
static void free_job(Job *j) {
    j->alive = 0;
    free(j->payload);
    j->payload = NULL;
}
static int queue_live_count(Queue *q) {
    int c = 0;
    for (int i = 0; i < njobs; i++) {
        Job *j = jobs[i];
        if (j->alive && j->state != JS_DEAD && !strcmp(j->ns, q->ns) && !strcmp(j->q, q->name)) c++;
    }
    return c;
}
static int queue_count_state(Queue *q, int st) {
    int c = 0;
    for (int i = 0; i < njobs; i++) {
        Job *j = jobs[i];
        if (j->alive && j->state == st && !strcmp(j->ns, q->ns) && !strcmp(j->q, q->name)) c++;
    }
    return c;
}
static Job *best_ready(const char *ns, const char *qn) {
    Job *best = NULL;
    for (int i = 0; i < njobs; i++) {
        Job *j = jobs[i];
        if (!j->alive || j->state != JS_READY) continue;
        if (strcmp(j->ns, ns) || strcmp(j->q, qn)) continue;
        if (!best || j->pri < best->pri || (j->pri == best->pri && j->seq < best->seq)) best = j;
    }
    return best;
}
static Key *find_key(const char *ns, const char *k, size_t kn) {
    for (int i = 0; i < nkeys; i++) {
        Key *x = keys[i];
        if (x->alive && x->kn == kn && !strcmp(x->ns, ns) && !memcmp(x->k, k, kn)) return x;
    }
    return NULL;
}
static int count_keys(const char *ns) {
    int c = 0;
    for (int i = 0; i < nkeys; i++)
        if (keys[i]->alive && !strcmp(keys[i]->ns, ns)) c++;
    return c;
}
static Key *new_key(const char *ns, const char *k, size_t kn) {
    if (nkeys == capkeys) {
        capkeys = capkeys ? capkeys * 2 : 16;
        keys = realloc(keys, sizeof(Key *) * (size_t)capkeys);
    }
    Key *x = calloc(1, sizeof *x);
    snprintf(x->ns, sizeof x->ns, "%s", ns);
    x->k = malloc(kn + 1);
    memcpy(x->k, k, kn);
    x->k[kn] = 0;
    x->kn = kn;
    x->alive = 1;
    keys[nkeys++] = x;
    return x;
}
static void key_setval(Key *x, const char *v, size_t vn) {
    free(x->v);
    x->v = malloc(vn + 1);
    memcpy(x->v, v, vn);
    x->v[vn] = 0;
    x->vn = vn;
}
static void free_key(Key *x) {
    x->alive = 0;
    free(x->k);
    free(x->v);
    x->k = x->v = NULL;
}

/* compaction keeps the arrays from growing without bound */
static void compact(void) {
    int w = 0;
    for (int i = 0; i < njobs; i++) {
        if (jobs[i]->alive) jobs[w++] = jobs[i];
        else free(jobs[i]);
    }
    njobs = w;
    w = 0;
    for (int i = 0; i < nkeys; i++) {
        if (keys[i]->alive) keys[w++] = keys[i];
        else free(keys[i]);
    }
    nkeys = w;
    w = 0;
    for (int i = 0; i < nqueues; i++) {
        if (queues[i]->alive) queues[w++] = queues[i];
        else free(queues[i]);
    }
    nqueues = w;
}

/* ------------------------------------------------------------------ */
/* validation                                                          */
/* ------------------------------------------------------------------ */

static int valid_name(const Tok *t, size_t maxlen, int allow_dot) {
    if (t->n < 1 || t->n > maxlen) return 0;
    for (size_t i = 0; i < t->n; i++) {
        unsigned char c = (unsigned char)t->p[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
            continue;
        if (c == '.' && allow_dot) continue;
        return 0;
    }
    return 1;
}

/* returns 0 on success, otherwise writes an error */
static int parse_num(const Tok *t, i64 lo, i64 hi, const char *what, i64 *out, Buf *o) {
    size_t i = 0;
    int neg = 0;
    if (t->n > 0 && t->p[0] == '-') { neg = 1; i = 1; }
    if (i >= t->n) { r_err(o, "E_NUM", "value for '%s' is not an integer", what); return -1; }
    for (size_t k = i; k < t->n; k++) {
        if (t->p[k] < '0' || t->p[k] > '9') {
            r_err(o, "E_NUM", "value for '%s' is not an integer", what);
            return -1;
        }
    }
    if (t->n - i > 18) {
        r_err(o, "E_RANGE", "value for '%s' must be between %lld and %lld", what, lo, hi);
        return -1;
    }
    i64 v = 0;
    for (size_t k = i; k < t->n; k++) v = v * 10 + (t->p[k] - '0');
    if (neg) v = -v;
    if (v < lo || v > hi) {
        r_err(o, "E_RANGE", "value for '%s' must be between %lld and %lld", what, lo, hi);
        return -1;
    }
    *out = v;
    return 0;
}

static int tok_ieq(const Tok *t, const char *s) {
    size_t n = strlen(s);
    if (t->n != n) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = t->p[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c != s[i]) return 0;
    }
    return 1;
}

static void tok_cstr(const Tok *t, char *out, size_t outsz) {
    size_t n = t->n < outsz - 1 ? t->n : outsz - 1;
    memcpy(out, t->p, n);
    out[n] = 0;
}

/* ------------------------------------------------------------------ */
/* lexer                                                               */
/* ------------------------------------------------------------------ */

static void cmd_free(Cmd *c) {
    for (int i = 0; i < c->argc; i++) free(c->argv[i].p);
    c->argc = 0;
}

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* returns 1 when tokens were produced, 0 for a blank line, -1 on error (error in o) */
static int lex(const char *s, size_t n, Cmd *c, Buf *o) {
    c->argc = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if ((ch < 0x20 && ch != '\t') || ch == 0x7f) {
            r_err(o, "E_CTRL", "control character in command");
            return -1;
        }
    }
    size_t i = 0;
    for (;;) {
        while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
        if (i >= n) break;
        if (c->argc == MAX_ARGS) {
            cmd_free(c);
            r_err(o, "E_ARGS", "too many arguments");
            return -1;
        }
        Buf t = {0};
        buf_reserve(&t, 0);
        if (s[i] == '"') {
            i++;
            int closed = 0;
            while (i < n) {
                char ch = s[i];
                if (ch == '"') { closed = 1; i++; break; }
                if (ch == '\\') {
                    if (i + 1 >= n) { buf_free(&t); cmd_free(c); r_err(o, "E_QUOTE", "unbalanced quotes"); return -1; }
                    char e = s[i + 1];
                    char outc;
                    if (e == 'n') outc = '\n';
                    else if (e == 't') outc = '\t';
                    else if (e == 'r') outc = '\r';
                    else if (e == '0') outc = '\0';
                    else if (e == '\\') outc = '\\';
                    else if (e == '"') outc = '"';
                    else if (e == 'x') {
                        if (i + 3 < n + 0 && hexval((unsigned char)s[i + 2]) >= 0 && hexval((unsigned char)s[i + 3]) >= 0) {
                            outc = (char)(hexval((unsigned char)s[i + 2]) * 16 + hexval((unsigned char)s[i + 3]));
                            buf_add(&t, &outc, 1);
                            i += 4;
                            continue;
                        }
                        buf_free(&t); cmd_free(c);
                        r_err(o, "E_ESCAPE", "invalid escape sequence");
                        return -1;
                    } else {
                        buf_free(&t); cmd_free(c);
                        r_err(o, "E_ESCAPE", "invalid escape sequence");
                        return -1;
                    }
                    buf_add(&t, &outc, 1);
                    i += 2;
                    continue;
                }
                buf_add(&t, &ch, 1);
                i++;
            }
            if (!closed) { buf_free(&t); cmd_free(c); r_err(o, "E_QUOTE", "unbalanced quotes"); return -1; }
            if (i < n && s[i] != ' ' && s[i] != '\t') {
                buf_free(&t); cmd_free(c);
                r_err(o, "E_QUOTE", "closing quote must be followed by a space");
                return -1;
            }
        } else if (s[i] == '\'') {
            i++;
            int closed = 0;
            while (i < n) {
                if (s[i] == '\'') { closed = 1; i++; break; }
                buf_add(&t, &s[i], 1);
                i++;
            }
            if (!closed) { buf_free(&t); cmd_free(c); r_err(o, "E_QUOTE", "unbalanced quotes"); return -1; }
            if (i < n && s[i] != ' ' && s[i] != '\t') {
                buf_free(&t); cmd_free(c);
                r_err(o, "E_QUOTE", "closing quote must be followed by a space");
                return -1;
            }
        } else {
            while (i < n && s[i] != ' ' && s[i] != '\t') {
                buf_add(&t, &s[i], 1);
                i++;
            }
        }
        c->argv[c->argc].p = t.p;
        c->argv[c->argc].n = t.n;
        c->argc++;
    }
    return c->argc > 0 ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* command table                                                       */
/* ------------------------------------------------------------------ */

typedef struct Conn_ Conn_;
enum { RC_OK = 0, RC_BLOCK = 1, RC_CLOSE = 2 };
typedef int (*cmdfn)(Conn *c, Cmd *cmd, Buf *o, int in_exec);

typedef struct {
    const char *name;
    int minargs, maxargs; /* excluding the name; -1 = unlimited */
    cmdfn fn;
} CmdDef;

static void send_to(Conn *c, Buf *o);
static void serve_waiters(void);

/* ---- connection commands ---- */

static int c_ping(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)c; (void)ie;
    if (cmd->argc == 1) r_simple(o, "PONG");
    else r_bulk(o, cmd->argv[1].p, cmd->argv[1].n);
    return RC_OK;
}
static int c_echo(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)c; (void)ie;
    r_bulk(o, cmd->argv[1].p, cmd->argv[1].n);
    return RC_OK;
}
static int c_use(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    if (!valid_name(&cmd->argv[1], MAX_NSNAME, 0)) { r_err(o, "E_NAME", "invalid namespace name"); return RC_OK; }
    tok_cstr(&cmd->argv[1], c->ns, sizeof c->ns);
    r_simple(o, "OK");
    return RC_OK;
}
static int c_whoami(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)cmd; (void)ie;
    buf_printf(o, "+id=%d ns=%s\r\n", c->id, c->ns);
    return RC_OK;
}
static int c_quit(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)c; (void)cmd; (void)ie;
    r_simple(o, "BYE");
    return RC_CLOSE;
}

/* ---- clock ---- */

typedef struct { i64 due; i64 id; Job *j; } Ev;
static int ev_cmp(const void *a, const void *b) {
    const Ev *x = a, *y = b;
    if (x->due != y->due) return x->due < y->due ? -1 : 1;
    if (x->id != y->id) return x->id < y->id ? -1 : 1;
    return 0;
}

static void job_make_ready(Job *j) {
    j->state = JS_READY;
    j->owner = 0;
    j->seq = ++g_seq;
}
static void job_make_dead(Job *j) {
    j->state = JS_DEAD;
    j->owner = 0;
    j->seq = ++g_seq;
}

static void advance_clock(i64 n) {
    g_clock += n;
    /* 1. key expiry */
    for (int i = 0; i < nkeys; i++) {
        Key *x = keys[i];
        if (x->alive && x->expire_at && x->expire_at <= g_clock) { free_key(x); g_expired_keys++; }
    }
    /* 2. job events */
    Ev *ev = malloc(sizeof(Ev) * (size_t)(njobs + 1));
    int ne = 0;
    for (int i = 0; i < njobs; i++) {
        Job *j = jobs[i];
        if (!j->alive) continue;
        if (j->state == JS_DELAYED && j->ready_at <= g_clock) { ev[ne].due = j->ready_at; ev[ne].id = j->id; ev[ne].j = j; ne++; }
        else if (j->state == JS_RESERVED && j->deadline <= g_clock) { ev[ne].due = j->deadline; ev[ne].id = j->id; ev[ne].j = j; ne++; }
    }
    qsort(ev, (size_t)ne, sizeof(Ev), ev_cmp);
    for (int k = 0; k < ne; k++) {
        Job *j = ev[k].j;
        if (j->state == JS_DELAYED) {
            job_make_ready(j);
        } else {
            Queue *q = find_queue(j->ns, j->q);
            j->timeouts++;
            j->nacks++;
            if (q) q->timeouts++;
            if (q && j->nacks >= q->maxnack) job_make_dead(j);
            else job_make_ready(j);
        }
    }
    free(ev);
    /* 3. waiters get jobs first, 4. then remaining waiters time out */
    serve_waiters();
    for (;;) {
        Conn *best = NULL;
        for (int i = 0; i < MAX_CONNS; i++) {
            Conn *w = &conns[i];
            if (!w->used || !w->blocked || w->wdeadline < 0 || w->wdeadline > g_clock) continue;
            if (!best || w->wseq < best->wseq) best = w;
        }
        if (!best) break;
        Buf o = {0};
        r_nilarr(&o);
        best->blocked = 0;
        best->woken = 1;
        send_to(best, &o);
        buf_free(&o);
    }
}

static int c_tick(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)c; (void)ie;
    i64 n;
    if (parse_num(&cmd->argv[1], 1, 1000000, "ticks", &n, o)) return RC_OK;
    advance_clock(n);
    r_int(o, g_clock);
    return RC_OK;
}
static int c_time(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)c; (void)cmd; (void)ie;
    r_int(o, g_clock);
    return RC_OK;
}

/* ---- option parsing ---- */

typedef struct {
    const char *name;   /* upper case keyword */
    const char *what;   /* name used in numeric errors */
    int has_value;
    i64 lo, hi;
    int seen;
    i64 val;
} Opt;

/* parse options from argv[start..]; returns 0 or -1 (error written) */
static int parse_opts(Cmd *cmd, int start, Opt *opts, int nopts, Buf *o) {
    int i = start;
    while (i < cmd->argc) {
        Tok *t = &cmd->argv[i];
        Opt *found = NULL;
        for (int k = 0; k < nopts; k++)
            if (tok_ieq(t, opts[k].name)) { found = &opts[k]; break; }
        if (!found) {
            char w[32];
            sanitize_word(t->p, t->n, w, sizeof w);
            r_err(o, "E_SYNTAX", "unknown option '%s'", w);
            return -1;
        }
        if (found->seen) { r_err(o, "E_DUPOPT", "duplicate option '%s'", found->name); return -1; }
        found->seen = 1;
        if (found->has_value) {
            if (i + 1 >= cmd->argc) { r_err(o, "E_SYNTAX", "option '%s' requires a value", found->name); return -1; }
            if (parse_num(&cmd->argv[i + 1], found->lo, found->hi, found->what, &found->val, o)) return -1;
            i += 2;
        } else {
            i += 1;
        }
    }
    return 0;
}

/* ---- queue commands ---- */

static int check_qname(Tok *t, char *out, Buf *o) {
    if (!valid_name(t, MAX_QNAME, 1)) { r_err(o, "E_NAME", "invalid queue name"); return -1; }
    tok_cstr(t, out, MAX_QNAME + 1);
    return 0;
}

static int c_qdeclare(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    char qn[MAX_QNAME + 1];
    if (check_qname(&cmd->argv[1], qn, o)) return RC_OK;
    Opt opts[2] = {{"MAXLEN", "maxlen", 1, 1, 1000, 0, 0}, {"MAXNACK", "maxnack", 1, 1, 100, 0, 0}};
    if (parse_opts(cmd, 2, opts, 2, o)) return RC_OK;
    Queue *q = find_queue(c->ns, qn);
    if (q) {
        if ((opts[0].seen && opts[0].val != q->maxlen) || (opts[1].seen && opts[1].val != q->maxnack)) {
            r_err(o, "E_CONFLICT", "queue exists with different settings");
            return RC_OK;
        }
        r_simple(o, "EXISTS");
        return RC_OK;
    }
    if (count_queues(c->ns) >= MAX_QUEUES_PER_NS) { r_err(o, "E_LIMIT", "too many queues in namespace"); return RC_OK; }
    new_queue(c->ns, qn, opts[0].seen ? (int)opts[0].val : DEFAULT_MAXLEN, opts[1].seen ? (int)opts[1].val : DEFAULT_MAXNACK);
    r_simple(o, "CREATED");
    return RC_OK;
}

static void fail_waiters_ns(const char *ns, const char *qn) {
    /* qn NULL = every queue of the namespace */
    for (;;) {
        Conn *best = NULL;
        for (int i = 0; i < MAX_CONNS; i++) {
            Conn *w = &conns[i];
            if (!w->used || !w->blocked || strcmp(w->wns, ns)) continue;
            if (qn && strcmp(w->wq, qn)) continue;
            if (!best || w->wseq < best->wseq) best = w;
        }
        if (!best) break;
        Buf o = {0};
        r_err(&o, "E_NOQUEUE", "queue dropped");
        best->blocked = 0;
        best->woken = 1;
        send_to(best, &o);
        buf_free(&o);
    }
}

static int drop_queue_jobs(const char *ns, const char *qn) {
    int n = 0;
    for (int i = 0; i < njobs; i++) {
        Job *j = jobs[i];
        if (j->alive && !strcmp(j->ns, ns) && !strcmp(j->q, qn)) { free_job(j); n++; }
    }
    return n;
}

static int c_qdrop(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    char qn[MAX_QNAME + 1];
    if (check_qname(&cmd->argv[1], qn, o)) return RC_OK;
    Queue *q = find_queue(c->ns, qn);
    if (!q) { r_err(o, "E_NOQUEUE", "no such queue"); return RC_OK; }
    int n = drop_queue_jobs(c->ns, qn);
    q->alive = 0;
    fail_waiters_ns(c->ns, qn);
    r_int(o, n);
    return RC_OK;
}

static int str_cmp_ptr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int c_qlist(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)cmd; (void)ie;
    const char *names[MAX_QUEUES_PER_NS + 4];
    int n = 0;
    for (int i = 0; i < nqueues; i++)
        if (queues[i]->alive && !strcmp(queues[i]->ns, c->ns)) names[n++] = queues[i]->name;
    qsort(names, (size_t)n, sizeof(char *), str_cmp_ptr);
    r_arr(o, n);
    for (int i = 0; i < n; i++) r_bulk(o, names[i], strlen(names[i]));
    return RC_OK;
}

static int c_push(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    char qn[MAX_QNAME + 1];
    if (check_qname(&cmd->argv[1], qn, o)) return RC_OK;
    if (cmd->argv[2].n > MAX_VALLEN) { r_err(o, "E_VALUE", "payload too long"); return RC_OK; }
    Opt opts[3] = {{"PRI", "pri", 1, 0, 9, 0, 0}, {"DELAY", "delay", 1, 0, 86400, 0, 0}, {"TTR", "ttr", 1, 1, 3600, 0, 0}};
    if (parse_opts(cmd, 3, opts, 3, o)) return RC_OK;
    Queue *q = find_queue(c->ns, qn);
    if (!q) {
        if (count_queues(c->ns) >= MAX_QUEUES_PER_NS) { r_err(o, "E_LIMIT", "too many queues in namespace"); return RC_OK; }
        q = new_queue(c->ns, qn, DEFAULT_MAXLEN, DEFAULT_MAXNACK);
    }
    if (queue_live_count(q) >= q->maxlen) { r_err(o, "E_FULL", "queue is full"); return RC_OK; }
    if (njobs == capjobs) {
        capjobs = capjobs ? capjobs * 2 : 32;
        jobs = realloc(jobs, sizeof(Job *) * (size_t)capjobs);
    }
    Job *j = calloc(1, sizeof *j);
    j->id = g_next_job++;
    snprintf(j->ns, sizeof j->ns, "%s", c->ns);
    snprintf(j->q, sizeof j->q, "%s", qn);
    j->payload = malloc(cmd->argv[2].n + 1);
    memcpy(j->payload, cmd->argv[2].p, cmd->argv[2].n);
    j->plen = cmd->argv[2].n;
    j->pri = opts[0].seen ? (int)opts[0].val : DEFAULT_PRI;
    j->ttr = opts[2].seen ? (int)opts[2].val : DEFAULT_TTR;
    j->alive = 1;
    if (opts[1].seen && opts[1].val > 0) {
        j->state = JS_DELAYED;
        j->ready_at = g_clock + opts[1].val;
    } else {
        job_make_ready(j);
    }
    jobs[njobs++] = j;
    q->pushed++;
    g_pushed++;
    r_int(o, j->id);
    return RC_OK;
}

static void reserve_job(Job *j, Conn *c, Buf *o) {
    j->state = JS_RESERVED;
    j->owner = c->id;
    j->reserves++;
    j->deadline = g_clock + j->ttr;
    r_arr(o, 3);
    r_int(o, j->id);
    r_int(o, j->reserves);
    r_bulk(o, j->payload, j->plen);
}

static int c_reserve(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    char qn[MAX_QNAME + 1];
    if (check_qname(&cmd->argv[1], qn, o)) return RC_OK;
    if (!find_queue(c->ns, qn)) { r_err(o, "E_NOQUEUE", "no such queue"); return RC_OK; }
    Job *j = best_ready(c->ns, qn);
    if (!j) { r_nilarr(o); return RC_OK; }
    reserve_job(j, c, o);
    return RC_OK;
}

static int c_breserve(Conn *c, Cmd *cmd, Buf *o, int ie) {
    char qn[MAX_QNAME + 1];
    if (check_qname(&cmd->argv[1], qn, o)) return RC_OK;
    i64 to;
    if (parse_num(&cmd->argv[2], 0, 100000, "timeout", &to, o)) return RC_OK;
    if (!find_queue(c->ns, qn)) { r_err(o, "E_NOQUEUE", "no such queue"); return RC_OK; }
    Job *j = best_ready(c->ns, qn);
    if (j) { reserve_job(j, c, o); return RC_OK; }
    if (ie) { r_nilarr(o); return RC_OK; }
    c->blocked = 1;
    snprintf(c->wq, sizeof c->wq, "%s", qn);
    snprintf(c->wns, sizeof c->wns, "%s", c->ns);
    c->wdeadline = to == 0 ? -1 : g_clock + to;
    c->wseq = ++g_wseq;
    return RC_BLOCK;
}

static void serve_waiters(void) {
    for (;;) {
        Conn *best = NULL;
        for (int i = 0; i < MAX_CONNS; i++) {
            Conn *w = &conns[i];
            if (!w->used || !w->blocked) continue;
            if (!best_ready(w->wns, w->wq)) continue;
            if (!best || w->wseq < best->wseq) best = w;
        }
        if (!best) break;
        Job *j = best_ready(best->wns, best->wq);
        Buf o = {0};
        reserve_job(j, best, &o);
        best->blocked = 0;
        best->woken = 1;
        send_to(best, &o);
        buf_free(&o);
    }
}

/* resolves a job id for ownership-checked commands; returns job or NULL (error written) */
static Job *owned_job(Conn *c, Tok *t, Buf *o) {
    i64 id;
    if (parse_num(t, 1, 1000000000000LL, "id", &id, o)) return NULL;
    Job *j = find_job(id);
    if (!j || strcmp(j->ns, c->ns)) { r_err(o, "E_NOJOB", "no such job"); return NULL; }
    if (j->state == JS_DEAD) { r_err(o, "E_DEAD", "job is dead"); return NULL; }
    if (j->state != JS_RESERVED) { r_err(o, "E_NOTRESERVED", "job is not reserved"); return NULL; }
    if (j->owner != c->id) { r_err(o, "E_NOTOWNER", "job is reserved by another client"); return NULL; }
    return j;
}

static int c_ack(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    Job *j = owned_job(c, &cmd->argv[1], o);
    if (!j) return RC_OK;
    Queue *q = find_queue(j->ns, j->q);
    if (q) q->acked++;
    g_acked++;
    free_job(j);
    r_int(o, 1);
    return RC_OK;
}

static int c_nack(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    Opt opts[1] = {{"DELAY", "delay", 1, 0, 86400, 0, 0}};
    Job *j = owned_job(c, &cmd->argv[1], o);
    if (!j) return RC_OK;
    if (parse_opts(cmd, 2, opts, 1, o)) return RC_OK;
    Queue *q = find_queue(j->ns, j->q);
    j->nacks++;
    if (q) q->nacked++;
    if (q && j->nacks >= q->maxnack) { job_make_dead(j); r_simple(o, "DEAD"); return RC_OK; }
    if (opts[0].seen && opts[0].val > 0) {
        j->state = JS_DELAYED;
        j->owner = 0;
        j->ready_at = g_clock + opts[0].val;
        r_simple(o, "DELAYED");
    } else {
        job_make_ready(j);
        r_simple(o, "READY");
    }
    return RC_OK;
}

static int c_release(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    Opt opts[2] = {{"DELAY", "delay", 1, 0, 86400, 0, 0}, {"PRI", "pri", 1, 0, 9, 0, 0}};
    Job *j = owned_job(c, &cmd->argv[1], o);
    if (!j) return RC_OK;
    if (parse_opts(cmd, 2, opts, 2, o)) return RC_OK;
    if (opts[1].seen) j->pri = (int)opts[1].val;
    if (opts[0].seen && opts[0].val > 0) {
        j->state = JS_DELAYED;
        j->owner = 0;
        j->ready_at = g_clock + opts[0].val;
    } else {
        job_make_ready(j);
    }
    r_simple(o, "RELEASED");
    return RC_OK;
}

static int c_touch(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    Job *j = owned_job(c, &cmd->argv[1], o);
    if (!j) return RC_OK;
    j->deadline = g_clock + j->ttr;
    r_int(o, j->deadline);
    return RC_OK;
}

static int c_peek(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    char qn[MAX_QNAME + 1];
    if (check_qname(&cmd->argv[1], qn, o)) return RC_OK;
    if (!find_queue(c->ns, qn)) { r_err(o, "E_NOQUEUE", "no such queue"); return RC_OK; }
    Job *j = best_ready(c->ns, qn);
    if (!j) { r_nilarr(o); return RC_OK; }
    r_arr(o, 3);
    r_int(o, j->id);
    r_int(o, j->pri);
    r_bulk(o, j->payload, j->plen);
    return RC_OK;
}

static int c_qstat(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    char qn[MAX_QNAME + 1];
    if (check_qname(&cmd->argv[1], qn, o)) return RC_OK;
    Queue *q = find_queue(c->ns, qn);
    if (!q) { r_err(o, "E_NOQUEUE", "no such queue"); return RC_OK; }
    Buf b = {0};
    buf_printf(&b, "name:%s\n", q->name);
    buf_printf(&b, "ready:%d\n", queue_count_state(q, JS_READY));
    buf_printf(&b, "delayed:%d\n", queue_count_state(q, JS_DELAYED));
    buf_printf(&b, "reserved:%d\n", queue_count_state(q, JS_RESERVED));
    buf_printf(&b, "dead:%d\n", queue_count_state(q, JS_DEAD));
    buf_printf(&b, "maxlen:%d\n", q->maxlen);
    buf_printf(&b, "maxnack:%d\n", q->maxnack);
    buf_printf(&b, "pushed:%lld\n", q->pushed);
    buf_printf(&b, "acked:%lld\n", q->acked);
    buf_printf(&b, "nacked:%lld\n", q->nacked);
    buf_printf(&b, "timeouts:%lld\n", q->timeouts);
    r_bulk(o, b.p, b.n);
    buf_free(&b);
    return RC_OK;
}

typedef struct { i64 seq; Job *j; } SeqJob;
static int seqjob_cmp(const void *a, const void *b) {
    const SeqJob *x = a, *y = b;
    return x->seq < y->seq ? -1 : (x->seq > y->seq ? 1 : 0);
}
static int dead_jobs(const char *ns, const char *qn, SeqJob **out) {
    SeqJob *v = malloc(sizeof(SeqJob) * (size_t)(njobs + 1));
    int n = 0;
    for (int i = 0; i < njobs; i++) {
        Job *j = jobs[i];
        if (j->alive && j->state == JS_DEAD && !strcmp(j->ns, ns) && !strcmp(j->q, qn)) { v[n].seq = j->seq; v[n].j = j; n++; }
    }
    qsort(v, (size_t)n, sizeof(SeqJob), seqjob_cmp);
    *out = v;
    return n;
}

static int c_deadlist(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    char qn[MAX_QNAME + 1];
    if (check_qname(&cmd->argv[1], qn, o)) return RC_OK;
    i64 cnt = 1000;
    if (cmd->argc == 3 && parse_num(&cmd->argv[2], 1, 1000, "count", &cnt, o)) return RC_OK;
    if (!find_queue(c->ns, qn)) { r_err(o, "E_NOQUEUE", "no such queue"); return RC_OK; }
    SeqJob *v;
    int n = dead_jobs(c->ns, qn, &v);
    if (n > cnt) n = (int)cnt;
    r_arr(o, n);
    for (int i = 0; i < n; i++) r_int(o, v[i].j->id);
    free(v);
    return RC_OK;
}

static int c_kick(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    char qn[MAX_QNAME + 1];
    if (check_qname(&cmd->argv[1], qn, o)) return RC_OK;
    i64 cnt = 1;
    if (cmd->argc == 3 && parse_num(&cmd->argv[2], 1, 1000, "count", &cnt, o)) return RC_OK;
    Queue *q = find_queue(c->ns, qn);
    if (!q) { r_err(o, "E_NOQUEUE", "no such queue"); return RC_OK; }
    SeqJob *v;
    int n = dead_jobs(c->ns, qn, &v);
    int room = q->maxlen - queue_live_count(q);
    if (room < 0) room = 0;
    int moved = 0;
    for (int i = 0; i < n && moved < cnt && moved < room; i++) {
        v[i].j->nacks = 0;
        job_make_ready(v[i].j);
        moved++;
    }
    free(v);
    r_int(o, moved);
    return RC_OK;
}

static int c_jobinfo(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    i64 id;
    if (parse_num(&cmd->argv[1], 1, 1000000000000LL, "id", &id, o)) return RC_OK;
    Job *j = find_job(id);
    if (!j || strcmp(j->ns, c->ns)) { r_err(o, "E_NOJOB", "no such job"); return RC_OK; }
    static const char *names[] = {"?", "ready", "delayed", "reserved", "dead"};
    Buf b = {0};
    buf_printf(&b, "id:%lld\n", j->id);
    buf_printf(&b, "queue:%s\n", j->q);
    buf_printf(&b, "state:%s\n", names[j->state]);
    buf_printf(&b, "pri:%d\n", j->pri);
    buf_printf(&b, "ttr:%d\n", j->ttr);
    buf_printf(&b, "nacks:%d\n", j->nacks);
    buf_printf(&b, "reserves:%d\n", j->reserves);
    buf_printf(&b, "timeouts:%d\n", j->timeouts);
    if (j->state == JS_DELAYED) buf_printf(&b, "ready_at:%lld\n", j->ready_at);
    if (j->state == JS_RESERVED) {
        buf_printf(&b, "owner:%d\n", j->owner);
        buf_printf(&b, "deadline:%lld\n", j->deadline);
    }
    r_bulk(o, b.p, b.n);
    buf_free(&b);
    return RC_OK;
}

/* ---- key/value commands ---- */

static int check_key(Tok *t, Buf *o) {
    if (t->n < 1 || t->n > MAX_KEYLEN) { r_err(o, "E_KEY", "key length must be between 1 and 64"); return -1; }
    return 0;
}

static int c_set(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    if (check_key(&cmd->argv[1], o)) return RC_OK;
    if (cmd->argv[2].n > MAX_VALLEN) { r_err(o, "E_VALUE", "value too long"); return RC_OK; }
    Opt opts[3] = {{"TTL", "ttl", 1, 1, 1000000, 0, 0}, {"NX", "nx", 0, 0, 0, 0, 0}, {"XX", "xx", 0, 0, 0, 0, 0}};
    if (parse_opts(cmd, 3, opts, 3, o)) return RC_OK;
    if (opts[1].seen && opts[2].seen) { r_err(o, "E_SYNTAX", "NX and XX are mutually exclusive"); return RC_OK; }
    Key *x = find_key(c->ns, cmd->argv[1].p, cmd->argv[1].n);
    if ((opts[1].seen && x) || (opts[2].seen && !x)) { r_nilbulk(o); return RC_OK; }
    if (!x) {
        if (count_keys(c->ns) >= MAX_KEYS_PER_NS) { r_err(o, "E_LIMIT", "too many keys in namespace"); return RC_OK; }
        x = new_key(c->ns, cmd->argv[1].p, cmd->argv[1].n);
    }
    key_setval(x, cmd->argv[2].p, cmd->argv[2].n);
    x->version++;
    x->expire_at = opts[0].seen ? g_clock + opts[0].val : 0;
    r_int(o, x->version);
    return RC_OK;
}

static int c_get(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    if (check_key(&cmd->argv[1], o)) return RC_OK;
    Key *x = find_key(c->ns, cmd->argv[1].p, cmd->argv[1].n);
    if (!x) { r_nilbulk(o); return RC_OK; }
    r_bulk(o, x->v, x->vn);
    return RC_OK;
}

static int c_getv(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    if (check_key(&cmd->argv[1], o)) return RC_OK;
    Key *x = find_key(c->ns, cmd->argv[1].p, cmd->argv[1].n);
    if (!x) { r_nilarr(o); return RC_OK; }
    r_arr(o, 2);
    r_int(o, x->version);
    r_bulk(o, x->v, x->vn);
    return RC_OK;
}

static int c_cas(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    if (check_key(&cmd->argv[1], o)) return RC_OK;
    i64 ver;
    if (parse_num(&cmd->argv[2], 0, 1000000000000LL, "version", &ver, o)) return RC_OK;
    if (cmd->argv[3].n > MAX_VALLEN) { r_err(o, "E_VALUE", "value too long"); return RC_OK; }
    Key *x = find_key(c->ns, cmd->argv[1].p, cmd->argv[1].n);
    i64 cur = x ? x->version : 0;
    if (cur != ver) { r_err(o, "E_VERSION", "version mismatch (current %lld)", cur); return RC_OK; }
    if (!x) {
        if (count_keys(c->ns) >= MAX_KEYS_PER_NS) { r_err(o, "E_LIMIT", "too many keys in namespace"); return RC_OK; }
        x = new_key(c->ns, cmd->argv[1].p, cmd->argv[1].n);
    }
    key_setval(x, cmd->argv[3].p, cmd->argv[3].n);
    x->version++;
    r_int(o, x->version);
    return RC_OK;
}

static int c_del(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    for (int i = 1; i < cmd->argc; i++)
        if (check_key(&cmd->argv[i], o)) return RC_OK;
    int n = 0;
    for (int i = 1; i < cmd->argc; i++) {
        Key *x = find_key(c->ns, cmd->argv[i].p, cmd->argv[i].n);
        if (x) { free_key(x); n++; }
    }
    r_int(o, n);
    return RC_OK;
}

static int parse_stored_int(const char *p, size_t n, i64 *out) {
    size_t i = 0;
    int neg = 0;
    if (n > 0 && p[0] == '-') { neg = 1; i = 1; }
    if (i >= n || n - i > 18) return -1;
    i64 v = 0;
    for (size_t k = i; k < n; k++) {
        if (p[k] < '0' || p[k] > '9') return -1;
        v = v * 10 + (p[k] - '0');
    }
    *out = neg ? -v : v;
    return 0;
}

static int c_incr(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    if (check_key(&cmd->argv[1], o)) return RC_OK;
    i64 delta = 1;
    if (cmd->argc == 3 && parse_num(&cmd->argv[2], -1000000000LL, 1000000000LL, "delta", &delta, o)) return RC_OK;
    Key *x = find_key(c->ns, cmd->argv[1].p, cmd->argv[1].n);
    i64 cur = 0;
    if (x && parse_stored_int(x->v, x->vn, &cur)) { r_err(o, "E_NOTINT", "value is not an integer"); return RC_OK; }
    i64 nv = cur + delta;
    if (nv > INT_LIMIT || nv < -INT_LIMIT) { r_err(o, "E_OVERFLOW", "increment would overflow"); return RC_OK; }
    if (!x) {
        if (count_keys(c->ns) >= MAX_KEYS_PER_NS) { r_err(o, "E_LIMIT", "too many keys in namespace"); return RC_OK; }
        x = new_key(c->ns, cmd->argv[1].p, cmd->argv[1].n);
    }
    char tmp[32];
    int tn = snprintf(tmp, sizeof tmp, "%lld", nv);
    key_setval(x, tmp, (size_t)tn);
    x->version++;
    r_int(o, nv);
    return RC_OK;
}

static int glob_match(const char *p, size_t pn, const char *s, size_t sn) {
    size_t pi = 0, si = 0, star_p = (size_t)-1, star_s = 0;
    while (si < sn) {
        if (pi < pn && (p[pi] == '?' || p[pi] == s[si])) { pi++; si++; }
        else if (pi < pn && p[pi] == '*') { star_p = pi++; star_s = si; }
        else if (star_p != (size_t)-1) { pi = star_p + 1; si = ++star_s; }
        else return 0;
    }
    while (pi < pn && p[pi] == '*') pi++;
    return pi == pn;
}

static int keycmp(const void *a, const void *b) {
    const Key *x = *(Key *const *)a, *y = *(Key *const *)b;
    size_t m = x->kn < y->kn ? x->kn : y->kn;
    int r = memcmp(x->k, y->k, m);
    if (r) return r;
    return x->kn < y->kn ? -1 : (x->kn > y->kn ? 1 : 0);
}

static int c_keys(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    const char *pat = "*";
    size_t pn = 1;
    if (cmd->argc == 2) {
        if (cmd->argv[1].n > MAX_KEYLEN) { r_err(o, "E_PATTERN", "pattern too long"); return RC_OK; }
        pat = cmd->argv[1].p;
        pn = cmd->argv[1].n;
    }
    Key **v = malloc(sizeof(Key *) * (size_t)(nkeys + 1));
    int n = 0;
    for (int i = 0; i < nkeys; i++) {
        Key *x = keys[i];
        if (x->alive && !strcmp(x->ns, c->ns) && glob_match(pat, pn, x->k, x->kn)) v[n++] = x;
    }
    qsort(v, (size_t)n, sizeof(Key *), keycmp);
    r_arr(o, n);
    for (int i = 0; i < n; i++) r_bulk(o, v[i]->k, v[i]->kn);
    free(v);
    return RC_OK;
}

static int c_ttl(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    if (check_key(&cmd->argv[1], o)) return RC_OK;
    Key *x = find_key(c->ns, cmd->argv[1].p, cmd->argv[1].n);
    if (!x) r_int(o, -2);
    else if (!x->expire_at) r_int(o, -1);
    else r_int(o, x->expire_at - g_clock);
    return RC_OK;
}

static int c_expire(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)ie;
    if (check_key(&cmd->argv[1], o)) return RC_OK;
    i64 t;
    if (parse_num(&cmd->argv[2], 0, 1000000, "ttl", &t, o)) return RC_OK;
    Key *x = find_key(c->ns, cmd->argv[1].p, cmd->argv[1].n);
    if (!x) { r_int(o, 0); return RC_OK; }
    x->expire_at = t == 0 ? 0 : g_clock + t;
    r_int(o, 1);
    return RC_OK;
}

/* ---- server commands ---- */

static int count_namespaces(void) {
    /* number of distinct namespaces holding a queue or a key */
    char seen[256][MAX_NSNAME + 1];
    int n = 0;
    for (int pass = 0; pass < 2; pass++) {
        int lim = pass ? nkeys : nqueues;
        for (int i = 0; i < lim; i++) {
            const char *ns;
            if (pass) { if (!keys[i]->alive) continue; ns = keys[i]->ns; }
            else { if (!queues[i]->alive) continue; ns = queues[i]->ns; }
            int dup = 0;
            for (int k = 0; k < n; k++) if (!strcmp(seen[k], ns)) { dup = 1; break; }
            if (!dup && n < 256) { snprintf(seen[n], sizeof seen[n], "%s", ns); n++; }
        }
    }
    return n;
}

static void info_section(Buf *b, const char *sec) {
    if (!strcmp(sec, "SERVER")) {
        buf_adds(b, "# Server\n");
        buf_printf(b, "version:%s\n", QSRV_VERSION);
        buf_printf(b, "clock:%lld\n", g_clock);
    } else if (!strcmp(sec, "CLIENTS")) {
        int cc = 0, bc = 0;
        for (int i = 0; i < MAX_CONNS; i++) if (conns[i].used && !conns[i].closing) { cc++; if (conns[i].blocked) bc++; }
        buf_adds(b, "# Clients\n");
        buf_printf(b, "connected:%d\n", cc);
        buf_printf(b, "blocked:%d\n", bc);
        buf_printf(b, "total_connections:%lld\n", g_total_conns);
    } else if (!strcmp(sec, "DATA")) {
        int jq = 0, jk = 0, jj = 0;
        for (int i = 0; i < nqueues; i++) if (queues[i]->alive) jq++;
        for (int i = 0; i < nkeys; i++) if (keys[i]->alive) jk++;
        for (int i = 0; i < njobs; i++) if (jobs[i]->alive) jj++;
        buf_adds(b, "# Data\n");
        buf_printf(b, "namespaces:%d\n", count_namespaces());
        buf_printf(b, "queues:%d\n", jq);
        buf_printf(b, "jobs:%d\n", jj);
        buf_printf(b, "keys:%d\n", jk);
    } else if (!strcmp(sec, "STATS")) {
        buf_adds(b, "# Stats\n");
        buf_printf(b, "commands:%lld\n", g_commands);
        buf_printf(b, "jobs_pushed:%lld\n", g_pushed);
        buf_printf(b, "jobs_acked:%lld\n", g_acked);
        buf_printf(b, "keys_expired:%lld\n", g_expired_keys);
    }
}

static int c_info(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)c; (void)ie;
    static const char *secs[] = {"SERVER", "CLIENTS", "DATA", "STATS"};
    Buf b = {0};
    buf_reserve(&b, 0);
    if (cmd->argc == 1 || tok_ieq(&cmd->argv[1], "ALL")) {
        for (int i = 0; i < 4; i++) {
            if (i) buf_adds(&b, "\n");
            info_section(&b, secs[i]);
        }
    } else {
        int found = -1;
        for (int i = 0; i < 4; i++) if (tok_ieq(&cmd->argv[1], secs[i])) found = i;
        if (found < 0) {
            char w[32];
            sanitize_word(cmd->argv[1].p, cmd->argv[1].n, w, sizeof w);
            r_err(o, "E_SYNTAX", "unknown section '%s'", w);
            buf_free(&b);
            return RC_OK;
        }
        info_section(&b, secs[found]);
    }
    r_bulk(o, b.p, b.n);
    buf_free(&b);
    return RC_OK;
}

static int c_flush(Conn *c, Cmd *cmd, Buf *o, int ie) {
    (void)cmd; (void)ie;
    int n = 0;
    for (int i = 0; i < nkeys; i++)
        if (keys[i]->alive && !strcmp(keys[i]->ns, c->ns)) { free_key(keys[i]); n++; }
    for (int i = 0; i < njobs; i++)
        if (jobs[i]->alive && !strcmp(jobs[i]->ns, c->ns)) { free_job(jobs[i]); n++; }
    for (int i = 0; i < nqueues; i++)
        if (queues[i]->alive && !strcmp(queues[i]->ns, c->ns)) queues[i]->alive = 0;
    fail_waiters_ns(c->ns, NULL);
    r_int(o, n);
    return RC_OK;
}

/* MULTI/EXEC/DISCARD are handled in dispatch; the table entries exist for arity */
static int c_dummy(Conn *c, Cmd *cmd, Buf *o, int ie) { (void)c; (void)cmd; (void)o; (void)ie; return RC_OK; }

static const CmdDef CMDS[] = {
    {"PING", 0, 1, c_ping},
    {"ECHO", 1, 1, c_echo},
    {"USE", 1, 1, c_use},
    {"WHOAMI", 0, 0, c_whoami},
    {"QUIT", 0, 0, c_quit},
    {"TICK", 1, 1, c_tick},
    {"TIME", 0, 0, c_time},
    {"QDECLARE", 1, 5, c_qdeclare},
    {"QDROP", 1, 1, c_qdrop},
    {"QLIST", 0, 0, c_qlist},
    {"PUSH", 2, 8, c_push},
    {"RESERVE", 1, 1, c_reserve},
    {"BRESERVE", 2, 2, c_breserve},
    {"ACK", 1, 1, c_ack},
    {"NACK", 1, 3, c_nack},
    {"RELEASE", 1, 5, c_release},
    {"TOUCH", 1, 1, c_touch},
    {"PEEK", 1, 1, c_peek},
    {"QSTAT", 1, 1, c_qstat},
    {"DEADLIST", 1, 2, c_deadlist},
    {"KICK", 1, 2, c_kick},
    {"JOBINFO", 1, 1, c_jobinfo},
    {"SET", 2, 6, c_set},
    {"GET", 1, 1, c_get},
    {"GETV", 1, 1, c_getv},
    {"CAS", 3, 3, c_cas},
    {"DEL", 1, -1, c_del},
    {"INCR", 1, 2, c_incr},
    {"KEYS", 0, 1, c_keys},
    {"TTL", 1, 1, c_ttl},
    {"EXPIRE", 2, 2, c_expire},
    {"INFO", 0, 1, c_info},
    {"FLUSH", 0, 0, c_flush},
    {"MULTI", 0, 0, c_dummy},
    {"EXEC", 0, 0, c_dummy},
    {"DISCARD", 0, 0, c_dummy},
};
#define NCMDS ((int)(sizeof CMDS / sizeof CMDS[0]))

static const CmdDef *lookup(const Tok *t) {
    for (int i = 0; i < NCMDS; i++)
        if (tok_ieq(t, CMDS[i].name)) return &CMDS[i];
    return NULL;
}

static void lower_name(const char *s, char *out) {
    size_t i = 0;
    for (; s[i]; i++) out[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] + 32) : s[i];
    out[i] = 0;
}

/* returns 0 if arity is fine; otherwise writes E_ARITY */
static int check_arity(const CmdDef *d, Cmd *cmd, Buf *o) {
    int n = cmd->argc - 1;
    if (n < d->minargs || (d->maxargs >= 0 && n > d->maxargs)) {
        char ln[32];
        lower_name(d->name, ln);
        r_err(o, "E_ARITY", "wrong number of arguments for '%s'", ln);
        return -1;
    }
    return 0;
}

static void tx_clear(Conn *c) {
    for (int i = 0; i < c->ntx; i++) { cmd_free(c->tx[i]); free(c->tx[i]); }
    c->ntx = 0;
    c->inmulti = 0;
    c->txabort = 0;
}

static int run_cmd(Conn *c, const CmdDef *d, Cmd *cmd, Buf *o, int in_exec) {
    int rc = d->fn(c, cmd, o, in_exec);
    serve_waiters();
    return rc;
}

/* processes one complete line. returns RC_* */
static int process_line(Conn *c, const char *line, size_t n, Buf *o) {
    Cmd cmd;
    int lr = lex(line, n, &cmd, o);
    if (lr == 0) return RC_OK;
    g_commands++;
    if (lr < 0) {
        if (c->inmulti) c->txabort = 1;
        return RC_OK;
    }
    const CmdDef *d = lookup(&cmd.argv[0]);
    if (!d) {
        char w[32];
        sanitize_word(cmd.argv[0].p, cmd.argv[0].n, w, sizeof w);
        r_err(o, "E_UNKNOWN", "unknown command '%s'", w);
        if (c->inmulti) c->txabort = 1;
        cmd_free(&cmd);
        return RC_OK;
    }
    if (check_arity(d, &cmd, o)) {
        if (c->inmulti) c->txabort = 1;
        cmd_free(&cmd);
        return RC_OK;
    }
    int rc = RC_OK;
    if (!strcmp(d->name, "MULTI")) {
        if (c->inmulti) r_err(o, "E_NESTED", "MULTI calls can not be nested");
        else { c->inmulti = 1; c->txabort = 0; c->ntx = 0; r_simple(o, "OK"); }
    } else if (!strcmp(d->name, "DISCARD")) {
        if (!c->inmulti) r_err(o, "E_NOMULTI", "DISCARD without MULTI");
        else { tx_clear(c); r_simple(o, "OK"); }
    } else if (!strcmp(d->name, "EXEC")) {
        if (!c->inmulti) r_err(o, "E_NOMULTI", "EXEC without MULTI");
        else if (c->txabort) { tx_clear(c); r_err(o, "E_ABORTED", "transaction discarded because of previous errors"); }
        else {
            int nq = c->ntx;
            Cmd *q[MAX_TX];
            for (int i = 0; i < nq; i++) q[i] = c->tx[i];
            c->ntx = 0;
            c->inmulti = 0;
            r_arr(o, nq);
            for (int i = 0; i < nq; i++) {
                const CmdDef *qd = lookup(&q[i]->argv[0]);
                run_cmd(c, qd, q[i], o, 1);
                cmd_free(q[i]);
                free(q[i]);
            }
        }
    } else if (c->inmulti && !strcmp(d->name, "QUIT")) {
        tx_clear(c);
        rc = run_cmd(c, d, &cmd, o, 0);
    } else if (c->inmulti && !strcmp(d->name, "USE")) {
        r_err(o, "E_INMULTI", "USE is not allowed inside MULTI");
    } else if (c->inmulti) {
        if (c->ntx >= MAX_TX) {
            r_err(o, "E_TXFULL", "transaction queue is full");
            c->txabort = 1;
        } else {
            Cmd *copy = malloc(sizeof *copy);
            *copy = cmd;
            cmd.argc = 0;
            c->tx[c->ntx++] = copy;
            r_simple(o, "QUEUED");
        }
    } else {
        rc = run_cmd(c, d, &cmd, o, 0);
    }
    cmd_free(&cmd);
    return rc;
}

/* ------------------------------------------------------------------ */
/* networking                                                          */
/* ------------------------------------------------------------------ */

static void send_to(Conn *c, Buf *o) {
    if (c->closing || c->fd < 0 || o->n == 0) return;
    size_t off = 0;
    while (off < o->n) {
        ssize_t w = send(c->fd, o->p + off, o->n - off, MSG_NOSIGNAL);
        if (w < 0) {
            if (errno == EINTR) continue;
            c->closing = 1;
            return;
        }
        off += (size_t)w;
    }
}

static void conn_close(Conn *c) {
    if (!c->used) return;
    if (c->fd >= 0) close(c->fd);
    c->fd = -1;
    c->blocked = 0;
    /* release reserved jobs of this client, lowest id first */
    for (;;) {
        Job *best = NULL;
        for (int i = 0; i < njobs; i++) {
            Job *j = jobs[i];
            if (j->alive && j->state == JS_RESERVED && j->owner == c->id && (!best || j->id < best->id)) best = j;
        }
        if (!best) break;
        job_make_ready(best);
    }
    tx_clear(c);
    buf_free(&c->in);
    c->used = 0;
    c->closing = 0;
    serve_waiters();
}

/* process buffered complete lines while not blocked. returns 1 if the connection was closed */
static int drain(Conn *c) {
    while (c->used && !c->blocked && !c->closing) {
        char *nl = memchr(c->in.p ? c->in.p : "", '\n', c->in.n);
        if (!nl) {
            if (c->in.n > MAX_LINE) {
                Buf o = {0};
                r_err(&o, "E_LINE", "line too long");
                send_to(c, &o);
                buf_free(&o);
                conn_close(c);
                return 1;
            }
            break;
        }
        size_t raw = (size_t)(nl - c->in.p);
        if (raw > MAX_LINE) {
            Buf o = {0};
            r_err(&o, "E_LINE", "line too long");
            send_to(c, &o);
            buf_free(&o);
            conn_close(c);
            return 1;
        }
        size_t ln = raw;
        if (ln > 0 && c->in.p[ln - 1] == '\r') ln--;
        char *line = malloc(ln + 1);
        memcpy(line, c->in.p, ln);
        line[ln] = 0;
        buf_consume(&c->in, raw + 1);
        Buf o = {0};
        int rc = process_line(c, line, ln, &o);
        free(line);
        send_to(c, &o);
        buf_free(&o);
        if (rc == RC_CLOSE || c->closing) { conn_close(c); return 1; }
        compact();
    }
    return c->used ? 0 : 1;
}

static void pump(void) {
    for (;;) {
        Conn *pick = NULL;
        for (int i = 0; i < MAX_CONNS; i++) {
            Conn *w = &conns[i];
            if (w->used && w->woken && (!pick || w->id < pick->id)) pick = w;
        }
        if (!pick) break;
        pick->woken = 0;
        drain(pick);
    }
}

static void usage(void) {
    fprintf(stderr, "usage: qsrv [--port PORT]\n");
    exit(2);
}

int main(int argc, char **argv) {
    int port = 7070;
    for (int i = 1; i < argc; i++) {
        if ((!strcmp(argv[i], "--port") || !strcmp(argv[i], "-p")) && i + 1 < argc) {
            port = atoi(argv[++i]);
            if (port <= 0 || port > 65535) usage();
        } else if (!strcmp(argv[i], "--version")) {
            printf("qsrv %s\n", QSRV_VERSION);
            return 0;
        } else {
            usage();
        }
    }
    signal(SIGPIPE, SIG_IGN);
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0) { perror("socket"); return 1; }
    int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(ls, (struct sockaddr *)&a, sizeof a) < 0) { perror("bind"); return 1; }
    if (listen(ls, 64) < 0) { perror("listen"); return 1; }
    for (int i = 0; i < MAX_CONNS; i++) conns[i].fd = -1;

    for (;;) {
        struct pollfd pf[MAX_CONNS + 1];
        int map[MAX_CONNS + 1];
        int np = 0;
        pf[np].fd = ls; pf[np].events = POLLIN; map[np] = -1; np++;
        for (int i = 0; i < MAX_CONNS; i++) {
            if (!conns[i].used) continue;
            pf[np].fd = conns[i].fd; pf[np].events = POLLIN; map[np] = i; np++;
        }
        int r = poll(pf, (nfds_t)np, -1);
        if (r < 0) { if (errno == EINTR) continue; perror("poll"); return 1; }
        if (pf[0].revents & POLLIN) {
            int fd = accept(ls, NULL, NULL);
            if (fd >= 0) {
                setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
                int slot = -1;
                for (int i = 0; i < MAX_CONNS; i++) if (!conns[i].used) { slot = i; break; }
                if (slot < 0) {
                    const char *m = "-E_LIMIT too many connections\r\n";
                    if (send(fd, m, strlen(m), MSG_NOSIGNAL) < 0) { /* ignore */ }
                    close(fd);
                } else {
                    Conn *c = &conns[slot];
                    memset(c, 0, sizeof *c);
                    c->used = 1;
                    c->fd = fd;
                    c->id = g_next_conn++;
                    snprintf(c->ns, sizeof c->ns, "default");
                    g_total_conns++;
                }
            }
            continue; /* re-poll so connection order is preserved */
        }
        for (int k = 1; k < np; k++) {
            if (!(pf[k].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            Conn *c = &conns[map[k]];
            if (!c->used || c->fd != pf[k].fd) continue;
            char tmp[8192];
            ssize_t n = recv(c->fd, tmp, sizeof tmp, 0);
            if (n <= 0) {
                if (n < 0 && errno == EINTR) continue;
                conn_close(c);
                pump();
                continue;
            }
            buf_add(&c->in, tmp, (size_t)n);
            drain(c);
            pump();
        }
    }
    return 0;
}
