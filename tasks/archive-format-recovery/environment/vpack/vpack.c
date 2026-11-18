/*
 * vpack - pack a directory tree into a single archive file.
 *
 * Copyright (c) 2026 the vpack author.
 * Released under the MIT License; see the LICENSE file next to this source.
 *
 * Usage:  vpack pack <dir> <out.vp>
 *
 * The archive keeps regular files, directories (including empty ones) and
 * symbolic links, with permission bits and nanosecond modification times.
 * Other file types (sockets, fifos, devices) are skipped with a warning.
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define VPACK_VERSION_STRING "vpack 3.1"
#define FORMAT_VERSION 3

/* Header flag bits. */
#define HF_NAMETAB 0x01u  /* shared name table present at start of table */
#define HF_TABLZ 0x02u    /* directory table is LZ compressed */

/* Entry kind byte. */
#define EK_TYPE_MASK 0x03u
#define EK_FILE 1u
#define EK_DIR 2u
#define EK_LINK 3u
#define EK_NSEC 0x04u
#define EK_LZ 0x08u
#define EK_SHARED 0x10u
#define EK_NAMEREF 0x20u

#define DEFAULT_FILE_MODE 0644u
#define DEFAULT_DIR_MODE 0755u

#define LZ_BLOCK 16384u
#define LZ_WINDOW 65536u
#define LZ_SHORT_MAX_LEN 10u
#define LZ_SHORT_MAX_DIST 4096u
#define LZ_MIN_MATCH 3u
#define LZ_HASH_BITS 15
#define LZ_HASH_SIZE (1u << LZ_HASH_BITS)
#define LZ_MAX_CHAIN 48

#define TABLE_LZ_THRESHOLD 512u

static const unsigned char MAGIC_HEAD[4] = {0x89, 'V', 'P', 'K'};
static const unsigned char MAGIC_TAIL[4] = {0x1a, 'K', 'P', 'V'};

static const char *progname = "vpack";

/* ------------------------------------------------------------------ */
/* error helpers                                                        */
/* ------------------------------------------------------------------ */

static void die(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void warn(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s: warning: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p)
        die("out of memory");
    return p;
}

static void *xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (!q)
        die("out of memory");
    return q;
}

static char *xstrdup(const char *s)
{
    size_t n = strlen(s);
    char *p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

/* ------------------------------------------------------------------ */
/* growable byte buffer                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t *p;
    size_t n;
    size_t cap;
} buf_t;

static void buf_reserve(buf_t *b, size_t extra)
{
    if (b->n + extra <= b->cap)
        return;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->n + extra) {
        if (cap > SIZE_MAX / 2)
            die("buffer too large");
        cap *= 2;
    }
    b->p = xrealloc(b->p, cap);
    b->cap = cap;
}

static void buf_put(buf_t *b, const void *src, size_t len)
{
    if (!len)
        return;
    buf_reserve(b, len);
    memcpy(b->p + b->n, src, len);
    b->n += len;
}

static void buf_byte(buf_t *b, uint8_t v)
{
    buf_reserve(b, 1);
    b->p[b->n++] = v;
}

static void buf_u32le(buf_t *b, uint32_t v)
{
    uint8_t t[4];
    for (int i = 0; i < 4; i++)
        t[i] = (uint8_t)(v >> (8 * i));
    buf_put(b, t, 4);
}

static void buf_u64le(buf_t *b, uint64_t v)
{
    uint8_t t[8];
    for (int i = 0; i < 8; i++)
        t[i] = (uint8_t)(v >> (8 * i));
    buf_put(b, t, 8);
}

/*
 * Variable length unsigned integer.  Seven bits per byte, most significant
 * group first, high bit set on every byte except the last.  Each
 * continuation step carries an implicit +1 so that every value has exactly
 * one encoding.
 */
static void buf_varint(buf_t *b, uint64_t v)
{
    uint8_t tmp[16];
    int i = 15;
    tmp[i] = (uint8_t)(v & 0x7f);
    while (v >>= 7) {
        v--;
        tmp[--i] = (uint8_t)(0x80 | (v & 0x7f));
    }
    buf_put(b, tmp + i, (size_t)(16 - i));
}

static uint64_t zigzag64(int64_t v)
{
    return ((uint64_t)v << 1) ^ (uint64_t)(v >> 63);
}

static void buf_free(buf_t *b)
{
    free(b->p);
    b->p = NULL;
    b->n = b->cap = 0;
}

/* ------------------------------------------------------------------ */
/* CRC-32 (IEEE 802.3, reflected, as used by zlib)                      */
/* ------------------------------------------------------------------ */

static uint32_t crc_table[256];

static void crc_init(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[i] = c;
    }
}

