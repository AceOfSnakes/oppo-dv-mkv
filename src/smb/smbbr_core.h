// smbnfs: read-only NFSv3 server on 127.0.0.1 in front of one SMB2/SMB3 share (libsmb2, hardware AES/SHA).
// The player's kernel only speaks SMB1; it mounts this as NFS.
//   smbnfs serve [-p PORT] [-v] [-d 0xDIALECT] [-e] [-s] [-c CREDFILE] //HOST[:PORT]/SHARE
//   smbnfs mount [-p PORT] DIR
// CREDFILE: user=..., password=..., domain=... (one per line). Without it: guest.
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include "hwcrypto.h"
#define BLK (1u << 20)
#define NSLOT 32
#define NWORK 3
#define PF 10
#define MAXREC (64 * 1024)
#define OUTSZ (BLK + 8192)
#define MAGIC 0x534d4233u
#define FSID 0x534d4233ull
#define TTL 4.0                 // seconds a listing / attributes are trusted
#define MAXNODES 200000
static int verbose;
static uint32_t gen;
static char host[256], share[256], user[128] = "guest", password[256], domain[128];
static int dialect, seal, sign_;
static void logf_(const char *fmt, ...) {
    char b[1024];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    int n = snprintf(b, sizeof b, "%02d:%02d:%02d.%03d ", tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b + n, sizeof b - n, fmt, ap);
    va_end(ap);
    fprintf(stderr, "%s\n", b);
}
#define LOG(...) logf_(__VA_ARGS__)
#define VLOG(...) do { if (verbose) logf_(__VA_ARGS__); } while (0)

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int readall(int fd, void *b, size_t n) {
    char *p = b;
    while (n) {
        ssize_t r = read(fd, p, n);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return -1;
        p += r; n -= r;
    }
    return 0;
}
static int writeall(int fd, const void *b, size_t n) {
    const char *p = b;
    while (n) {
        ssize_t r = write(fd, p, n);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return -1;
        p += r; n -= r;
    }
    return 0;
}


/* ---------------- SMB ---------------- */
typedef struct { struct smb2_context *c; struct smb2fh *fh; uint32_t fid; } sconn;
static void sc_close(sconn *s) {
    if (s->c) smb2_destroy_context(s->c);   // also drops the file handles
    s->c = NULL; s->fh = NULL; s->fid = 0;
}
static pthread_mutex_t connmu = PTHREAD_MUTEX_INITIALIZER;
static int tcp_ok(const char *hp, int ms);
static int do_mount(int port, const char *dir);
static int net_err(const char *e) {       // 2 = the connection is the problem, 1 = odd, 0 = the server's final word
    if (strstr(e, "NAME_NOT_FOUND") || strstr(e, "PATH_NOT_FOUND") || strstr(e, "ACCESS_DENIED") || strstr(e, "LOGON_FAILURE") || strstr(e, "BAD_NETWORK_NAME")) return 0;
    if (strstr(e, "STATUS_") && !strstr(e, "DELETED") && !strstr(e, "CONNECTION") && !strstr(e, "TIMEOUT") && !strstr(e, "CANCELLED") && !strstr(e, "EXPIRED")) return 1;
    return 2;
}
static char lasterr[256];
static int sc_open(sconn *s) {
    if (s->c) return 0;
    pthread_mutex_lock(&connmu);
    int r = -1;
    struct smb2_context *c;
    if (!tcp_ok(host, 4000)) { snprintf(lasterr, sizeof lasterr, "no connection"); goto out; }
    if (!(c = smb2_init_context())) goto out;
    smb2_set_version(c, dialect ? dialect : SMB2_VERSION_ANY);
    if (seal) smb2_set_seal(c, 1);
    if (sign_) smb2_set_sign(c, 1);
    smb2_set_security_mode(c, sign_ ? SMB2_NEGOTIATE_SIGNING_REQUIRED : SMB2_NEGOTIATE_SIGNING_ENABLED);
    smb2_set_timeout(c, 8);
    smb2_set_user(c, user);
    if (password[0]) smb2_set_password(c, password);
    if (domain[0]) smb2_set_domain(c, domain);
    if (smb2_connect_share(c, host, share, user) < 0) { snprintf(lasterr, sizeof lasterr, "%s", smb2_get_error(c)); VLOG("connect //%s/%s: %s", host, share, lasterr); smb2_destroy_context(c); goto out; }
    s->c = c; r = 0;
out:
    pthread_mutex_unlock(&connmu);
    return r;
}

