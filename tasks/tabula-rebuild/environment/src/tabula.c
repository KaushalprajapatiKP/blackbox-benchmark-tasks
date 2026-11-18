/*
 * tabula - a small CSV/TSV table processor
 *
 * MIT License
 *
 * Copyright (c) 2026 The tabula authors
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
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define VERSION "2.3.1"

typedef __int128 i128;
typedef unsigned __int128 u128;

__attribute__((used)) static const char tabula_ident[] = "TABULA-IDENT-3f9c2a7e51d04b88";

/* ------------------------------------------------------------------ */
/* memory and string helpers                                           */
/* ------------------------------------------------------------------ */

static void oom(void) {
    fputs("tabula: out of memory\n", stderr);
    exit(70);
}

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) oom();
    return p;
}

static void *xrealloc(void *p, size_t n) {
    p = realloc(p, n ? n : 1);
    if (!p) oom();
    return p;
}

static char *xstrndup(const char *s, size_t n) {
    char *r = xmalloc(n + 1);
    memcpy(r, s, n);
    r[n] = 0;
    return r;
}

static char *xstrdup(const char *s) { return xstrndup(s, strlen(s)); }

typedef struct {
    char *p;
    size_t n, cap;
} SB;

static void sb_putn(SB *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t c = b->cap ? b->cap : 64;
        while (c < b->n + n + 1) c *= 2;
        b->p = xrealloc(b->p, c);
        b->cap = c;
    }
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}

static void sb_puts(SB *b, const char *s) { sb_putn(b, s, strlen(s)); }
static void sb_putc(SB *b, char c) { sb_putn(b, &c, 1); }

static char *sb_take(SB *b) {
    char *r;
    if (!b->p) sb_putn(b, "", 0);
    r = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return r;
}

/* ------------------------------------------------------------------ */
/* diagnostics                                                         */
/* ------------------------------------------------------------------ */

