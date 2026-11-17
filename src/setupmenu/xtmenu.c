// oppo-dv-mkv: "Extras" page in the player's own Setup menu (RAM only). Loaded by xtmenu.sh into a fixed RWX region
// (0x1b80000) and started once through the loader stub (xtload.c). It adds one row, "Extras", to the Setup category
// list (before Exit) and builds the page behind it with the Setup menu's own widget library (wgl_menu): the same
// descriptors, columns, bullets, option lists and callbacks as Oppo's pages. A change is written to options.txt at
// once. Dolby Vision MKV and L5 also take effect at once (xtmenu.sh hands over where their switches are); the others at
// the next start of the player, as when the options file comes from the USB stick.
// Addresses: RU 65-0131 player program (49b649bd) and its jailbreak copies; xtmenu.sh checks every place it uses.
typedef unsigned u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef int i32;

// ---- player program -------------------------------------------------------------------------------------------
#define DO_CMD    ((int (*)(u32, u32, u32, u32))0x00740230)     /* c_wgl_do_cmd, ARM */
#define SETVIS    ((int (*)(u32, u32))0x007404cc)               /* c_wgl_set_visibility: 0 shown, 2 hidden */
#define WMOVE     ((int (*)(u32, const void *, u32))0x007402f0)  /* c_wgl_move */
#define REPAINT   ((int (*)(u32, u32, u32))0x007403c8)          /* c_wgl_repaint */
#define WSHOW     ((int (*)(u32, u32))0x007404d0)               /* c_wgl_show */
#define SETFOCUS  ((int (*)(u32, u32))0x007404c0)               /* c_wgl_set_focus */
#define W2SLEN    ((u32 (*)(const u16 *))0x00635de8)            /* c_uc_w2s_strlen */
#define MENU_REG  ((int (*)(const void *, u32))0x0066e08c)      /* menu_reg_cb_entries(table, n): ARM */
#define HIDE_ALL  ((int (*)(void))0x0083f01d)                   /* Setup: hide the page decorations (Thumb) */
#define LINE      ((int (*)(u32, u32, u32, u32, u32))0x0083f78d) /* Setup: show a connector line (level, row, row) */
#define FRONT_TXT ((void (*)(u32))0x0088adad)                   /* Setup: text for the front display (Thumb) */
#define ROOT_SEL  ((int (*)(u32 *, void *, u32, u32))0x00840599) /* id_menu_setup select (event 0) */
#define ROOT_HLT  ((int (*)(u32 *, u32, u32, u32))0x008405dd)    /* id_menu_setup highlight (event 2) */
#define ROOT_FOC  ((int (*)(u32 *, u32, u32, u32))0x00840989)    /* id_menu_setup get focus (event 3) */
#define APP_SEND  ((int (*)(u32, u32, const void *, u32, void *, void *))0x00b6b6ec)   /* c_app_send_msg */
#define HGET_OBJ  ((int (*)(u32, void *))0x00784c48)            /* handle_get_obj */
#define HTYPE_OBJ ((int (*)(u32, u16 *, u32 *))0x00784ccc)      /* handle_get_type_obj (no lock: WGL's 0x6b9178 locks) */
#define TPL_CREATE ((int (*)(u32, u32, u32 *))0x006aecb4)       /* c_wgl_img_tpl_create(segment type, image, &tpl) */
#define GET_RECT  ((int (*)(u32, u32, int *))0x006bbfd0)        /* widget rect (handle, 1, rect) */
#define SETUP_MSG 0x00824121u                                   /* Setup's message handler (its AMB record + 0xd0) */
#define H_SETUP   (*(volatile u32 *)0x01345120)                 /* Setup's app handle */
#define SG        0x0134512cu                                   /* Setup globals: +0x14 language, +0x2c category list */
#define GW(o)     (*(volatile u32 *)(SG + (o)))
#define STRTAB    ((u16 *const *)0x01a55cd4)                    /* osdmsg.bin strings: [id * 12 + language] */
#define STR(id)   (STRTAB[(id) * 12 + GW(0x14)])
#define F_NEXT    (*(volatile u8 *)(0x0123886c + 0xc))          /* the highlighted row opens something */
#define F_25      (*(volatile u8 *)(0x013464c4 + 0x25))
#define F_FOCUS   (*(volatile u8 *)(0x013464c4 + 0x26))          /* a page just got the focus */
#define ROW_BYTE  (*(volatile u8 *)0x0134756f)                   /* Oppo's own copy of the category row (0x3f260) */
#define BAR_RECT  ((const void *)0x012388ec)
#define ROOT_DESC ((struct desc *)0x0126aed4)                    /* the category list's descriptor */
#define CBG       ((volatile u32 *)0x012a6a68)                   /* registered menu callbacks: count, -, array */
#define PAGE_COLS ((const void *)0x0126ad58)                     /* Playback Setup's 5 columns */
#define OPT_COLS  ((const void *)0x012692d8)                     /* Auto Play Mode's option-list columns */
#define BULLET    ((const void *)0x019aae7c)                     /* the dot before a value */
#define RADIO     ((const void *)0x019aae9c)                     /* the mark in an option list */
#define CT_PAGE   0x019aaf10u                                    /* content-type words of those descriptors */
#define CT_OPT    0x019ab4a4u
#define BOX_IMG   (*(volatile u32 *)(0x01342ee4 + 0x1ac))        /* Setup image 107: the category box, 484 x 632 */
#define COLOR_A   (*(const u32 *)0x00fee308)
#define COLOR_B   (*(const u32 *)0x00fee304)
#define COLOR_C   (*(const u32 *)0x00fee30c)
#define PAGE_C2   ((const u32 *)0x0134634c)

