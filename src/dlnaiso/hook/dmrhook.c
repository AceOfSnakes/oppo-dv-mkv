// DLNA renderer (DMR) start hook for bdpprog 49b649bd (RU JB layout).
// A pushed URL that is a disc image (MIME video/iso & co, or URL path *.iso / *.blurayiso / *.dvdiso) is published
// through dlnaiso (/tmp/dlnaiso.sh NAME URL -> /mnt/dlnaiso/NAME, an NFS view of the HTTP URL) and handed to the
// jailbreak's ISO player (bd_play_ISO_and_file: losetup + udf mount + BD playback). So is a URL inside a Blu-ray or
// DVD folder (path part /BDMV/ or /VIDEO_TS/, e.g. a WebDAV link to .../BDMV/STREAM/00800.m2ts): dlnaiso lists the
// folder and serves it as a disc image. Anything else, or a folder dlnaiso can't list: stock playback.
typedef int (*fn_ipcd_exec)(const char *cmd, int flags);
typedef int (*fn_printf)(const char *fmt, ...);
typedef int (*fn_access)(const char *path, int mode);
typedef int (*fn_play_iso)(const char *path, int a, int b, int c);
#define IPCD_EXEC ((fn_ipcd_exec)0x00031d60)  /* PLT ipcd_exec (runs a shell command, waits) */
#define PRINTF ((fn_printf)0x000316e0)        /* PLT printf (console) */
#define ACCESS ((fn_access)0x000306f8)        /* PLT access */
#define PLAY_ISO ((fn_play_iso)0x01c02d78)    /* JB bd_play_ISO_and_file (ARM) */

// On-screen DLNA browser: its item classifier FUN_00c04198 strstr()s the item URL for video extensions and drops
// items it can't type. dmrhook.sh repoints two of its literal-pool words to these strings so disc images are
// listed: 0xc04628 ".mpe" -> ".iso" (type MPE), 0xc04630 ".xvid" -> "blurayiso" (Emby's Blu-ray ISO container,
// type XVID). Playing such an item goes through the same start function as a push, so dmr_try() takes it over.
__attribute__((used, section(".rodata.ext"))) const char ext_iso[8] = ".iso";
__attribute__((used, section(".rodata.ext"))) const char ext_bdiso[12] = "blurayiso";

static char cmd[2304];
static char path[64];
static unsigned seq;