static uint32_t crc_update(uint32_t crc, const uint8_t *p, size_t n)
{
    crc = ~crc;
    while (n--)
        crc = crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

/* ------------------------------------------------------------------ */
/* LZ compressor                                                        */
/* ------------------------------------------------------------------ */

static uint32_t lz_hash(const uint8_t *p)
{
    uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
    return (v * 2654435761u) >> (32 - LZ_HASH_BITS);
}

typedef struct {
    const uint8_t *src;
    size_t n;
    int32_t *head;
    int32_t *prev;
    size_t inserted;
} lz_state_t;

static void lz_insert_upto(lz_state_t *st, size_t pos)
{
    while (st->inserted < pos) {
        size_t i = st->inserted++;
        if (i + LZ_MIN_MATCH > st->n)
            continue;
        uint32_t h = lz_hash(st->src + i);
        st->prev[i] = st->head[h];
        st->head[h] = (int32_t)i;
    }
}

/* Find the longest match for position pos that does not run past limit. */
static size_t lz_find(lz_state_t *st, size_t pos, size_t limit, size_t *dist_out)
{
    size_t best_len = 0, best_dist = 0;
    if (pos + LZ_MIN_MATCH > st->n || pos + LZ_MIN_MATCH > limit)
        return 0;
    uint32_t h = lz_hash(st->src + pos);
    int32_t cand = st->head[h];
    int chain = 0;
    size_t maxl = limit - pos;
    while (cand >= 0 && chain++ < LZ_MAX_CHAIN) {
        size_t c = (size_t)cand;
        size_t dist = pos - c;
        if (dist > LZ_WINDOW)
            break;
        if (st->src[c + best_len] == st->src[pos + best_len] || best_len == 0) {
            size_t l = 0;
            while (l < maxl && st->src[c + l] == st->src[pos + l])
                l++;
            int ok = (dist <= LZ_SHORT_MAX_DIST) ? (l >= LZ_MIN_MATCH) : (l >= LZ_MIN_MATCH + 1);
            if (ok && l > best_len) {
                best_len = l;
                best_dist = dist;
                if (l == maxl)
                    break;
            }
        }
        cand = st->prev[c];
    }
    *dist_out = best_dist;
    return best_len;
}

static void lz_emit_match(buf_t *b, size_t len, size_t dist)
{
    size_t d = dist - 1;
    if (len <= LZ_SHORT_MAX_LEN && dist <= LZ_SHORT_MAX_DIST) {
        buf_byte(b, (uint8_t)(((len - LZ_MIN_MATCH) << 4) | (d >> 8)));
        buf_byte(b, (uint8_t)(d & 0xff));
        return;
    }
    size_t l = len - LZ_MIN_MATCH;
    if (l >= 127) {
        buf_byte(b, 0x80 | 127);
        buf_byte(b, (uint8_t)(d & 0xff));
        buf_byte(b, (uint8_t)(d >> 8));
        buf_varint(b, (uint64_t)(len - LZ_MIN_MATCH - 127));
    } else {
        buf_byte(b, (uint8_t)(0x80 | l));
        buf_byte(b, (uint8_t)(d & 0xff));
        buf_byte(b, (uint8_t)(d >> 8));
    }
}

/*
 * Compress src[0..n) into out as a sequence of blocks.  Each block covers
 * up to LZ_BLOCK input bytes and is either raw or token coded.  Matches may
 * refer back into earlier blocks but never run past the current block.
 */
static void lz_compress(const uint8_t *src, size_t n, buf_t *out)
{
    lz_state_t st;
    st.src = src;
    st.n = n;
    st.head = xmalloc(sizeof(int32_t) * LZ_HASH_SIZE);
    st.prev = xmalloc(sizeof(int32_t) * (n ? n : 1));
    st.inserted = 0;
    for (size_t i = 0; i < LZ_HASH_SIZE; i++)
        st.head[i] = -1;

    buf_t blk = {0};
    for (size_t bs = 0; bs < n; bs += LZ_BLOCK) {
        size_t be = bs + LZ_BLOCK < n ? bs + LZ_BLOCK : n;
        size_t ulen = be - bs;
        blk.n = 0;
        size_t pos = bs;
        size_t ctrl_at = 0;
        int items = 8;
        while (pos < be) {
            if (items == 8) {
                ctrl_at = blk.n;
                buf_byte(&blk, 0);
                items = 0;
            }
            lz_insert_upto(&st, pos);
            size_t dist = 0;
            size_t len = lz_find(&st, pos, be, &dist);
            if (len >= LZ_MIN_MATCH) {
                blk.p[ctrl_at] |= (uint8_t)(1u << items);
                lz_emit_match(&blk, len, dist);
                pos += len;
            } else {
                buf_byte(&blk, src[pos]);
                pos++;
            }
            items++;
        }
        lz_insert_upto(&st, be);
        if (blk.n >= ulen) {
            buf_varint(out, ((uint64_t)ulen << 1) | 1u);
            buf_put(out, src + bs, ulen);
        } else {
            buf_varint(out, (uint64_t)blk.n << 1);
            buf_put(out, blk.p, blk.n);
        }
    }
    buf_free(&blk);
    free(st.head);
    free(st.prev);
}

/* ------------------------------------------------------------------ */
/* tree scan                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned type;      /* EK_FILE, EK_DIR, EK_LINK */
    size_t parent;      /* 0 = root, k = entries[k-1] */
    char *name;
    size_t namelen;
    char *path;
    uint32_t mode;
    int64_t sec;
    uint32_t nsec;
    uint8_t *data;      /* file contents or link target */
    size_t size;
    uint32_t crc;
    long shared_ref;    /* -1 or index of entry holding identical data */
    long name_ref;      /* -1 or index into name table */
    long group;         /* name group used for the name table */
    int lz;
    buf_t blob;
} entry_t;