/* ---------------- nodes ---------------- */
typedef struct { uint32_t id, parent; int dir, gone; uint64_t size; uint32_t mtime; double t; char *name; char *path; int *kids; int nk; double lt; uint64_t lastb; } node;
static node **N; static uint32_t nn;                  // N[0] = root; ids are indexes, never reused
static pthread_mutex_t nmu = PTHREAD_MUTEX_INITIALIZER, metamu = PTHREAD_MUTEX_INITIALIZER;
static sconn meta;
static uint32_t *hash; static uint32_t hsz = 1 << 18;
static uint32_t hf(uint32_t p, const char *s) { uint32_t h = p * 2654435761u ^ 2166136261u; while (*s) h = (h ^ (uint8_t)*s++) * 16777619u; return h; }
static uint32_t node_find(uint32_t par, const char *name) {   // under nmu
    for (uint32_t h = hf(par, name);; h++) { uint32_t i = hash[h & (hsz - 1)]; if (!i) return 0; if (N[i]->parent == par && !strcmp(N[i]->name, name)) return i; }
}
static uint32_t node_put(uint32_t par, const char *name, const struct smb2_stat_64 *st) {   // under nmu
    uint32_t i = node_find(par, name); node *e;
    int dir = st->smb2_type == SMB2_TYPE_DIRECTORY;
    if (i && (N[i]->dir != dir || N[i]->size != st->smb2_size || N[i]->mtime != (uint32_t)st->smb2_mtime) && !dir) {
        N[i]->name[0] = 0; N[i]->gone = 1; i = 0;             // changed file: new id, so cached blocks cannot be mixed
    }
    if (!i) {
        if (nn >= MAXNODES) return 0;
        e = calloc(1, sizeof *e); i = nn; e->id = i; e->parent = par; e->name = strdup(name);
        const char *pp = N[par]->path; size_t l = strlen(pp) + strlen(name) + 2; e->path = malloc(l); snprintf(e->path, l, "%s%s%s", pp, *pp ? "/" : "", name);
        N[nn++] = e;
        uint32_t h = hf(par, name); while (hash[h & (hsz - 1)]) h++; hash[h & (hsz - 1)] = i;
    }
    e = N[i]; e->dir = dir; e->size = st->smb2_size; e->mtime = (uint32_t)st->smb2_mtime; e->t = now(); e->gone = 0;
    return i;
}
static int ent_get(uint32_t id, node *o) {
    pthread_mutex_lock(&nmu); int ok = id < nn && !N[id]->gone; if (ok) *o = *N[id]; pthread_mutex_unlock(&nmu); return ok;
}
static int ent_seq(uint32_t id, uint64_t b) { pthread_mutex_lock(&nmu); uint64_t l = N[id]->lastb; N[id]->lastb = b; pthread_mutex_unlock(&nmu); return b == l || b == l + 1; }
// refresh attributes when older than TTL; 0, or -errno
static int refresh(uint32_t id) {
    node e; if (!ent_get(id, &e)) return -ENOENT;
    if (!id || now() - e.t < TTL) return 0;
    struct smb2_stat_64 st; int r = -EIO;
    pthread_mutex_lock(&metamu);
    double t0 = now();
    for (int k = 0; k < 2 || (now() - t0 < 10 && !meta.c); k++) { if (sc_open(&meta)) { sleep(1); continue; } r = smb2_stat(meta.c, e.path, &st); if (r >= 0 || r == -ENOENT) break; sc_close(&meta); }
    pthread_mutex_unlock(&metamu);
    if (r < 0) return r;
    pthread_mutex_lock(&nmu); uint32_t i = node_put(e.parent, e.name, &st); pthread_mutex_unlock(&nmu);
    return i == id ? 0 : -ESTALE;
}
static uint32_t lookup(uint32_t par, const char *name, int *err) {
    node p, e; *err = 0;
    if (!ent_get(par, &p)) { *err = -ESTALE; return 0; }
    pthread_mutex_lock(&nmu); uint32_t i = node_find(par, name); if (i && now() - N[i]->t >= TTL) i = 0; pthread_mutex_unlock(&nmu);
    if (i) return i;
    char path[4096]; snprintf(path, sizeof path, "%s%s%s", p.path, *p.path ? "/" : "", name);
    struct smb2_stat_64 st; int r = -EIO; (void)e;
    pthread_mutex_lock(&metamu);
    double t0 = now();
    for (int k = 0; k < 2 || (now() - t0 < 10 && !meta.c); k++) { if (sc_open(&meta)) { sleep(1); continue; } r = smb2_stat(meta.c, path, &st); if (r >= 0 || r == -ENOENT) break; sc_close(&meta); }
    pthread_mutex_unlock(&metamu);
    if (r < 0) { *err = r; return 0; }
    pthread_mutex_lock(&nmu); i = node_put(par, name, &st); pthread_mutex_unlock(&nmu);
    return i;
}
static int listdir(uint32_t id) {        // fills kids; 0 or -errno
    node e; if (!ent_get(id, &e)) return -ESTALE;
    if (e.lt && now() - e.lt < TTL) return 0;
    int r = -EIO;
    pthread_mutex_lock(&metamu);
    double t0 = now();
    for (int k = 0; k < 2 || (now() - t0 < 10 && !meta.c); k++) {
        if (sc_open(&meta)) { sleep(1); continue; }
        struct smb2dir *d = smb2_opendir(meta.c, e.path);
        if (!d) { LOG("list '%s': %s", e.path, smb2_get_error(meta.c)); sc_close(&meta); continue; }
        struct smb2dirent *de; int n = 0, cap = 0; int *kk = NULL;
        pthread_mutex_lock(&nmu);
        while ((de = smb2_readdir(meta.c, d))) {
            if (!strcmp(de->name, ".") || !strcmp(de->name, "..")) continue;
            if (de->st.smb2_type != SMB2_TYPE_DIRECTORY && de->st.smb2_type != SMB2_TYPE_FILE) continue;
            uint32_t i = node_put(id, de->name, &de->st); if (!i) break;
            if (n == cap) kk = realloc(kk, (cap = cap ? cap * 2 : 64) * sizeof *kk);
            kk[n++] = i;
        }
        node *q = N[id]; free(q->kids); q->kids = kk; q->nk = n; q->lt = now();
        pthread_mutex_unlock(&nmu);
        smb2_closedir(meta.c, d); r = 0; break;
    }
    pthread_mutex_unlock(&metamu);
    return r;
}
/* ---------------- block cache ---------------- */
enum { S_EMPTY, S_LOADING, S_READY, S_ERR };
typedef struct {
    int state, ref, queued;
    uint32_t id, len;
    uint64_t blk, stamp;
    char *data;
} slot_t;
static slot_t slots[NSLOT];
static int queue[NSLOT], qh, qn;  // ring of slot indexes waiting for a worker
static uint64_t lru;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv_slot = PTHREAD_COND_INITIALIZER, cv_job = PTHREAD_COND_INITIALIZER;
static uint64_t st_served, st_fetched, st_hits, st_miss, st_err;