static int lc(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static int slen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

// a[0..n) equals the lower-case string b, ignoring case
static int ieq(const char *a, int n, const char *b) {
    for (int i = 0; i < n; i++)
        if (!b[i] || lc(a[i]) != b[i]) return 0;
    return b[n] == 0;
}

// protocolInfo "http-get:*:MIME:..."
static int iso_mime(const char *pi) {
    if (!pi) return 0;
    int f = 0;
    while (*pi && f < 2) {
        if (*pi == ':') f++;
        pi++;
    }
    if (f < 2) return 0;
    int n = 0;
    while (pi[n] && pi[n] != ':' && pi[n] != ';') n++;
    return ieq(pi, n, "video/iso") || ieq(pi, n, "video/x-iso") || ieq(pi, n, "application/x-iso9660-image") ||
           ieq(pi, n, "application/x-iso-image") || ieq(pi, n, "application/x-cd-image");
}

// extension of the last path segment (query and fragment ignored)
static int iso_url(const char *u) {
    int end = 0;
    while (u[end] && u[end] != '?' && u[end] != '#') end++;
    int dot = -1;
    for (int i = end - 1; i >= 0 && u[i] != '/'; i--)
        if (u[i] == '.') { dot = i; break; }
    if (dot < 0) return 0;
    const char *e = u + dot + 1;
    int n = end - dot - 1;
    return ieq(e, n, "iso") || ieq(e, n, "blurayiso") || ieq(e, n, "dvdiso");
}

// a/n starts with the lower-case path part b ("bdmv"), followed by '/' or the end
static int ipart(const char *a, int n, const char *b) {
    int i;
    for (i = 0; b[i]; i++)
        if (i >= n || lc(a[i]) != b[i]) return 0;
    return i == n || a[i] == '/';
}

// a file in a Blu-ray or DVD folder, or the BDMV / VIDEO_TS folder itself (query and fragment ignored)
static int disc_path(const char *u) {
    int end = 0;
    while (u[end] && u[end] != '?' && u[end] != '#') end++;
    for (int i = 0; i < end; i++)
        if (u[i] == '/' && (ipart(u + i + 1, end - i - 1, "bdmv") || ipart(u + i + 1, end - i - 1, "video_ts")))
            return 1;
    return 0;
}

static char *put(char *d, const char *s) {
    while (*s) *d++ = *s++;
    *d = 0;
    return d;
}

static char *putu(char *d, unsigned v) {
    char t[12];
    int n = 0;
    do { t[n++] = '0' + v % 10; v /= 10; } while (v);
    while (n) *d++ = t[--n];
    *d = 0;
    return d;
}

// ---- DMR state while a pushed disc image plays ----------------------------------------------------------------
// The jailbreak's ISO player is plain disc playback: nothing tells the DLNA renderer about it, so the control point
// (Emby "Play on", BubbleUPnP, ...) saw TRANSITIONING forever and its Stop/Pause did nothing. A thread per pushed
// image reports the disc's state and title time through the renderer's player callback, exactly like the stock DLNA
// file player does, and dmc_try() turns the control point's Stop/Pause/Play into remote keys.
typedef void (*fn_dmrcb)(int inst, int action, unsigned arg);
typedef int (*fn_thread)(unsigned *tid, void *attr, void *(*start)(void *), void *arg);
#define DMR_CB (*(fn_dmrcb volatile *)0x0134e418)            /* gpf_dmrCallback (DMR lib 0xc0200c) */
#define PTHREAD_CREATE ((fn_thread)0x0003105c)                 /* PLT */
#define PTHREAD_DETACH ((int (*)(unsigned))0x00032894)         /* PLT */
#define USLEEP ((int (*)(unsigned))0x0003294c)                 /* PLT */
#define PB_HANDLE ((unsigned (*)(void))0x009105c5)             /* current playback-manager handle (Thumb) */
#define PBC_STT ((int (*)(unsigned, int *))0x0013a921)         /* c_pbmg_pbc_stt_get: 0 stop 1 play 2 pause 3.. */
#define TIME_GET ((int (*)(unsigned, int, unsigned char *))0x00237a0d) /* c_pbmg_time_info_get: h, m, s */
#define IN_MENU ((int (*)(void))0x007de00d)                    /* IP control QPL: PLAY vs DISC MENU (Thumb) */
#define SEND_KEY ((int (*)(unsigned, unsigned))0x0071410c)     /* remote key, as IP control does (ARM) */
#define STRSET ((int (*)(char **, const char *))0x00c067d5)    /* DMR lib: free + strdup into a state string */
// DMR state strings TrackDuration / CurrentMediaDuration. The callback's duration action only fills them while they
// read 00:00:00, so a disc's first title (an intro) would keep its length: they are set here directly.
#define DMR_DUR1 ((char **)0x0127e238)
#define DMR_DUR2 ((char **)0x0127e240)
// player callback actions (DLNA_DMR_PLAYER_ACTION_*) and remote keys (IP control PLA/PAU/STP)
enum { A_START = 0, A_PAUSE = 1, A_STOP = 2, A_POS = 5, A_DUR = 6 };
#define KEY_PLAY 0x9f000
#define KEY_PAUSE 0x9f001
#define KEY_STOP 0x9f002

static volatile unsigned gen;     // session number; a newer push ends the older thread
static volatile int active;       // a pushed disc image is playing (thread running)
static volatile int paused;
static unsigned url_hash;

static unsigned fnv(const char *s) {
    unsigned h = 2166136261u;
    while (*s) h = (h ^ (unsigned char)*s++) * 16777619u;
    return h;
}

static void notify(int action, unsigned arg) {
    fn_dmrcb f = DMR_CB;
    if (f) f(0, action, arg);
}

static unsigned secs(const unsigned char *t) { return t[0] * 3600u + t[1] * 60u + t[2]; }

static void hms(char *d, unsigned s) {
    unsigned v[3] = {s / 3600, s / 60 % 60, s % 60};
    for (int i = 0; i < 3; i++) {
        if (v[i] > 99) v[i] = 99;
        *d++ = '0' + v[i] / 10;
        *d++ = '0' + v[i] % 10;
        *d++ = i < 2 ? ':' : 0;
    }
}

static int disc_mounted(void) { return ACCESS("/mnt/iso/BDMV", 0) == 0 || ACCESS("/mnt/iso/VIDEO_TS", 0) == 0; }

static void *mon(void *arg) {
    unsigned my = (unsigned)arg;
    int started = 0, stops = 0, last = -1, tick = 0;
    unsigned lastdur = ~0u;
    char t[12];
    for (;;) {
        USLEEP(1000000);
        tick++;
        if (gen != my) return 0;
        int s = -1;
        unsigned h = PB_HANDLE();
        if (PBC_STT(h, &s) != 0) s = -1;
        int mounted = disc_mounted();
        if (!started) {
            if (mounted && s >= 1 && s <= 9) {
                started = 1;
                PRINTF("[dlnaiso] DMR state: disc playing\n");
            } else if (tick > 180) {
                PRINTF("[dlnaiso] DMR state: disc never started\n");
                break;
            } else
                continue;
        }
        if (!mounted || s == 0) {
            if (++stops >= 3) break;
            continue;
        }
        if (s < 0) continue;
        stops = 0;
        int st = (s == 2 || s == 7) ? A_PAUSE : A_START;  // PAUSE, STEP
        paused = st == A_PAUSE;
        if (st != last) {
            notify(st, 0);
            last = st;
        }
        unsigned pos = 0, dur = 0;
        if (!IN_MENU()) {
            unsigned char a[16] = {0}, b[16] = {0};
            if (TIME_GET(h, 0, a) == 0) pos = secs(a);
            if (TIME_GET(h, 1, b) == 0) dur = pos + secs(b);
        }
        if (dur + 2 < lastdur || dur > lastdur + 2) {  // elapsed + remaining wobbles by a second
            hms(t, dur);
            STRSET(DMR_DUR1, t);
            STRSET(DMR_DUR2, t);
            lastdur = dur;
        }
        hms(t, pos);
        notify(A_POS, (unsigned)t);
    }
    if (gen == my) {
        PRINTF("[dlnaiso] DMR state: STOPPED\n");
        active = 0;
        notify(A_STOP, 0);
    }
    return 0;
}

static void session_start(const char *url) {
    unsigned tid = 0;
    url_hash = fnv(url);
    paused = 0;
    active = 1;
    if (PTHREAD_CREATE(&tid, 0, mon, (void *)++gen) == 0)
        PTHREAD_DETACH(tid);
    else
        active = 0;
}

// lu_dmr_player_msg_cb (0x946318): control point actions for the player (0 play [url], 1 pause, 2 stop, 3 seek,
// 9 position request = every GetPositionInfo). While a pushed disc image plays they become remote keys; returns 1
// when handled (stock code skipped).
int dmc_try(int type, char **p) {
    if (!active) return 0;
    if (type == 9) return 1;  // stock would answer with the disc menu's clip time; mon() keeps the position
    PRINTF("[dlnaiso] DMC action %d during disc playback\n", type);
    switch (type) {
    case 0:  // Play: the same URL again = resume, not a new push
        if (!p || !p[0]) return 0;
        if (fnv(p[0]) != url_hash) {  // another item: our session ends without a STOPPED that could land on it
            gen++;
            active = 0;
            return 0;
        }
        if (paused) SEND_KEY(KEY_PLAY, 200);
        return 1;
    case 1:
        SEND_KEY(KEY_PAUSE, 200);
        return 1;
    case 2:
        SEND_KEY(KEY_STOP, 200);
        return 1;
    case 3:
        if (p) PRINTF("[dlnaiso] seek %x %x\n", (unsigned)p[0xd], (unsigned)p[0xe]);
        return 1;
    }
    return 0;
}

int dmr_try(char **info) {
    const char *url = info[0], *pi = info[1];
    if (!url || !(iso_mime(pi) || iso_url(url) || disc_path(url))) return 0;
    if (lc(url[0]) != 'h' || slen(url) > 2000) return 0;
    for (const char *p = url; *p; p++)
        if (*p == '\'' || *p < 0x20) return 0;  // goes inside '...' on a shell line
    char name[24], *q;
    q = put(name, "dmr");
    q = putu(q, ++seq);
    put(q, ".iso");
    q = put(cmd, "sh /tmp/dlnaiso.sh ");
    q = put(q, name);
    q = put(q, " '");
    q = put(q, url);
    put(q, "'");
    PRINTF("[dlnaiso] DMR disc %s -> %s\n", disc_path(url) ? "folder" : "image", name);
    IPCD_EXEC(cmd, 0);
    q = put(path, "/mnt/dlnaiso/");
    put(q, name);
    if (ACCESS(path, 0) != 0) {
        PRINTF("[dlnaiso] %s not available, stock playback\n", path);
        return 0;
    }
    PLAY_ISO(path, 0, 0, 8);
    session_start(url);
    return 1;
}
