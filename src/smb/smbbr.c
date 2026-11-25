// smbbr: lets the player's own Network menu use SMB2/SMB3 servers.
// The player runs Samba 3.0 / mount programs for everything (nmblookup, net, mount, umount.cifs); this program is
// bind-mounted over them (smbbr.sh). Each call first runs the real program (kept in REAL); only when that fails, or
// finds less than we do, SMB2/3 steps in. A mount is an NFSv3 mount of a small server in this process (smbbr_core.h).
#include "smbbr_core.h"
#include <ifaddrs.h>
#include <net/if.h>
#include <poll.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <smb2/libsmb2-raw.h>
#include <smb2/libsmb2-dcerpc.h>
#include <smb2/libsmb2-dcerpc-srvsvc.h>
#define DIR_ "/tmp/smbbr"
#define REAL DIR_ "/real/"
#define HOSTS DIR_ "/hosts"     // "IP NAME GROUP" of the last scan
#define LOGF DIR_ "/log"

/* ---------------- small helpers ---------------- */
static int conn_to(struct in_addr ip, int port, int ms) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0); if (fd < 0) return -1;
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(port), .sin_addr = ip};
    if (connect(fd, (void *)&a, sizeof a) && errno != EINPROGRESS) { close(fd); return -1; }
    struct pollfd p = {fd, POLLOUT, 0}; int e = 0; socklen_t l = sizeof e;
    if (poll(&p, 1, ms) != 1 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &l) || e) { close(fd); return -1; }
    fcntl(fd, F_SETFL, 0);
    return fd;
}
// "10.0.0.5", "10.0.0.5:4455" or a name of the last scan
static int host_ip(const char *hp, struct in_addr *ip, int *port) {
    char h[256]; snprintf(h, sizeof h, "%s", hp); char *c = strchr(h, ':'); *port = 445;
    if (c) { *c = 0; *port = atoi(c + 1); }
    if (inet_aton(h, ip)) return 0;
    FILE *f = fopen(HOSTS, "r"); char l[256], i[32], n[64];
    if (f) { while (fgets(l, sizeof l, f)) if (sscanf(l, "%31s %63s", i, n) == 2 && !strcasecmp(n, h) && inet_aton(i, ip)) { fclose(f); return 0; } fclose(f); }
    return -1;
}
static int tcp_ok(const char *hp, int ms) {
    struct in_addr ip; int port; if (host_ip(hp, &ip, &port)) return 0;
    int fd = conn_to(ip, port, ms); if (fd < 0) return 0;
    close(fd); return 1;
}
static void real_exec(const char *name, char **argv) {
    char p[128]; snprintf(p, sizeof p, REAL "%s", name); execv(p, argv);
    fprintf(stderr, "%s: cannot run %s\n", name, p); _exit(127);
}
// run the real program; its stdout (and stderr with err=1) goes to buf. Returns its exit code, -1 when killed for time.
static int real_run(const char *name, char **argv, char *buf, size_t sz, int err, int secs) {
    int pp[2]; if (pipe(pp)) return -1;
    pid_t pid = fork();
    if (!pid) { dup2(pp[1], 1); if (err) dup2(pp[1], 2); close(pp[0]); close(pp[1]); real_exec(name, argv); }
    close(pp[1]); size_t n = 0; double t = now(); int st = 0, killed = 0;
    for (;;) {
        int left = (int)((secs - (now() - t)) * 1000);
        struct pollfd p = {pp[0], POLLIN, 0};
        int r = left > 0 ? poll(&p, 1, left) : 0;
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) { kill(pid, SIGKILL); killed = 1; break; }
        char tmp[4096]; ssize_t k = read(pp[0], tmp, sizeof tmp); if (k <= 0) break;
        for (ssize_t j = 0; j < k && n + 1 < sz; j++) buf[n++] = tmp[j];
    }
    buf[n] = 0; close(pp[0]); waitpid(pid, &st, 0);
    return killed ? -1 : WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static void to_log(void) { int fd = open(LOGF, O_WRONLY | O_CREAT | O_APPEND, 0600); if (fd >= 0) { struct stat st; if (!fstat(fd, &st) && st.st_size > 200000) ftruncate(fd, 0); dup2(fd, 2); if (fd > 2) close(fd); } }