static void q_push(int i, int front) {
    if (front) { qh = (qh + NSLOT - 1) % NSLOT; queue[qh] = i; }
    else queue[(qh + qn) % NSLOT] = i;
    qn++;
    slots[i].queued = 1;
    pthread_cond_signal(&cv_job);
}
static void q_promote(int i) {  // demand read waiting on a queued read-ahead block: move it to the front
    for (int k = 0; k < qn; k++)
        if (queue[(qh + k) % NSLOT] == i) {
            for (int j = k; j > 0; j--) queue[(qh + j) % NSLOT] = queue[(qh + j - 1) % NSLOT];
            queue[qh] = i;
            return;
        }
}
static int find_slot(uint32_t id, uint64_t b) {
    for (int i = 0; i < NSLOT; i++)
        if (slots[i].state != S_EMPTY && slots[i].id == id && slots[i].blk == b) return i;
    return -1;
}
static int claim_slot(void) {
    int best = -1;
    for (int i = 0; i < NSLOT; i++) {
        slot_t *s = &slots[i];
        if (s->state == S_EMPTY) { best = i; break; }
        if (s->state == S_LOADING || s->ref) continue;
        if (best < 0 || s->stamp < slots[best].stamp) best = i;
    }
    if (best >= 0 && !slots[best].data) {
        slots[best].data = malloc(BLK);
        if (!slots[best].data) return -1;
    }
    return best;
}
static void slot_setup(int i, const node *e, uint64_t b) {
    slot_t *s = &slots[i];
    s->state = S_LOADING;
    s->id = e->id;
    s->blk = b;
    uint64_t rem = e->size - b * BLK;
    s->len = rem < BLK ? (uint32_t)rem : BLK;
    s->ref = 0;
}