typedef struct {
    entry_t *v;
    size_t n;
    size_t cap;
} entries_t;

static entries_t entries;

static entry_t *entry_new(void)
{
    if (entries.n == entries.cap) {
        entries.cap = entries.cap ? entries.cap * 2 : 64;
        entries.v = xrealloc(entries.v, entries.cap * sizeof(entry_t));
    }
    entry_t *e = &entries.v[entries.n++];
    memset(e, 0, sizeof(*e));
    e->shared_ref = -1;
    e->name_ref = -1;
    e->group = -1;
    return e;
}

static char *path_join(const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b);
    char *p = xmalloc(la + lb + 2);
    memcpy(p, a, la);
    p[la] = '/';
    memcpy(p + la + 1, b, lb + 1);
    return p;
}

static uint8_t *read_file(const char *path, size_t expect, size_t *got)
{
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
        die("cannot open %s: %s", path, strerror(errno));
    size_t cap = expect + 1, n = 0;
    uint8_t *p = xmalloc(cap);
    for (;;) {
        if (n == cap) {
            cap *= 2;
            p = xrealloc(p, cap);
        }
        ssize_t r = read(fd, p + n, cap - n);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            die("cannot read %s: %s", path, strerror(errno));
        }
        if (r == 0)
            break;
        n += (size_t)r;
    }
    close(fd);
    *got = n;
    return p;
}

static uint8_t *read_link(const char *path, size_t *got)
{
    size_t cap = 256;
    for (;;) {
        char *p = xmalloc(cap);
        ssize_t r = readlink(path, p, cap);
        if (r < 0)
            die("cannot read link %s: %s", path, strerror(errno));
        if ((size_t)r < cap) {
            *got = (size_t)r;
            return (uint8_t *)p;
        }
        free(p);
        cap *= 2;
    }
}

