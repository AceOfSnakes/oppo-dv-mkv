// dlnaiso: read-only NFSv3 server on 127.0.0.1 that exposes HTTP URLs (DLNA items) as files.
// The kernel NFS client mounts it, so the player's stock ISO path (losetup + udf mount) can open
// a disc image that is only reachable over HTTP. Reads become HTTP Range requests (1 MB blocks,
// LRU cache, read-ahead by worker threads).
//
//   dlnaiso serve [-p PORT] [-r REGDIR] [-v]   run the server (foreground, log on stderr)
//   dlnaiso mount [-p PORT] DIR                mount the server on DIR (kernel nfs, v3/tcp, soft, ro)
//   dlnaiso add [-r REGDIR] NAME URL           publish URL as DIR/NAME (writes REGDIR/NAME.url)
//   dlnaiso disc [-v] URL [IMG]                debug: list a disc folder (-v: files), IMG: sparse test image
//   dlnaiso get URL OFF LEN                    debug: fetch a range to stdout
//
// A file appears in the mount for every REGDIR/<name>.url (one URL per file). A URL inside a Blu-ray or DVD folder
// on a WebDAV server (a path with a /BDMV/ or /VIDEO_TS/ part) or a folder URL ending in '/' is published as a disc
// image made up from the folder's files (see "disc folders" below).
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
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

#define BLK (1u << 20)          // cache block = max NFS read size
#define NSLOT 24                // cache blocks (24 MB)
#define NWORK 3                 // HTTP fetch threads
#define PF 8                    // read-ahead blocks when sequential
#define MAXENT 32
#define MAXREC (64 * 1024)      // largest RPC call we accept (we never take WRITE data)
#define OUTSZ (BLK + 8192)
#define MAGIC 0x444c4953u       // "DLIS"
#define FSID 0x444c4953ull

static const char *regdir = "/tmp/dlnaiso";
static int verbose;
static uint32_t gen;

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

/* ---------------- HTTP ---------------- */

// http://[user[:password]@]host[:port][/path]. The login (percent-decoded) becomes "Basic <base64>" in auth
// (empty without one): WebDAV servers such as rclone serve want it, and their links carry it.
static int parse_url(const char *url, char *host, size_t hl, int *port, const char **path, char *auth, size_t al) {
    if (strncasecmp(url, "http://", 7)) return -1;
    const char *h = url + 7, *slash = strchr(h, '/');
    const char *end = slash ? slash : h + strlen(h);
    const char *at = NULL;
    for (const char *p = h; p < end; p++)
        if (*p == '@') at = p;  // the last '@' of the authority ends the login
    auth[0] = 0;
    if (at) {
        unsigned char plain[384];
        size_t n = 0;
        for (const char *p = h; p < at; p++) {
            int c = (unsigned char)*p;
            if (c == '%' && p + 2 < at) {
                char hx[3] = {p[1], p[2], 0};
                char *e;
                long v = strtol(hx, &e, 16);
                if (*e == 0) { c = (int)v; p += 2; }
            }
            if (n >= sizeof plain) return -1;
            plain[n++] = (unsigned char)c;
        }
        static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        if (al < 7 + (n + 2) / 3 * 4 + 1) return -1;
        char *o = auth + sprintf(auth, "Basic ");
        for (size_t i = 0; i < n; i += 3) {
            unsigned v = plain[i] << 16 | (i + 1 < n ? plain[i + 1] << 8 : 0) | (i + 2 < n ? plain[i + 2] : 0);
            *o++ = b64[v >> 18 & 63];
            *o++ = b64[v >> 12 & 63];
            *o++ = i + 1 < n ? b64[v >> 6 & 63] : '=';
            *o++ = i + 2 < n ? b64[v & 63] : '=';
        }
        *o = 0;
        h = at + 1;
    }
    const char *colon = memchr(h, ':', end - h);
    size_t n = (colon ? colon : end) - h;
    if (n == 0 || n >= hl) return -1;
    memcpy(host, h, n);
    host[n] = 0;
    *port = colon ? atoi(colon + 1) : 80;
    *path = slash ? slash : "/";
    return 0;
}

// The URL for logs: the login replaced by "***".
static const char *redact(const char *url, char *b, size_t bl) {
    const char *h = strstr(url, "://"), *at = NULL;
    if (h) {
        h += 3;
        for (const char *p = h; *p && *p != '/'; p++)
            if (*p == '@') at = p;
    }
    if (!at) return url;
    snprintf(b, bl, "%.*s***%s", (int)(h - url), url, at);
    return b;
}

// Request path: spaces (some apps send them raw) as %20, everything else as given.
static int put_path(char *d, size_t dl, const char *path) {
    size_t n = 0;
    for (; *path; path++) {
        const char *s = *path == ' ' ? "%20" : NULL;
        size_t k = s ? 3 : 1;
        if (n + k >= dl) return -1;
        if (s) memcpy(d + n, s, 3); else d[n] = *path;
        n += k;
    }
    d[n] = 0;
    return 0;
}

typedef struct { int fd; char host[256]; int port; } hconn;

static void hc_close(hconn *c) {
    if (c->fd >= 0) close(c->fd);
    c->fd = -1;
}

static int hc_open(hconn *c, const char *host, int port) {
    if (c->fd >= 0 && c->port == port && !strcmp(c->host, host)) return 0;
    hc_close(c);
    struct addrinfo hints = {0}, *ai;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char ps[16];
    snprintf(ps, sizeof ps, "%d", port);
    if (getaddrinfo(host, ps, &hints, &ai)) return -1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { freeaddrinfo(ai); return -1; }
    struct timeval tv = {15, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    int one = 1, rb = 1 << 20;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rb, sizeof rb);
    if (connect(fd, ai->ai_addr, ai->ai_addrlen) < 0) { close(fd); freeaddrinfo(ai); return -1; }
    freeaddrinfo(ai);
    c->fd = fd;
    snprintf(c->host, sizeof c->host, "%s", host);
    c->port = port;
    return 0;
}

// One HTTP request on a keep-alive connection. head=1: HEAD (total = Content-Length).
// Otherwise GET bytes=off..off+len-1 into buf; *nb = bytes stored; *total from Content-Range.
// Returns the HTTP status, or -1 on transport/protocol error.
static int hc_do(hconn *c, const char *url, int head, uint64_t off, uint32_t len, char *buf, uint32_t *nb,
                 uint64_t *total) {
    char host[256], auth[600], ah[640] = "", rp[3072];
    int port;
    const char *path;
    if (nb) *nb = 0;
    if (parse_url(url, host, sizeof host, &port, &path, auth, sizeof auth)) return -1;
    if (put_path(rp, sizeof rp, path)) return -1;
    if (auth[0]) snprintf(ah, sizeof ah, "Authorization: %s\r\n", auth);
    for (int attempt = 0; attempt < 2; attempt++) {
        if (hc_open(c, host, port)) continue;
        char req[4096];
        int n;
        if (head)
            n = snprintf(req, sizeof req,
                         "HEAD %s HTTP/1.1\r\nHost: %s:%d\r\nUser-Agent: dlnaiso/1\r\n%sConnection: keep-alive\r\n\r\n",
                         rp, host, port, ah);
        else
            n = snprintf(req, sizeof req,
                         "GET %s HTTP/1.1\r\nHost: %s:%d\r\nUser-Agent: dlnaiso/1\r\n%sRange: bytes=%llu-%llu\r\n"
                         "Connection: keep-alive\r\n\r\n",
                         rp, host, port, ah, (unsigned long long)off, (unsigned long long)(off + len - 1));
        if (n <= 0 || n >= (int)sizeof req) return -1;
        if (writeall(c->fd, req, n)) { hc_close(c); continue; }
        char hb[8192];
        int hl = 0;
        char *e = NULL;
        while (!e && hl < (int)sizeof hb - 1) {
            ssize_t r = read(c->fd, hb + hl, sizeof hb - 1 - hl);
            if (r < 0 && errno == EINTR) continue;
            if (r <= 0) break;
            hl += r;
            hb[hl] = 0;
            e = strstr(hb, "\r\n\r\n");
        }
        if (!e) {
            hc_close(c);
            if (hl == 0) continue;  // stale keep-alive connection: retry on a new one
            return -1;
        }
        int st = 0;
        if (sscanf(hb, "HTTP/%*d.%*d %d", &st) != 1) { hc_close(c); return -1; }
        int hdrlen = e + 4 - hb, extra = hl - hdrlen;
        uint64_t clen = UINT64_MAX, ctot = 0;
        int closeit = 0, chunked = 0;
        *e = 0;
        for (char *l = strstr(hb, "\r\n"); l;) {
            l += 2;
            char *nl = strstr(l, "\r\n");
            if (nl) *nl = 0;  // one header line at a time
            if (!strncasecmp(l, "content-length:", 15)) clen = strtoull(l + 15, NULL, 10);
            else if (!strncasecmp(l, "content-range:", 14)) {
                char *s = strchr(l, '/');
                if (s) ctot = strtoull(s + 1, NULL, 10);
            } else if (!strncasecmp(l, "connection:", 11) && strcasestr(l, "close"))
                closeit = 1;
            else if (!strncasecmp(l, "transfer-encoding:", 18) && strcasestr(l, "chunked"))
                chunked = 1;
            l = nl;
        }
        if (head) {
            if (total) *total = clen != UINT64_MAX ? clen : 0;
            if (closeit || extra) hc_close(c);
            return st;
        }
        if (chunked || clen == UINT64_MAX) { hc_close(c); return -1; }
        if (st == 206) {
            if (total && ctot) *total = ctot;
        } else if (st == 200) {
            if (total) *total = clen;
            if (off != 0) { hc_close(c); return st; }  // range ignored: unusable past 0
        } else {
            hc_close(c);
            return st;
        }
        uint32_t take = clen < len ? (uint32_t)clen : len;
        uint32_t got = (uint32_t)extra < take ? (uint32_t)extra : take;
        memcpy(buf, hb + hdrlen, got);
        if (readall(c->fd, buf + got, take - got)) { hc_close(c); return -1; }
        if (clen > take || (uint32_t)extra > take || closeit) hc_close(c);
        if (nb) *nb = take;
        return st;
    }
    return -1;
}