// Pin the cached block b of e (fetching it if needed). Returns slot index or -1.
static int get_block(const node *e, uint64_t b) {
    pthread_mutex_lock(&mu);
    int miss = 0;
    for (;;) {
        int i = find_slot(e->id, b);
        if (i >= 0) {
            slot_t *s = &slots[i];
            if (s->state == S_READY) {
                s->ref++;
                s->stamp = ++lru;
                if (miss) st_miss++; else st_hits++;
                pthread_mutex_unlock(&mu);
                return i;
            }
            if (s->state == S_ERR) {
                s->state = S_EMPTY;
                st_err++;
                pthread_mutex_unlock(&mu);
                return -1;
            }
            miss = 1;
            if (s->queued) q_promote(i);
            pthread_cond_wait(&cv_slot, &mu);
            continue;
        }
        i = claim_slot();
        if (i < 0) { pthread_cond_wait(&cv_slot, &mu); continue; }
        slot_setup(i, e, b);
        q_push(i, 1);
        miss = 1;
    }
}
static void put_block(int i) {
    pthread_mutex_lock(&mu);
    slots[i].ref--;
    pthread_cond_broadcast(&cv_slot);
    pthread_mutex_unlock(&mu);
}

static void prefetch(const node *e, uint64_t lastb, int seq) {
    int n = seq ? PF : 1;
    pthread_mutex_lock(&mu);
    for (int k = 1; k <= n; k++) {
        uint64_t b = lastb + k;
        if (b * BLK >= e->size) break;
        if (find_slot(e->id, b) >= 0) continue;
        int i = claim_slot();
        if (i < 0) break;
        slot_setup(i, e, b);
        q_push(i, 0);
    }
    pthread_mutex_unlock(&mu);
}
static __thread char ferr[256];
static int fetch(sconn *s, const node *e, uint64_t b, char *d, uint32_t len) {
    if (sc_open(s)) { snprintf(ferr, sizeof ferr, "connect"); return -1; }
    if (s->fid != e->id) {
        if (s->fh) smb2_close(s->c, s->fh);
        s->fid = 0; s->fh = smb2_open(s->c, e->path, O_RDONLY);
        if (!s->fh) { snprintf(ferr, sizeof ferr, "%s", smb2_get_error(s->c)); LOG("open '%s': %s", e->path, smb2_get_error(s->c)); return -1; }
        s->fid = e->id;
    }
    uint32_t got = 0;
    while (got < len) {
        uint32_t m = smb2_get_max_read_size(s->c), q = len - got; if (q > m) q = m;
        int r = smb2_pread(s->c, s->fh, (uint8_t *)d + got, q, b * BLK + got);
        if (r <= 0) { snprintf(ferr, sizeof ferr, "%s", smb2_get_error(s->c)); LOG("read '%s' blk %llu: %d %s", e->path, (unsigned long long)b, r, smb2_get_error(s->c)); return -1; }
        got += r;
    }
    return 0;
}
static double st_worst;
#define RETRY 22.0              // seconds a read waits for a server that went away
static void *worker(void *arg) {
    (void)arg;
    sconn c = {0};
    pthread_mutex_lock(&mu);
    for (;;) {
        while (qn == 0) pthread_cond_wait(&cv_job, &mu);
        int i = queue[qh];
        qh = (qh + 1) % NSLOT;
        qn--;
        slot_t *s = &slots[i];
        s->queued = 0;
        uint32_t id = s->id, len = s->len;
        uint64_t b = s->blk;
        char *d = s->data;
        pthread_mutex_unlock(&mu);
        node e;
        int ok = 0;
        double t = now();
        if (ent_get(id, &e)) for (int k = 0; !ok; k++) {
            ferr[0] = 0; ok = fetch(&c, &e, b, d, len) == 0;
            if (ok) break;
            sc_close(&c);
            int w = net_err(ferr);
            if ((w == 0 && k >= 1) || (w == 1 && k >= 2) || now() - t > RETRY) break;
            if (k) sleep(1);
        }
        t = now() - t;
        pthread_mutex_lock(&mu);
        if (t > st_worst) st_worst = t;
        s->state = ok ? S_READY : S_ERR;
        s->stamp = ++lru;
        st_fetched += ok ? len : 0;
        pthread_cond_broadcast(&cv_slot);
    }
    return NULL;
}
static int read_range(const node *e, uint64_t off, uint32_t n, uint8_t *dst) {
    uint64_t lastb = off / BLK;
    prefetch(e, off / BLK, ent_seq(e->id, off / BLK));
    while (n) {
        uint64_t b = off / BLK;
        uint32_t bo = off % BLK;
        int i = get_block(e, b);
        if (i < 0) return -1;
        uint32_t take = slots[i].len > bo ? slots[i].len - bo : 0;
        if (take > n) take = n;
        if (!take) { put_block(i); return -1; }
        memcpy(dst, slots[i].data + bo, take);
        put_block(i);
        dst += take; off += take; n -= take;
        lastb = b;
    }
    (void)lastb;
    return 0;
}
/* ---------------- XDR / RPC ---------------- */
typedef struct { const uint8_t *p; uint32_t n, o; int err; } xin;
typedef struct { uint8_t *p; uint32_t n, o; } xout;