#define CMD_SETTEXT  0x400f1402u   /* text widget */
#define CMD_ICON_IMG 0x1007fc06u   /* icon: (1, {1, image template x 3}) */
#define MC_ITEM_TEXT 0x10003002u   /* menu: (row << 16 | column), text */
#define MC_GET_SUB   0x10003010u   /* menu: the option list now open, or 0 */
#define MC_GET_HLT   0x1000301du   /* menu: highlighted row */
#define MC_CHECK     0x10003021u   /* menu: mark row (row, 1) */
#define MC_INSERT    0x10003030u   /* menu: insert item (row, item) */

// ---- menu descriptors (wgl_menu) --------------------------------------------------------------------------------
struct item { u8 flags, p1, p2, p3; u32 r4; const void *const *cols; struct desc *sub; u32 r10; };
struct desc {
    u32 parent, r04, id, style, ctype;
    u16 h, w, dist, count;     /* item height, width; gap between items; items */
    struct item *items;
    u32 ncols;
    const void *cols;
    u16 r28, per_page;
    u32 r2c, r30;
    void *rt;
};
_Static_assert(sizeof(struct item) == 0x14, "item");
_Static_assert(sizeof(struct desc) == 0x38, "desc");

// ---- the options --------------------------------------------------------------------------------------------------
#define NOPT  7
#define NVMAX 13
#define XT_ROW 6                   /* "Extras" goes before Exit */
#define ID_PAGE 0x1f0              /* menu ids nobody uses (Setup's run 1..0xf8, other menus from 0x5dd) */
#define ID_OPT0 0x1f1
enum { LIVE_NONE, LIVE_DV, LIVE_L5 };
struct opt {
    const char *key;
    const u16 *label, *help;
    u8 n, def, live;
    const char *val[NVMAX];
    const u16 *txt[NVMAX];
    u16 sid[NVMAX];                /* Oppo's own string (translated) instead of txt */
};
#define ONOFF { "on", "off" }, { 0 }, { 1553, 1554 }
static const struct opt O[NOPT] = {
    { "dv_mkv", u"Dolby Vision MKV",
      u"Dolby Vision from MKV files. Off: the player's own way", 2, 0, LIVE_DV, ONOFF },
    { "dv_l5", u"Dolby Vision L5",
      u"Active area (L5) to the TV in TV-led Dolby Vision. Off: Oppo's way", 2, 0, LIVE_L5, ONOFF },
    { "native_25p", u"Native 25p / 30p",
      u"25 and 30 fps HD video at its own rate (after a restart)", 2, 1, LIVE_NONE, ONOFF },
    { "menu_refresh", u"Home Menu Refresh",
      u"Refresh rate of the home menu and screen saver (after a restart)", 3, 0, LIVE_NONE,
      { "stock", "60", "50" }, { u"Stock", u"60 Hz", u"50 Hz" }, { 0 } },
    { "default_language", u"Reset Language",
      u"Language set by Reset Factory Defaults (after a restart)", 13, 0, LIVE_NONE,
      { "stock", "en", "de", "fr", "es", "it", "sv", "nl", "pl", "zh", "zh-tw", "ja", "ru" },
      { u"Stock", u"English", u"Deutsch", u"Français", u"Español", u"Italiano", u"Svenska", u"Nederlands",
        u"Polski", u"简体中文", u"繁體中文", u"日本語",
        u"Русский" }, { 0 } },
    { "boot_message", u"Boot Message",
      u"Show which parts loaded after each boot (after a restart)", 2, 0, LIVE_NONE, ONOFF },
    { "dlna_iso", u"DLNA Disc Images",
      u"Blu-ray, UHD and DVD ISO files over DLNA (after a restart)", 2, 0, LIVE_NONE, ONOFF },
};

// ---- header (written and read by xtmenu.sh) -------------------------------------------------------------------------
int xt_init(u32 a);
struct xhdr {
    u32 magic;                     // +0   "XTMN"
    u32 version;                   // +4
    int (*init)(u32);              // +8
    volatile u32 status;           // +12  0 not started, 1 waiting for the Setup thread, 2 on, 3 failed
    volatile i32 err;              // +16  which step failed (negative), or 0
    volatile u32 saves, save_err;  // +20, +24
    u8 cur[8];                     // +28  current values
    char path[96];                 // +36  options.txt
    volatile u32 inserts, msgs;    // +132, +136
    volatile u32 box;              // +140 the taller box is in
    u32 dv_addr;                   // +144 dvhook's "Dolby Vision from MKV off" byte (liblp), 0 = not on
    u32 l5_pa;                     // +148 physical address of dvcm's L5 word (bdpdrv1), 0 = not on
    char ver[24];                  // +152 oppo-dv-mkv version
};
_Static_assert(__builtin_offsetof(struct xhdr, inserts) == 132 && __builtin_offsetof(struct xhdr, ver) == 152, "xtmenu.sh");
__attribute__((section(".hdr"), used)) struct xhdr X = {
    0x4e4d5458, 2, xt_init, 0, 0, 0, 0, { 0 }, "/mnt/ubi_boot/oppo-dv-mkv/options.txt" };