static void die(int code, const char *fmt, ...) {
    va_list ap;
    fputs("tabula: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(code);
}

static void die_usage(const char *fmt, ...) {
    va_list ap;
    fputs("tabula: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fputs("Try 'tabula --help' for more information.\n", stderr);
    exit(1);
}

/* ------------------------------------------------------------------ */
/* exact decimals                                                      */
/* ------------------------------------------------------------------ */

static i128 P10[39];

static void init_p10(void) {
    int i;
    P10[0] = 1;
    for (i = 1; i < 39; i++) P10[i] = P10[i - 1] * 10;
}

typedef struct {
    i128 m;
    int s;
} Dec;

static int dec_parse(const char *p, Dec *d) {
    const char *q = p;
    int neg = 0, digits = 0, scale = 0, dot = 0;
    i128 m = 0;
    if (*q == '+' || *q == '-') {
        neg = (*q == '-');
        q++;
    }
    for (; *q; q++) {
        if (*q >= '0' && *q <= '9') {
            digits++;
            if (digits > 15) return 0;
            m = m * 10 + (*q - '0');
            if (dot) scale++;
        } else if (*q == '.' && !dot) {
            dot = 1;
        } else {
            return 0;
        }
    }
    if (!digits) return 0;
    if (d) {
        d->m = neg ? -m : m;
        d->s = scale;
    }
    return 1;
}

static int is_num(const char *p) { return dec_parse(p, NULL); }

static int dec_cmp(Dec a, Dec b) {
    int s = a.s > b.s ? a.s : b.s;
    i128 x = a.m * P10[s - a.s];
    i128 y = b.m * P10[s - b.s];
    return x < y ? -1 : (x > y ? 1 : 0);
}

static void render_i128(SB *b, i128 m, int scale, int thousands) {
    char digs[80];
    int n = 0, i, intlen;
    int neg = m < 0;
    u128 u = neg ? (u128)0 - (u128)m : (u128)m;
    do {
        digs[n++] = (char)('0' + (int)(u % 10));
        u /= 10;
    } while (u);
    while (n < scale + 1) digs[n++] = '0';
    if (neg) sb_putc(b, '-');
    intlen = n - scale;
    for (i = 0; i < intlen; i++) {
        int rem = intlen - 1 - i;
        sb_putc(b, digs[n - 1 - i]);
        if (thousands && rem > 0 && rem % 3 == 0) sb_putc(b, ',');
    }
    if (scale > 0) {
        sb_putc(b, '.');
        for (i = intlen; i < n; i++) sb_putc(b, digs[n - 1 - i]);
    }
}

enum { R_HALF_EVEN, R_HALF_UP, R_DOWN, R_UP };

static i128 round_div(i128 num, i128 den, int mode) {
    int neg = num < 0;
    i128 a = neg ? -num : num;
    i128 q = a / den, r = a % den;
    switch (mode) {
    case R_DOWN:
        break;
    case R_UP:
        if (r) q++;
        break;
    case R_HALF_UP:
        if (2 * r >= den) q++;
        break;
    default:
        if (2 * r > den || (2 * r == den && (q & 1))) q++;
        break;
    }
    return neg ? -q : q;
}

static i128 dec_round_to(Dec d, int n, int mode) {
    if (d.s <= n) return d.m * P10[n - d.s];
    return round_div(d.m, P10[d.s - n], mode);
}

/* ------------------------------------------------------------------ */
/* options                                                             */
/* ------------------------------------------------------------------ */

enum { Q_MINIMAL, Q_ALL, Q_NONNUMERIC, Q_NONE };

static struct {
    char in_delim;
    int in_delim_set;
    char out_delim;
    int out_delim_set;
    int no_header, no_header_out;
    SB select, exclude, sortkeys, group, agg;
    int has_select, has_exclude, has_sort, has_group, has_agg;
    char **where;
    int nwhere;
    int unique;
    long limit, skip;
    int precision;
    int round_mode;
    int thousands;
    int quote_mode, quote_set;
    const char *empty;
    int pretty;
    int max_width;
    int columns, count, strict, trim;
    const char *file;
} O;

enum { OPT_STRICT = 1001, OPT_TRIM = 1002 };

typedef struct {
    const char *name;
    int has_arg;
    int code;
} LongOpt;

static const LongOpt LONGOPTS[] = {
    {"delimiter", 1, 'd'},  {"out-delimiter", 1, 'D'}, {"no-header", 0, 'H'},
    {"no-header-out", 0, 'N'}, {"select", 1, 's'},     {"exclude", 1, 'x'},
    {"where", 1, 'w'},      {"sort", 1, 'S'},          {"group", 1, 'g'},
    {"agg", 1, 'a'},        {"unique", 0, 'u'},        {"limit", 1, 'n'},
    {"skip", 1, 'k'},       {"precision", 1, 'p'},     {"round", 1, 'r'},
    {"thousands", 0, 'T'},  {"quote", 1, 'q'},         {"empty", 1, 'e'},
    {"pretty", 0, 't'},     {"max-width", 1, 'm'},     {"columns", 0, 'c'},
    {"count", 0, 'C'},      {"strict", 0, OPT_STRICT}, {"trim", 0, OPT_TRIM},
    {"help", 0, 'h'},       {"version", 0, 'V'},       {NULL, 0, 0}};

static const char SHORT_ALL[] = "dDHNsxwSgaunkprTqetmcChV";
static const char SHORT_ARG[] = "dDsxwSgankprqem";

static const char HELP[] =
    "Usage: tabula [OPTION]... [FILE]\n"
    "Process a CSV or TSV table read from FILE (or standard input when FILE is\n"
    "absent or '-') and write the result to standard output.\n"
    "\n"
    "Input:\n"
    "  -d, --delimiter=CHAR      input field delimiter (default ',' or TAB for *.tsv)\n"
    "  -H, --no-header           input has no header row; columns are named c1, c2, ...\n"
    "      --trim                strip leading and trailing blanks from every field\n"
    "      --strict              reject rows whose field count differs from the header\n"
    "\n"
    "Selection and filtering:\n"
    "  -s, --select=LIST         output only the listed columns, in the given order\n"
    "  -x, --exclude=LIST        output every column except the listed ones\n"
    "  -w, --where=EXPR          keep only rows for which EXPR is true\n"
    "  -u, --unique              drop repeated output rows\n"
    "  -k, --skip=N              skip the first N output rows\n"
    "  -n, --limit=N             output at most N rows\n"
    "\n"
    "Ordering and grouping:\n"
    "  -S, --sort=KEYS           sort rows by KEYS (COL[:FLAGS], FLAGS from n N r i)\n"
    "  -g, --group=LIST          group rows by the listed columns\n"
    "  -a, --agg=SPECS           aggregate columns ([NAME=]FUNC(COL), comma separated)\n"
    "\n"
    "Output:\n"
    "  -D, --out-delimiter=CHAR  output field delimiter (default: the input delimiter)\n"
    "  -N, --no-header-out       do not print the header row\n"
    "  -q, --quote=MODE          quoting: minimal, all, nonnumeric or none\n"
    "  -e, --empty=STR           print STR in place of empty fields\n"
    "  -p, --precision=N         print numbers with N decimal places (0-9)\n"
    "  -r, --round=MODE          rounding for --precision: half-even, half-up, down, up\n"
    "  -T, --thousands           group integer digits with commas\n"
    "  -t, --pretty              print an aligned table instead of CSV\n"
    "  -m, --max-width=N         with --pretty, shorten cells wider than N columns\n"
    "  -c, --columns             list the column names and exit\n"
    "  -C, --count               print the number of output rows and exit\n"
    "\n"
    "  -h, --help                display this help and exit\n"
    "  -V, --version             output version information and exit\n"
    "\n"
    "Exit status: 0 success, 1 usage error, 2 bad expression or spec,\n"
    "3 bad column reference, 4 output error, 5 input error, 6 file error.\n";

static long parse_num(const char *s, const char *name) {
    size_t i, n = strlen(s);
    if (n == 0 || n > 9) die(1, "invalid number for %s: '%s'", name, s);
    for (i = 0; i < n; i++)
        if (s[i] < '0' || s[i] > '9') die(1, "invalid number for %s: '%s'", name, s);
    return atol(s);
}

static char parse_delim(const char *s, const char *name) {
    if (!strcmp(s, "tab") || !strcmp(s, "\\t")) return '\t';
    if (strlen(s) == 1 && s[0] != '"' && s[0] != '\n' && s[0] != '\r') return s[0];
    die(1, "invalid delimiter for %s: '%s'", name, s);
    return 0;
}

static void list_append(SB *b, int *has, const char *arg) {
    if (*has) sb_putc(b, ',');
    sb_puts(b, arg);
    *has = 1;
}

static void apply_opt(int code, const char *arg) {
    long v;
    switch (code) {
    case 'd':
        O.in_delim = parse_delim(arg, "--delimiter");
        O.in_delim_set = 1;
        break;
    case 'D':
        O.out_delim = parse_delim(arg, "--out-delimiter");
        O.out_delim_set = 1;
        break;
    case 'H': O.no_header = 1; break;
    case 'N': O.no_header_out = 1; break;
    case 's': list_append(&O.select, &O.has_select, arg); break;
    case 'x': list_append(&O.exclude, &O.has_exclude, arg); break;
    case 'S': list_append(&O.sortkeys, &O.has_sort, arg); break;
    case 'g': list_append(&O.group, &O.has_group, arg); break;
    case 'a': list_append(&O.agg, &O.has_agg, arg); break;
    case 'w':
        O.where = xrealloc(O.where, sizeof(char *) * (size_t)(O.nwhere + 1));
        O.where[O.nwhere++] = xstrdup(arg);
        break;
    case 'u': O.unique = 1; break;
    case 'n': O.limit = parse_num(arg, "--limit"); break;
    case 'k': O.skip = parse_num(arg, "--skip"); break;
    case 'p':
        v = parse_num(arg, "--precision");
        if (v > 9) die(1, "precision out of range (0-9): '%s'", arg);
        O.precision = (int)v;
        break;
    case 'r':
        if (!strcmp(arg, "half-even")) O.round_mode = R_HALF_EVEN;
        else if (!strcmp(arg, "half-up")) O.round_mode = R_HALF_UP;
        else if (!strcmp(arg, "down")) O.round_mode = R_DOWN;
        else if (!strcmp(arg, "up")) O.round_mode = R_UP;
        else die(1, "invalid rounding mode: '%s' (expected half-even, half-up, down, up)", arg);
        break;
    case 'T': O.thousands = 1; break;
    case 'q':
        if (!strcmp(arg, "minimal")) O.quote_mode = Q_MINIMAL;
        else if (!strcmp(arg, "all")) O.quote_mode = Q_ALL;
        else if (!strcmp(arg, "nonnumeric")) O.quote_mode = Q_NONNUMERIC;
        else if (!strcmp(arg, "none")) O.quote_mode = Q_NONE;
        else die(1, "invalid quote mode: '%s' (expected minimal, all, nonnumeric, none)", arg);
        O.quote_set = 1;
        break;
    case 'e': O.empty = arg; break;
    case 't': O.pretty = 1; break;
    case 'm':
        v = parse_num(arg, "--max-width");
        if (v < 2) die(1, "--max-width must be at least 2");
        O.max_width = (int)v;
        break;
    case 'c': O.columns = 1; break;
    case 'C': O.count = 1; break;
    case OPT_STRICT: O.strict = 1; break;
    case OPT_TRIM: O.trim = 1; break;
    case 'h':
        fputs(HELP, stdout);
        exit(0);
    case 'V':
        fputs("tabula " VERSION "\n", stdout);
        exit(0);
    }
}

static void add_positional(const char *a, int *nfiles) {
    if (*nfiles >= 1) die_usage("extra operand '%s'", a);
    O.file = a;
    (*nfiles)++;
}

static void parse_args(int argc, char **argv) {
    int i, nfiles = 0, only_pos = 0;
    O.limit = -1;
    O.precision = -1;
    O.in_delim = ',';
    for (i = 1; i < argc; i++) {
        char *a = argv[i];
        if (only_pos || a[0] != '-' || a[1] == 0) {
            add_positional(a, &nfiles);
            continue;
        }
        if (!strcmp(a, "--")) {
            only_pos = 1;
            continue;
        }
        if (a[1] == '-') {
            const char *body = a + 2;
            const char *eq = strchr(body, '=');
            size_t nl = eq ? (size_t)(eq - body) : strlen(body);
            int k, found = -1, nmatch = 0;
            const char *arg = NULL;
            for (k = 0; LONGOPTS[k].name; k++) {
                if (strlen(LONGOPTS[k].name) == nl && !strncmp(LONGOPTS[k].name, body, nl)) {
                    found = k;
                    break;
                }
            }
            if (found < 0) {
                for (k = 0; LONGOPTS[k].name; k++)
                    if (!strncmp(LONGOPTS[k].name, body, nl)) {
                        nmatch++;
                        found = k;
                    }
                if (nmatch == 0) die_usage("unrecognized option '--%.*s'", (int)nl, body);
                if (nmatch > 1) {
                    fprintf(stderr, "tabula: option '--%.*s' is ambiguous; possibilities:", (int)nl, body);
                    for (k = 0; LONGOPTS[k].name; k++)
                        if (!strncmp(LONGOPTS[k].name, body, nl)) fprintf(stderr, " '--%s'", LONGOPTS[k].name);
                    fputc('\n', stderr);
                    fputs("Try 'tabula --help' for more information.\n", stderr);
                    exit(1);
                }
            }
            if (!LONGOPTS[found].has_arg) {
                if (eq) die_usage("option '--%s' doesn't allow an argument", LONGOPTS[found].name);
            } else if (eq) {
                arg = eq + 1;
            } else if (i + 1 < argc) {
                arg = argv[++i];
            } else {
                die_usage("option '--%s' requires an argument", LONGOPTS[found].name);
            }
            apply_opt(LONGOPTS[found].code, arg);
            continue;
        }
        {
            int j;
            for (j = 1; a[j]; j++) {
                char c = a[j];
                if (!strchr(SHORT_ALL, c)) die_usage("invalid option -- '%c'", c);
                if (strchr(SHORT_ARG, c)) {
                    const char *arg;
                    if (a[j + 1]) arg = a + j + 1;
                    else if (i + 1 < argc) arg = argv[++i];
                    else die_usage("option requires an argument -- '%c'", c);
                    apply_opt(c, arg);
                    break;
                }
                apply_opt(c, NULL);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* input                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    char **f;
    int n;
    int line;
} Rec;

static int nrecs, caprecs;
static Rec *recs;

static void push_field(Rec *r, SB *f) {
    r->f = xrealloc(r->f, sizeof(char *) * (size_t)(r->n + 1));
    r->f[r->n++] = sb_take(f);
}

static void push_rec(Rec *r) {
    if (nrecs == caprecs) {
        caprecs = caprecs ? caprecs * 2 : 64;
        recs = xrealloc(recs, sizeof(Rec) * (size_t)caprecs);
    }
    recs[nrecs++] = *r;
}

static void parse_csv(const char *buf, size_t len, char delim) {
    size_t pos = 0, i;
    int line = 1;
    for (i = 0; i < len; i++) {
        if (buf[i] == '\n') line++;
        if (buf[i] == 0) die(5, "line %d: NUL byte in input", line);
    }
    line = 1;
    if (len >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF)
        pos = 3;
    while (pos < len) {
        Rec r = {NULL, 0, line};
        if (buf[pos] == '\n') {
            pos++;
            line++;
            continue;
        }
        if (buf[pos] == '\r' && pos + 1 < len && buf[pos + 1] == '\n') {
            pos += 2;
            line++;
            continue;
        }
        for (;;) {
            SB f = {0};
            if (pos < len && buf[pos] == '"') {
                int fline = line;
                pos++;
                for (;;) {
                    if (pos >= len) die(5, "line %d: unterminated quoted field", fline);
                    if (buf[pos] == '"') {
                        if (pos + 1 < len && buf[pos + 1] == '"') {
                            sb_putc(&f, '"');
                            pos += 2;
                            continue;
                        }
                        pos++;
                        break;
                    }
                    if (buf[pos] == '\n') line++;
                    sb_putc(&f, buf[pos]);
                    pos++;
                }
                if (pos >= len) {
                    push_field(&r, &f);
                    break;
                }
                if (buf[pos] == delim) {
                    pos++;
                    push_field(&r, &f);
                    continue;
                }
                if (buf[pos] == '\n') {
                    pos++;
                    line++;
                    push_field(&r, &f);
                    break;
                }
                if (buf[pos] == '\r' && pos + 1 < len && buf[pos + 1] == '\n') {
                    pos += 2;
                    line++;
                    push_field(&r, &f);
                    break;
                }
                die(5, "line %d: unexpected character after closing quote", line);
            }
            while (pos < len && buf[pos] != delim && buf[pos] != '\n' &&
                   !(buf[pos] == '\r' && pos + 1 < len && buf[pos + 1] == '\n')) {
                sb_putc(&f, buf[pos]);
                pos++;
            }
            push_field(&r, &f);
            if (pos >= len) break;
            if (buf[pos] == delim) {
                pos++;
                continue;
            }
            if (buf[pos] == '\n') {
                pos++;
            } else {
                pos += 2;
            }
            line++;
            break;
        }
        push_rec(&r);
    }
}

static char *read_all(FILE *fp, size_t *len) {
    SB b = {0};
    char tmp[65536];
    size_t n;
    while ((n = fread(tmp, 1, sizeof tmp, fp)) > 0) sb_putn(&b, tmp, n);
    *len = b.n;
    return sb_take(&b);
}

static const char *errtext(int e) {
    switch (e) {
    case ENOENT: return "No such file or directory";
    case EACCES: return "Permission denied";
    case ENOTDIR: return "Not a directory";
    case EISDIR: return "Is a directory";
    default: return "Input/output error";
    }
}

/* ------------------------------------------------------------------ */
/* table                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    int ncols;
    char **names;
    int nrows;
    char ***rows;
} Table;

static int find_col(const Table *t, const char *name) {
    int i;
    for (i = 0; i < t->ncols; i++)
        if (!strcmp(t->names[i], name)) return i;
    return -1;
}

static int all_digits(const char *s, size_t n) {
    size_t i;
    if (n == 0) return 0;
    for (i = 0; i < n; i++)
        if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

/* index text -> 0-based column, or dies */
static int index_col(const char *s, size_t n, int ncols) {
    long v;
    if (n > 9) die(3, "column index out of range: %.*s", (int)n, s);
    v = atol(xstrndup(s, n));
    if (v < 1 || v > ncols) die(3, "column index out of range: %.*s", (int)n, s);
    return (int)(v - 1);
}

/* split a comma list; returns item count */
static int split_list(const char *list, char ***items) {
    int n = 0;
    const char *p = list, *c;
    *items = NULL;
    for (;;) {
        c = strchr(p, ',');
        *items = xrealloc(*items, sizeof(char *) * (size_t)(n + 1));
        if (!c) {
            (*items)[n++] = xstrdup(p);
            break;
        }
        (*items)[n++] = xstrndup(p, (size_t)(c - p));
        p = c + 1;
    }
    return n;
}

static void check_list_syntax(const char *list) {
    char **items;
    int i, n = split_list(list, &items);
    for (i = 0; i < n; i++)
        if (!items[i][0]) die(2, "empty item in column list '%s'", list);
}

static int resolve_list(const char *list, const Table *t, int **out) {
    char **items;
    int i, n = split_list(list, &items), cnt = 0, cap = 16;
    *out = xmalloc(sizeof(int) * (size_t)cap);
    for (i = 0; i < n; i++) {
        const char *it = items[i];
        size_t len = strlen(it);
        const char *dash = strchr(it, '-');
        int lo, hi, k, step;
        if (all_digits(it, len)) {
            lo = hi = index_col(it, len, t->ncols);
        } else if (dash && all_digits(it, (size_t)(dash - it)) &&
                   (all_digits(dash + 1, strlen(dash + 1)) || dash[1] == 0)) {
            lo = index_col(it, (size_t)(dash - it), t->ncols);
            hi = dash[1] ? index_col(dash + 1, strlen(dash + 1), t->ncols) : t->ncols - 1;
        } else if (it[0] == '-' && all_digits(it + 1, len - 1)) {
            if (t->ncols < 1) die(3, "column index out of range: 1");
            lo = 0;
            hi = index_col(it + 1, len - 1, t->ncols);
        } else {
            lo = hi = find_col(t, it);
            if (lo < 0) die(3, "unknown column '%s'", it);
        }
        step = lo <= hi ? 1 : -1;
        for (k = lo;; k += step) {
            if (cnt == cap) {
                cap *= 2;
                *out = xrealloc(*out, sizeof(int) * (size_t)cap);
            }
            (*out)[cnt++] = k;
            if (k == hi) break;
        }
    }
    return cnt;
}

/* ------------------------------------------------------------------ */
/* filter expressions                                                  */
/* ------------------------------------------------------------------ */

enum { T_IDENT, T_BQ, T_DOLLAR, T_NUM, T_STR, T_OP, T_AND, T_OR, T_NOT, T_LP, T_RP, T_END };

typedef struct {
    int type;
    int off, end;
    char *text; /* decoded value for STR/BQ/IDENT/NUM/DOLLAR digits, op text */
} Tok;

typedef struct Node {
    int kind; /* 0 or,1 and,2 not,3 cmp,4 truth */
    struct Node *a, *b;
    char op[3];
    int lk, rk;   /* operand kinds: token types */
    char *lt, *rt;
    int lc, rc;   /* resolved column, -1 literal */
} Node;

typedef struct {
    const char *src;
    Tok *toks;
    int ntok, pos;
    Node **operands; /* in token order, resolution happens later */
    int nops;
} Expr;

static void expr_err(int off, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "tabula: expression error at offset %d: ", off);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(2);
}

static int is_ident_start(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
static int is_ident_char(int c) { return is_ident_start(c) || (c >= '0' && c <= '9'); }
static int is_digit(int c) { return c >= '0' && c <= '9'; }

static int ci_eq(const char *a, size_t n, const char *kw) {
    size_t i;
    if (strlen(kw) != n) return 0;
    for (i = 0; i < n; i++) {
        int c = (unsigned char)a[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        if (c != kw[i]) return 0;
    }
    return 1;
}

static void tokenize(Expr *e) {
    const char *s = e->src;
    int n = (int)strlen(s), i = 0, cap = 16;
    e->toks = xmalloc(sizeof(Tok) * (size_t)cap);
    e->ntok = 0;
    for (;;) {
        Tok t;
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
        t.off = i;
        t.text = NULL;
        if (i >= n) {
            t.type = T_END;
            t.end = i;
        } else {
            unsigned char c = (unsigned char)s[i];
            if (c == '(') { t.type = T_LP; i++; }
            else if (c == ')') { t.type = T_RP; i++; }
            else if (c == '`') {
                const char *q = strchr(s + i + 1, '`');
                if (!q) expr_err(i, "unterminated quoted column name");
                t.type = T_BQ;
                t.text = xstrndup(s + i + 1, (size_t)(q - (s + i + 1)));
                i = (int)(q - s) + 1;
            } else if (c == '\'' || c == '"') {
                SB b = {0};
                int j = i + 1;
                for (;;) {
                    if (j >= n) expr_err(i, "unterminated string");
                    if ((unsigned char)s[j] == c) break;
                    if (s[j] == '\\') {
                        if (j + 1 >= n) expr_err(i, "unterminated string");
                        j++;
                        if (s[j] == 'n') sb_putc(&b, '\n');
                        else if (s[j] == 't') sb_putc(&b, '\t');
                        else sb_putc(&b, s[j]);
                        j++;
                        continue;
                    }
                    sb_putc(&b, s[j]);
                    j++;
                }
                t.type = T_STR;
                t.text = sb_take(&b);
                i = j + 1;
            } else if (c == '$') {
                if (i + 1 < n && s[i + 1] == '=') {
                    t.type = T_OP;
                    t.text = xstrdup("$=");
                    i += 2;
                } else if (i + 1 < n && is_digit(s[i + 1])) {
                    int j = i + 1;
                    while (j < n && is_digit(s[j])) j++;
                    t.type = T_DOLLAR;
                    t.text = xstrndup(s + i + 1, (size_t)(j - i - 1));
                    i = j;
                } else {
                    expr_err(i, "invalid column reference");
                }
            } else if (is_digit(c) || (c == '.' && i + 1 < n && is_digit(s[i + 1])) ||
                       ((c == '+' || c == '-') && i + 1 < n &&
                        (is_digit(s[i + 1]) || (s[i + 1] == '.' && i + 2 < n && is_digit(s[i + 2]))))) {
                int j = i;
                if (c == '+' || c == '-') j++;
                while (j < n && (is_digit(s[j]) || s[j] == '.')) j++;
                t.type = T_NUM;
                t.text = xstrndup(s + i, (size_t)(j - i));
                i = j;
            } else if (is_ident_start(c)) {
                int j = i;
                while (j < n && is_ident_char((unsigned char)s[j])) j++;
                if (ci_eq(s + i, (size_t)(j - i), "and")) t.type = T_AND;
                else if (ci_eq(s + i, (size_t)(j - i), "or")) t.type = T_OR;
                else if (ci_eq(s + i, (size_t)(j - i), "not")) t.type = T_NOT;
                else {
                    t.type = T_IDENT;
                    t.text = xstrndup(s + i, (size_t)(j - i));
                }
                i = j;
            } else if (c == '=') {
                t.type = T_OP;
                if (i + 1 < n && s[i + 1] == '=') { t.text = xstrdup("=="); i += 2; }
                else { t.text = xstrdup("="); i++; }
            } else if (c == '!') {
                if (i + 1 < n && s[i + 1] == '=') { t.type = T_OP; t.text = xstrdup("!="); i += 2; }
                else { t.type = T_NOT; i++; }
            } else if (c == '<') {
                t.type = T_OP;
                if (i + 1 < n && s[i + 1] == '=') { t.text = xstrdup("<="); i += 2; }
                else if (i + 1 < n && s[i + 1] == '>') { t.text = xstrdup("<>"); i += 2; }
                else { t.text = xstrdup("<"); i++; }
            } else if (c == '>') {
                t.type = T_OP;
                if (i + 1 < n && s[i + 1] == '=') { t.text = xstrdup(">="); i += 2; }
                else { t.text = xstrdup(">"); i++; }
            } else if (c == '~') {
                t.type = T_OP; t.text = xstrdup("~"); i++;
            } else if (c == '^' && i + 1 < n && s[i + 1] == '=') {
                t.type = T_OP; t.text = xstrdup("^="); i += 2;
            } else if (c == '&' && i + 1 < n && s[i + 1] == '&') {
                t.type = T_AND; i += 2;
            } else if (c == '|' && i + 1 < n && s[i + 1] == '|') {
                t.type = T_OR; i += 2;
            } else {
                if (c >= 0x21 && c <= 0x7e) expr_err(i, "invalid character '%c'", c);
                expr_err(i, "invalid character 0x%02x", c);
            }
            t.end = i;
        }
        if (e->ntok == cap) {
            cap *= 2;
            e->toks = xrealloc(e->toks, sizeof(Tok) * (size_t)cap);
        }
        e->toks[e->ntok++] = t;
        if (t.type == T_END) break;
    }
}

static Tok *cur(Expr *e) { return &e->toks[e->pos]; }

static void unexpected(Expr *e) {
    Tok *t = cur(e);
    if (t->type == T_END) expr_err(t->off, "unexpected end of expression");
    expr_err(t->off, "unexpected token '%.*s'", t->end - t->off, e->src + t->off);
}

static Node *new_node(int kind) {
    Node *n = xmalloc(sizeof(Node));
    memset(n, 0, sizeof *n);
    n->kind = kind;
    n->lc = n->rc = -1;
    return n;
}

static int is_operand(int type) {
    return type == T_IDENT || type == T_BQ || type == T_DOLLAR || type == T_NUM || type == T_STR;
}

static Node *parse_or(Expr *e);

static void note_operand(Expr *e, Node *n) {
    e->operands = xrealloc(e->operands, sizeof(Node *) * (size_t)(e->nops + 1));
    e->operands[e->nops++] = n;
}

static Node *parse_cmp(Expr *e) {
    Tok *t = cur(e);
    if (t->type == T_LP) {
        Node *n;
        e->pos++;
        n = parse_or(e);
        if (cur(e)->type != T_RP) expr_err(cur(e)->off, "expected ')'");
        e->pos++;
        return n;
    }
    if (is_operand(t->type)) {
        Node *n = new_node(4);
        n->lk = t->type;
        n->lt = t->text;
        note_operand(e, n);
        e->pos++;
        if (cur(e)->type == T_OP) {
            strcpy(n->op, cur(e)->text);
            e->pos++;
            if (!is_operand(cur(e)->type)) unexpected(e);
            n->kind = 3;
            n->rk = cur(e)->type;
            n->rt = cur(e)->text;
            e->pos++;
        }
        return n;
    }
    unexpected(e);
    return NULL;
}

static Node *parse_not(Expr *e) {
    if (cur(e)->type == T_NOT) {
        Node *n = new_node(2);
        e->pos++;
        n->a = parse_not(e);
        return n;
    }
    return parse_cmp(e);
}

static Node *parse_and(Expr *e) {
    Node *l = parse_not(e);
    while (cur(e)->type == T_AND) {
        Node *n = new_node(1);
        e->pos++;
        n->a = l;
        n->b = parse_not(e);
        l = n;
    }
    return l;
}

static Node *parse_or(Expr *e) {
    Node *l = parse_and(e);
    while (cur(e)->type == T_OR) {
        Node *n = new_node(0);
        e->pos++;
        n->a = l;
        n->b = parse_and(e);
        l = n;
    }
    return l;
}

static Node *parse_expr(Expr *e) {
    Node *n;
    tokenize(e);
    e->pos = 0;
    n = parse_or(e);
    if (cur(e)->type != T_END) unexpected(e);
    return n;
}

static int resolve_operand(const Table *t, int kind, const char *text) {
    int c;
    if (kind == T_DOLLAR) return index_col(text, strlen(text), t->ncols);
    if (kind == T_BQ) {
        c = find_col(t, text);
        if (c < 0) die(3, "unknown column '%s'", text);
        return c;
    }
    if (kind == T_IDENT) return find_col(t, text);
    return -1;
}

static void resolve_expr(Expr *e, const Table *t) {
    int i;
    for (i = 0; i < e->nops; i++) {
        Node *n = e->operands[i];
        n->lc = resolve_operand(t, n->lk, n->lt);
        if (n->kind == 3) n->rc = resolve_operand(t, n->rk, n->rt);
    }
}

static int bytecmp(const char *a, const char *b) {
    int c = strcmp(a, b);
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

static int truthy(const char *v) {
    Dec d;
    if (!v[0]) return 0;
    if (dec_parse(v, &d) && d.m == 0) return 0;
    return 1;
}

static int eval(const Node *n, char **row) {
    const char *l, *r;
    switch (n->kind) {
    case 0: return eval(n->a, row) || eval(n->b, row);
    case 1: return eval(n->a, row) && eval(n->b, row);
    case 2: return !eval(n->a, row);
    case 4:
        l = n->lc >= 0 ? row[n->lc] : n->lt;
        return truthy(l);
    default:
        break;
    }
    l = n->lc >= 0 ? row[n->lc] : n->lt;
    r = n->rc >= 0 ? row[n->rc] : n->rt;
    if (!strcmp(n->op, "~")) return strstr(l, r) != NULL;
    if (!strcmp(n->op, "^=")) return strncmp(l, r, strlen(r)) == 0;
    if (!strcmp(n->op, "$=")) {
        size_t a = strlen(l), b = strlen(r);
        return a >= b && !strcmp(l + a - b, r);
    }
    {
        Dec x, y;
        int c;
        if (dec_parse(l, &x) && dec_parse(r, &y)) c = dec_cmp(x, y);
        else c = bytecmp(l, r);
        if (!strcmp(n->op, "=") || !strcmp(n->op, "==")) return c == 0;
        if (!strcmp(n->op, "!=") || !strcmp(n->op, "<>")) return c != 0;
        if (!strcmp(n->op, "<")) return c < 0;
        if (!strcmp(n->op, "<=")) return c <= 0;
        if (!strcmp(n->op, ">")) return c > 0;
        return c >= 0;
    }
}

/* ------------------------------------------------------------------ */
/* sort keys                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    char *col;
    int col_idx;
    int numeric, natural, reverse, icase;
} SortKey;

static SortKey *skeys;
static int nskeys;

static void parse_sortkeys(const char *spec) {
    char **items;
    int i, n = split_list(spec, &items);
    skeys = xmalloc(sizeof(SortKey) * (size_t)n);
    nskeys = n;
    for (i = 0; i < n; i++) {
        char *it = items[i];
        char *colon = strrchr(it, ':');
        SortKey *k = &skeys[i];
        memset(k, 0, sizeof *k);
        if (colon) {
            const char *f;
            for (f = colon + 1; *f; f++) {
                if (*f == 'n') k->numeric = 1;
                else if (*f == 'N') k->natural = 1;
                else if (*f == 'r') k->reverse = 1;
                else if (*f == 'i') k->icase = 1;
                else die(2, "invalid sort flag '%c' in key '%s'", *f, it);
            }
            if (k->numeric && k->natural) die(2, "conflicting sort flags in key '%s'", it);
            k->col = xstrndup(it, (size_t)(colon - it));
        } else {
            k->col = xstrdup(it);
        }
        if (!k->col[0]) die(2, "empty sort key");
    }
}

static int fold(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static int strcmp_fold(const char *a, const char *b, int icase) {
    const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
    for (;; x++, y++) {
        int c1 = icase ? fold(*x) : *x, c2 = icase ? fold(*y) : *y;
        if (c1 != c2) return c1 < c2 ? -1 : 1;
        if (!c1) return 0;
    }
}

static int natcmp(const char *a, const char *b, int icase) {
    const unsigned char *s = (const unsigned char *)a, *t = (const unsigned char *)b;
    size_t i = 0, j = 0, n1 = strlen(a), n2 = strlen(b);
    for (;;) {
        if (i == n1 && j == n2) return 0;
        if (i == n1) return -1;
        if (j == n2) return 1;
        if (is_digit(s[i]) && is_digit(t[j])) {
            size_t i0 = i, j0 = j, l1, l2;
            while (i0 < n1 && s[i0] == '0') i0++;
            while (j0 < n2 && t[j0] == '0') j0++;
            i = i0;
            j = j0;
            while (i < n1 && is_digit(s[i])) i++;
            while (j < n2 && is_digit(t[j])) j++;
            l1 = i - i0;
            l2 = j - j0;
            if (l1 != l2) return l1 < l2 ? -1 : 1;
            {
                int c = memcmp(s + i0, t + j0, l1);
                if (c) return c < 0 ? -1 : 1;
            }
            continue;
        }
        if (is_digit(s[i])) return -1;
        if (is_digit(t[j])) return 1;
        {
            int c1 = icase ? fold(s[i]) : s[i], c2 = icase ? fold(t[j]) : t[j];
            if (c1 != c2) return c1 < c2 ? -1 : 1;
        }
        i++;
        j++;
    }
}

static char ***sort_rows;

static int key_cmp(const SortKey *k, const char *a, const char *b) {
    int c;
    if (k->numeric) {
        Dec x, y;
        int na = dec_parse(a, &x), nb = dec_parse(b, &y);
        if (na && nb) c = dec_cmp(x, y);
        else if (na) return -1;
        else if (nb) return 1;
        else return 0;
    } else if (k->natural) {
        c = natcmp(a, b, k->icase);
    } else {
        c = strcmp_fold(a, b, k->icase);
    }
    return k->reverse ? -c : c;
}

static int row_cmp(const void *pa, const void *pb) {
    int ia = *(const int *)pa, ib = *(const int *)pb, i;
    for (i = 0; i < nskeys; i++) {
        int c = key_cmp(&skeys[i], sort_rows[ia][skeys[i].col_idx], sort_rows[ib][skeys[i].col_idx]);
        if (c) return c;
    }
    return ia < ib ? -1 : (ia > ib ? 1 : 0);
}

/* ------------------------------------------------------------------ */
/* aggregates                                                          */
/* ------------------------------------------------------------------ */

enum { A_COUNT, A_SUM, A_AVG, A_MIN, A_MAX, A_FIRST, A_LAST, A_DISTINCT };
static const char *AGG_NAMES[] = {"count", "sum", "avg", "min", "max", "first", "last", "distinct", NULL};

typedef struct {
    int func;
    char *alias;
    char *arg; /* "*" for count(*) */
    int col;
    char *header;
} Agg;

static Agg *aggs;
static int naggs;

static int split_aggs(const char *spec, char ***items) {
    int n = 0, depth = 0;
    const char *p = spec, *start = spec;
    *items = NULL;
    for (;; p++) {
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        if (*p == 0 || (*p == ',' && depth == 0)) {
            *items = xrealloc(*items, sizeof(char *) * (size_t)(n + 1));
            (*items)[n++] = xstrndup(start, (size_t)(p - start));
            if (!*p) break;
            start = p + 1;
        }
    }
    return n;
}

static void parse_aggs(const char *spec) {
    char **items;
    int i, n = split_aggs(spec, &items);
    aggs = xmalloc(sizeof(Agg) * (size_t)n);
    naggs = n;
    for (i = 0; i < n; i++) {
        char *it = items[i];
        char *lp = strchr(it, '(');
        size_t len = strlen(it);
        char *eq, *fname, *arg;
        int f;
        Agg *a = &aggs[i];
        memset(a, 0, sizeof *a);
        if (!lp || len < 3 || it[len - 1] != ')') die(2, "invalid aggregate spec '%s'", it);
        arg = xstrndup(lp + 1, (size_t)(it + len - 1 - (lp + 1)));
        if (!arg[0] || strchr(arg, '(') || strchr(arg, ')')) die(2, "invalid aggregate spec '%s'", it);
        eq = memchr(it, '=', (size_t)(lp - it));
        if (eq) {
            if (eq == it) die(2, "invalid aggregate spec '%s'", it);
            a->alias = xstrndup(it, (size_t)(eq - it));
            fname = xstrndup(eq + 1, (size_t)(lp - eq - 1));
        } else {
            fname = xstrndup(it, (size_t)(lp - it));
        }
        if (!fname[0]) die(2, "invalid aggregate spec '%s'", it);
        for (f = 0; AGG_NAMES[f]; f++)
            if (!strcmp(AGG_NAMES[f], fname)) break;
        if (!AGG_NAMES[f]) die(2, "unknown aggregate function '%s'", fname);
        if (!strcmp(arg, "*") && f != A_COUNT) die(2, "'*' is only valid with count");
        a->func = f;
        a->arg = arg;
    }
}

static int resolve_name_or_index(const Table *t, const char *s) {
    int c;
    if (all_digits(s, strlen(s))) return index_col(s, strlen(s), t->ncols);
    c = find_col(t, s);
    if (c < 0) die(3, "unknown column '%s'", s);
    return c;
}

static char *dec_str(i128 m, int scale) {
    SB b = {0};
    render_i128(&b, m, scale, 0);
    return sb_take(&b);
}

static char *agg_value(const Agg *a, char ***rows, const int *idx, int n) {
    int i;
    char buf[64];
    if (a->func == A_COUNT) {
        int c = 0;
        for (i = 0; i < n; i++)
            if (a->col < 0 || rows[idx[i]][a->col][0]) c++;
        snprintf(buf, sizeof buf, "%d", c);
        return xstrdup(buf);
    }
    if (a->func == A_FIRST) return xstrdup(n ? rows[idx[0]][a->col] : "");
    if (a->func == A_LAST) return xstrdup(n ? rows[idx[n - 1]][a->col] : "");
    if (a->func == A_DISTINCT) {
        int c = 0, j;
        for (i = 0; i < n; i++) {
            const char *v = rows[idx[i]][a->col];
            if (!v[0]) continue;
            for (j = 0; j < i; j++)
                if (!strcmp(rows[idx[j]][a->col], v)) break;
            if (j == i) c++;
        }
        snprintf(buf, sizeof buf, "%d", c);
        return xstrdup(buf);
    }
    if (a->func == A_SUM || a->func == A_AVG) {
        i128 sum = 0;
        int scale = 0, cnt = 0;
        for (i = 0; i < n; i++) {
            Dec d;
            if (!dec_parse(rows[idx[i]][a->col], &d)) continue;
            if (d.s > scale) {
                sum *= P10[d.s - scale];
                scale = d.s;
            }
            sum += d.m * P10[scale - d.s];
            cnt++;
        }
        if (!cnt) return xstrdup("");
        if (a->func == A_SUM) return dec_str(sum, scale);
        {
            i128 num, den, q;
            char *s;
            size_t L;
            if (scale <= 6) {
                num = sum * P10[6 - scale];
                den = cnt;
            } else {
                num = sum;
                den = (i128)cnt * P10[scale - 6];
            }
            q = round_div(num, den, R_HALF_EVEN);
            s = dec_str(q, 6);
            L = strlen(s);
            while (L && s[L - 1] == '0') s[--L] = 0;
            if (L && s[L - 1] == '.') s[--L] = 0;
            return s;
        }
    }
    /* min / max */
    {
        int allnum = 1, any = 0;
        const char *best = NULL;
        for (i = 0; i < n; i++) {
            const char *v = rows[idx[i]][a->col];
            if (!v[0]) continue;
            any = 1;
            if (!is_num(v)) allnum = 0;
        }
        if (!any) return xstrdup("");
        for (i = 0; i < n; i++) {
            const char *v = rows[idx[i]][a->col];
            int c;
            if (!v[0]) continue;
            if (!best) {
                best = v;
                continue;
            }
            if (allnum) {
                Dec x, y;
                dec_parse(v, &x);
                dec_parse(best, &y);
                c = dec_cmp(x, y);
            } else {
                c = bytecmp(v, best);
            }
            if ((a->func == A_MIN && c < 0) || (a->func == A_MAX && c > 0)) best = v;
        }
        return xstrdup(best);
    }
}

/* ------------------------------------------------------------------ */
/* row keys (group / unique)                                           */
/* ------------------------------------------------------------------ */

static char *row_key(char **row, const int *cols, int ncols) {
    SB b = {0};
    int i;
    char len[32];
    for (i = 0; i < ncols; i++) {
        const char *v = row[cols ? cols[i] : i];
        snprintf(len, sizeof len, "%zu:", strlen(v));
        sb_puts(&b, len);
        sb_puts(&b, v);
    }
    return sb_take(&b);
}

typedef struct {
    char **keys;
    int n, cap;
    int *hash_slots;
    int nslots;
} KeySet;

static unsigned long hash_str(const char *s) {
    unsigned long h = 1469598103UL;
    while (*s) h = (h ^ (unsigned char)*s++) * 1099511UL;
    return h;
}

/* returns existing index or adds and returns -1-new_index */
static int keyset_find_add(KeySet *ks, char *key) {
    unsigned long h;
    int i;
    if (ks->nslots == 0 || ks->n * 2 >= ks->nslots) {
        int ns = ks->nslots ? ks->nslots * 2 : 64, k;
        ks->hash_slots = xrealloc(ks->hash_slots, sizeof(int) * (size_t)ns);
        for (k = 0; k < ns; k++) ks->hash_slots[k] = -1;
        ks->nslots = ns;
        for (k = 0; k < ks->n; k++) {
            unsigned long hh = hash_str(ks->keys[k]) % (unsigned long)ns;
            while (ks->hash_slots[hh] >= 0) hh = (hh + 1) % (unsigned long)ns;
            ks->hash_slots[hh] = k;
        }
    }
    h = hash_str(key) % (unsigned long)ks->nslots;
    while ((i = ks->hash_slots[h]) >= 0) {
        if (!strcmp(ks->keys[i], key)) return i;
        h = (h + 1) % (unsigned long)ks->nslots;
    }
    if (ks->n == ks->cap) {
        ks->cap = ks->cap ? ks->cap * 2 : 64;
        ks->keys = xrealloc(ks->keys, sizeof(char *) * (size_t)ks->cap);
    }
    ks->keys[ks->n] = key;
    ks->hash_slots[h] = ks->n;
    ks->n++;
    return -ks->n;
}

/* ------------------------------------------------------------------ */
/* display width                                                       */
/* ------------------------------------------------------------------ */

static int in_ranges(unsigned cp, const unsigned (*r)[2], int n) {
    int i;
    for (i = 0; i < n; i++)
        if (cp >= r[i][0] && cp <= r[i][1]) return 1;
    return 0;
}

static const unsigned ZERO_W[][2] = {
    {0x0300, 0x036F}, {0x0483, 0x0489}, {0x0591, 0x05BD}, {0x0610, 0x061A}, {0x064B, 0x065F},
    {0x1AB0, 0x1AFF}, {0x1DC0, 0x1DFF}, {0x200B, 0x200F}, {0x20D0, 0x20FF}, {0xFE00, 0xFE0F},
    {0xFE20, 0xFE2F}};
static const unsigned WIDE_W[][2] = {
    {0x1100, 0x115F},   {0x2E80, 0x303E},   {0x3041, 0x33FF},   {0x3400, 0x4DBF},
    {0x4E00, 0x9FFF},   {0xA000, 0xA4CF},   {0xAC00, 0xD7A3},   {0xF900, 0xFAFF},
    {0xFE30, 0xFE4F},   {0xFF00, 0xFF60},   {0xFFE0, 0xFFE6},   {0x1F300, 0x1F64F},
    {0x1F900, 0x1F9FF}, {0x20000, 0x2FFFD}, {0x30000, 0x3FFFD}};

static int cp_width(unsigned cp) {
    if (in_ranges(cp, ZERO_W, (int)(sizeof ZERO_W / sizeof ZERO_W[0]))) return 0;
    if (in_ranges(cp, WIDE_W, (int)(sizeof WIDE_W / sizeof WIDE_W[0]))) return 2;
    return 1;
}

/* decode one display unit at s; returns bytes consumed, fills out (rendered bytes) and width */
static int next_unit(const unsigned char *s, char *out, int *outlen, int *width) {
    unsigned c = s[0];
    int len, k;
    unsigned cp;
    if (c < 0x20 || c == 0x7f) {
        out[0] = '^';
        out[1] = (char)(c ^ 0x40);
        *outlen = 2;
        *width = 2;
        return 1;
    }
    if (c < 0x80) {
        out[0] = (char)c;
        *outlen = 1;
        *width = 1;
        return 1;
    }
    if (c >= 0xC2 && c <= 0xDF) { len = 2; cp = c & 0x1F; }
    else if (c >= 0xE0 && c <= 0xEF) { len = 3; cp = c & 0x0F; }
    else if (c >= 0xF0 && c <= 0xF4) { len = 4; cp = c & 0x07; }
    else goto invalid;
    for (k = 1; k < len; k++) {
        if (s[k] < 0x80 || s[k] > 0xBF) goto invalid;
        cp = (cp << 6) | (s[k] & 0x3F);
    }
    if (c == 0xE0 && s[1] < 0xA0) goto invalid;
    if (c == 0xED && s[1] > 0x9F) goto invalid;
    if (c == 0xF0 && s[1] < 0x90) goto invalid;
    if (c == 0xF4 && s[1] > 0x8F) goto invalid;
    memcpy(out, s, (size_t)len);
    *outlen = len;
    *width = cp_width(cp);
    return len;
invalid:
    out[0] = (char)c;
    *outlen = 1;
    *width = 1;
    return 1;
}

static char *display_cell(const char *v, int maxw, int *wout) {
    SB b = {0};
    const unsigned char *s = (const unsigned char *)v;
    int total = 0, i = 0, n = (int)strlen(v);
    char unit[8];
    int ul, uw;
    /* full width first */
    while (i < n) {
        i += next_unit(s + i, unit, &ul, &uw);
        total += uw;
    }
    i = 0;
    if (maxw <= 0 || total <= maxw) {
        while (i < n) {
            i += next_unit(s + i, unit, &ul, &uw);
            sb_putn(&b, unit, (size_t)ul);
        }
        *wout = total;
        return sb_take(&b);
    }
    total = 0;
    while (i < n) {
        int used = next_unit(s + i, unit, &ul, &uw);
        if (total + uw > maxw - 1) break;
        sb_putn(&b, unit, (size_t)ul);
        total += uw;
        i += used;
    }
    sb_puts(&b, "\xE2\x80\xA6");
    *wout = total + 1;
    return sb_take(&b);
}

/* ------------------------------------------------------------------ */
/* output                                                              */
/* ------------------------------------------------------------------ */

static SB OUT;

static int needs_quote(const char *v, char delim, int ncols) {
    size_t n = strlen(v);
    if (n == 0) return ncols == 1;
    if (strchr(v, delim) || strchr(v, '"') || strchr(v, '\n') || strchr(v, '\r')) return 1;
    if (v[0] == ' ' || v[0] == '\t' || v[n - 1] == ' ' || v[n - 1] == '\t') return 1;
    return 0;
}

static void put_quoted(const char *v) {
    sb_putc(&OUT, '"');
    for (; *v; v++) {
        if (*v == '"') sb_putc(&OUT, '"');
        sb_putc(&OUT, *v);
    }
    sb_putc(&OUT, '"');
}

static void write_csv_row(char **row, int ncols, char delim, int line) {
    int j;
    for (j = 0; j < ncols; j++) {
        const char *v = row[j];
        int q;
        if (j) sb_putc(&OUT, delim);
        switch (O.quote_mode) {
        case Q_ALL: q = 1; break;
        case Q_NONNUMERIC: q = !is_num(v) || needs_quote(v, delim, ncols); break;
        case Q_NONE:
            if (needs_quote(v, delim, ncols))
                die(4, "cannot write field without quoting (line %d, column %d)", line, j + 1);
            q = 0;
            break;
        default: q = needs_quote(v, delim, ncols); break;
        }
        if (q) put_quoted(v);
        else sb_puts(&OUT, v);
    }
    sb_putc(&OUT, '\n');
}

static void write_pretty(char **names, int ncols, char ***rows, int nrows, int show_header) {
    int *w = xmalloc(sizeof(int) * (size_t)(ncols ? ncols : 1));
    int *right = xmalloc(sizeof(int) * (size_t)(ncols ? ncols : 1));
    char ***disp = xmalloc(sizeof(char **) * (size_t)(nrows + 1));
    int **dw = xmalloc(sizeof(int *) * (size_t)(nrows + 1));
    int i, j;
    if (ncols == 0) return;
    for (j = 0; j < ncols; j++) {
        int any = 0, allnum = 1;
        for (i = 0; i < nrows; i++) {
            const char *v = rows[i][j];
            if (!v[0]) continue;
            any = 1;
            if (!is_num(v)) allnum = 0;
        }
        right[j] = any && allnum;
        w[j] = 1;
    }
    for (i = 0; i <= nrows; i++) {
        char **src = i == 0 ? names : rows[i - 1];
        disp[i] = xmalloc(sizeof(char *) * (size_t)ncols);
        dw[i] = xmalloc(sizeof(int) * (size_t)ncols);
        if (i == 0 && !show_header) continue;
        for (j = 0; j < ncols; j++) {
            disp[i][j] = display_cell(src[j], O.max_width, &dw[i][j]);
            if (dw[i][j] > w[j]) w[j] = dw[i][j];
        }
    }
    for (i = show_header ? 0 : 1; i <= nrows; i++) {
        size_t start = OUT.n;
        for (j = 0; j < ncols; j++) {
            int pad = w[j] - dw[i][j], k;
            if (j) sb_puts(&OUT, " | ");
            if (right[j])
                for (k = 0; k < pad; k++) sb_putc(&OUT, ' ');
            sb_puts(&OUT, disp[i][j]);
            if (!right[j])
                for (k = 0; k < pad; k++) sb_putc(&OUT, ' ');
        }
        while (OUT.n > start && OUT.p[OUT.n - 1] == ' ') OUT.n--;
        sb_putc(&OUT, '\n');
        if (i == 0) {
            for (j = 0; j < ncols; j++) {
                int k;
                if (j) sb_puts(&OUT, "-+-");
                for (k = 0; k < w[j]; k++) sb_putc(&OUT, '-');
            }
            sb_putc(&OUT, '\n');
        }
    }
}

static char *format_number(const char *v) {
    Dec d;
    SB b = {0};
    if (!dec_parse(v, &d)) return NULL;
    if (O.precision >= 0) render_i128(&b, dec_round_to(d, O.precision, O.round_mode), O.precision, O.thousands);
    else render_i128(&b, d.m, d.s, O.thousands);
    return sb_take(&b);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    Expr *exprs = NULL;
    Node **roots = NULL;
    char *buf;
    size_t len;
    Table T;
    int i, j, ncols, ndata, nrows;
    char ***rows;
    char **names;
    int grouped;
    int *sel = NULL, nsel = 0;
    int *gcols = NULL, ngcols = 0;

    init_p10();
    parse_args(argc, argv);

    if (O.has_select && O.has_exclude) die(1, "--select and --exclude are mutually exclusive");
    if ((O.has_select || O.has_exclude) && (O.has_group || O.has_agg))
        die(1, "--select/--exclude cannot be combined with --group/--agg");
    if (O.count && O.pretty) die(1, "--count cannot be combined with --pretty");
    if (O.quote_set && O.pretty) die(1, "--quote cannot be combined with --pretty");
    if (O.max_width && !O.pretty) die(1, "--max-width requires --pretty");

    if (O.nwhere) {
        exprs = xmalloc(sizeof(Expr) * (size_t)O.nwhere);
        roots = xmalloc(sizeof(Node *) * (size_t)O.nwhere);
        for (i = 0; i < O.nwhere; i++) {
            memset(&exprs[i], 0, sizeof(Expr));
            exprs[i].src = O.where[i];
            roots[i] = parse_expr(&exprs[i]);
        }
    }
    if (O.has_select) check_list_syntax(O.select.p);
    if (O.has_exclude) check_list_syntax(O.exclude.p);
    if (O.has_group) check_list_syntax(O.group.p);
    if (O.has_agg) parse_aggs(O.agg.p);
    if (O.has_sort) parse_sortkeys(O.sortkeys.p);

    /* input */
    {
        FILE *fp = stdin;
        const char *fname = O.file;
        if (fname && strcmp(fname, "-")) {
            struct stat st;
            if (stat(fname, &st) != 0) die(6, "cannot open '%s': %s", fname, errtext(errno));
            if (S_ISDIR(st.st_mode)) die(6, "cannot read '%s': %s", fname, errtext(EISDIR));
            fp = fopen(fname, "rb");
            if (!fp) die(6, "cannot open '%s': %s", fname, errtext(errno));
            if (!O.in_delim_set) {
                size_t fl = strlen(fname);
                if (fl >= 4 && !strcmp(fname + fl - 4, ".tsv")) O.in_delim = '\t';
            }
        }
        buf = read_all(fp, &len);
        if (fp != stdin) fclose(fp);
    }
    if (!O.out_delim_set) O.out_delim = O.in_delim;
    parse_csv(buf, len, O.in_delim);

    if (O.trim) {
        for (i = 0; i < nrecs; i++)
            for (j = 0; j < recs[i].n; j++) {
                char *v = recs[i].f[j];
                size_t a = 0, b = strlen(v);
                while (a < b && (v[a] == ' ' || v[a] == '\t')) a++;
                while (b > a && (v[b - 1] == ' ' || v[b - 1] == '\t')) b--;
                recs[i].f[j] = xstrndup(v + a, b - a);
            }
    }

    if (nrecs == 0 && !O.no_header) die(5, "empty input");
    ncols = nrecs ? recs[0].n : 0;
    T.ncols = ncols;
    T.names = xmalloc(sizeof(char *) * (size_t)(ncols ? ncols : 1));
    for (j = 0; j < ncols; j++) {
        char nm[32];
        snprintf(nm, sizeof nm, "c%d", j + 1);
        if (!O.no_header && recs[0].f[j][0]) T.names[j] = recs[0].f[j];
        else T.names[j] = xstrdup(nm);
    }
    {
        int first = O.no_header ? 0 : 1;
        ndata = nrecs - first;
        if (ndata < 0) ndata = 0;
        T.rows = xmalloc(sizeof(char **) * (size_t)(ndata ? ndata : 1));
        T.nrows = ndata;
        for (i = 0; i < ndata; i++) {
            Rec *r = &recs[i + first];
            if (r->n != ncols) {
                if (O.strict) die(5, "line %d: expected %d fields, got %d", r->line, ncols, r->n);
                T.rows[i] = xmalloc(sizeof(char *) * (size_t)(ncols ? ncols : 1));
                for (j = 0; j < ncols; j++) T.rows[i][j] = j < r->n ? r->f[j] : "";
            } else {
                T.rows[i] = r->f;
            }
        }
    }

    if (O.columns) {
        for (j = 0; j < ncols; j++) printf("%d\t%s\n", j + 1, T.names[j]);
        return 0;
    }

    /* resolve references */
    for (i = 0; i < O.nwhere; i++) resolve_expr(&exprs[i], &T);
    grouped = O.has_group || O.has_agg;
    if (O.has_select) nsel = resolve_list(O.select.p, &T, &sel);
    if (O.has_exclude) {
        int *ex, nex = resolve_list(O.exclude.p, &T, &ex), k;
        sel = xmalloc(sizeof(int) * (size_t)(ncols ? ncols : 1));
        nsel = 0;
        for (j = 0; j < ncols; j++) {
            for (k = 0; k < nex; k++)
                if (ex[k] == j) break;
            if (k == nex) sel[nsel++] = j;
        }
        if (nsel == 0) die(3, "no columns left to output");
    }
    if (O.has_group) ngcols = resolve_list(O.group.p, &T, &gcols);
    if (grouped) {
        if (!O.has_agg) {
            aggs = xmalloc(sizeof(Agg));
            memset(aggs, 0, sizeof(Agg));
            aggs[0].func = A_COUNT;
            aggs[0].arg = "*";
            naggs = 1;
        }
        for (i = 0; i < naggs; i++) {
            Agg *a = &aggs[i];
            SB h = {0};
            a->col = strcmp(a->arg, "*") ? resolve_name_or_index(&T, a->arg) : -1;
            if (a->alias) sb_puts(&h, a->alias);
            else if (a->col < 0) sb_puts(&h, "count");
            else {
                sb_puts(&h, AGG_NAMES[a->func]);
                sb_putc(&h, '_');
                sb_puts(&h, T.names[a->col]);
            }
            a->header = sb_take(&h);
        }
    }

    /* filter */
    rows = xmalloc(sizeof(char **) * (size_t)(T.nrows ? T.nrows : 1));
    nrows = 0;
    for (i = 0; i < T.nrows; i++) {
        int keep = 1, k;
        for (k = 0; k < O.nwhere && keep; k++) keep = eval(roots[k], T.rows[i]);
        if (keep) rows[nrows++] = T.rows[i];
    }
    names = T.names;

    /* group */
    if (grouped) {
        int ocols = ngcols + naggs, g;
        KeySet ks = {0};
        int **members = NULL, *msize = NULL, ngroups = 0;
        char ***orows;
        char **onames = xmalloc(sizeof(char *) * (size_t)ocols);
        for (j = 0; j < ngcols; j++) onames[j] = T.names[gcols[j]];
        for (j = 0; j < naggs; j++) onames[ngcols + j] = aggs[j].header;
        for (i = 0; i < nrows; i++) {
            char *key = row_key(rows[i], gcols, ngcols);
            int r = keyset_find_add(&ks, key);
            if (r < 0) {
                g = -r - 1;
                ngroups = g + 1;
                members = xrealloc(members, sizeof(int *) * (size_t)ngroups);
                msize = xrealloc(msize, sizeof(int) * (size_t)ngroups);
                members[g] = NULL;
                msize[g] = 0;
            } else {
                g = r;
            }
            members[g] = xrealloc(members[g], sizeof(int) * (size_t)(msize[g] + 1));
            members[g][msize[g]++] = i;
        }
        if (!O.has_group && ngroups == 0) {
            ngroups = 1;
            members = xmalloc(sizeof(int *));
            msize = xmalloc(sizeof(int));
            members[0] = NULL;
            msize[0] = 0;
        }
        orows = xmalloc(sizeof(char **) * (size_t)(ngroups ? ngroups : 1));
        for (g = 0; g < ngroups; g++) {
            orows[g] = xmalloc(sizeof(char *) * (size_t)ocols);
            for (j = 0; j < ngcols; j++) orows[g][j] = rows[members[g][0]][gcols[j]];
            for (j = 0; j < naggs; j++) orows[g][ngcols + j] = agg_value(&aggs[j], rows, members[g], msize[g]);
        }
        rows = orows;
        nrows = ngroups;
        names = onames;
        ncols = ocols;
    }

    /* sort */
    if (O.has_sort) {
        Table ot;
        int *order = xmalloc(sizeof(int) * (size_t)(nrows ? nrows : 1));
        char ***sorted = xmalloc(sizeof(char **) * (size_t)(nrows ? nrows : 1));
        ot.ncols = ncols;
        ot.names = names;
        for (i = 0; i < nskeys; i++) skeys[i].col_idx = resolve_name_or_index(&ot, skeys[i].col);
        for (i = 0; i < nrows; i++) order[i] = i;
        sort_rows = rows;
        qsort(order, (size_t)nrows, sizeof(int), row_cmp);
        for (i = 0; i < nrows; i++) sorted[i] = rows[order[i]];
        rows = sorted;
    }

    /* select / exclude */
    if (sel) {
        char **nn = xmalloc(sizeof(char *) * (size_t)nsel);
        for (j = 0; j < nsel; j++) nn[j] = names[sel[j]];
        for (i = 0; i < nrows; i++) {
            char **r = xmalloc(sizeof(char *) * (size_t)nsel);
            for (j = 0; j < nsel; j++) r[j] = rows[i][sel[j]];
            rows[i] = r;
        }
        names = nn;
        ncols = nsel;
    }

    /* unique */
    if (O.unique) {
        KeySet ks = {0};
        int k = 0;
        for (i = 0; i < nrows; i++) {
            if (keyset_find_add(&ks, row_key(rows[i], NULL, ncols)) < 0) rows[k++] = rows[i];
        }
        nrows = k;
    }

    /* skip / limit */
    if (O.skip > 0) {
        if (O.skip >= nrows) nrows = 0;
        else {
            rows += O.skip;
            nrows -= (int)O.skip;
        }
    }
    if (O.limit >= 0 && O.limit < nrows) nrows = (int)O.limit;

    if (O.count) {
        printf("%d\n", nrows);
        return 0;
    }

    /* cell formatting */
    for (i = 0; i < nrows; i++) {
        char **r = xmalloc(sizeof(char *) * (size_t)(ncols ? ncols : 1));
        for (j = 0; j < ncols; j++) {
            char *v = rows[i][j];
            if (O.precision >= 0 || O.thousands) {
                char *f = format_number(v);
                if (f) v = f;
            }
            if (O.empty && !v[0]) v = (char *)O.empty;
            r[j] = v;
        }
        rows[i] = r;
    }

    {
        int show_header = !O.no_header && !O.no_header_out;
        if (O.pretty) {
            write_pretty(names, ncols, rows, nrows, show_header);
        } else if (ncols > 0) {
            int line = 1;
            if (show_header) write_csv_row(names, ncols, O.out_delim, line++);
            for (i = 0; i < nrows; i++) write_csv_row(rows[i], ncols, O.out_delim, line++);
        }
    }
    fwrite(OUT.p ? OUT.p : "", 1, OUT.n, stdout);
    return 0;
}