static uint32_t gu32(xin *x) {
    if (x->o + 4 > x->n) { x->err = 1; return 0; }
    uint32_t v;
    memcpy(&v, x->p + x->o, 4);
    x->o += 4;
    return ntohl(v);
}
static uint64_t gu64(xin *x) { uint64_t h = gu32(x); return h << 32 | gu32(x); }
static uint32_t gopaque(xin *x, const uint8_t **ptr) {
    uint32_t l = gu32(x);
    if (x->err || l > x->n - x->o) { x->err = 1; return 0; }
    *ptr = x->p + x->o;
    x->o += (l + 3) & ~3u;
    if (x->o > x->n) x->o = x->n;
    return l;
}
static void pu32(xout *x, uint32_t v) {
    if (x->o + 4 <= x->n) { v = htonl(v); memcpy(x->p + x->o, &v, 4); }
    x->o += 4;
}
static void pu64(xout *x, uint64_t v) { pu32(x, v >> 32); pu32(x, (uint32_t)v); }
static void popaque(xout *x, const void *d, uint32_t l) {
    pu32(x, l);
    if (x->o + ((l + 3) & ~3u) <= x->n) {
        memcpy(x->p + x->o, d, l);
        memset(x->p + x->o + l, 0, ((l + 3) & ~3u) - l);
    }
    x->o += (l + 3) & ~3u;
}