// ---- state --------------------------------------------------------------------------------------------------------
static const void *rcol[5];                      // the "Extras" row in the category list
static struct item ritem;
static const void *pcol[NOPT][5];                // page rows: -, -, label, bullet, value
static struct item pitem[NOPT];
static struct desc pdesc;
static const void *ocol[NOPT][NVMAX][5];         // option rows: -, mark, text, -, -
static struct item oitem[NOPT][NVMAX];
static struct desc odesc[NOPT];
static struct { u32 key; const void *fn; } cbt[7 * (NOPT + 1)];
static u32 page_h;                               // our page's widget, once created
static volatile u32 inserted;                    // our row is in the category list (only the Setup thread sets it)
static u16 xhelp[64];                            // "oppo-dv-mkv <version> options"
static char fbuf[8192], obuf[8192 + 512];

// ---- compiler runtime (no libc) ----------------------------------------------------------------------------------
void __aeabi_memclr(void *d, u32 n) { u8 *p = d; while (n--) *p++ = 0; }
void __aeabi_memclr4(void *d, u32 n) { __aeabi_memclr(d, n); }
void __aeabi_memclr8(void *d, u32 n) { __aeabi_memclr(d, n); }
void __aeabi_memcpy(void *d, const void *s, u32 n) { u8 *p = d; const u8 *q = s; while (n--) *p++ = *q++; }
void __aeabi_memcpy4(void *d, const void *s, u32 n) { __aeabi_memcpy(d, s, n); }
void __aeabi_memcpy8(void *d, const void *s, u32 n) { __aeabi_memcpy(d, s, n); }
void *memset(void *d, int c, u32 n) { u8 *p = d; while (n--) *p++ = c; return d; }
void *memcpy(void *d, const void *s, u32 n) { __aeabi_memcpy(d, s, n); return d; }