static int cmp_names(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void scan_dir(const char *dirpath, size_t parent_code, int depth)
{
    if (depth > 4096)
        die("directory nesting too deep at %s", dirpath);
    DIR *d = opendir(dirpath);
    if (!d)
        die("cannot open directory %s: %s", dirpath, strerror(errno));
    char **names = NULL;
    size_t nn = 0, cap = 0;
    struct dirent *de;
    errno = 0;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        if (nn == cap) {
            cap = cap ? cap * 2 : 32;
            names = xrealloc(names, cap * sizeof(char *));
        }
        names[nn++] = xstrdup(de->d_name);
    }
    if (errno)
        die("cannot read directory %s: %s", dirpath, strerror(errno));
    closedir(d);
    if (nn)
        qsort(names, nn, sizeof(char *), cmp_names);

    for (size_t i = 0; i < nn; i++) {
        char *path = path_join(dirpath, names[i]);
        struct stat sb;
        if (lstat(path, &sb) < 0)
            die("cannot stat %s: %s", path, strerror(errno));
        unsigned type;
        if (S_ISREG(sb.st_mode))
            type = EK_FILE;
        else if (S_ISDIR(sb.st_mode))
            type = EK_DIR;
        else if (S_ISLNK(sb.st_mode))
            type = EK_LINK;
        else {
            warn("skipping %s: unsupported file type", path);
            free(path);
            free(names[i]);
            continue;
        }
        entry_t *e = entry_new();
        size_t my_index = entries.n - 1;
        e->type = type;
        e->parent = parent_code;
        e->name = names[i];
        e->namelen = strlen(names[i]);
        e->path = path;
        e->mode = (uint32_t)(sb.st_mode & 07777);
        e->sec = (int64_t)sb.st_mtim.tv_sec;
        e->nsec = (uint32_t)sb.st_mtim.tv_nsec;
        if (type == EK_FILE) {
            e->data = read_file(path, (size_t)sb.st_size, &e->size);
        } else if (type == EK_LINK) {
            e->data = read_link(path, &e->size);
        }
        if (type == EK_DIR)
            scan_dir(path, my_index + 1, depth + 1);
    }
    free(names);
}

/* ------------------------------------------------------------------ */
/* name table and content sharing                                       */
/* ------------------------------------------------------------------ */

static int cmp_entry_names(const void *a, const void *b)
{
    const entry_t *x = &entries.v[*(const size_t *)a];
    const entry_t *y = &entries.v[*(const size_t *)b];
    size_t m = x->namelen < y->namelen ? x->namelen : y->namelen;
    int c = memcmp(x->name, y->name, m);
    if (c)
        return c;
    if (x->namelen != y->namelen)
        return x->namelen < y->namelen ? -1 : 1;
    size_t ia = *(const size_t *)a, ib = *(const size_t *)b;
    return ia < ib ? -1 : (ia > ib ? 1 : 0);
}

static int same_name(const entry_t *x, const entry_t *y)
{
    return x->namelen == y->namelen && memcmp(x->name, y->name, x->namelen) == 0;
}

/* Returns the number of names in the table, filling order[] with entry
 * indices whose names make up the table in table order. */
static size_t build_name_table(size_t **order_out)
{
    size_t n = entries.n;
    size_t *idx = xmalloc(sizeof(size_t) * (n ? n : 1));
    for (size_t i = 0; i < n; i++)
        idx[i] = i;
    if (n)
        qsort(idx, n, sizeof(size_t), cmp_entry_names);
    long groups = 0;
    size_t i = 0;
    while (i < n) {
        size_t j = i + 1;
        while (j < n && same_name(&entries.v[idx[i]], &entries.v[idx[j]]))
            j++;
        if (j - i >= 2) {
            for (size_t k = i; k < j; k++)
                entries.v[idx[k]].group = groups;
            groups++;
        }
        i = j;
    }
    free(idx);

    long *group_slot = xmalloc(sizeof(long) * (groups ? (size_t)groups : 1));
    for (long g = 0; g < groups; g++)
        group_slot[g] = -1;
    size_t *order = xmalloc(sizeof(size_t) * (groups ? (size_t)groups : 1));
    size_t count = 0;
    for (size_t e = 0; e < n; e++) {
        long g = entries.v[e].group;
        if (g < 0)
            continue;
        if (group_slot[g] < 0) {
            group_slot[g] = (long)count;
            order[count++] = e;
        }
        entries.v[e].name_ref = group_slot[g];
    }
    free(group_slot);
    *order_out = order;
    return count;
}