enum { NFS_OK = 0, ERR_NOENT = 2, ERR_IO = 5, ERR_NOTDIR = 20, ERR_ISDIR = 21, ERR_INVAL = 22, ERR_ROFS = 30,
       ERR_STALE = 70, ERR_BADHANDLE = 10001, ERR_NOTSUPP = 10004 };

static void put_fh(xout *o, uint32_t id) {
    uint32_t fh[3] = {htonl(MAGIC), htonl(id), htonl(gen)};
    popaque(o, fh, sizeof fh);
}
static int get_fh(xin *in, uint32_t *id) {  // 0 or nfsstat3
    const uint8_t *p;
    uint32_t l = gopaque(in, &p);
    if (in->err) return ERR_BADHANDLE;
    if (l != 12) return ERR_BADHANDLE;
    uint32_t v[3];
    memcpy(v, p, 12);
    if (ntohl(v[0]) != MAGIC) return ERR_BADHANDLE;
    *id = ntohl(v[1]);
    // the root handle stays valid across server restarts (so a mount survives one); file handles carry the
    // server's start time, so a handle from before a restart can never reach a different URL
    if (*id != 0 && ntohl(v[2]) != gen) return ERR_STALE;
    return 0;
}


static void fattr(xout *o, const node *e) {
    pu32(o, e->dir ? 2 : 1); pu32(o, e->dir ? 0555 : 0444); pu32(o, e->dir ? 2 : 1); pu32(o, 0); pu32(o, 0);
    pu64(o, e->dir ? 4096 : e->size); pu64(o, e->dir ? 4096 : e->size); pu32(o, 0); pu32(o, 0);
    pu64(o, FSID); pu64(o, 1 + (uint64_t)e->id);
    for (int k = 0; k < 3; k++) { pu32(o, e->mtime); pu32(o, 0); }
}
static void post(xout *o, uint32_t id) { node e; if (ent_get(id, &e)) { pu32(o, 1); fattr(o, &e); } else pu32(o, 0); }
static int nerr(int r) { return r == -ENOENT ? ERR_NOENT : r == -ESTALE ? ERR_STALE : ERR_IO; }
static int resolve(xin *in, uint32_t *id, node *e, int ref) {
    int st = get_fh(in, id);
    if (st) return st;
    if (ref) { int r = refresh(*id); if (r) return r == -ENOENT ? ERR_STALE : nerr(r); }
    return ent_get(*id, e) ? 0 : ERR_STALE;
}
static int nfs3(uint32_t proc, xin *in, xout *o) {
    uint32_t id = 0; node e; int st;
    switch (proc) {
    case 0: return 0;
    case 1:
        st = resolve(in, &id, &e, 1); if (in->err) return 4;
        pu32(o, st); if (!st) fattr(o, &e); return 0;
    case 3: {
        st = resolve(in, &id, &e, 0);
        const uint8_t *nm; uint32_t nl = gopaque(in, &nm); if (in->err) return 4;
        if (!st && !e.dir) st = ERR_NOTDIR;
        if (st) { pu32(o, st); pu32(o, 0); return 0; }
        char name[1024]; int err = 0; uint32_t c;
        if (!nl || nl >= sizeof name || memchr(nm, '/', nl)) { pu32(o, ERR_NOENT); post(o, id); return 0; }
        memcpy(name, nm, nl); name[nl] = 0;
        if (!strcmp(name, ".")) c = id; else if (!strcmp(name, "..")) c = e.parent; else c = lookup(id, name, &err);
        if (err || (!c && name[0] != '.')) { pu32(o, nerr(err ? err : -EIO)); post(o, id); return 0; }
        pu32(o, 0); put_fh(o, c); post(o, c); post(o, id); return 0; }
    case 4: {
        st = resolve(in, &id, &e, 0); uint32_t acc = gu32(in); if (in->err) return 4;
        pu32(o, st); if (st) { pu32(o, 0); return 0; }
        post(o, id); pu32(o, acc & (e.dir ? 0x23 : 0x01)); return 0; }
    case 5: get_fh(in, &id); pu32(o, ERR_INVAL); pu32(o, 0); return 0;
    case 6: {
        st = resolve(in, &id, &e, 0); uint64_t off = gu64(in); uint32_t cnt = gu32(in); if (in->err) return 4;
        if (!st && e.dir) st = ERR_ISDIR;
        if (st) { pu32(o, st); pu32(o, 0); return 0; }
        if (cnt > BLK) cnt = BLK;
        uint32_t n = off >= e.size ? 0 : (e.size - off < cnt ? (uint32_t)(e.size - off) : cnt), start = o->o;
        pu32(o, 0); post(o, id); pu32(o, n); pu32(o, off + n >= e.size); pu32(o, n);
        if (o->o + ((n + 3) & ~3u) > o->n || (n && read_range(&e, off, n, o->p + o->o))) { o->o = start; pu32(o, ERR_IO); pu32(o, 0); return 0; }
        memset(o->p + o->o + n, 0, ((n + 3) & ~3u) - n); o->o += (n + 3) & ~3u;
        pthread_mutex_lock(&mu); st_served += n; pthread_mutex_unlock(&mu);
        return 0; }
    case 16: {
        st = resolve(in, &id, &e, 0); uint64_t ck = gu64(in); gu64(in); uint32_t cnt = gu32(in); if (in->err) return 4;
        if (!st && !e.dir) st = ERR_NOTDIR;
        int r; if (!st && (!ck || !e.kids) && (r = listdir(id))) st = nerr(r);
        if (st) { pu32(o, st); pu32(o, 0); return 0; }
        pu32(o, 0); post(o, id); pu64(o, 0);
        uint32_t base = o->o; int eof = 1; if (cnt > 60000) cnt = 60000;
        pthread_mutex_lock(&nmu);
        for (uint64_t k = ck; k < (uint64_t)N[id]->nk; k++) {
            node *c = N[N[id]->kids[k]]; if (c->gone) continue;
            uint32_t l = strlen(c->name), need = 4 + 8 + 4 + ((l + 3) & ~3u) + 8;
            if (o->o - base + need + 120 > cnt) { eof = 0; break; }
            pu32(o, 1); pu64(o, 1 + (uint64_t)c->id); popaque(o, c->name, l); pu64(o, k + 1);
        }
        pthread_mutex_unlock(&nmu);
        pu32(o, 0); pu32(o, eof); return 0; }
    case 17: get_fh(in, &id); pu32(o, ERR_NOTSUPP); pu32(o, 0); return 0;
    case 18: st = get_fh(in, &id); if (in->err) return 4; pu32(o, st); if (st) { pu32(o, 0); return 0; }
        post(o, id); pu64(o, 1ull << 40); pu64(o, 0); pu64(o, 0); pu64(o, 1000); pu64(o, 0); pu64(o, 0); pu32(o, 0); return 0;
    case 19: st = get_fh(in, &id); if (in->err) return 4; pu32(o, st); if (st) { pu32(o, 0); return 0; }
        post(o, id); pu32(o, BLK); pu32(o, BLK); pu32(o, 4096); pu32(o, 65536); pu32(o, 65536); pu32(o, 4096);
        pu32(o, 32768); pu64(o, 0x7fffffffffffffffull); pu32(o, 1); pu32(o, 0); pu32(o, 0x8); return 0;
    case 20: st = get_fh(in, &id); if (in->err) return 4; pu32(o, st); if (st) { pu32(o, 0); return 0; }
        post(o, id); pu32(o, 1); pu32(o, 255); pu32(o, 1); pu32(o, 1); pu32(o, 0); pu32(o, 1); return 0;
    case 2: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 21:
        pu32(o, ERR_ROFS); pu32(o, 0); pu32(o, 0); pu32(o, 0); pu32(o, 0); return 0;
    }
    return 3;
}
static int mnt3(uint32_t proc, xin *in, xout *o) {
    const uint8_t *p;
    switch (proc) {
    case 0: return 0;
    case 1: {  // MNT
        uint32_t l = gopaque(in, &p);
        if (in->err) return 4;
        LOG("mount request '%.*s'", (int)l, p);
        pu32(o, 0); put_fh(o, 0); pu32(o, 1); pu32(o, 1);  // OK, root fh, flavors [AUTH_UNIX]
        return 0;
    }
    case 2: pu32(o, 0); return 0;  // DUMP: empty
    case 3: gopaque(in, &p); return 0;  // UMNT
    case 4: return 0;                    // UMNTALL
    case 5:                              // EXPORT: "/" for everyone
        pu32(o, 1); popaque(o, "/", 1); pu32(o, 0); pu32(o, 0);
        return 0;
    }
    return 3;
}