#define DLOG(...) do { int s_ = dup(2); to_log(); LOG(__VA_ARGS__); dup2(s_, 2); close(s_); } while (0)

/* ---------------- who is there ---------------- */
static int rd_pkt(int fd, uint8_t *b, size_t sz) {       // one NetBIOS-framed message
    struct timeval tv = {2, 0}; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv); setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    uint8_t h[4]; if (readall(fd, h, 4)) return -1;
    uint32_t n = h[1] << 16 | h[2] << 8 | h[3]; if (h[0] || n > sz || readall(fd, b, n)) return -1;
    return n;
}
static void smb2_hdr(uint8_t *h, int cmd, int mid) {
    memset(h, 0, 64); memcpy(h, "\xfeSMB", 4); h[4] = 64; h[6] = 1; h[12] = cmd; h[14] = 31; h[24] = mid; h[32] = 0xff; h[33] = 0xfe;
}
static int snd_pkt(int fd, const uint8_t *b, int n) {
    uint8_t h[4] = {0, n >> 16, n >> 8, n}; return writeall(fd, h, 4) || writeall(fd, b, n) ? -1 : 0;
}
static void av_name(const uint8_t *p, int n, char *o, size_t sz) {
    size_t k = 0; for (int i = 0; i + 1 < n && k + 1 < sz && k < 15; i += 2) { int c = p[i]; if (p[i + 1] || c <= ' ' || c >= 127 || strchr("'\"`$\\/%;&|<>()*", c)) c = '-'; o[k++] = c; } o[k] = 0;
}
// the server's computer and domain name, from the NTLM challenge (no account needed). 0 = an SMB2/3 server.
static int smb2_name(int fd, char *name, char *group, size_t sz) {
    static const uint8_t spn[] = {0x60,0x48,0x06,0x06,0x2b,0x06,0x01,0x05,0x05,0x02,0xa0,0x3e,0x30,0x3c,0xa0,0x0e,0x30,0x0c,0x06,0x0a,0x2b,0x06,0x01,0x04,0x01,0x82,0x37,0x02,0x02,0x0a,0xa2,0x2a,0x04,0x28,
        'N','T','L','M','S','S','P',0,1,0,0,0,0x05,0x02,0x88,0xa0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x0f};
    uint8_t b[4096]; int n; name[0] = group[0] = 0;
    smb2_hdr(b, 0, 0); memset(b + 64, 0, 36 + 6); b[64] = 36; b[66] = 3; b[68] = 1; for (int i = 0; i < 16; i++) b[76 + i] = rand();
    memcpy(b + 100, "\x02\x02\x10\x02\x00\x03", 6);
    if (snd_pkt(fd, b, 106) || (n = rd_pkt(fd, b, sizeof b)) < 128 || memcmp(b, "\xfeSMB", 4) || b[8] | b[9] | b[10] | b[11]) return -1;
    smb2_hdr(b, 1, 1); memset(b + 64, 0, 24); b[64] = 25; b[67] = 1; b[76] = 88; b[78] = sizeof spn; memcpy(b + 88, spn, sizeof spn);
    if (snd_pkt(fd, b, 88 + sizeof spn) || (n = rd_pkt(fd, b, sizeof b)) < 72) return 0;
    for (int i = 64; i + 56 <= n; i++) if (!memcmp(b + i, "NTLMSSP\0\2", 9)) {
        const uint8_t *m = b + i; uint32_t l = m[40] | m[41] << 8, o = m[44] | m[45] << 8 | m[46] << 16 | (uint32_t)m[47] << 24;
        if (o > (uint32_t)(n - i) || l > n - i - o) break;
        for (const uint8_t *p = m + o, *e = p + l; p + 4 <= e;) {
            int t = p[0] | p[1] << 8, k = p[2] | p[3] << 8; if (!t || p + 4 + k > e) break;
            if (t == 1) av_name(p + 4, k, name, sz); else if (t == 2) av_name(p + 4, k, group, sz);
            p += 4 + k;
        }
        break;
    }
    return 0;
}
typedef struct { struct in_addr ip; char name[64], group[64]; int nb; } found_t;   // nb: answers NetBIOS itself
static found_t fnd[64]; static int nfnd;
static pthread_mutex_t fmu = PTHREAD_MUTEX_INITIALIZER;
static void *ident(void *a) {
    struct in_addr ip = {.s_addr = (uint32_t)(uintptr_t)a}; found_t f = {.ip = ip};
    int fd = conn_to(ip, 445, 1000); if (fd < 0) return NULL;
    if (smb2_name(fd, f.name, f.group, sizeof f.name) == 0) {
        if (!f.name[0]) { char t[32]; inet_ntop(AF_INET, &ip, t, sizeof t); for (char *c = t; *c; c++) if (*c == '.') *c = '-'; snprintf(f.name, sizeof f.name, "SMB-%s", t); }
        if (!f.group[0] || !strcasecmp(f.group, f.name)) strcpy(f.group, "WORKGROUP");
        for (char *c = f.name; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
        pthread_mutex_lock(&fmu); if (nfnd < 64) fnd[nfnd++] = f; pthread_mutex_unlock(&fmu);
    }
    close(fd); return NULL;
}
static void scan(void) {                 // every address of our (at most /24) networks, port 445
    struct ifaddrs *ia, *i; if (getifaddrs(&ia)) return;
    static struct pollfd p[254]; static uint32_t ad[254]; struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); srand(ts.tv_nsec);
    pthread_t th[64]; int nt = 0;
    for (i = ia; i; i = i->ifa_next) {
        if (!i->ifa_addr || !i->ifa_netmask || i->ifa_addr->sa_family != AF_INET || i->ifa_flags & (IFF_LOOPBACK | IFF_POINTOPOINT) || !(i->ifa_flags & IFF_UP)) continue;
        uint32_t me = ntohl(((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr), mk = ntohl(((struct sockaddr_in *)i->ifa_netmask)->sin_addr.s_addr);
        if (mk < 0xffffff00u) mk = 0xffffff00u;
        int n = 0;
        for (uint32_t h = (me & mk) + 1; h < (me | ~mk) && n < 254; h++) {
            if (h == me) continue;
            int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0); if (fd < 0) break;
            struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(445), .sin_addr.s_addr = htonl(h)};
            if (connect(fd, (void *)&a, sizeof a) && errno != EINPROGRESS) { close(fd); continue; }
            p[n].fd = fd; p[n].events = POLLOUT; ad[n++] = a.sin_addr.s_addr;
        }
        double t = now(); int left = n;
        while (left > 0 && now() - t < 0.9) {
            int r = poll(p, n, 100); if (r <= 0) continue;
            for (int k = 0; k < n; k++) if (p[k].fd >= 0 && p[k].revents) {
                int e = 0; socklen_t l = sizeof e; getsockopt(p[k].fd, SOL_SOCKET, SO_ERROR, &e, &l);
                close(p[k].fd); p[k].fd = -1; left--;
                if (!e && nt < 64 && !pthread_create(&th[nt], NULL, ident, (void *)(uintptr_t)ad[k])) nt++;
            }
        }
        for (int k = 0; k < n; k++) if (p[k].fd >= 0) close(p[k].fd);
    }
    for (int k = 0; k < nt; k++) pthread_join(th[k], NULL);
    freeifaddrs(ia);
}
static void hosts_save(void) {
    char t[64]; snprintf(t, sizeof t, HOSTS ".%d", getpid()); FILE *f = fopen(t, "w"); if (!f) return;
    for (int k = 0; k < nfnd; k++) { char s[32]; inet_ntop(AF_INET, &fnd[k].ip, s, sizeof s); fprintf(f, "%s %s %s %d\n", s, fnd[k].name, fnd[k].group, fnd[k].nb); }
    fclose(f); rename(t, HOSTS);
}
static void *scan_thread(void *a) { (void)a; scan(); return NULL; }
static int hosts_load(void) {
    FILE *f = fopen(HOSTS, "r"); char l[256], i[32]; nfnd = 0; if (!f) return 0;
    while (nfnd < 64 && fgets(l, sizeof l, f)) if (sscanf(l, "%31s %63s %63s %d", i, fnd[nfnd].name, fnd[nfnd].group, &fnd[nfnd].nb) == 4 && inet_aton(i, &fnd[nfnd].ip)) nfnd++;
    fclose(f); return nfnd;
}
static int has_line(const char *buf, const char *ip) {   // a line starting with "IP "
    size_t l = strlen(ip);
    for (const char *p = buf; p; p = strchr(p, '\n'), p = p ? p + 1 : NULL) if (!strncmp(p, ip, l) && p[l] == ' ') return 1;
    return 0;
}
static int do_nmblookup(int argc, char **argv) {
    static char buf[65536], blk[16384]; char s[32];
    if (argc >= 3 && !strcmp(argv[1], "-A")) {
        // one call per address: an address without NetBIOS gets its answer from the scan
        hosts_load();
        for (int a = 2; a < argc; a++) {
            int dup = 0; for (int b = 2; b < a; b++) if (!strcmp(argv[a], argv[b])) dup = 1;
            if (dup) continue;
            found_t *f = NULL; for (int k = 0; k < nfnd; k++) { inet_ntop(AF_INET, &fnd[k].ip, s, sizeof s); if (!strcmp(s, argv[a])) f = &fnd[k]; }
            if (!f || f->nb) { char *v[] = {argv[0], "-A", argv[a], NULL}; real_run("nmblookup", v, blk, sizeof blk, 0, 10); }
            if (f && (!f->nb || !strstr(blk, "<00>"))) printf("Looking up status of %s\n\t%-15s <00> -         B <ACTIVE> \n\t%-15s <00> - <GROUP> B <ACTIVE> \n\t%-15s <1e> - <GROUP> B <ACTIVE> \n\n", argv[a], f->name, f->group, f->group);
            else fputs(blk, stdout);
        }
        return 0;
    }
    if (argc != 2 || argv[1][0] == '-') real_exec("nmblookup", argv);
    int all = !strcmp(argv[1], "*"), rc; pthread_t t; int th = 0;
    struct stat st; if (all && !(stat(HOSTS, &st) == 0 && time(NULL) - st.st_mtime >= 0 && time(NULL) - st.st_mtime < 10)) th = !pthread_create(&t, NULL, scan_thread, NULL);
    rc = real_run("nmblookup", argv, buf, sizeof buf, 0, 15);
    if (th) pthread_join(t, NULL); else hosts_load();
    fputs(buf, stdout);
    if (th) { for (int k = 0; k < nfnd; k++) fnd[k].nb = has_line(buf, inet_ntop(AF_INET, &fnd[k].ip, s, sizeof s)); hosts_save(); }
    for (int k = 0; k < nfnd; k++) {
        if (!all && strcasecmp(argv[1], fnd[k].name) && strcasecmp(argv[1], fnd[k].group)) continue;
        // a name lookup of a server NetBIOS knows stays as it is; the group and "*" get what is missing
        if (!all && !strcasecmp(argv[1], fnd[k].name) && strstr(buf, "<00>")) continue;
        inet_ntop(AF_INET, &fnd[k].ip, s, sizeof s); if (has_line(buf, s)) continue;
        printf("%s %s<00>\n", s, argv[1]); rc = 0;
    }
    return rc < 0 ? 1 : rc;
}

