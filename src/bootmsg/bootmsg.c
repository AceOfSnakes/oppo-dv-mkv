// oppo-dv-mkv boot message: one line of text in the player's own tip popup (the bar behind "Zoom is not available
// for Dolby Vision content."), shown once when bootmsg.sh asks for it. Lives in the jailbreak's free .text1 tail at
// 0x1c07800 (RWX). While a request is pending the player's usleep import (GOT) points at bm_usleep: the first thread
// that sleeps after the request (the RS-232 thread polls every 100 ms; Oppo's own code shows this tip from several
// threads) claims it, does it, and continues into the real usleep. Addresses: RU 65-0131 player program.
typedef unsigned u32;
struct hdr { u32 magic; volatile u32 state; int (*orig)(u32); u32 ms; u32 shows; int ret; int ret2; int y; char text[128]; };
// state: 0 idle; 1 show requested -> 2 -> 3 shown; 4 restore requested -> 5 -> 6 restored (bar back, icon visible)
__attribute__((section(".hdr"), used)) struct hdr bm = { 0x47534d42 };

#define C_STRLEN   ((u32 (*)(const char *))0x00475ea0)                       /* ARM */
#define PS_TO_W2S  ((int (*)(const char *, unsigned short *, u32))0x00635ddc) /* c_uc_ps_to_w2s, ARM */
#define W2S_STRLEN ((u32 (*)(const unsigned short *))0x00635de8)            /* c_uc_w2s_strlen, ARM */
#define WGL_DO_CMD ((int (*)(u32, u32, void *, u32))0x00740230)              /* c_wgl_do_cmd, ARM */
#define WGL_MOVE   ((int (*)(u32, int *, u32))0x007402f0)                   /* c_wgl_move, ARM */
#define WGL_VIS    ((int (*)(u32, u32))0x007404cc)                          /* c_wgl_set_visibility, ARM: 0 shown, 2 hidden */
#define SHOW_TMO   ((int (*)(u32, u32, u32, u32))0x00ac8345)                 /* widget show with timeout, Thumb */
#define HIDE_CB    0x0080cd35u                                              /* _misc_dv_lock_tip_ui_timeout_cb_wgl */
#define HIDE_CB2   0x0080cd81u                                              /* _misc_dv_lock_tip_ui_timeout_cb (timer) */
#define TSTART     ((int (*)(u32, u32, u32, u32, u32))0x00b6adfc)           /* x_timer_start stub, ARM */
#define TSTOP      ((int (*)(u32))0x00b6ae00)                               /* x_timer_stop stub, ARM */
#define TIP_TIMER  (*(volatile u32 *)0x01344a6c)                            /* the tip's own fallback timer */
#define FRAME      (*(volatile u32 *)0x01344c74)                            /* h_id_frame_dv_lock_info (at 100,43) */
#define ICON       (*(volatile u32 *)0x01344c78)                            /* the red "not allowed" icon */
#define BAR        (*(volatile u32 *)0x01344c7c)                            /* h_id_frame_dv_lock_info_bg */
#define TEXT       (*(volatile u32 *)0x01344c80)                            /* h_id_text_dv_lock_info, child of BAR */
#define SETTEXT    0x400f1402u
#define BAR_L 200
#define BAR_T 800                                                           /* created at {200, 800, 1520, 920} */
#define BAR_R 1520
#define BAR_H 120

static void show(void)
{
    unsigned short w[128];
    int r[4];
    u32 n = C_STRLEN(bm.text), ms = bm.ms ? bm.ms : 4000;
    if (n > 120) n = 120;
    for (u32 i = 0; i < 128; i++) w[i] = 0;
    PS_TO_W2S(bm.text, w, n);
    WGL_DO_CMD(TEXT, SETTEXT, w, W2S_STRLEN(w) & 0xffff);
    WGL_VIS(ICON, 2);
    r[0] = BAR_L; r[1] = bm.y; r[2] = BAR_R; r[3] = bm.y + BAR_H;
    WGL_MOVE(BAR, r, 0);
    bm.ret = SHOW_TMO(FRAME, ms, 1, HIDE_CB);
    if (bm.ret != 0 && TIP_TIMER) {     // as the stock tip: its own timer hides it (the popup is already shown)
        TSTOP(TIP_TIMER);
        bm.ret2 = TSTART(TIP_TIMER, ms, 1, HIDE_CB2, 0);
    }
    bm.shows++;
}

static void restore(void)
{
    int r[4] = { BAR_L, BAR_T, BAR_R, BAR_T + BAR_H };
    WGL_MOVE(BAR, r, 0);
    WGL_VIS(ICON, 0);
}

int bm_usleep(u32 us)
{
    u32 s = bm.state;
    if ((s == 1 || s == 4) && __atomic_compare_exchange_n(&bm.state, &s, s + 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
        if (s == 1) show(); else restore();
        __atomic_store_n(&bm.state, s + 2, __ATOMIC_RELEASE);
    }
    return bm.orig(us);
}