static void handle(xin *in, xout *o) {
    uint32_t xid = gu32(in), mt = gu32(in);
    if (in->err || mt != 0) { o->o = 0; return; }
    uint32_t rv = gu32(in), prog = gu32(in), vers = gu32(in), proc = gu32(in);
    const uint8_t *p;
    gu32(in); gopaque(in, &p);  // cred
    gu32(in); gopaque(in, &p);  // verf
    if (in->err) { o->o = 0; return; }
    pu32(o, xid); pu32(o, 1);
    if (rv != 2) { pu32(o, 1); pu32(o, 0); pu32(o, 2); pu32(o, 2); return; }
    pu32(o, 0); pu32(o, 0); pu32(o, 0);  // MSG_ACCEPTED, verf AUTH_NULL
    uint32_t apos = o->o;
    pu32(o, 0);
    int r;
    if (prog == 100003 && vers == 3) r = nfs3(proc, in, o);
    else if (prog == 100005 && vers == 3) r = mnt3(proc, in, o);
    else if (prog == 100003 || prog == 100005) {
        o->o = apos; pu32(o, 2); pu32(o, 3); pu32(o, 3);  // PROG_MISMATCH 3..3
        return;
    } else { o->o = apos; pu32(o, 1); return; }  // PROG_UNAVAIL
    if (r) {
        VLOG("prog %u proc %u -> accept_stat %d", prog, proc, r);
        o->o = apos;
        pu32(o, r);
    }
}