// ---- small helpers ------------------------------------------------------------------------------------------------
static u32 sc3(u32 n, u32 a, u32 b, u32 c)
{
    register u32 r0 __asm__("r0") = a;
    register u32 r1 __asm__("r1") = b;
    register u32 r2 __asm__("r2") = c;
    register u32 r7 __asm__("r7") = n;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_LSEEK 19
#define SYS_RENAME 38
#define SYS_FSYNC 118
#define SYS_MPROTECT 125
#define SYS_CACHEFLUSH 0x0f0002
static u32 slen(const char *s) { u32 n = 0; while (s[n]) n++; return n; }
static const u16 *vtext(int i, int v) { return O[i].sid[v] ? STR(O[i].sid[v]) : O[i].txt[v]; }
static void help(const u16 *s) { if (s) DO_CMD(GW(0x70), CMD_SETTEXT, (u32)s, W2SLEN(s) & 0xffff); }
static void hints(u32 v58, u32 v5c, u32 v60, u32 v64)
{
    SETVIS(GW(0x74), 2); SETVIS(GW(0x58), v58); SETVIS(GW(0x5c), v5c); SETVIS(GW(0x60), v60); SETVIS(GW(0x64), v64);
}
static void page_frame(void)   // the box around our page and the line under "Extras" (Oppo's level-0 line for its row)
{
    SETVIS(GW(0x3c), 0);
    LINE(GW(0x2c), 0, 0, XT_ROW, 0);
}
static int opt_of(u32 id) { return id >= ID_OPT0 && id < ID_OPT0 + NOPT ? (int)(id - ID_OPT0) : -1; }

// ---- options.txt --------------------------------------------------------------------------------------------------
static int is_sp(char c) { return c == ' ' || c == '\t' || c == '\r'; }
static int is_tok(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}
// line [s, e): "key = value"? -> start of the value, or 0
static const char *match(const char *s, const char *e, const char *key)
{
    while (s < e && is_sp(*s)) s++;
    u32 k = slen(key);
    if ((u32)(e - s) < k) return 0;
    for (u32 i = 0; i < k; i++) if (s[i] != key[i]) return 0;
    s += k;
    while (s < e && is_sp(*s)) s++;
    if (s >= e || *s != '=') return 0;
    s++;
    while (s < e && is_sp(*s)) s++;
    return s;
}
static u32 readall(void)
{
    i32 fd = (i32)sc3(SYS_OPEN, (u32)X.path, 0, 0), r;
    u32 n = 0;
    if (fd >= 0) {
        while (n < sizeof(fbuf) - 1 && (r = (i32)sc3(SYS_READ, fd, (u32)fbuf + n, sizeof(fbuf) - 1 - n)) > 0) n += r;
        sc3(SYS_CLOSE, fd, 0, 0);
    }
    fbuf[n] = 0;
    return n;
}
static void load(void)
{
    readall();
    for (int i = 0; i < NOPT; i++) {
        int v = O[i].def;
        for (const char *s = fbuf; *s; ) {
            const char *e = s;
            while (*e && *e != '\n') e++;
            const char *p = match(s, e, O[i].key);
            if (p) {                                   // the last "key = value" line counts, as in dvmkv.sh
                char t[16]; u32 m = 0;
                while (p < e && is_tok(*p) && m < sizeof(t) - 1) { char c = *p++; t[m++] = c >= 'A' && c <= 'Z' ? c + 32 : c; }
                t[m] = 0;
                for (int j = 0; j < O[i].n; j++) {
                    const char *a = O[i].val[j]; u32 q = 0;
                    while (a[q] && a[q] == t[q]) q++;
                    if (!a[q] && !t[q]) { v = j; break; }
                }
            }
            s = *e ? e + 1 : e;
        }
        X.cur[i] = v;
    }
}
// rewrite options.txt: every "key = value" line of our options gets the current value (comments and other lines
// stay); a missing one is added at the end. Written to options.txt.tmp, synced, then renamed over it.
static int save(void)
{
    char tmp[104];
    u32 n = 0, pl = slen(X.path);
    int found[NOPT] = { 0 };
    i32 fd, r;
    readall();
    for (const char *s = fbuf; *s; ) {
        const char *e = s;
        while (*e && *e != '\n') e++;
        int hit = -1;
        for (int i = 0; i < NOPT && hit < 0; i++) if (match(s, e, O[i].key)) hit = i;
        if (hit >= 0) {
            const char *k = O[hit].key, *v = O[hit].val[X.cur[hit]];
            while (*k && n < sizeof(obuf) - 64) obuf[n++] = *k++;
            obuf[n++] = ' '; obuf[n++] = '='; obuf[n++] = ' ';
            while (*v && n < sizeof(obuf) - 64) obuf[n++] = *v++;
            found[hit] = 1;
        } else {
            for (const char *p = s; p < e && n < sizeof(obuf) - 64; p++) obuf[n++] = *p;
        }
        if (*e) { obuf[n++] = '\n'; s = e + 1; } else s = e;
    }
    if (n && obuf[n - 1] != '\n') obuf[n++] = '\n';
    for (int i = 0; i < NOPT; i++) if (!found[i]) {
        const char *k = O[i].key, *v = O[i].val[X.cur[i]];
        while (*k && n < sizeof(obuf) - 2) obuf[n++] = *k++;
        if (n < sizeof(obuf) - 40) { obuf[n++] = ' '; obuf[n++] = '='; obuf[n++] = ' '; }
        while (*v && n < sizeof(obuf) - 2) obuf[n++] = *v++;
        obuf[n++] = '\n';
    }
    if (pl > sizeof(tmp) - 5) return -1;
    for (u32 i = 0; i < pl; i++) tmp[i] = X.path[i];
    tmp[pl] = '.'; tmp[pl + 1] = 't'; tmp[pl + 2] = 'm'; tmp[pl + 3] = 'p'; tmp[pl + 4] = 0;
    fd = (i32)sc3(SYS_OPEN, (u32)tmp, 01 | 0100 | 01000, 0644);   // O_WRONLY | O_CREAT | O_TRUNC
    if (fd < 0) return fd;
    for (u32 o = 0; o < n; o += r) if ((r = (i32)sc3(SYS_WRITE, fd, (u32)obuf + o, n - o)) <= 0) { sc3(SYS_CLOSE, fd, 0, 0); return -5; }
    sc3(SYS_FSYNC, fd, 0, 0);
    sc3(SYS_CLOSE, fd, 0, 0);
    return (i32)sc3(SYS_RENAME, (u32)tmp, (u32)X.path, 0);
}
// Dolby Vision MKV and L5 at once: dvhook's flag byte is in this process (liblp); dvcm's L5 word is in the driver's
// code, written through /dev/mem like dvcm.sh does (only when it holds one of its two known values)
static void apply_live(int i)
{
    int on = X.cur[i] == 0;
    if (O[i].live == LIVE_DV && X.dv_addr) *(volatile u8 *)X.dv_addr = on ? 0 : 1;
    if (O[i].live == LIVE_L5 && X.l5_pa) {
        i32 fd = (i32)sc3(SYS_OPEN, (u32)"/dev/mem", 2, 0);
        u32 w = 0, nw = on ? 0xe2423006u : 0xe2423005u;     // sub r3, r2, #6 keeps L5; #5 is Oppo's (drops it)
        if (fd < 0) return;
        if ((i32)sc3(SYS_LSEEK, fd, X.l5_pa, 0) == (i32)X.l5_pa && sc3(SYS_READ, fd, (u32)&w, 4) == 4 &&
            (w == 0xe2423006u || w == 0xe2423005u) && w != nw && (i32)sc3(SYS_LSEEK, fd, X.l5_pa, 0) == (i32)X.l5_pa)
            sc3(SYS_WRITE, fd, (u32)&nw, 4);
        sc3(SYS_CLOSE, fd, 0, 0);
    }
}

// ---- category list (id_menu_setup): Extras at row 6, Exit moved to row 7 ----------------------------------------
static void root_row(int row)   // what Oppo's highlight / get-focus do for a row, for our two rows
{
    if (row == XT_ROW) {
        help(xhelp);
        hints(0, 0, 2, 0);
        F_NEXT = 1; F_25 = 0;
    } else {                    // Exit (Oppo's row 6)
        help(STR(1883));
        hints(0, 0, 2, 2);
        F_NEXT = 0; F_25 = 0;
        FRONT_TXT(0x5c9);
    }
}
static int root_sel(u32 *c, u16 *p, u32 a, u32 b)
{
    if (!inserted) return ROOT_SEL(c, p, a, b);
    if (p[0] == XT_ROW) return 0;
    if (p[0] == XT_ROW + 1) { u16 q[2] = { XT_ROW, p[1] }; return ROOT_SEL(c, q, a, b); }   // Exit
    return ROOT_SEL(c, p, a, b);
}
static int root_hlt(u32 *c, u32 a, u32 b, u32 d)
{
    u16 row = 0;
    if (!inserted) return ROOT_HLT(c, a, b, d);
    DO_CMD(c[2], MC_GET_HLT, (u32)&row, 0);
    if (row < XT_ROW) return ROOT_HLT(c, a, b, d);
    u32 v[4] = { 1, 0, 0, 0 };
    v[1] = v[2] = v[3] = (*(volatile u8 *)(0x013450d4 + 0x1c) == 0 || *(volatile u8 *)0x0134512a == 0)
                         ? *(volatile u32 *)(0x0134642c + 0xc) : 0;
    DO_CMD(GW(0x20), 0x1007fc06, 1, (u32)v);
    root_row(row);
    REPAINT(GW(0x20), 0, 0);
    return 0;
}
static int root_foc(u32 *c, u32 a, u32 b, u32 d)
{
    u16 row = 0;
    if (!inserted) return ROOT_FOC(c, a, b, d);
    DO_CMD(c[2], MC_GET_HLT, (u32)&row, 0);
    if (row < XT_ROW) return ROOT_FOC(c, a, b, d);
    u32 sub = 0;
    DO_CMD(c[2], MC_GET_SUB, (u32)&sub, 0);
    if (!sub) return 0;
    HIDE_ALL();
    SETVIS(GW(0x30), 0);        // the category box back (Oppo's also shows +0x7c, the mark layout() keeps hidden)
    WMOVE(GW(0x28), BAR_RECT, 0);
    root_row(row);
    WSHOW(GW(0x20), 0);
    return 0;
}

// ---- our page -------------------------------------------------------------------------------------------------------
static void page_values(u32 h)
{
    for (int i = 0; i < NOPT; i++) DO_CMD(h, MC_ITEM_TEXT, (u32)i << 16 | 4, (u32)vtext(i, X.cur[i]));
}
static void styles(u32 h, int page)   // Oppo's pre-create for a page (Playback Setup) or an option list (Auto Play)
{
    u32 z[7] = { 0 }, col[7];
    DO_CMD(h, 0x40600475, (u32)z, 0);
    DO_CMD(h, 0x40600495, (u32)z, 0);
    col[0] = COLOR_A; col[1] = COLOR_B; col[2] = COLOR_C; col[3] = COLOR_A; col[4] = COLOR_A;
    col[5] = page ? COLOR_B : COLOR_A; col[6] = COLOR_C;
    DO_CMD(h, 0x4060047e, 0x20002, (u32)col);
    if (!page) return;
    u32 m[4] = { 0x1e, 0, 0, 0 };
    DO_CMD(h, 0x4060047a, 2, (u32)m);
    m[0] = 0; m[1] = 0x12;
    DO_CMD(h, 0x4060047a, 3, (u32)m);
    DO_CMD(h, 0x4060047e, 0x40002, (u32)col);
    u32 r[4] = { 0, 0, PAGE_C2[1], PAGE_C2[2] };
    DO_CMD(h, 0x40600471, (u32)r, 0);
}
static void page_row(u16 row)
{
    if (row >= NOPT) row = 0;
    help(O[row].help);
    hints(0, 0, 0, 0);
    F_NEXT = 1; F_25 = 0;
}
static int pg_pre(u32 *c) { page_h = c[2]; styles(c[2], 1); page_values(c[2]); return 0; }
static int pg_sel(void) { return 0; }
static int pg_hlt(u32 *c)
{
    u16 row = 0;
    if (!F_FOCUS) {
        DO_CMD(c[2], MC_GET_HLT, (u32)&row, 0);
        page_row(row);
        REPAINT(GW(0x70), 0, 0); REPAINT(GW(0x74), 0, 0);
        return 0;
    }
    F_FOCUS = 0;
    HIDE_ALL();
    DO_CMD(c[2], MC_GET_HLT, (u32)&row, 0);
    page_row(row);
    page_frame();
    WMOVE(GW(0x28), BAR_RECT, 0);
    WSHOW(GW(0x20), 0);
    return 0;
}
static int pg_foc(u32 *c)
{
    u16 row = 0; u32 sub = 0;
    page_h = c[2];
    DO_CMD(c[2], MC_GET_HLT, (u32)&row, 0);
    DO_CMD(c[2], MC_GET_SUB, (u32)&sub, 0);
    if (!sub) { F_FOCUS = 1; return 0; }
    WMOVE(GW(0x28), BAR_RECT, 0);
    F_FOCUS = 0;
    HIDE_ALL();
    page_row(row);
    page_frame();
    page_values(c[2]);
    DO_CMD(c[2], 0x4060049d, 5, 0);
    u32 r[4] = { 0, 0, PAGE_C2[1], PAGE_C2[2] };
    DO_CMD(c[2], 0x40600471, (u32)r, 0);
    WSHOW(GW(0x20), 0);
    return 0;
}
static int pg_lose(u32 *c)
{
    u32 sub = 0;
    DO_CMD(GW(0x2c), MC_GET_SUB, (u32)&sub, 0);   // a_get_current_optionmenu_handle, first level
    if (!sub) {
        page_values(c[2]);
        DO_CMD(c[2], 0x4060049d, 5, 0);
    } else if (*(volatile u8 *)0x0134799f != 0xc) {
        DO_CMD(c[2], 0x4060049d, 3, 0);
        u32 r[4] = { 0, 0, *(volatile u32 *)(0x0134630c + 0x10), *(volatile u32 *)(0x0134630c + 0x14) };
        DO_CMD(c[2], 0x40600471, (u32)r, 0);
    }
    SETVIS(GW(0x74), 2);
    F_NEXT = 0; F_25 = 0;
    return 0;
}
static int nop(void) { return 0; }

// ---- option lists ---------------------------------------------------------------------------------------------------
// Only the mark, never a highlight, as Oppo's Auto Play list: the page creates the option list of its highlighted row
// at once (hidden), also while it just gets the focus; a highlight here would run op_hlt with F_FOCUS still set (the
// page's frame and bar then never came). Setup's own key handling highlights the row once the list really opens.
static int op_pre(u32 *c)
{
    int i = opt_of(c[3]);
    styles(c[2], 0);
    if (i >= 0) DO_CMD(c[2], MC_CHECK, X.cur[i], 1);
    return 0;
}
static int op_sel(u32 *c, u16 *p)
{
    int i = opt_of(c[3]);
    if (i < 0 || !(p[1] & 0xff)) return 0;
    if (p[0] < O[i].n && p[0] != X.cur[i]) {
        X.cur[i] = p[0];
        int r = save();
        X.saves++;
        if (r < 0) X.save_err = -r;
        apply_live(i);
        DO_CMD(c[2], MC_CHECK, X.cur[i], 1);
        if (page_h) { DO_CMD(page_h, MC_ITEM_TEXT, (u32)i << 16 | 4, (u32)vtext(i, X.cur[i])); REPAINT(page_h, 0, 0); }
    }
    SETVIS(c[2], 2);
    SETFOCUS(c[1], 0);
    return 0;
}
static int op_hlt(u32 *c)
{
    int i = opt_of(c[3]);
    const u16 *h = i >= 0 ? O[i].help : 0;
    if (F_FOCUS) {
        F_FOCUS = 0;
        HIDE_ALL();
        help(h);
        SETVIS(GW(0x58), 0); SETVIS(GW(0x5c), 0); SETVIS(GW(0x60), 0); SETVIS(GW(0x64), 2);
        SETVIS(GW(0x40), 0);
        if (i >= 0 && O[i].n > 7) SETVIS(GW(0x34), 0);
        LINE(GW(0x2c), 0, 0, XT_ROW, 0);       // Oppo has no category-6 lines into an option list
        WSHOW(GW(0x20), 0);
        F_25 = 0; F_NEXT = 0;
        return 0;
    }
    help(h);
    SETVIS(GW(0x58), 0); SETVIS(GW(0x5c), 0); SETVIS(GW(0x60), 0); SETVIS(GW(0x64), 2);
    REPAINT(GW(0x70), 0, 0); REPAINT(GW(0x4c), 0, 0);
    return 0;
}
static int op_foc(u32 *c)
{
    int i = opt_of(c[3]);
    F_FOCUS = 1;
    WMOVE(GW(0x28), BAR_RECT, 0);
    if (i >= 0) DO_CMD(c[2], MC_CHECK, X.cur[i], 1);
    return 0;
}
static int op_lose(u32 *c) { SETVIS(c[2], 2); return 0; }

// ---- descriptors and callbacks --------------------------------------------------------------------------------------
static void build(void)
{
    for (int i = 0; i < NOPT; i++) {
        struct desc *d = &odesc[i];
        for (int v = 0; v < O[i].n; v++) {
            ocol[i][v][1] = RADIO;
            ocol[i][v][2] = vtext(i, v);
            oitem[i][v].cols = ocol[i][v];
        }
        d->id = ID_OPT0 + i; d->style = 0x00208002; d->ctype = CT_OPT;
        d->h = 0x48; d->w = 0x1c2; d->dist = 6; d->count = O[i].n; d->items = oitem[i];
        d->ncols = 5; d->cols = OPT_COLS; d->r28 = 1; d->per_page = 7; d->r2c = 0x01000203;
        pcol[i][2] = O[i].label; pcol[i][3] = BULLET; pcol[i][4] = vtext(i, X.cur[i]);
        pitem[i].cols = pcol[i]; pitem[i].sub = d;
    }
    pdesc.id = ID_PAGE; pdesc.style = 0x10208002; pdesc.ctype = CT_PAGE;
    pdesc.h = 0x48; pdesc.w = 0x3be; pdesc.dist = 6; pdesc.count = NOPT; pdesc.items = pitem;
    pdesc.ncols = 5; pdesc.cols = PAGE_COLS; pdesc.r28 = 1; pdesc.per_page = 7; pdesc.r2c = 0x01000203; pdesc.r30 = 0xfe3e;
    rcol[2] = u"Extras";
    ritem.cols = rcol; ritem.sub = &pdesc;
    static const void *const pf[7] = { pg_sel, pg_pre, pg_hlt, pg_foc, pg_lose, nop, nop };
    static const void *const of[7] = { op_sel, op_pre, op_hlt, op_foc, op_lose, nop, nop };
    int k = 0;
    for (int e = 0; e < 7; e++) { cbt[k].key = ID_PAGE << 8 | e; cbt[k++].fn = pf[e]; }
    for (int i = 0; i < NOPT; i++)
        for (int e = 0; e < 7; e++) { cbt[k].key = (ID_OPT0 + i) << 8 | e; cbt[k++].fn = of[e]; }
    // help line of the Extras row: "oppo-dv-mkv <version> options"
    static const char a[] = "oppo-dv-mkv ", z[] = " options";
    u32 n = 0;
    for (const char *s = a; *s; ) xhelp[n++] = *s++;
    for (u32 i = 0; i < sizeof(X.ver) && X.ver[i] && n < 40; i++) xhelp[n++] = (u8)X.ver[i];
    for (const char *s = z; *s; ) xhelp[n++] = *s++;
    xhelp[n] = 0;
}
static int cb_has(u32 key)
{
    u32 n = CBG[0], *a = (u32 *)CBG[2];
    for (u32 i = 0; a && i < n; i++) if (a[2 * i] == key) return 1;
    return 0;
}
static volatile u32 *cb_slot(u32 key, u32 want)
{
    u32 n = CBG[0], *a = (u32 *)CBG[2];
    if (!a) return 0;
    for (u32 i = 0; i < n; i++) if (a[2 * i] == key) return a[2 * i + 1] == want ? &a[2 * i + 1] : 0;
    return 0;
}

// ---- in the Setup thread ----------------------------------------------------------------------------------------------
// Oppo's own UP/DOWN handling for the category list (0x3f260 / 0x3f880, called by Setup's message handler before
// the menu) keeps the row in a byte and wraps after row 6 (DOWN: cmp r1, #5 / bgt -> row 0; UP: row 0 -> movs r2, #6).
// With our row in, the list has 8 rows: 6 -> 7. Setup's show routine puts the category box back to {221, 216, 705, 848}
// (0x840516 mov.w r3, #0x350): 926 instead, one row taller (with Oppo's own box image, which does not stretch, a taller
// rect changes nothing). The code pages are made writable for the halfwords, then read/execute again; caches flushed.
#define DOWN_CMP ((volatile u16 *)0x0003f2b2)   /* 2905 cmp r1, #5 -> 2906 */
#define UP_MOV   ((volatile u16 *)0x0003f8ea)   /* 2206 movs r2, #6 -> 2207 */
#define BOX_BOT  ((volatile u16 *)0x00840516)   /* f44f 7354 mov.w r3, #0x350 -> f240 339e movw r3, #926 */
static int code_ok(void)
{
    return *DOWN_CMP == 0x2905 && *UP_MOV == 0x2206 && BOX_BOT[0] == 0xf44f && BOX_BOT[1] == 0x7354;
}
static int patch_code(void)
{
    if ((i32)sc3(SYS_MPROTECT, 0x3f000, 0x1000, 7)) return -1;
    *DOWN_CMP = 0x2906;
    *UP_MOV = 0x2207;
    sc3(SYS_MPROTECT, 0x3f000, 0x1000, 5);
    sc3(SYS_CACHEFLUSH, 0x3f000, 0x40000, 0);
    if ((i32)sc3(SYS_MPROTECT, 0x840000, 0x1000, 7)) return -2;
    BOX_BOT[0] = 0xf240; BOX_BOT[1] = 0x339e;
    sc3(SYS_MPROTECT, 0x840000, 0x1000, 5);
    sc3(SYS_CACHEFLUSH, 0x840000, 0x841000, 0);
    return 0;
}
// 8 rows visible instead of 7 (no scrolling: Oppo positions the category highlight bar by row number, 78 px per row
// from the list's top): the list's visible-row count and window, the list widget and its box one row taller
// (downwards), the arrow / OK key hints under the box 56 px lower.
static const int ROOT_RECT[4] = { 238, 240, 688, 828 + 78 }, ROOT_BOX[4] = { 221, 216, 705, 848 + 78 };
static int layout(u32 root)
{
    u32 obj = 0, inst; u16 type = 0;
    if (HTYPE_OBJ(root, &type, &obj) || type != 0x3cf || !obj) return -1;
    inst = *(volatile u32 *)(obj + 0x80);
    if (!inst || *(volatile u32 *)(inst + 0xd8) != (u32)ROOT_DESC) return -2;
    volatile u16 *pp = (volatile u16 *)(inst + 0x64), *first = (volatile u16 *)(inst + 0x74), *last = (volatile u16 *)(inst + 0x76);
    if (*pp != 7 || *(volatile u16 *)(inst + 0x7c) != 8) return -3;
    *pp = 8;
    *first = 0; *last = 7;
    *(volatile u8 *)(inst + 0x78) = 0; *(volatile u8 *)(inst + 0x79) = 0;   // nothing above / below
    WMOVE(root, ROOT_RECT, 0);
    // the box is a fixed 484x632 bitmap; a template of the same bitmap with segments (stretchable: corners kept, sides
    // stretched) draws it one row taller. A template can only be made from an application's thread (this one).
    u32 img = BOX_IMG, tpl = 0;
    if (img && *(volatile u32 *)img == 0x027801e4 && TPL_CREATE(3, img, &tpl) == 0 && tpl) {
        u32 v[4] = { 1, tpl, tpl, tpl };
        if (DO_CMD(GW(0x30), CMD_ICON_IMG, 1, (u32)v) == 0) { WMOVE(GW(0x30), ROOT_BOX, 0); X.box = 1; }
    }
    SETVIS(GW(0x7c), 2);                                  // the "more below" mark under the box
    static const u8 hint[6] = { 0x4c, 0x54, 0x58, 0x5c, 0x60, 0x64 };
    for (int i = 0; i < 6; i++) {
        int r[4];
        if (GET_RECT(GW(hint[i]), 1, r) == 0) { r[1] += 56; r[3] += 56; WMOVE(GW(hint[i]), r, 0); }
    }
    return 0;
}
int xt_op(u32 op)
{
    u32 root = GW(0x2c);
    X.msgs++;
    if (op != 1 || inserted || !root || ROOT_DESC->count != XT_ROW + 1) return 0;
    if (!code_ok()) { X.err = -8; X.status = 3; return 0; }              // not Oppo's code
    int r = DO_CMD(root, MC_INSERT, XT_ROW, (u32)&ritem);
    if (r) { X.err = -200 + r; X.status = 3; return 0; }
    inserted = 1; X.inserts++;
    u16 row = 0;                                          // Setup open on Exit: keep Oppo's row byte in step
    if (DO_CMD(root, MC_GET_HLT, (u32)&row, 0) == 0 && row == XT_ROW + 1) ROW_BYTE = XT_ROW + 1;
    if ((r = patch_code()) != 0) X.err = -50 + r;
    else if ((r = layout(root)) != 0) X.err = -60 + r;
    REPAINT(GW(0x20), 0, 0);                              // Setup may be on screen
    X.status = 2;
    return 0;
}
// Setup's message handler (AMB record + 0xd0) points here: our message -> xt_op; every other message: a jump (not a
// call) to Oppo's handler, so no return address into this code stays on the Setup thread's stack
#define XT_MSG 0x58540000u
__attribute__((naked)) int xt_msg(u32 h, u32 type, void *msg, u32 len)
{
    __asm__ volatile(
        "movw r12, #0x5854          \n"   /* XT_MSG >> 16 */
        "cmp.w r12, r1, lsr #16     \n"
        "bne 1f                     \n"
        "push {r4, lr}              \n"
        "uxth r0, r1                \n"
        "bl xt_op                   \n"
        "movs r0, #0                \n"
        "pop {r4, pc}               \n"
        "1: ldr r12, =0x00824121    \n"
        "bx r12                     \n"
        ".ltorg                     \n");
}

// ---- init (once per boot, from the loader stub in some player thread: memory only, then a message to Setup) ---------
// Menu widgets may only be changed by the Setup thread (a change from another thread deadlocked with a repaint): the
// row goes in when Setup's handler gets the message, at once at boot (Setup never opened yet), else when Setup is
// opened next. The row is never taken out again (the menu library's delete calls back into Setup's handler).
int xt_init(u32 arg)
{
    (void)arg;
    if (X.status) return 1;
    extern char __bss_start[], __bss_end[];
    __aeabi_memclr(__bss_start, __bss_end - __bss_start);
    X.status = 1; X.err = 0;
    u32 root = GW(0x2c), rec = 0;
    if (!root || !ROOT_DESC->rt || !H_SETUP) { X.err = -1; X.status = 3; return -1; }   // Setup not built yet
    if (ROOT_DESC->count != XT_ROW + 1 || ROOT_DESC->items[XT_ROW].sub) { X.err = -2; X.status = 3; return -2; }
    if (!cb_slot(0x100, 0x00840599) || !cb_slot(0x102, 0x008405dd) || !cb_slot(0x103, 0x00840989)) {
        X.err = -3; X.status = 3; return -3;                                          // callbacks not Oppo's
    }
    if (cb_has(ID_PAGE << 8)) { X.err = -4; X.status = 3; return -4; }
    if (HGET_OBJ(H_SETUP, &rec) || !rec) { X.err = -6; X.status = 3; return -6; }
    volatile u32 *f = (volatile u32 *)(rec + 0xc0);
    if (f[0] != 0x008239a9 || f[3] != 0x008231b5 || f[4] != SETUP_MSG) { X.err = -7; X.status = 3; return -7; }
    if (!code_ok()) { X.err = -8; X.status = 3; return -8; }
    load();
    build();
    int r = MENU_REG(cbt, sizeof(cbt) / sizeof(cbt[0]));
    if (r) { X.err = -100 + r; X.status = 3; return X.err; }
    // menu_reg_cb_entries built a new array (and freed the old one): the slots are looked up now
    volatile u32 *s0 = cb_slot(0x100, 0x00840599), *s2 = cb_slot(0x102, 0x008405dd), *s3 = cb_slot(0x103, 0x00840989);
    if (!s0 || !s2 || !s3) { X.err = -5; X.status = 3; return -5; }
    *s0 = (u32)root_sel; *s2 = (u32)root_hlt; *s3 = (u32)root_foc;   // Oppo's behaviour until the row is in
    f[4] = (u32)xt_msg;
    r = APP_SEND(H_SETUP, XT_MSG | 1, 0, 0, 0, 0);
    if (r) { f[4] = SETUP_MSG; X.err = -300 + r; X.status = 3; return X.err; }
    return 0;                                   // X.status becomes 2 once the Setup thread has put the row in
}