/* ---------------- login: the share list ---------------- */
static int se_done, se_n;
static void se_cb(struct smb2_context *c, int status, void *data, void *priv) {
    struct srvsvc_NetrShareEnum_rep *rep = data; se_done = 1; (void)priv;
    if (status || !rep) { se_n = -1; return; }
    printf("\nEnumerating shared resources (exports) on remote server:\n\n\nShare name   Type     Description\n----------   ----     -----------\n");
    for (uint32_t i = 0; i < rep->ses.ShareInfo.Level1.EntriesRead; i++) {
        struct srvsvc_SHARE_INFO_1 *s = &rep->ses.ShareInfo.Level1.Buffer->share_info_1[i];
        if ((s->type & 3) == SHARE_TYPE_DISKTREE && s->netname.utf8 && *s->netname.utf8 && s->netname.utf8[strlen(s->netname.utf8) - 1] != '$') { printf("%-12s Disk     \n", s->netname.utf8); se_n++; }
    }
    smb2_free_data(c, rep);
}
static const char *nt_name(const char *e) {
    static const char *const k[] = {"LOGON_FAILURE", "ACCESS_DENIED", "ACCOUNT_DISABLED", "ACCOUNT_RESTRICTION", "ACCOUNT_LOCKED_OUT", "PASSWORD_EXPIRED", "WRONG_PASSWORD", "NO_SUCH_USER", "LOGON_TYPE_NOT_GRANTED", "BAD_NETWORK_NAME"};
    for (unsigned i = 0; i < sizeof k / sizeof *k; i++) if (strstr(e, k[i])) return k[i];
    return NULL;
}
static int share_list(const char *srv) {     // 0 = listed, 1 = refused (lasterr), -1 = no SMB2 server
    snprintf(host, sizeof host, "%s", srv); strcpy(share, "IPC$");
    if (sc_open(&meta)) return nt_name(lasterr) ? 1 : -1;
    if (smb2_share_enum_async(meta.c, SHARE_INFO_1, se_cb, NULL)) return -1;
    double t = now();
    while (!se_done && now() - t < 10) {
        struct pollfd p = {smb2_get_fd(meta.c), smb2_which_events(meta.c), 0};
        if (poll(&p, 1, 500) < 0 || (p.revents && smb2_service(meta.c, p.revents) < 0)) break;
    }
    if (se_done && se_n >= 0) return 0;
    snprintf(lasterr, sizeof lasterr, "%s", smb2_get_error(meta.c)); if (!nt_name(lasterr)) strcpy(lasterr, "ACCESS_DENIED");
    return 1;
}
static void set_user(const char *u) {
    const char *b = strchr(u, '/'); if (!b) b = strchr(u, '\\');
    if (b) { snprintf(domain, sizeof domain, "%.*s", (int)(b - u), u); u = b + 1; }
    const char *p = strchr(u, '%'); snprintf(user, sizeof user, "%.*s", p ? (int)(p - u) : (int)strlen(u), u);
    snprintf(password, sizeof password, "%s", p ? p + 1 : "");
    if (!user[0]) strcpy(user, "guest");
}
static int do_net(int argc, char **argv) {
    const char *srv = NULL, *u = NULL;
    if (argc < 6 || strcmp(argv[1], "rpc") || strcmp(argv[2], "share") || strcmp(argv[3], "list")) real_exec("net", argv);
    for (int i = 4; i + 1 < argc; i++) { if (!strcmp(argv[i], "-S")) srv = argv[i + 1]; if (!strcmp(argv[i], "-U")) u = argv[i + 1]; }
    if (!srv || !u) real_exec("net", argv);
    static char buf[65536]; int rc = real_run("net", argv, buf, sizeof buf, 1, 20);
    if (rc == 0 && strstr(buf, " Disk ")) { fputs(buf, stdout); return 0; }
    set_user(u);
    int r = share_list(srv);
    DLOG("login %s: SMB1 %s, SMB2/3 %s%s", srv, rc == 0 ? "no shares" : "failed", r == 0 ? "ok" : r > 0 ? "refused " : "no ", r ? lasterr : "");
    if (r == 0) { fflush(stdout); _exit(0); }
    if (r > 0 && !(rc == 0)) { printf("Could not connect to server %s\nConnection failed: NT_STATUS_%s\n", srv, nt_name(lasterr)); return 255; }
    fputs(buf, stdout); return rc < 0 ? 255 : rc;
}