static int url_size(const char *url, uint64_t *sz) {
    hconn c = {.fd = -1};
    uint64_t t = 0;
    char b[2];
    uint32_t nb;
    int st = hc_do(&c, url, 1, 0, 0, NULL, NULL, &t);
    if (st == 200 && t > 0) { hc_close(&c); *sz = t; return 0; }
    t = 0;
    st = hc_do(&c, url, 0, 0, 1, b, &nb, &t);
    hc_close(&c);
    if ((st == 206 || st == 200) && t > 0) { *sz = t; return 0; }
    char rb[2048];
    LOG("size: %s -> status %d", redact(url, rb, sizeof rb), st);
    return -1;
}

// One request on its own connection (Connection: close). The whole body (chunked or not, at most 32 MB) is returned
// NUL-terminated in *body (free it). Returns the HTTP status or -1.
static int http_req(const char *url, const char *method, const char *hdrs, const char *payload, char **body,
                    size_t *blen) {
    char host[256], auth[600], ah[640] = "", rp[3072];
    int port;
    const char *path;
    *body = NULL;
    *blen = 0;
    if (parse_url(url, host, sizeof host, &port, &path, auth, sizeof auth) || put_path(rp, sizeof rp, path)) return -1;
    if (auth[0]) snprintf(ah, sizeof ah, "Authorization: %s\r\n", auth);
    hconn c = {.fd = -1};
    if (hc_open(&c, host, port)) return -1;
    size_t pl = payload ? strlen(payload) : 0;
    char req[4096 + 1024];
    int n = snprintf(req, sizeof req,
                     "%s %s HTTP/1.1\r\nHost: %s:%d\r\nUser-Agent: dlnaiso/1\r\n%s%sContent-Length: %zu\r\n"
                     "Connection: close\r\n\r\n",
                     method, rp, host, port, ah, hdrs, pl);
    if (n <= 0 || n >= (int)sizeof req || writeall(c.fd, req, n) || (pl && writeall(c.fd, payload, pl))) {
        hc_close(&c);
        return -1;
    }
    size_t cap = 1 << 16, len = 0;
    char *b = malloc(cap + 1);
    while (b) {
        if (len == cap) {
            char *nb = cap < (32u << 20) ? realloc(b, cap * 2 + 1) : NULL;
            if (!nb) break;
            b = nb;
            cap *= 2;
        }
        ssize_t r = read(c.fd, b + len, cap - len);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;
        len += r;
    }
    hc_close(&c);
    if (!b) return -1;
    b[len] = 0;
    char *e = strstr(b, "\r\n\r\n");
    int st = 0, chunked = 0;
    if (!e || sscanf(b, "HTTP/%*d.%*d %d", &st) != 1) { free(b); return -1; }
    *e = 0;
    for (char *l = strstr(b, "\r\n"); l;) {
        l += 2;
        char *nl = strstr(l, "\r\n");
        if (nl) *nl = 0;
        if (!strncasecmp(l, "transfer-encoding:", 18) && strcasestr(l, "chunked")) chunked = 1;
        l = nl;
    }
    char *s = e + 4, *end = b + len, *d = b;
    if (chunked) {
        while (s < end) {
            char *q;
            unsigned long long cl = strtoull(s, &q, 16);
            char *nl = strstr(s, "\r\n");
            if (!nl) break;
            s = nl + 2;
            if (cl == 0) break;
            if (cl > (unsigned long long)(end - s)) cl = end - s;
            memmove(d, s, cl);
            d += cl;
            s += cl + 2;
        }
    } else {
        memmove(d, s, end - s);
        d += end - s;
    }
    *d = 0;
    *body = b;
    *blen = d - b;
    return st;
}

/* ---------------- disc folders (BDMV / VIDEO_TS) as a UDF image ---------------- */
// A URL inside a Blu-ray or DVD folder on a WebDAV server (rclone serve webdav and others: .../BDMV/STREAM/00800.m2ts,
// .../BDMV/index.bdmv) or the folder itself (ending in '/') is published as a disc image made up here: the folder
// tree comes from PROPFIND, the UDF 2.01 structures (anchors, volume descriptors, file entries, directories) are
// built in RAM, and every file's data sits in the image at its own 1 MB aligned place, so an image block is one
// Range read of one file. The kernel mounts it like any disc image (losetup + udf).

#define SEC 2048u
#define PSTART 512u          // partition start sector (1 MB)
#define MAXNODE 20000
#define MAXREQ 4000
#define CHUNK 0x3ff00000u    // largest extent of a file entry (multiple of SEC, < 1 GB)

typedef struct {
    char *name, *url;          // url: absolute, with the login (files: data, dirs: listing)
    uint64_t size;
    int dir, parent, child, last, next, listed, nsub;
    uint32_t fe, dlbn, dblocks, fidlen, start;  // start: image block (BLK) of the data
} dnode;

typedef struct disc {
    char key[2048];
    dnode *v;
    int n, cap, nfiles, ndirs, nreq, ndata;
    int *files;                // nodes with data (ndata), in image order
    uint8_t *meta;             // image blocks [0, meta_blk)
    uint32_t meta_blk, tail_blk;
    uint8_t avdp[SEC];         // last sector of the image
    uint64_t size, bytes;
} disc_t;

static int hexv(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    c |= 32;
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
static void pdec(const char *s, size_t n, char *o) {  // percent-decode n bytes into o
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        int a, b;
        if (s[i] == '%' && i + 2 < n && (a = hexv(s[i + 1])) >= 0 && (b = hexv(s[i + 2])) >= 0) {
            o[k++] = (char)(a << 4 | b);
            i += 2;
        } else
            o[k++] = s[i];
    }
    o[k] = 0;
}
static char *utf8(char *o, unsigned cp) {
    if (cp < 0x80) *o++ = cp;
    else if (cp < 0x800) { *o++ = 0xc0 | cp >> 6; *o++ = 0x80 | (cp & 63); }
    else { *o++ = 0xe0 | (cp >> 12 & 15); *o++ = 0x80 | (cp >> 6 & 63); *o++ = 0x80 | (cp & 63); }
    return o;
}
static void xdec(char *s) {  // XML entities, in place
    static const char *en[] = {"&amp;", "&lt;", "&gt;", "&quot;", "&apos;"};
    static const char ch[] = "&<>\"'";
    char *o = s;
    for (char *p = s; *p;) {
        if (*p == '&') {
            int k;
            for (k = 0; k < 5; k++)
                if (!strncmp(p, en[k], strlen(en[k]))) break;
            if (k < 5) { *o++ = ch[k]; p += strlen(en[k]); continue; }
            if (p[1] == '#') {
                char *q;
                unsigned long cp = p[2] == 'x' ? strtoul(p + 3, &q, 16) : strtoul(p + 2, &q, 10);
                if (*q == ';' && cp && cp < 0x10000) { o = utf8(o, (unsigned)cp); p = q + 1; continue; }
            }
        }
        *o++ = *p++;
    }
    *o = 0;
}
static size_t url_base(const char *u) {  // length of "http://login@host:port"
    const char *h = strstr(u, "://");
    if (!h) return 0;
    h += 3;
    const char *s = strchr(h, '/');
    return s ? (size_t)(s - u) : strlen(u);
}