static void find_shared_content(void)
{
    for (size_t i = 0; i < entries.n; i++) {
        entry_t *e = &entries.v[i];
        if (e->type != EK_FILE)
            continue;
        e->crc = crc_update(0, e->data, e->size);
        if (e->size == 0)
            continue;
        for (size_t j = 0; j < i; j++) {
            entry_t *o = &entries.v[j];
            if (o->type != EK_FILE || o->shared_ref >= 0 || o->size != e->size || o->crc != e->crc)
                continue;
            if (memcmp(o->data, e->data, e->size) == 0) {
                e->shared_ref = (long)j;
                break;
            }
        }
    }
}

static void compress_entries(void)
{
    for (size_t i = 0; i < entries.n; i++) {
        entry_t *e = &entries.v[i];
        if (e->type != EK_FILE || e->size == 0 || e->shared_ref >= 0)
            continue;
        buf_t z = {0};
        lz_compress(e->data, e->size, &z);
        if (z.n < e->size) {
            e->lz = 1;
            e->blob = z;
        } else {
            buf_free(&z);
            e->lz = 0;
            buf_put(&e->blob, e->data, e->size);
        }
    }
}

/* ------------------------------------------------------------------ */
/* archive writer                                                       */
/* ------------------------------------------------------------------ */

static void put_name(buf_t *b, const entry_t *e)
{
    buf_varint(b, e->namelen);
    buf_put(b, e->name, e->namelen);
}

static void build_table(buf_t *t, int64_t base, const size_t *name_order, size_t nnames)
{
    if (nnames) {
        buf_varint(t, nnames);
        for (size_t i = 0; i < nnames; i++)
            put_name(t, &entries.v[name_order[i]]);
    }
    buf_varint(t, entries.n);
    for (size_t i = 0; i < entries.n; i++) {
        const entry_t *e = &entries.v[i];
        uint8_t kind = (uint8_t)e->type;
        if (e->nsec)
            kind |= EK_NSEC;
        if (e->type == EK_FILE && e->lz)
            kind |= EK_LZ;
        if (e->type == EK_FILE && e->shared_ref >= 0)
            kind |= EK_SHARED;
        if (e->name_ref >= 0)
            kind |= EK_NAMEREF;
        buf_byte(t, kind);
        buf_varint(t, e->parent);
        if (e->name_ref >= 0)
            buf_varint(t, (uint64_t)e->name_ref);
        else
            put_name(t, e);
        if (e->type == EK_FILE)
            buf_varint(t, e->mode ^ DEFAULT_FILE_MODE);
        else if (e->type == EK_DIR)
            buf_varint(t, e->mode ^ DEFAULT_DIR_MODE);
        buf_varint(t, (uint64_t)(e->sec - base));
        if (e->nsec)
            buf_varint(t, e->nsec);
        if (e->type == EK_FILE) {
            buf_varint(t, e->size);
            if (e->size) {
                if (e->shared_ref >= 0) {
                    buf_varint(t, (uint64_t)e->shared_ref);
                } else {
                    if (e->lz)
                        buf_varint(t, e->blob.n);
                    buf_u32le(t, e->crc);
                }
            }
        } else if (e->type == EK_LINK) {
            buf_varint(t, e->size);
            buf_put(t, e->data, e->size);
        }
    }
}

static void write_all(int fd, const uint8_t *p, size_t n, const char *path)
{
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            die("cannot write %s: %s", path, strerror(errno));
        }
        p += w;
        n -= (size_t)w;
    }
}