static volatile int nconn;
static void *conn_thread(void *arg) {
    int fd = (int)(intptr_t)arg;
    uint8_t *rb = malloc(MAXREC), *ob = malloc(OUTSZ);
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    __sync_fetch_and_add(&nconn, 1);
    VLOG("rpc connection %d", fd);
    while (rb && ob) {
        uint32_t len = 0;
        int last = 0;
        while (!last) {
            uint32_t mk;
            if (readall(fd, &mk, 4)) goto out;
            mk = ntohl(mk);
            last = mk >> 31;
            uint32_t fl = mk & 0x7fffffff;
            if (len + fl > MAXREC) { LOG("rpc record too large (%u)", len + fl); goto out; }
            if (readall(fd, rb + len, fl)) goto out;
            len += fl;
        }
        xin in = {rb, len, 0, 0};
        xout o = {ob + 4, OUTSZ - 4, 0};
        handle(&in, &o);
        if (!o.o) continue;
        if (o.o > o.n) { LOG("reply overflow"); goto out; }
        uint32_t mk = htonl(0x80000000u | o.o);
        memcpy(ob, &mk, 4);
        if (writeall(fd, ob, o.o + 4)) goto out;
    }
out:
    __sync_fetch_and_sub(&nconn, 1);
    VLOG("rpc connection %d closed", fd);
    close(fd);
    free(rb);
    free(ob);
    return NULL;
}