typedef struct { char *href, *dpath; int dir; uint64_t size; } davent;

static void dav_free(davent *v, int n) {
    for (int i = 0; i < n; i++) { free(v[i].href); free(v[i].dpath); }
    free(v);
}

// Next XML element at or after p: local name (prefix dropped) in ln, kind 0 open, 1 close, 2 empty. NULL at the end.
static const char *xel(const char *p, char *ln, size_t lnn, int *kind) {
    for (;;) {
        p = strchr(p, '<');
        if (!p) return NULL;
        p++;
        if (*p == '?' || *p == '!') {
            const char *e = strchr(p, '>');
            if (!e) return NULL;
            p = e + 1;
            continue;
        }
        *kind = 0;
        if (*p == '/') { *kind = 1; p++; }
        const char *s = p;
        while (*p && !strchr(" \t\r\n/>", *p)) p++;
        for (const char *q = s; q < p; q++)
            if (*q == ':') s = q + 1;
        size_t l = p - s;
        if (l >= lnn) l = lnn - 1;
        memcpy(ln, s, l);
        ln[l] = 0;
        const char *e = strchr(p, '>');
        if (!e) return NULL;
        if (*kind == 0 && e[-1] == '/') *kind = 2;
        return e + 1;
    }
}

// PROPFIND url (Depth: depth). Entries: href (path, as sent), dpath (decoded path), collection, size.
static int dav_propfind(const char *url, const char *depth, davent **out, int *nout) {
    static const char q[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?><D:propfind xmlns:D=\"DAV:\"><D:prop>"
                            "<D:resourcetype/><D:getcontentlength/></D:prop></D:propfind>";
    char hd[96], *body;
    size_t bl;
    snprintf(hd, sizeof hd, "Depth: %s\r\nContent-Type: application/xml; charset=utf-8\r\n", depth);
    *out = NULL;
    *nout = 0;
    int st = http_req(url, "PROPFIND", hd, q, &body, &bl);
    if (st != 207) { free(body); return st; }
    davent *v = NULL, cur;
    int n = 0, cap = 0, in = 0, kind;
    char ln[64];
    const char *p = body;
    memset(&cur, 0, sizeof cur);
    while ((p = xel(p, ln, sizeof ln, &kind))) {
        if (!strcmp(ln, "response")) {
            if (kind == 0) {
                free(cur.href); free(cur.dpath);
                memset(&cur, 0, sizeof cur);
                in = 1;
            } else if (kind == 1 && in) {
                in = 0;
                if (!cur.href) continue;
                if (n == cap) {
                    davent *nv = realloc(v, (cap = cap ? cap * 2 : 64) * sizeof *v);
                    if (!nv) break;
                    v = nv;
                }
                v[n++] = cur;
                memset(&cur, 0, sizeof cur);
            }
        } else if (!in)
            continue;
        else if (!strcmp(ln, "href") && kind == 0 && !cur.href) {
            const char *e = strchr(p, '<');
            if (!e) break;
            while (p < e && strchr(" \t\r\n", *p)) p++;
            const char *t = e;
            while (t > p && strchr(" \t\r\n", t[-1])) t--;
            char *h = strndup(p, t - p);
            if (!h) break;
            xdec(h);
            if (strstr(h, "://")) {
                size_t b = url_base(h);
                memmove(h, h + b, strlen(h + b) + 1);
            }
            size_t pl = strcspn(h, "?#");
            if (!h[0] || !(cur.dpath = malloc(pl + 1))) { free(h); continue; }
            pdec(h, pl, cur.dpath);
            cur.href = h;
        } else if (!strcmp(ln, "collection") && kind != 1)
            cur.dir = 1;
        else if (!strcmp(ln, "getcontentlength") && kind == 0)
            cur.size = strtoull(p, NULL, 10);
    }
    free(cur.href); free(cur.dpath);
    free(body);
    *out = v;
    *nout = n;
    return st;
}

static void strip_slash(char *s) {
    size_t l = strlen(s);
    while (l > 1 && s[l - 1] == '/') s[--l] = 0;
}
static const char *lastseg(const char *p) {
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}
static int is_disc_dir(const char *name) { return !strcasecmp(name, "BDMV") || !strcasecmp(name, "VIDEO_TS"); }
static int top_ok(const char *name, size_t n) {  // what a disc image holds at its root
    static const char *ok[] = {"BDMV", "CERTIFICATE", "AACS", "VIDEO_TS", "AUDIO_TS"};
    for (int k = 0; k < 5; k++)
        if (strlen(ok[k]) == n && !strncasecmp(name, ok[k], n)) return 1;
    return 0;
}

// The disc folder of url ("http://.../Movie/", ending in '/'): the part before a /BDMV/ or /VIDEO_TS/ component, or
// for a folder URL, the folder (or one of its subfolders) that holds BDMV or VIDEO_TS. 0 if none.
static int disc_root(const char *url, char *root, size_t rl, int probe) {
    size_t b = url_base(url);
    const char *path = url + b;
    size_t pl = strcspn(path, "?#"), cut = 0;
    for (size_t i = 0; i < pl; i++) {
        if (path[i] != '/') continue;
        static const char *nm[] = {"bdmv", "video_ts"};
        for (int k = 0; k < 2; k++) {
            size_t l = strlen(nm[k]);
            if (i + 1 + l <= pl && !strncasecmp(path + i + 1, nm[k], l) && (i + 1 + l == pl || path[i + 1 + l] == '/'))
                cut = i + 1;
        }
    }
    if (cut) {
        if (b + cut >= rl) return 0;
        snprintf(root, rl, "%.*s", (int)(b + cut), url);
        return 1;
    }
    if (!pl || path[pl - 1] != '/' || b + pl >= rl) return 0;
    if (!probe) return 1;
    snprintf(root, rl, "%.*s", (int)(b + pl), url);
    char self[2048];
    pdec(path, pl, self);
    strip_slash(self);
    for (int depth = 0; depth < 2; depth++) {
        davent *v;
        int n, st = dav_propfind(root, "1", &v, &n);
        if (st != 207) {
            char rb[2048];
            LOG("disc: PROPFIND %s -> status %d", redact(root, rb, sizeof rb), st);
            return 0;
        }
        int found = 0, sub = -1, nsub = 0;
        for (int i = 0; i < n; i++) {
            strip_slash(v[i].dpath);
            if (!v[i].dir || !strcmp(v[i].dpath, self)) continue;
            if (is_disc_dir(lastseg(v[i].dpath))) found = 1;
            else if (lastseg(v[i].dpath)[0] != '.') { sub = i; nsub++; }
        }
        if (!found && depth == 0 && nsub == 1 && b + strlen(v[sub].href) + 1 < rl) {
            // a release folder that holds one disc folder
            snprintf(root, rl, "%.*s%s", (int)b, url, v[sub].href);
            if (root[strlen(root) - 1] != '/') strcat(root, "/");
            snprintf(self, sizeof self, "%s", v[sub].dpath);
        }
        dav_free(v, n);
        if (found) return 1;
        if (depth == 0 && nsub != 1) break;
    }
    return 0;
}

static int dn_find(disc_t *d, int par, const char *name, size_t nl) {
    for (int c = d->v[par].child; c >= 0; c = d->v[c].next)
        if (strlen(d->v[c].name) == nl && !memcmp(d->v[c].name, name, nl)) return c;
    return -1;
}
static int dn_add(disc_t *d, int par, const char *name, size_t nl, int dir) {
    if (d->n >= MAXNODE) return -1;
    if (d->n == d->cap) {
        dnode *nv = realloc(d->v, (d->cap = d->cap ? d->cap * 2 : 256) * sizeof *nv);
        if (!nv) return -1;
        d->v = nv;
    }
    dnode *x = &d->v[d->n];
    memset(x, 0, sizeof *x);
    x->name = strndup(name, nl);
    x->dir = dir;
    x->parent = par;
    x->child = x->last = x->next = -1;
    if (par >= 0) {
        dnode *p = &d->v[par];
        if (p->last >= 0) d->v[p->last].next = d->n;
        else p->child = d->n;
        p->last = d->n;
    }
    return d->n++;
}

// Adds one listing entry at its path below the disc folder (rel, decoded, no leading or trailing '/').
static int dn_insert(disc_t *d, const char *rel, const davent *e, const char *url) {
    int cur = 0;
    const char *p = rel;
    for (;;) {
        const char *s = strchr(p, '/');
        size_t nl = s ? (size_t)(s - p) : strlen(p);
        if (nl == 0 || nl > 200 || p[0] == '.') return 0;  // hidden files (._*, .DS_Store)
        if (cur == 0 && !top_ok(p, nl)) return 0;
        int c = dn_find(d, cur, p, nl);
        if (c < 0 && (c = dn_add(d, cur, p, nl, s ? 1 : e->dir)) < 0) return -1;
        if (!s) {
            dnode *x = &d->v[c];
            x->dir = e->dir;
            x->size = e->dir ? 0 : e->size;
            if (!x->url) x->url = strdup(url);
            d->v[cur].listed = 1;  // the listing had its children
            return x->url ? 1 : -1;
        }
        cur = c;
        p = s + 1;
    }
}

// Lists every folder of the disc (Depth: infinity per top folder when the server allows it, else folder by folder).
static int disc_walk(disc_t *d, const char *rootp, size_t baselen) {
    size_t rpl = strlen(rootp);
    for (int i = 0; i < d->n; i++) {
        if (!d->v[i].dir || d->v[i].listed) continue;
        davent *v = NULL;
        int n = 0, st = -1;
        if (d->v[i].parent == 0) {
            d->nreq++;
            st = dav_propfind(d->v[i].url, "infinity", &v, &n);
        }
        if (st != 207) {
            if (++d->nreq > MAXREQ) return -1;
            st = dav_propfind(d->v[i].url, "1", &v, &n);
        }
        if (st != 207) {
            char rb[2048];
            LOG("disc: PROPFIND %s -> status %d", redact(d->v[i].url, rb, sizeof rb), st);
            return -1;
        }
        for (int k = 0; k < n; k++) {
            strip_slash(v[k].dpath);
            const char *dp = v[k].dpath;
            if (strncmp(dp, rootp, rpl) || dp[rpl] != '/' || !dp[rpl + 1]) continue;  // outside, or the folder
            char u[3072];
            if (baselen + strlen(v[k].href) >= sizeof u) continue;
            snprintf(u, sizeof u, "%.*s%s", (int)baselen, d->key, v[k].href);
            if (dn_insert(d, dp + rpl + 1, &v[k], u) < 0) { dav_free(v, n); return -1; }
        }
        dav_free(v, n);
        d->v[i].listed = 1;
    }
    return 0;
}

static void w16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void w32(uint8_t *p, uint32_t v) { w16(p, v & 0xffff); w16(p + 2, v >> 16); }
static void w64(uint8_t *p, uint64_t v) { w32(p, (uint32_t)v); w32(p + 4, (uint32_t)(v >> 32)); }
static uint16_t crc_itu(const uint8_t *p, size_t n) {
    uint16_t c = 0;
    while (n--) {
        c ^= (uint16_t)(*p++ << 8);
        for (int k = 0; k < 8; k++) c = c & 0x8000 ? (uint16_t)(c << 1 ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}
static void utag(uint8_t *d, unsigned id, uint32_t loc, size_t len) {  // descriptor tag, filled in last
    w16(d, id);
    w16(d + 2, 3);
    w16(d + 6, 1);
    w16(d + 8, crc_itu(d + 16, len - 16));
    w16(d + 10, (unsigned)(len - 16));
    w32(d + 12, loc);
    uint8_t s = 0;
    for (int i = 0; i < 16; i++)
        if (i != 4) s += d[i];
    d[4] = s;
}
static const uint8_t suf_udf[8] = {0x01, 0x02};  // UDF revision 2.01
static const uint8_t suf_none[8];
static void ureg(uint8_t *d, const char *id, const uint8_t *suf) {
    memcpy(d + 1, id, strlen(id));
    memcpy(d + 24, suf, 8);
}
static void ucs(uint8_t *d) { memcpy(d + 1, "OSTA Compressed Unicode", 23); }
static void udstr(uint8_t *d, int n, const char *s) {
    int l = (int)strlen(s);
    if (l > n - 2) l = n - 2;
    if (!l) return;
    d[0] = 8;
    memcpy(d + 1, s, l);
    d[n - 1] = (uint8_t)(l + 1);
}
static void uts(uint8_t *d, time_t t) {
    struct tm tm;
    gmtime_r(&t, &tm);
    w16(d, 0x1000);
    w16(d + 2, tm.tm_year + 1900);
    d[4] = tm.tm_mon + 1; d[5] = tm.tm_mday; d[6] = tm.tm_hour; d[7] = tm.tm_min; d[8] = tm.tm_sec;
}
// File identifier (OSTA CS0): 8-bit when every character fits, else 16-bit. Returns its length, 0 if unusable.
static int uname(const char *s, uint8_t *o) {
    unsigned cp[256];
    int n = 0, wide = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        unsigned c = *p++;
        if (c >= 0xc0 && c < 0xe0 && (*p & 0xc0) == 0x80) c = (c & 31) << 6 | (*p++ & 63);
        else if (c >= 0xe0 && c < 0xf0 && (p[0] & 0xc0) == 0x80 && (p[1] & 0xc0) == 0x80) {
            c = (c & 15) << 12 | (p[0] & 63) << 6 | (p[1] & 63);
            p += 2;
        } else if (c >= 0x80) c = '_';
        if (n == 255) return 0;
        if (c > 0xff) wide = 1;
        cp[n++] = c;
    }
    if (!n || (wide ? 1 + 2 * n : 1 + n) > 255) return 0;
    o[0] = wide ? 16 : 8;
    for (int i = 0; i < n; i++) {
        if (wide) { o[1 + 2 * i] = (uint8_t)(cp[i] >> 8); o[2 + 2 * i] = (uint8_t)cp[i]; }
        else o[1 + i] = (uint8_t)cp[i];
    }
    return wide ? 1 + 2 * n : 1 + n;
}
static uint32_t fid_len(int nl) { return (38 + nl + 3) & ~3u; }

// Partition blocks: directory entry, its FIDs, its files' entries, then the subfolders.
static void disc_lay(disc_t *d, int i, uint32_t *next) {
    dnode *x = &d->v[i];
    uint8_t nm[256];
    x->fe = (*next)++;
    x->fidlen = 40;  // the parent entry
    for (int c = x->child; c >= 0; c = d->v[c].next) {
        x->fidlen += fid_len(uname(d->v[c].name, nm));
        if (d->v[c].dir) x->nsub++;
    }
    x->dlbn = *next;
    x->dblocks = (x->fidlen + SEC - 1) / SEC;
    *next += x->dblocks;
    d->ndirs++;
    for (int c = x->child; c >= 0; c = d->v[c].next)
        if (!d->v[c].dir) {
            d->v[c].fe = (*next)++;
            d->nfiles++;
            if (d->v[c].size) d->files[d->ndata++] = c;
        }
    for (int c = x->child; c >= 0; c = d->v[c].next)
        if (d->v[c].dir) disc_lay(d, c, next);
}

static void disc_free(disc_t *d) {
    if (!d) return;
    for (int i = 0; i < d->n; i++) { free(d->v[i].name); free(d->v[i].url); }
    free(d->v);
    free(d->files);
    free(d->meta);
    free(d);
}

static void ufe(uint8_t *p, uint32_t lbn, int dir, unsigned links, uint64_t len, uint64_t uniq, time_t t,
                const uint8_t *ads, uint32_t lad) {
    w16(p + 20, 4);           // ICB tag: strategy 4
    w16(p + 24, 1);           // max entries
    p[27] = dir ? 4 : 5;      // file type; flags 0 = short_ad
    w32(p + 44, 0x14a5);      // r-x for owner, group, other
    w16(p + 48, links);
    w64(p + 56, len);
    w64(p + 64, (len + SEC - 1) / SEC);
    uts(p + 72, t); uts(p + 84, t); uts(p + 96, t);
    w32(p + 108, 1);
    ureg(p + 128, "*dlnaiso", suf_none);
    w64(p + 160, uniq);
    w32(p + 172, lad);
    memcpy(p + 176, ads, lad);
    utag(p, 261, lbn, 176 + lad);
}

static uint32_t ufid(uint8_t *dir, uint32_t pos, uint32_t dlbn, int chars, const uint8_t *nm, int nl, uint32_t icb,
                     uint64_t uniq) {
    uint8_t *p = dir + pos;
    uint32_t len = fid_len(nl);
    w16(p + 16, 1);
    p[18] = (uint8_t)chars;
    p[19] = (uint8_t)nl;
    w32(p + 20, SEC);
    w32(p + 24, icb);
    w32(p + 32, (uint32_t)uniq);
    memcpy(p + 38, nm, nl);
    utag(p, 257, dlbn + pos / SEC, len);
    return pos + len;
}

static void disc_vds(uint8_t *M, uint32_t s, const char *vol, const char *vset, time_t t, uint32_t plen) {
    uint8_t *p = M + (size_t)s * SEC;  // primary volume descriptor
    w32(p + 16, 1);
    udstr(p + 24, 32, vol);
    w16(p + 56, 1); w16(p + 58, 1); w16(p + 60, 2); w16(p + 62, 3);
    w32(p + 64, 1); w32(p + 68, 1);
    udstr(p + 72, 128, vset);
    ucs(p + 200); ucs(p + 264);
    ureg(p + 344, "*dlnaiso", suf_none);
    uts(p + 376, t);
    ureg(p + 388, "*dlnaiso", suf_none);
    utag(p, 1, s, 512);
    p += SEC;  // implementation use volume descriptor (LV info)
    w32(p + 16, 2);
    ureg(p + 20, "*UDF LV Info", suf_udf);
    ucs(p + 52);
    udstr(p + 116, 128, vol);
    ureg(p + 352, "*dlnaiso", suf_none);
    utag(p, 4, s + 1, 512);
    p += SEC;  // partition descriptor
    w32(p + 16, 3);
    w16(p + 20, 1);
    ureg(p + 24, "+NSR03", suf_none);
    w32(p + 184, 1);  // read-only
    w32(p + 188, PSTART);
    w32(p + 192, plen);
    ureg(p + 196, "*dlnaiso", suf_none);
    utag(p, 5, s + 2, 512);
    p += SEC;  // logical volume descriptor
    w32(p + 16, 4);
    ucs(p + 20);
    udstr(p + 84, 128, vol);
    w32(p + 212, SEC);
    ureg(p + 216, "*OSTA UDF Compliant", suf_udf);
    w32(p + 248, SEC);  // file set descriptor at partition block 0
    w32(p + 264, 6);
    w32(p + 268, 1);
    ureg(p + 272, "*dlnaiso", suf_none);
    w32(p + 432, 2 * SEC);
    w32(p + 436, 64);  // integrity sequence
    p[440] = 1; p[441] = 6; w16(p + 442, 1); w16(p + 444, 0);  // type 1 map: volume 1, partition 0
    utag(p, 6, s + 3, 446);
    p += SEC;  // unallocated space descriptor
    w32(p + 16, 5);
    utag(p, 7, s + 4, 24);
    p += SEC;
    utag(p, 8, s + 5, 512);  // terminating descriptor
}

static void disc_avdp(uint8_t *p, uint32_t s) {
    w32(p + 16, 16 * SEC); w32(p + 20, 32);
    w32(p + 24, 16 * SEC); w32(p + 28, 48);
    utag(p, 2, s, 512);
}

static disc_t *disc_build(const char *url) {
    double t0 = now();
    char rb[2048];
    disc_t *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    snprintf(d->key, sizeof d->key, "%s", url);
    char root[2048], rootp[2048];
    if (!disc_root(url, root, sizeof root, 1)) {
        LOG("disc: no BDMV or VIDEO_TS folder at %s", redact(url, rb, sizeof rb));
        goto fail;
    }
    size_t baselen = url_base(root);
    if (baselen != url_base(url) || strncmp(root, url, baselen)) goto fail;
    pdec(root + baselen, strcspn(root + baselen, "?#"), rootp);
    strip_slash(rootp);
    char vol[31];
    const char *vn = lastseg(rootp);
    int vl = 0;
    for (; *vn && vl < 30; vn++)
        if (*vn > 0x20 && *vn < 0x7f) vol[vl++] = *vn;
    vol[vl] = 0;
    if (!vl) strcpy(vol, "DLNAISO");
    if (dn_add(d, -1, vol, strlen(vol), 1) < 0 || !(d->v[0].url = strdup(root))) goto fail;
    if (disc_walk(d, rootp, baselen)) goto fail;
    int has = 0;
    for (int c = d->v[0].child; c >= 0; c = d->v[c].next)
        if (d->v[c].dir && is_disc_dir(d->v[c].name)) has = 1;
    if (!has) {
        LOG("disc: %s has no BDMV or VIDEO_TS", redact(root, rb, sizeof rb));
        goto fail;
    }
    if (!(d->files = malloc(d->n * sizeof *d->files))) goto fail;
    uint32_t next = 2;  // 0 file set descriptor, 1 terminator
    disc_lay(d, 0, &next);
    d->meta_blk = (PSTART + next + (BLK / SEC) - 1) / (BLK / SEC);
    uint32_t b = d->meta_blk;
    for (int k = 0; k < d->ndata; k++) {
        dnode *x = &d->v[d->files[k]];
        x->start = b;
        b += (uint32_t)((x->size + BLK - 1) / BLK);
        d->bytes += x->size;
    }
    d->tail_blk = b;
    d->size = (uint64_t)(b + 1) * BLK;
    uint32_t last = (b + 1) * (BLK / SEC) - 1, plen = last - PSTART;
    if (!(d->meta = calloc(d->meta_blk, BLK))) goto fail;
    uint8_t *M = d->meta;
    time_t t = time(NULL);
#define SP(s) (M + (size_t)(s) * SEC)
#define LBP(l) SP(PSTART + (l))
    static const char *vrs[] = {"BEA01", "NSR03", "TEA01"};
    for (int k = 0; k < 3; k++) {
        memcpy(SP(16 + k) + 1, vrs[k], 5);
        SP(16 + k)[6] = 1;
    }
    char vset[64];
    snprintf(vset, sizeof vset, "%08X%08X%.30s", (unsigned)t, (unsigned)d->bytes, vol);
    disc_vds(M, 32, vol, vset, t, plen);
    disc_vds(M, 48, vol, vset, t, plen);
    uint8_t *p = SP(64);  // logical volume integrity descriptor (closed)
    uts(p + 16, t);
    w32(p + 28, 1);
    w64(p + 40, 16 + (uint64_t)d->n);
    w32(p + 72, 1);
    w32(p + 76, 46);
    w32(p + 84, plen);
    ureg(p + 88, "*dlnaiso", suf_none);
    w32(p + 120, d->nfiles);
    w32(p + 124, d->ndirs);
    w16(p + 128, 0x0201); w16(p + 130, 0x0201); w16(p + 132, 0x0201);
    utag(p, 9, 64, 134);
    utag(SP(65), 8, 65, 512);
    disc_avdp(SP(256), 256);
    disc_avdp(d->avdp, last);
    p = LBP(0);  // file set descriptor
    uts(p + 16, t);
    w16(p + 28, 3); w16(p + 30, 3); w32(p + 32, 1); w32(p + 36, 1);
    ucs(p + 48);
    udstr(p + 112, 128, vol);
    ucs(p + 240);
    udstr(p + 304, 32, vol);
    w32(p + 400, SEC);
    w32(p + 404, d->v[0].fe);
    ureg(p + 416, "*OSTA UDF Compliant", suf_udf);
    utag(p, 256, 0, 512);
    utag(LBP(1), 8, 1, 512);
    for (int i = 0; i < d->n; i++) {
        dnode *x = &d->v[i];
        uint64_t uniq = i ? 16 + (uint64_t)i : 0;
        uint8_t ads[SEC - 176];
        uint32_t lad = 0;
        if (x->dir) {
            uint8_t *dd = LBP(x->dlbn), nm[256];
            uint32_t pos = ufid(dd, 0, x->dlbn, 0x0a, NULL, 0, i ? d->v[x->parent].fe : x->fe, 0);
            for (int c = x->child; c >= 0; c = d->v[c].next) {
                int nl = uname(d->v[c].name, nm);
                pos = ufid(dd, pos, x->dlbn, d->v[c].dir ? 0x02 : 0, nm, nl, d->v[c].fe, 16 + (uint64_t)c);
            }
            w32(ads, x->fidlen);
            w32(ads + 4, x->dlbn);
            ufe(LBP(x->fe), x->fe, 1, 1 + x->nsub, x->fidlen, uniq, t, ads, 8);
            continue;
        }
        uint64_t rem = x->size;
        uint32_t lb = x->start * (BLK / SEC) - PSTART;
        while (rem) {
            if (lad + 8 > sizeof ads) {
                LOG("disc: %s is too large", x->name);
                goto fail;
            }
            uint32_t l = rem > CHUNK ? CHUNK : (uint32_t)rem;
            w32(ads + lad, l);
            w32(ads + lad + 4, lb);
            lad += 8;
            lb += l / SEC;
            rem -= l;
        }
        ufe(LBP(x->fe), x->fe, 0, 1, x->size, uniq, t, ads, lad);
    }
#undef SP
#undef LBP
    LOG("disc %s: %d files, %d folders, %.1f GB, image %llu MB, %d requests, %.0f ms", redact(root, rb, sizeof rb),
        d->nfiles, d->ndirs, d->bytes / 1e9, (unsigned long long)(d->size >> 20), d->nreq, (now() - t0) * 1000);
    return d;
fail:
    disc_free(d);
    return NULL;
}

static int disc_file_at(const disc_t *d, uint64_t b) {  // node whose data covers image block b, or -1
    int lo = 0, hi = d->ndata - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        const dnode *x = &d->v[d->files[m]];
        uint64_t end = x->start + (x->size + BLK - 1) / BLK;
        if (b < x->start) hi = m - 1;
        else if (b >= end) lo = m + 1;
        else return d->files[m];
    }
    return -1;
}

static uint64_t disc_end_blk(const disc_t *d, uint64_t b) {  // last block of the file holding b (b if none)
    int f = disc_file_at(d, b);
    return f < 0 ? b : d->v[f].start + (d->v[f].size + BLK - 1) / BLK - 1;
}

// Image block b (len bytes) into buf: structures from RAM, file data by one Range read. 1 = ok.
static int disc_read(hconn *c, const disc_t *d, uint64_t b, char *buf, uint32_t len, uint32_t *nb) {
    *nb = len;
    if (b < d->meta_blk) {
        memcpy(buf, d->meta + b * BLK, len);
        return 1;
    }
    int f = disc_file_at(d, b);
    if (f < 0) {
        memset(buf, 0, len);
        if (b == d->tail_blk && len >= SEC) memcpy(buf + len - SEC, d->avdp, SEC);
        return 1;
    }
    const dnode *x = &d->v[f];
    uint64_t off = (b - x->start) * BLK;
    uint32_t want = x->size - off < len ? (uint32_t)(x->size - off) : len, got = 0;
    int st = hc_do(c, x->url, 0, off, want, buf, &got, NULL);
    if (!((st == 206 || (st == 200 && off == 0)) && got == want)) {
        char rb[2048];
        LOG("disc fetch %s @%llu failed: status %d got %u/%u", redact(x->url, rb, sizeof rb), (unsigned long long)off,
            st, got, want);
        *nb = got;
        return 0;
    }
    memset(buf + want, 0, len - want);
    return 1;
}

// Built discs, by URL (the same disc pushed again is not listed again).
#define NDISC 4
static disc_t *discs[NDISC];
static double disc_used[NDISC];
static pthread_mutex_t discmu = PTHREAD_MUTEX_INITIALIZER;

static disc_t *disc_get(const char *url) {
    pthread_mutex_lock(&discmu);
    for (int i = 0; i < NDISC; i++)
        if (discs[i] && !strcmp(discs[i]->key, url)) {
            disc_used[i] = now();
            pthread_mutex_unlock(&discmu);
            return discs[i];
        }
    pthread_mutex_unlock(&discmu);
    disc_t *d = disc_build(url);
    if (!d) return NULL;
    pthread_mutex_lock(&discmu);
    int k = 0;
    for (int i = 1; i < NDISC; i++)
        if (!discs[i] ? discs[k] != NULL : discs[k] && disc_used[i] < disc_used[k]) k = i;
    disc_free(discs[k]);  // the oldest: its entry was replaced long ago
    discs[k] = d;
    disc_used[k] = now();
    pthread_mutex_unlock(&discmu);
    return d;
}

/* ---------------- registry (REGDIR/<name>.url) ---------------- */

typedef struct {
    int used, dead;
    uint32_t id;
    char name[128];
    char url[2048];
    uint64_t size;
    time_t mtime;
    disc_t *disc;              // a disc folder published as an image (NULL: the URL is the image)
    uint64_t last_blk[4];      // recent read positions (a UHD disc reads two streams at once)
    int last_i;
} ent_t;
static ent_t ents[MAXENT];
static uint32_t nextid = 1;
static pthread_mutex_t regmu = PTHREAD_MUTEX_INITIALIZER;

static int ent_get(uint32_t id, ent_t *out) {
    int ok = 0;
    pthread_mutex_lock(&regmu);
    for (int i = 0; i < MAXENT; i++)
        if (ents[i].used && !ents[i].dead && ents[i].id == id) { *out = ents[i]; ok = 1; break; }
    pthread_mutex_unlock(&regmu);
    return ok;
}

static int ent_seq(uint32_t id, uint64_t blk) {  // records the block read; returns 1 if it continues a recent read
    int seq = 0;
    pthread_mutex_lock(&regmu);
    for (int i = 0; i < MAXENT; i++)
        if (ents[i].used && ents[i].id == id) {
            ent_t *e = &ents[i];
            int k;
            for (k = 0; k < 4; k++)
                if (blk >= e->last_blk[k] && blk <= e->last_blk[k] + 2) break;
            if (k < 4) seq = 1;
            else { k = e->last_i; e->last_i = (k + 1) & 3; }
            e->last_blk[k] = blk;
            break;
        }
    pthread_mutex_unlock(&regmu);
    return seq;
}

static int read_url_file(const char *name, char *url, size_t ul, time_t *mt) {
    char p[512];
    snprintf(p, sizeof p, "%s/%s.url", regdir, name);
    int fd = open(p, O_RDONLY);
    if (fd < 0) return -1;
    struct stat st;
    fstat(fd, &st);
    ssize_t n = read(fd, url, ul - 1);
    close(fd);
    if (n <= 0) return -1;
    url[n] = 0;
    while (n > 0 && (url[n - 1] == '\n' || url[n - 1] == '\r' || url[n - 1] == ' ')) url[--n] = 0;
    *mt = st.st_mtime;
    return n > 0 ? 0 : -1;
}

static pthread_mutex_t buildmu = PTHREAD_MUTEX_INITIALIZER;

// Name -> live entry (re-reads the .url file; a new URL gets a new id, so old handles go stale).
static int ent_lookup(const char *name, ent_t *out) {
    char url[2048];
    time_t mt;
    int have = read_url_file(name, url, sizeof url, &mt) == 0;
    pthread_mutex_lock(&regmu);
    int cur = -1;
    for (int i = 0; i < MAXENT; i++)
        if (ents[i].used && !ents[i].dead && !strcmp(ents[i].name, name)) cur = i;
    if (!have) {
        if (cur >= 0) ents[cur].dead = 1;
        pthread_mutex_unlock(&regmu);
        return 0;
    }
    if (cur >= 0 && !strcmp(ents[cur].url, url)) {
        *out = ents[cur];
        pthread_mutex_unlock(&regmu);
        return 1;
    }
    pthread_mutex_unlock(&regmu);
    // one publish at a time: listing a disc takes seconds and the NFS client may ask again meanwhile
    pthread_mutex_lock(&buildmu);
    pthread_mutex_lock(&regmu);
    for (int i = 0; i < MAXENT; i++)
        if (ents[i].used && !ents[i].dead && !strcmp(ents[i].name, name) && !strcmp(ents[i].url, url)) {
            *out = ents[i];
            pthread_mutex_unlock(&regmu);
            pthread_mutex_unlock(&buildmu);
            return 1;
        }
    pthread_mutex_unlock(&regmu);
    uint64_t sz = 0;
    disc_t *dk = NULL;
    char root[2048];
    if (disc_root(url, root, sizeof root, 0)) {
        dk = disc_get(url);
        if (dk) sz = dk->size;
    } else if (url_size(url, &sz))
        sz = 0;
    if (!sz) { pthread_mutex_unlock(&buildmu); return 0; }
    pthread_mutex_lock(&regmu);
    for (int i = 0; i < MAXENT; i++)
        if (ents[i].used && !ents[i].dead && !strcmp(ents[i].name, name)) cur = i;
    if (cur >= 0) ents[cur].dead = 1;
    int k = -1;
    for (int i = 0; i < MAXENT && k < 0; i++) if (!ents[i].used) k = i;
    for (int i = 0; i < MAXENT && k < 0; i++) if (ents[i].dead) k = i;
    if (k < 0) k = 0;
    memset(&ents[k], 0, sizeof ents[k]);
    ents[k].used = 1;
    ents[k].id = nextid++;
    snprintf(ents[k].name, sizeof ents[k].name, "%s", name);
    snprintf(ents[k].url, sizeof ents[k].url, "%s", url);
    ents[k].size = sz;
    ents[k].mtime = mt;
    ents[k].disc = dk;
    *out = ents[k];
    pthread_mutex_unlock(&regmu);
    pthread_mutex_unlock(&buildmu);
    LOG("publish %s id=%u size=%llu%s", name, out->id, (unsigned long long)sz, dk ? " (disc folder)" : "");
    return 1;
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
static void slot_setup(int i, const ent_t *e, uint64_t b) {
    slot_t *s = &slots[i];
    s->state = S_LOADING;
    s->id = e->id;
    s->blk = b;
    uint64_t rem = e->size - b * BLK;
    s->len = rem < BLK ? (uint32_t)rem : BLK;
    s->ref = 0;
}

// Pin the cached block b of e (fetching it if needed). Returns slot index or -1.
static int get_block(const ent_t *e, uint64_t b) {
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
static void prefetch(const ent_t *e, uint64_t lastb, int seq) {
    int n = seq ? PF : 1;
    uint64_t lim = e->disc ? disc_end_blk(e->disc, lastb) : UINT64_MAX;  // a disc: within the same file
    pthread_mutex_lock(&mu);
    for (int k = 1; k <= n; k++) {
        uint64_t b = lastb + k;
        if (b * BLK >= e->size || b > lim) break;
        if (find_slot(e->id, b) >= 0) continue;
        int i = claim_slot();
        if (i < 0) break;
        slot_setup(i, e, b);
        q_push(i, 0);
    }
    pthread_mutex_unlock(&mu);
}

static void *worker(void *arg) {
    (void)arg;
    hconn c = {.fd = -1};
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
        ent_t e;
        int ok = 0;
        uint32_t nb = 0;
        double t = now();
        int have = ent_get(id, &e);
        if (have && e.disc)
            ok = disc_read(&c, e.disc, b, d, len, &nb);
        else if (have) {
            int st = hc_do(&c, e.url, 0, b * BLK, len, d, &nb, NULL);
            ok = (st == 206 || (st == 200 && b == 0)) && nb == len;
            if (!ok) LOG("fetch id=%u blk=%llu failed: status %d got %u/%u", id, (unsigned long long)b, st, nb, len);
        }
        VLOG("fetch id=%u blk=%llu %u B %.0f ms", id, (unsigned long long)b, nb, (now() - t) * 1000);
        pthread_mutex_lock(&mu);
        s->state = ok ? S_READY : S_ERR;
        s->stamp = ++lru;
        st_fetched += nb;
        pthread_cond_broadcast(&cv_slot);
    }
    return NULL;
}

static int read_range(const ent_t *e, uint64_t off, uint32_t n, uint8_t *dst) {
    uint64_t lastb = off / BLK;
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
    prefetch(e, lastb, ent_seq(e->id, lastb));
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

static void put_fattr_root(xout *o) {
    struct stat st;
    time_t mt = stat(regdir, &st) == 0 ? st.st_mtime : 0;
    pu32(o, 2); pu32(o, 0555); pu32(o, 2); pu32(o, 0); pu32(o, 0);  // dir, mode, nlink, uid, gid
    pu64(o, 4096); pu64(o, 4096); pu32(o, 0); pu32(o, 0);           // size, used, rdev
    pu64(o, FSID); pu64(o, 1);                                       // fsid, fileid
    for (int k = 0; k < 3; k++) { pu32(o, (uint32_t)mt); pu32(o, 0); }
}
static void put_fattr_ent(xout *o, const ent_t *e) {
    pu32(o, 1); pu32(o, 0444); pu32(o, 1); pu32(o, 0); pu32(o, 0);
    pu64(o, e->size); pu64(o, e->size); pu32(o, 0); pu32(o, 0);
    pu64(o, FSID); pu64(o, 100 + (uint64_t)e->id);
    for (int k = 0; k < 3; k++) { pu32(o, (uint32_t)e->mtime); pu32(o, 0); }
}
static void post_root(xout *o) { pu32(o, 1); put_fattr_root(o); }
static void post_ent(xout *o, const ent_t *e) { pu32(o, 1); put_fattr_ent(o, e); }

// Resolves a handle: 0 = ok (root if *e unset/id 0), else nfsstat3.
static int resolve(xin *in, uint32_t *id, ent_t *e) {
    int st = get_fh(in, id);
    if (st) return st;
    if (*id == 0) return 0;
    return ent_get(*id, e) ? 0 : ERR_STALE;
}

// Returns 0 = done, 3 = PROC_UNAVAIL, 4 = GARBAGE_ARGS
static int nfs3(uint32_t proc, xin *in, xout *o) {
    uint32_t id = 0;
    ent_t e;
    int st;
    switch (proc) {
    case 0:  // NULL
        return 0;
    case 1:  // GETATTR
        st = resolve(in, &id, &e);
        if (in->err) return 4;
        pu32(o, st);
        if (!st) { if (id) put_fattr_ent(o, &e); else put_fattr_root(o); }
        return 0;
    case 3: {  // LOOKUP
        st = get_fh(in, &id);
        const uint8_t *nm;
        uint32_t nl = gopaque(in, &nm);
        if (in->err) return 4;
        if (st) { pu32(o, st); pu32(o, 0); return 0; }
        if (id) { pu32(o, ERR_NOTDIR); pu32(o, 0); return 0; }
        char name[128];
        if (nl == 0 || nl >= sizeof name || memchr(nm, '/', nl)) { pu32(o, ERR_NOENT); post_root(o); return 0; }
        memcpy(name, nm, nl);
        name[nl] = 0;
        if (!strcmp(name, ".") || !strcmp(name, "..")) {
            pu32(o, 0); put_fh(o, 0); post_root(o); post_root(o);
            return 0;
        }
        if (!ent_lookup(name, &e)) { VLOG("lookup %s: none", name); pu32(o, ERR_NOENT); post_root(o); return 0; }
        VLOG("lookup %s -> id %u", name, e.id);
        pu32(o, 0); put_fh(o, e.id); post_ent(o, &e); post_root(o);
        return 0;
    }
    case 4: {  // ACCESS
        st = resolve(in, &id, &e);
        uint32_t acc = gu32(in);
        if (in->err) return 4;
        pu32(o, st);
        if (st) { pu32(o, 0); return 0; }
        if (id) post_ent(o, &e); else post_root(o);
        pu32(o, acc & (id ? 0x01 : 0x23));  // READ (+LOOKUP, EXECUTE on the dir)
        return 0;
    }
    case 5:  // READLINK
        get_fh(in, &id);
        pu32(o, ERR_INVAL); pu32(o, 0);
        return 0;
    case 6: {  // READ
        st = resolve(in, &id, &e);
        uint64_t off = gu64(in);
        uint32_t cnt = gu32(in);
        if (in->err) return 4;
        if (!st && !id) st = ERR_ISDIR;
        if (st) { pu32(o, st); pu32(o, 0); return 0; }
        if (cnt > BLK) cnt = BLK;
        uint32_t n = off >= e.size ? 0 : (e.size - off < cnt ? (uint32_t)(e.size - off) : cnt);
        uint32_t start = o->o;
        pu32(o, 0); post_ent(o, &e); pu32(o, n); pu32(o, off + n >= e.size); pu32(o, n);
        if (o->o + ((n + 3) & ~3u) > o->n) { o->o = start; pu32(o, ERR_IO); pu32(o, 0); return 0; }
        if (n && read_range(&e, off, n, o->p + o->o)) {
            o->o = start;
            pu32(o, ERR_IO); pu32(o, 0);
            return 0;
        }
        memset(o->p + o->o + n, 0, ((n + 3) & ~3u) - n);
        o->o += (n + 3) & ~3u;
        pthread_mutex_lock(&mu);
        st_served += n;
        pthread_mutex_unlock(&mu);
        VLOG("read id=%u off=%llu n=%u", id, (unsigned long long)off, n);
        return 0;
    }
    case 16: {  // READDIR
        st = get_fh(in, &id);
        uint64_t cookie = gu64(in);
        gu64(in);  // cookieverf
        uint32_t cnt = gu32(in);
        if (in->err) return 4;
        if (!st && id) st = ERR_NOTDIR;
        if (st) { pu32(o, st); pu32(o, 0); return 0; }
        pu32(o, 0); post_root(o); pu64(o, 0);
        uint32_t base = o->o, idx = 0;
        int eof = 1;
        DIR *d = opendir(regdir);
        struct dirent *de;
        while (d && (de = readdir(d))) {
            size_t l = strlen(de->d_name);
            if (l < 5 || l >= 128 + 4 || strcmp(de->d_name + l - 4, ".url")) continue;
            if (++idx <= cookie) continue;
            char name[128];
            memcpy(name, de->d_name, l - 4);
            name[l - 4] = 0;
            if (!ent_lookup(name, &e)) continue;
            uint32_t need = 4 + 8 + 4 + ((strlen(name) + 3) & ~3u) + 8;
            if (o->o - base + need + 8 > cnt) { eof = 0; break; }
            pu32(o, 1); pu64(o, 100 + (uint64_t)e.id); popaque(o, name, strlen(name)); pu64(o, idx);
        }
        if (d) closedir(d);
        pu32(o, 0); pu32(o, eof);
        return 0;
    }
    case 17:  // READDIRPLUS: not offered (mounted with nordirplus); client falls back to READDIR
        get_fh(in, &id);
        pu32(o, ERR_NOTSUPP); pu32(o, 0);
        return 0;
    case 18:  // FSSTAT
        st = get_fh(in, &id);
        if (in->err) return 4;
        pu32(o, st);
        if (st) { pu32(o, 0); return 0; }
        post_root(o);
        pu64(o, 1ull << 40); pu64(o, 0); pu64(o, 0); pu64(o, 1000); pu64(o, 0); pu64(o, 0); pu32(o, 0);
        return 0;
    case 19:  // FSINFO
        st = get_fh(in, &id);
        if (in->err) return 4;
        pu32(o, st);
        if (st) { pu32(o, 0); return 0; }
        post_root(o);
        pu32(o, BLK); pu32(o, BLK); pu32(o, 4096);       // rtmax rtpref rtmult
        pu32(o, 65536); pu32(o, 65536); pu32(o, 4096);   // wtmax wtpref wtmult
        pu32(o, 32768); pu64(o, 0x7fffffffffffffffull);  // dtpref maxfilesize
        pu32(o, 1); pu32(o, 0); pu32(o, 0x8);            // time_delta, FSF3_HOMOGENEOUS
        return 0;
    case 20:  // PATHCONF
        st = get_fh(in, &id);
        if (in->err) return 4;
        pu32(o, st);
        if (st) { pu32(o, 0); return 0; }
        post_root(o);
        pu32(o, 1); pu32(o, 255); pu32(o, 1); pu32(o, 1); pu32(o, 0); pu32(o, 1);
        return 0;
    case 2: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 21:
        pu32(o, ERR_ROFS); pu32(o, 0); pu32(o, 0); pu32(o, 0); pu32(o, 0);  // covers wcc_data x2
        return 0;
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

static void *conn_thread(void *arg) {
    int fd = (int)(intptr_t)arg;
    uint8_t *rb = malloc(MAXREC), *ob = malloc(OUTSZ);
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
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
    VLOG("rpc connection %d closed", fd);
    close(fd);
    free(rb);
    free(ob);
    return NULL;
}

static void *stats_thread(void *arg) {
    (void)arg;
    uint64_t ls = 0, lf = 0;
    for (;;) {
        sleep(5);
        pthread_mutex_lock(&mu);
        uint64_t s = st_served, f = st_fetched, h = st_hits, m = st_miss, er = st_err;
        pthread_mutex_unlock(&mu);
        if (s != ls || f != lf)
            LOG("served %.1f MB/s fetched %.1f MB/s | total served %llu MB fetched %llu MB hits %llu miss %llu err %llu",
                (s - ls) / 5e6, (f - lf) / 5e6, (unsigned long long)(s >> 20), (unsigned long long)(f >> 20),
                (unsigned long long)h, (unsigned long long)m, (unsigned long long)er);
        ls = s; lf = f;
    }
    return NULL;
}

static int serve(int port) {
    signal(SIGPIPE, SIG_IGN);
    mkdir(regdir, 0755);
    gen = (uint32_t)time(NULL);
    int ls = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(ls, (struct sockaddr *)&a, sizeof a) || listen(ls, 8)) { perror("bind"); return 1; }
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&at, 256 * 1024);
    pthread_t t;
    for (int i = 0; i < NWORK; i++) pthread_create(&t, &at, worker, NULL);
    pthread_create(&t, &at, stats_thread, NULL);
    LOG("dlnaiso serving %s on 127.0.0.1:%d (cache %d x %u KB, %d workers)", regdir, port, NSLOT, BLK >> 10, NWORK);
    for (;;) {
        int fd = accept(ls, NULL, NULL);
        if (fd < 0) { if (errno == EINTR) continue; perror("accept"); sleep(1); continue; }
        if (pthread_create(&t, &at, conn_thread, (void *)(intptr_t)fd)) close(fd);
    }
}

static int do_mount(int port, const char *dir) {
    char opts[512];
    mkdir(dir, 0755);
    snprintf(opts, sizeof opts,
             "vers=3,proto=tcp,port=%d,mountport=%d,mountproto=tcp,mountvers=3,nolock,soft,timeo=100,retrans=3,"
             "rsize=%u,nordirplus,lookupcache=positive,actimeo=1,addr=127.0.0.1,mountaddr=127.0.0.1",
             port, port, BLK);
    if (mount("127.0.0.1:/", dir, "nfs", MS_RDONLY | MS_NOSUID | MS_NODEV, opts)) {
        fprintf(stderr, "mount %s: %s\n", dir, strerror(errno));
        return 1;
    }
    return 0;
}

static int do_add(const char *name, const char *url) {
    char p[512], t[520];
    mkdir(regdir, 0755);
    snprintf(p, sizeof p, "%s/%s.url", regdir, name);
    snprintf(t, sizeof t, "%s.tmp", p);
    FILE *f = fopen(t, "w");
    if (!f) { perror(t); return 1; }
    fprintf(f, "%s\n", url);
    fclose(f);
    if (rename(t, p)) { perror(p); return 1; }
    return 0;
}

// Debug: list a disc folder like a publish would; with IMG, write a sparse image holding the structures, small
// files and the first and last block of every large file (fetched through the image mapping), for udfinfo/7z/mount.
static int do_disc(const char *url, const char *img) {
    disc_t *d = disc_build(url);
    if (!d) return 1;
    printf("%d files %d folders %llu bytes, image %llu bytes, structures %u MB\n", d->nfiles, d->ndirs,
           (unsigned long long)d->bytes, (unsigned long long)d->size, d->meta_blk);
    for (int k = 0; verbose && k < d->ndata; k++) {
        const dnode *x = &d->v[d->files[k]];
        char path[1024] = "";
        for (int i = d->files[k]; i > 0; i = d->v[i].parent) {
            char t[1024];
            snprintf(t, sizeof t, "/%s%s", d->v[i].name, path);
            snprintf(path, sizeof path, "%s", t);
        }
        printf("%8u %12llu %s\n", x->start, (unsigned long long)x->size, path);
    }
    if (!img) return 0;
    int fd = open(img, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || ftruncate(fd, d->size)) { perror(img); return 1; }
    char *buf = malloc(BLK);
    hconn c = {.fd = -1};
    uint32_t nb;
    int bad = 0;
    for (uint64_t b = 0; b < d->meta_blk; b++) {
        disc_read(&c, d, b, buf, BLK, &nb);
        if (pwrite(fd, buf, BLK, b * BLK) != BLK) bad++;
    }
    disc_read(&c, d, d->tail_blk, buf, BLK, &nb);
    if (pwrite(fd, buf, BLK, (uint64_t)d->tail_blk * BLK) != BLK) bad++;
    for (int k = 0; k < d->ndata; k++) {
        const dnode *x = &d->v[d->files[k]];
        uint64_t first = x->start, lastb = x->start + (x->size + BLK - 1) / BLK - 1;
        for (uint64_t b = first; b <= lastb; b = b == first && lastb > first ? lastb : lastb + 1) {
            if (!disc_read(&c, d, b, buf, BLK, &nb)) { bad++; continue; }
            if (pwrite(fd, buf, BLK, b * BLK) != BLK) bad++;
        }
    }
    close(fd);
    printf("image written, %d errors\n", bad);
    return bad != 0;
}

static int do_get(const char *url, uint64_t off, uint32_t len) {
    hconn c = {.fd = -1};
    char *b = malloc(len);
    uint32_t nb;
    uint64_t tot = 0;
    int st = hc_do(&c, url, 0, off, len, b, &nb, &tot);
    fprintf(stderr, "status %d got %u total %llu\n", st, nb, (unsigned long long)tot);
    fwrite(b, 1, nb, stdout);
    return st == 206 || st == 200 ? 0 : 1;
}

int main(int argc, char **argv) {
    int port = 20490;
    int i = 2;
    if (argc < 2) goto usage;
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-p") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-r") && i + 1 < argc) regdir = argv[++i];
        else if (!strcmp(argv[i], "-v")) verbose = 1;
        else goto usage;
    }
    if (!strcmp(argv[1], "serve")) return serve(port);
    if (!strcmp(argv[1], "mount") && i + 1 == argc) return do_mount(port, argv[i]);
    if (!strcmp(argv[1], "add") && i + 2 == argc) return do_add(argv[i], argv[i + 1]);
    if (!strcmp(argv[1], "disc") && (i + 1 == argc || i + 2 == argc)) return do_disc(argv[i], argv[i + 1]);
    if (!strcmp(argv[1], "get") && i + 3 == argc) return do_get(argv[i], strtoull(argv[i + 1], 0, 0), strtoul(argv[i + 2], 0, 0));
usage:
    fprintf(stderr, "usage: dlnaiso serve [-p PORT] [-r REGDIR] [-v] | mount [-p PORT] DIR | add [-r REGDIR] NAME URL | disc [-v] URL [IMG] | get URL OFF LEN\n");
    return 2;
}