static int pack(const char *dir, const char *out)
{
    struct stat rs;
    if (stat(dir, &rs) < 0)
        die("cannot stat %s: %s", dir, strerror(errno));
    if (!S_ISDIR(rs.st_mode))
        die("%s is not a directory", dir);

    scan_dir(dir, 0, 0);
    find_shared_content();
    compress_entries();
    size_t *name_order = NULL;
    size_t nnames = build_name_table(&name_order);

    int64_t base = (int64_t)rs.st_mtim.tv_sec;
    for (size_t i = 0; i < entries.n; i++)
        if (entries.v[i].sec < base)
            base = entries.v[i].sec;

    unsigned flags = 0;
    if (nnames)
        flags |= HF_NAMETAB;

    buf_t raw = {0};
    build_table(&raw, base, name_order, nnames);
    buf_t table = {0};
    if (raw.n > TABLE_LZ_THRESHOLD) {
        buf_t z = {0};
        lz_compress(raw.p, raw.n, &z);
        buf_t cand = {0};
        buf_varint(&cand, raw.n);
        buf_put(&cand, z.p, z.n);
        buf_free(&z);
        if (cand.n < raw.n) {
            flags |= HF_TABLZ;
            table = cand;
        } else {
            buf_free(&cand);
        }
    }
    if (!(flags & HF_TABLZ)) {
        table = raw;
        raw.p = NULL;
        raw.n = raw.cap = 0;
    }
    buf_free(&raw);

    buf_t hdr = {0};
    buf_put(&hdr, MAGIC_HEAD, 4);
    buf_byte(&hdr, FORMAT_VERSION);
    buf_byte(&hdr, (uint8_t)flags);
    buf_varint(&hdr, zigzag64(base));
    buf_varint(&hdr, (rs.st_mode & 07777) ^ DEFAULT_DIR_MODE);
    uint64_t rt = (uint64_t)((int64_t)rs.st_mtim.tv_sec - base) << 1;
    if (rs.st_mtim.tv_nsec)
        rt |= 1;
    buf_varint(&hdr, rt);
    if (rs.st_mtim.tv_nsec)
        buf_varint(&hdr, (uint64_t)rs.st_mtim.tv_nsec);

    uint64_t data_len = 0;
    for (size_t i = 0; i < entries.n; i++)
        data_len += entries.v[i].blob.n;

    buf_t foot = {0};
    buf_u64le(&foot, hdr.n + data_len);
    buf_u32le(&foot, (uint32_t)table.n);
    uint32_t c = crc_update(0, hdr.p, hdr.n);
    c = crc_update(c, table.p, table.n);
    buf_u32le(&foot, c);
    buf_put(&foot, MAGIC_TAIL, 4);

    size_t olen = strlen(out);
    char *tmp = xmalloc(olen + 16);
    snprintf(tmp, olen + 16, "%s.tmp%ld", out, (long)getpid());
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        die("cannot create %s: %s", tmp, strerror(errno));
    write_all(fd, hdr.p, hdr.n, tmp);
    for (size_t i = 0; i < entries.n; i++)
        write_all(fd, entries.v[i].blob.p, entries.v[i].blob.n, tmp);
    write_all(fd, table.p, table.n, tmp);
    write_all(fd, foot.p, foot.n, tmp);
    if (fsync(fd) < 0 && errno != EINVAL)
        die("cannot sync %s: %s", tmp, strerror(errno));
    if (close(fd) < 0)
        die("cannot close %s: %s", tmp, strerror(errno));
    if (rename(tmp, out) < 0) {
        unlink(tmp);
        die("cannot rename %s to %s: %s", tmp, out, strerror(errno));
    }
    free(tmp);
    free(name_order);
    buf_free(&hdr);
    buf_free(&table);
    buf_free(&foot);
    return 0;
}

static void usage(FILE *f)
{
    fprintf(f,
            "usage: vpack pack <dir> <out.vp>\n"
            "       vpack --version\n"
            "\n"
            "Packs the directory tree <dir> into the archive <out.vp>.\n"
            "Regular files, directories and symbolic links are stored with\n"
            "their permission bits and modification times.\n");
}

int main(int argc, char **argv)
{
    crc_init();
    if (argc >= 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        usage(stdout);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "--version")) {
        puts(VPACK_VERSION_STRING);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "pack") != 0) {
        fprintf(stderr, "%s: unknown command '%s' (this build can only pack)\n", progname, argv[1]);
        usage(stderr);
        return 2;
    }
    if (argc != 4) {
        usage(stderr);
        return 2;
    }
    return pack(argv[2], argv[3]);
}