/* ---------------- mount / unmount ---------------- */
static int mounted(int port, const char *dir) {         // our NFS mount with that port / on that directory?
    FILE *f = fopen("/proc/mounts", "r"); char l[2048], d[512], ty[32], k[32]; int r = 0; if (!f) return 1;
    snprintf(k, sizeof k, ",port=%d,", port);
    while (!r && fgets(l, sizeof l, f)) {
        if (sscanf(l, "%*s %511s %31s", d, ty) != 2 || strcmp(ty, "nfs") || !strstr(l, "mountaddr=127.0.0.1")) continue;
        if (dir ? !strcmp(d, dir) && strstr(l, ",port=2") && !strstr(l, ",port=20490,") : strstr(l, k) != NULL) r = 1;
    }
    fclose(f); return r;
}
static void server(int ls, int port) {       // in the child; never returns
    signal(SIGPIPE, SIG_IGN); gen = (uint32_t)time(NULL) ^ (getpid() << 16);
    pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED); pthread_attr_setstacksize(&at, 256 * 1024);
    pthread_t t; for (int i = 0; i < NWORK; i++) pthread_create(&t, &at, worker, NULL);
    double idle = 0, start = now(), chk = 0; int seen = 0, freed = 0;
    for (;;) {
        struct pollfd p = {ls, POLLIN, 0};
        if (poll(&p, 1, 5000) == 1) { int fd = accept(ls, NULL, NULL); if (fd >= 0 && pthread_create(&t, &at, conn_thread, (void *)(intptr_t)fd)) close(fd); }
        if (now() - chk < 5) continue;
        chk = now();
        if (mounted(port, NULL)) { seen = 1; idle = 0; freed = 0; continue; }
        if (!seen && now() - start < 60) continue;
        if (nconn) { idle = 0; continue; }
        if (!idle) idle = now();
        // unmounted and nobody connected. An unmount can be lazy with a paused film behind it (its idle connection
        // closes after 5 minutes): give the memory back soon, stay reachable for a while.
        if (!freed && now() - idle > 20) {
            pthread_mutex_lock(&mu); for (int i = 0; i < NSLOT; i++) if (slots[i].state != S_LOADING && !slots[i].ref) { free(slots[i].data); slots[i].data = NULL; slots[i].state = S_EMPTY; } pthread_mutex_unlock(&mu); freed = 1;
        }
        if (now() - idle > 900) { LOG("//%s/%s: unmounted, end", host, share); _exit(0); }
    }
}
static int bridge(const char *h, const char *sh, const char *dir, int argc, char **argv) {  // 0 = mounted, 1 = refused, -1 = no SMB2
    snprintf(host, sizeof host, "%s", h); snprintf(share, sizeof share, "%s", sh);
    int pp[2]; if (pipe(pp)) return -1;
    fflush(NULL);
    pid_t pid = fork(); if (pid < 0) return -1;
    if (!pid) {
        // no "mount" in our name: the player kills programs by name when they take long
        prctl(PR_SET_NAME, "smbbridge"); setsid(); close(pp[0]);
        for (int i = 0; i < argc; i++) memset(argv[i], i ? 0 : '-', strlen(argv[i]));      // the password was in there
        int nul = open("/dev/null", O_RDWR); dup2(nul, 0); dup2(nul, 1); to_log();
        for (int fd = 3; fd < 256; fd++) if (fd != pp[1]) close(fd);
        chdir("/");
        struct { int port; char err[200]; } m = {0};
        N = calloc(MAXNODES, sizeof *N); hash = calloc(hsz, sizeof *hash);
        if (!N || !hash) _exit(1);
        N[0] = calloc(1, sizeof(node)); N[0]->name = strdup(""); N[0]->path = strdup(""); N[0]->dir = 1; N[0]->mtime = time(NULL); nn = 1;
        int ls = -1;
        if (sc_open(&meta)) snprintf(m.err, sizeof m.err, "%s", lasterr);
        else {
            ls = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); int one = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
            for (int p = 20491; p < 20591 && !m.port; p++) { struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(p), .sin_addr.s_addr = htonl(INADDR_LOOPBACK)}; if (!mounted(p, NULL) && !bind(ls, (void *)&a, sizeof a) && !listen(ls, 8)) m.port = p; }
            LOG("//%s/%s: SMB %x, hardware AES %s, port %d", host, share, smb2_get_dialect(meta.c), hw_ok() ? "yes" : "no", m.port);
        }
        write(pp[1], &m, sizeof m); close(pp[1]);
        if (!m.port) _exit(1);
        server(ls, m.port);
    }
    close(pp[1]);
    struct { int port; char err[200]; } m = {0};
    struct pollfd p = {pp[0], POLLIN, 0}; int got = poll(&p, 1, 20000) == 1 && read(pp[0], &m, sizeof m) == sizeof m; close(pp[0]);
    if (!got || !m.port) { if (!got) kill(pid, SIGKILL); snprintf(lasterr, sizeof lasterr, "%s", m.err); return nt_name(m.err) ? 1 : -1; }
    if (do_mount(m.port, dir)) { kill(pid, SIGKILL); return -1; }
    return 0;
}
static int do_mountcmd(int argc, char **argv) {
    const char *src = NULL, *dir = NULL, *o = NULL; int cifs = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-t") && i + 1 < argc) cifs = !strcmp(argv[++i], "cifs");
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) o = argv[++i];
        else if (argv[i][0] != '-') { if (!src) src = argv[i]; else if (!dir) dir = argv[i]; }
    }
    // only the player's own share mounts; anything else is none of our business
    if (!cifs || !src || !dir || !o || strncmp(dir, "/mnt/cifs", 9) || !strstr(o, ",noserverino")) real_exec("mount", argv);
    char h[256], sh[256]; if (sscanf(src, "//%255[^/]/%255[^/]", h, sh) != 2) real_exec("mount", argv);
    static char buf[8192]; int rc = real_run("mount", argv, buf, sizeof buf, 1, 12);
    if (rc == 0) { fputs(buf, stderr); return 0; }
    // ro,iocharset=utf8,user=U,password=P[,dom=D],noserverino,rsize=49152 (a comma in the password is fine here)
    char op[1024]; snprintf(op, sizeof op, "%s", o); char *e = strstr(op, ",noserverino"), *d, *pw, *us; if (e) *e = 0;
    us = strstr(op, ",user="); pw = us ? strstr(us, ",password=") : NULL; d = NULL;
    if (pw) for (char *x = pw; (x = strstr(x, ",dom=")); x++) d = x;
    if (d) { *d = 0; snprintf(domain, sizeof domain, "%s", d + 5); }
    if (pw) { *pw = 0; snprintf(password, sizeof password, "%s", pw + 10); }
    if (us && us[6]) snprintf(user, sizeof user, "%s", us + 6);
    if (!strcmp(user, "guest") && !strcmp(password, "guest")) password[0] = 0;
    for (char *l = buf + strlen(buf); l > buf && l[-1] == '\n';) *--l = 0;
    int r = bridge(h, sh, dir, argc, argv);
    DLOG("mount //%s/%s on %s: SMB1 rc %d (%s), SMB2/3 %s%s", h, sh, dir, rc, buf, r == 0 ? "ok" : "failed: ", r ? lasterr : "");
    if (r == 0) return 0;
    fprintf(stderr, "%s%s", buf, *buf ? "\n" : "");
    if (r > 0 && strcmp(nt_name(lasterr), "BAD_NETWORK_NAME")) fprintf(stderr, "mount error: Permission denied\n");
    return rc < 0 ? 32 : rc;
}
static int do_umountcifs(int argc, char **argv) {
    const char *dir = NULL; int lazy = 0;
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "-l")) lazy = 1; else if (argv[i][0] != '-') dir = argv[i];
    if (!dir || !mounted(0, dir)) real_exec("umount.cifs", argv);
    int r = umount2(dir, lazy ? MNT_DETACH : 0);
    DLOG("unmount%s %s: %s", lazy ? " -l" : "", dir, r ? strerror(errno) : "ok");
    return r ? 1 : 0;
}
static int do_mount(int port, const char *dir) {
    char opts[512];
    snprintf(opts, sizeof opts, "vers=3,proto=tcp,port=%d,mountport=%d,mountproto=tcp,mountvers=3,nolock,soft,timeo=300,retrans=2,rsize=%u,nordirplus,lookupcache=positive,actimeo=3,addr=127.0.0.1,mountaddr=127.0.0.1", port, port, BLK);
    return mount("127.0.0.1:/", dir, "nfs", MS_RDONLY | MS_NOSUID | MS_NODEV, opts) ? -1 : 0;
}
int main(int argc, char **argv) {
    const char *n = strrchr(argv[0], '/'); n = n ? n + 1 : argv[0];
    if (!strcmp(n, "mount")) return do_mountcmd(argc, argv);
    if (!strcmp(n, "umount.cifs")) return do_umountcifs(argc, argv);
    if (!strcmp(n, "net")) return do_net(argc, argv);
    if (!strcmp(n, "nmblookup")) return do_nmblookup(argc, argv);
    if (argc == 2 && !strcmp(argv[1], "hello")) return puts("smbbr") < 0;
    if (argc == 2 && !strcmp(argv[1], "scan")) { scan(); for (int i = 0; i < nfnd; i++) printf("%s %s %s\n", inet_ntoa(fnd[i].ip), fnd[i].name, fnd[i].group); return 0; }
    return 2;
}
