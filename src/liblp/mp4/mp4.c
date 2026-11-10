/* MP4 audio/subtitle languages, tx3g (MP4 text) subtitles and a language list for Oppo UDP-203 liblp.so (md5 e39b358c...).
 * Linked at liblp VA 0x5402fc, over vMPCTestOperation4 (MPC self-test code, called only by other self-tests).
 * All liblp addresses are link-time VAs, made absolute with the runtime base passed in by the shims.
 *
 * Stock gaps this fills (CLpMp4Parser / CLpMp4MediaManagerProxy):
 *  - the mdhd handler never reads the track language, and the proxy never sends language messages (0x42)
 *    for the file's audio and subtitle tracks, so the menus show no language;
 *  - subtitle tracks with Apple's 'sbtl' handler are ignored, and a subtitle track whose sample entry is
 *    not 'stpp' or 'mp4s' is dropped, so tx3g text subtitles never appear. They are set up as the same
 *    UTF-8 text format (9) MKV SRT tracks use; the text decoder skips each sample's 2-byte length;
 *  - the player keeps at most 8 audio and 8 subtitle tracks (fixed storage in the parser; with a 9th the demuxer
 *    gets none of that type), the first ones in the file. With a language list (dvhook.sh langs ...), a pre-scan of
 *    the moov picks which 8: for audio first the file's default track (the original language, usually), then every
 *    track of the first listed language, then the next, then the rest in file order;
 *  - Nero chapters ('chpl') are only handled at the top of the moov: the box walker does not enter 'udta', where
 *    muxers put them (moov/udta/chpl), so files had no chapters. The pre-scan reads moov/udta/chpl and fills the
 *    parser's Nero chapter table as vMP4NEROCHAPHandle does; the first chapter starts at 0, as the MKV parser does. */
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef unsigned long long u64;
#define CAVE3 __attribute__((section(".text.cave3"), noinline))   /* moov pre-scan: in vMPCTestDvdRwdSpeedThr (link.ld) */
#define CAVE4 __attribute__((section(".text.cave4"), noinline))   /* subtitle fixes: in vMPCTestOperation1 (link.ld) */
#include "iso639.h"

#define ST_VA       0x757e80u   /* RW bss slack after dvhook's state (0x757e00..0x757e44) */
#define VA_READ     0x40caecu   /* CLpMp4Parser::mrAsyncTxData2Buf(u64 file pos, u8 *buf, u64 len), 0 = ok (synchronous) */
#define P_BOXPOS    0xb898      /* parser: u64 file position of the box being handled, + 4 (the walker reads from -4) */
#define P_TRACKID   0xb710      /* parser: track_ID of the trak being parsed (tkhd) */
#define P_NAUD      0x298d      /* parser: audio track count */
#define P_NSP       0x298e      /* parser: subtitle track count */
#define P_AUDREC    0x3c88      /* parser: audio track records, 0x628 each */
#define P_SPREC     0x8160      /* parser: subtitle track records, 0x628 each */
#define REC_SIZE    0x628
#define REC_TID     0x5d4       /* track record: track_ID */
#define P_SPSTRM    0xb2a0      /* parser: MPCSpElmStrmInf[8], 0x30 each (+4 format, +0xc mode, +0x10/+0x14 and
                                   +0x1c/+0x20 video width/height, as the MKV parser fills them) */
#define P_CHVER     0xb8c8      /* parser: Nero chapters: u8 chpl version, u32 count (+4, max 32), u32 count (+8),
                                   u32 start[32] (+0xc, 90 kHz); vMP4NEROCHAPHandle / u4GetNeroChapterNo */
#define MAXCHAP     32
#define P_VIDW      0xb6fc      /* parser: u16 video width (height at +2) */
#define X_PARSER    0x40e8      /* MP4 proxy: CLpMp4Parser* */
#define X_MSG       0xc         /* MP4 proxy: -> -> playback message object */
#define X_NSPINT    0xad8       /* MP4 proxy: internal subtitle count */
#define SPS_TXT     0x9b80      /* subtitle decoder: text pointer handed to the renderer */
#define SPS_LEN     0x9b7c      /* subtitle decoder: text length */
#define SPS_AUQ     0xaa8       /* subtitle decoder: queued AUs: [0] head, [1] tail, [2] count, 128 x 0x60 at +0x10 */
#define SPS_STARTPTS 0x9b50     /* subtitle decoder: current AU's PTS (u64, 90 kHz; u8SpsPrGetSpuPts) */
#define SPS_ENDPTS  0x9b60      /* subtitle decoder: current AU's end PTS (u64, -1 = none; u8SpsPrGetSpuEndPts) */
#define MSG_LANG    0x42        /* PB message: stream language code */
#define SLOTS       8           /* audio and subtitle tracks the player can use */
#define MAXTRK      64          /* track_IDs handled (state is indexed by track_ID) */
#define MAXREADS    2000        /* pre-scan read budget */
#define SPARSE      30          /* subtitle track with < 1 sample per SPARSE seconds = 'sparse' (see the selection) */
#define FMT_TEXT    9           /* MPC subtitle format: UTF-8 text (MKV S_TEXT/UTF8) */
#define FMT_DVDSP   1           /* MPC subtitle format of MP4 VobSub ('mp4s') tracks */
#define FCC(a,b,c,d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))

struct st {
    u32 parser;      /* parser of the file whose moov was parsed last */
    u16 lang[MAXTRK];/* mdhd language (packed ISO 639-2) by track_ID; 0 = none */
    u16 tx3g;        /* bit i: subtitle track i is tx3g */
    u16 space;       /* " ": shown for an empty tx3g sample (a zero-length text clears nothing) */
    u8 pref[8][4];   /* language list (dvhook.sh langs): 2- or 3-letter codes, lower case, 0-padded */
    u32 skip[2];     /* track_IDs the pre-scan left out (bit per ID) */
    u32 hits[8];     /* moov, mdhd, lang calls, tx3g, gsi fixes, lang msgs, text strips, queued line ends */
    u32 scan[4];     /* last pre-scan: reads, tracks, audio candidates << 16 | subtitle candidates, skipped */
    u8 tk_tid, nsd;  /* last tkhd's track_ID; VobSub frame sizes kept in sd[] */
    u16 tk_w, tk_h, rsv;   /* last tkhd's width and height (integer pixels) */
    struct { u8 tid, rsv; u16 w, h, rsv2; } sd[SLOTS];   /* 'subp' (VobSub) tracks: frame size from their tkhd */
};
#define ST(base) ((volatile struct st *)((base) + ST_VA))

struct langmsg { u32 type; u16 idx; u8 c[3]; };
typedef void (*send_t)(void *obj, u32 msg, void *data);
typedef u32 (*read_t)(void *p, u64 pos, u8 *buf, u64 len);

static u32 be32(const u8 *b) { return (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3]; }

static __attribute__((noinline)) u32 iso2(u32 l, u32 base) {  /* packed ISO 639-2 -> 'xy' (0 = unknown) */
    const u32 *t = (const u32 *)(base + (u32)iso639);   /* the table's link VA, made absolute like every other address */
    u32 lo = 0, hi = ISO639_N;
    while (lo < hi) {
        u32 m = (lo + hi) >> 1, k = t[m] >> 16;
        if (k == l) return t[m] & 0xffff;
        if (k < l) lo = m + 1; else hi = m;
    }
    return 0;
}

/* ---- pre-scan of the moov: per track its type, language and (subtitles) sample entry ---- */
struct scan { u8 *p; u32 base, reads; };
struct trk { u8 tid, kind, on, sparse; u16 a2; };   /* kind 1 = audio, 2 = usable subtitle; on = tkhd 'enabled';
                                                       sparse = subtitle with < 1 sample per SPARSE s (forced-only tracks) */

static CAVE3 int rd(struct scan *c, u64 pos, u8 *buf, u32 n) {
    if (++c->reads > MAXREADS) return -1;
    return ((read_t)(c->base + VA_READ))(c->p, pos, buf, n) ? -1 : 0;
}
/* box header at pos (within end): returns header length, 0 = stop */
static CAVE3 u32 box(struct scan *c, u64 pos, u64 end, u64 *size, u32 *type) {
    u8 h[16];
    if (pos + 8 > end || rd(c, pos, h, 8)) return 0;
    u64 sz = be32(h); *type = be32(h + 4);
    u32 hl = 8;
    if (sz == 1) { if (pos + 16 > end || rd(c, pos + 8, h + 8, 8)) return 0; sz = (u64)be32(h + 8) << 32 | be32(h + 12); hl = 16; }
    else if (sz == 0) sz = end - pos;
    if (sz < hl || pos + sz > end) return 0;
    *size = sz; return hl;
}
static CAVE3 u32 child(struct scan *c, u64 pos, u64 end, u32 want, u64 *cpos, u64 *cend) {   /* first child box of type want */
    u64 sz; u32 t, hl;
    while ((hl = box(c, pos, end, &sz, &t))) {
        if (t == want) { *cpos = pos; *cend = pos + sz; return hl; }
        pos += sz;
    }
    return 0;
}
static CAVE3 void scan_trak(struct scan *c, u64 pos, u64 end, struct trk *k) {
    u8 b[0x2c]; u64 a, e, ma, me; u32 hl;
    k->tid = 0xff; k->kind = 0; k->on = 0; k->sparse = 0; k->a2 = 0;
    if (!child(c, pos, end, FCC('t','k','h','d'), &a, &e) || e - a < 0x20 || rd(c, a, b, 0x20)) return;
    u32 tid = be32(b + (b[8] == 1 ? 0x1c : 0x14));
    if (tid >= MAXTRK) return;
    k->tid = tid; k->on = b[11] & 1;   /* tkhd flags (bytes 9..11): 1 = track enabled */
    if (!(hl = child(c, pos, end, FCC('m','d','i','a'), &ma, &me))) return;
    ma += hl;
    if (!child(c, ma, me, FCC('h','d','l','r'), &a, &e) || e - a < 0x14 || rd(c, a, b, 0x14)) return;
    u32 h = be32(b + 0x10);
    u32 tscale = 0; u64 dur = 0;
    /* mdhd: version 0 is 0x20 bytes (language at +0x1c), version 1 (Apple) 0x2c (language at +0x28) */
    if (child(c, ma, me, FCC('m','d','h','d'), &a, &e) && e - a >= 0x20 && !rd(c, a, b, e - a >= 0x2c ? 0x2c : 0x20)
        && (b[8] != 1 || e - a >= 0x2c)) {
        k->a2 = iso2(((b[8] == 1 ? (b[0x28] << 8 | b[0x29]) : (b[0x1c] << 8 | b[0x1d]))) & 0x7fff, c->base);
        if (b[8] == 1) { tscale = be32(b + 0x1c); dur = (u64)be32(b + 0x20) << 32 | be32(b + 0x24); }
        else { tscale = be32(b + 0x14); dur = be32(b + 0x18); }
    }
    if (h == FCC('s','o','u','n')) { k->kind = 1; return; }
    if (h != FCC('s','b','t','l') && h != FCC('s','u','b','p') && h != FCC('s','u','b','t')) return;
    /* subtitle: only entries the player can show take a slot (stsd: header 8, version/flags 4, count 4, entry size 4, type) */
    if (!(hl = child(c, ma, me, FCC('m','i','n','f'), &a, &e))) return;
    if (!(hl = child(c, a + hl, e, FCC('s','t','b','l'), &ma, &me))) return;
    ma += hl;
    if (!child(c, ma, me, FCC('s','t','s','d'), &a, &e) || e - a < 0x18 || rd(c, a, b, 0x18)) return;
    u32 en = be32(b + 0x14);
    if (en != FCC('t','x','3','g') && en != FCC('m','p','4','s') && en != FCC('s','t','p','p')) return;
    k->kind = 2;
    /* stsz: header 8, version/flags 4, sample size 4, sample count 4 */
    if (child(c, ma, me, FCC('s','t','s','z'), &a, &e) && e - a >= 0x14 && !rd(c, a, b, 0x14)
        && (u64)be32(b + 0x10) * SPARSE * tscale < dur)
        k->sparse = 1;
}

/* moov/udta/chpl -> the parser's Nero chapter table (entries: u64 start in 100 ns, u8 title length, title).
 * In the main blob (.text): the pre-scan cave is full. */
static __attribute__((noinline)) void scan_chpl(struct scan *c, u64 pos, u64 end, u8 *p) {
    u8 b[9]; u64 a, e; u32 hl, n, i;
    if (!(hl = child(c, pos, end, FCC('c','h','p','l'), &a, &e))) return;
    a += hl;
    if (e - a < 9 || rd(c, a, b, 9)) return;
    n = b[0] ? b[8] : b[4];                       /* version 1 has 4 reserved bytes before the count */
    a += b[0] ? 9 : 5;
    if (n > MAXCHAP) n = MAXCHAP;
    volatile u32 *t = (volatile u32 *)(p + P_CHVER + 0xc);
    for (i = 0; i < n; i++) {
        if (a + 9 > e || rd(c, a, b, 9)) break;
        u32 x = (u32)(((u64)be32(b) << 32 | be32(b + 4)) >> 8);   /* 25.6 us units: 32 bits last 30 hours */
        t[i] = i ? (u32)(((u64)x * 2473901163u) >> 30) : 0;      /* x * 256 * 9 / 1000 = 90 kHz */
        a += 9 + b[8];
    }
    if (!i) return;
    p[P_CHVER] = 1;
    *(volatile u32 *)(p + P_CHVER + 4) = i;
    *(volatile u32 *)(p + P_CHVER + 8) = i;
}

static CAVE3 u32 pref_a2(volatile struct st *s, u32 i, u32 base) {   /* list entry -> 'xy' */
    u8 a = s->pref[i][0], b = s->pref[i][1], c = s->pref[i][2];
    if (!a || !b) return 0;
    if (!c) return a == 'h' && b == 'e' ? ('i' << 8 | 'w') : (u32)a << 8 | b;   /* the menus know Hebrew as 'iw' */
    if (a < 'a' || a > 'z' || b < 'a' || b > 'z' || c < 'a' || c > 'z') return 0;
    return iso2((u32)(a - 0x60) << 10 | (u32)(b - 0x60) << 5 | (c - 0x60), base);
}

/* moov box (b = the parser's first bytes of it): a new file (or a re-parse) starts. Forget the previous one, and when
 * a language list is set, decide which audio/subtitle tracks the parser keeps. */
/* tkhd box (b = its first 0x28 bytes): remember the track's width and height for the hdlr hook. They sit past the
 * bytes the parser hands its handler (+0x54 in version 0, +0x60 in version 1), so the box is read from the file. The
 * stock handler keeps only the track_ID; MP4 VobSub tracks then reach the decoder with no frame size, and it assumes
 * a 720x576 frame (NTSC 720x480 subtitles were drawn too high). */
CAVE4 void mp4_tkhd(u8 *p, const u8 *b, u32 base) {
    volatile struct st *s = ST(base);
    u8 t[0x68]; u32 v = b[8], n = v == 1 ? 0x68 : 0x5c;
    s->tk_tid = 0xff;
    if ((u32)p != s->parser || v > 1) return;
    if (((read_t)(base + VA_READ))(p, *(u64 *)(p + P_BOXPOS) - 4, t, n)) return;
    if (be32(t + 4) != FCC('t','k','h','d') || be32(t) < n) return;
    u32 tid = be32(t + (v == 1 ? 0x1c : 0x14));
    if (tid >= MAXTRK) return;
    s->tk_w = be32(t + n - 8) >> 16; s->tk_h = be32(t + n - 4) >> 16; s->tk_tid = tid;
}

CAVE3 void mp4_moov(u8 *p, const u8 *b, u32 base) {
    volatile struct st *s = ST(base);
    for (u32 i = 0; i < MAXTRK; i++) s->lang[i] = 0;
    s->tx3g = 0; s->parser = (u32)p; s->skip[0] = s->skip[1] = 0; s->hits[0]++;
    s->nsd = 0; s->tk_tid = 0xff;
    u32 sel = s->pref[0][0] != 0;   /* track selection only with a language list; chapters always */
    struct scan c; c.p = p; c.base = base; c.reads = 0;
    struct trk t[MAXTRK]; u32 n = 0, na = 0, ns = 0;
    u64 pos = *(u64 *)(p + P_BOXPOS) - 4, sz = be32(b), end;
    u32 hl = 8, ty;
    if (sz == 1) { sz = (u64)be32(b + 8) << 32 | be32(b + 12); hl = 16; }
    if (sz < hl) return;
    end = pos + sz; pos += hl;
    while ((hl = box(&c, pos, end, &sz, &ty))) {
        if (ty == FCC('u','d','t','a')) scan_chpl(&c, pos + hl, pos + sz, p);
        else if (sel && n < MAXTRK && ty == FCC('t','r','a','k')) {
            scan_trak(&c, pos + hl, pos + sz, &t[n]);
            if (t[n].kind == 1) na++; else if (t[n].kind == 2) ns++;
            n++;
        }
        pos += sz;
    }
    s->scan[0] = c.reads; s->scan[1] = n; s->scan[2] = na << 16 | ns; s->scan[3] = 0;
    if (!sel || c.reads > MAXREADS) return;   /* incomplete: keep the stock choice */
    for (u32 kind = 1; kind <= 2; kind++) {
        if ((kind == 1 ? na : ns) <= SLOTS) continue;
        u8 keep[MAXTRK]; u32 used = 0, first = n, dflt = n;
        for (u32 i = 0; i < n; i++) {
            keep[i] = 0;
            if (t[i].kind == kind) { if (first == n) first = i; if (t[i].on && dflt == n) dflt = i; }
        }
        /* audio: the file's default track (tkhd enabled; else the first) is usually the original language - always kept */
        if (kind == 1) { u32 d = dflt < n ? dflt : first; if (d < n) { keep[d] = 1; used++; } }
        for (u32 j = 0; j < 8 && used < SLOTS; j++) {           /* listed languages, in list order */
            u32 a2 = pref_a2(s, j, base);
            if (!a2) continue;
            for (u32 i = 0; i < n && used < SLOTS; i++)
                if (t[i].kind == kind && !keep[i] && t[i].a2 == a2) { keep[i] = 1; used++; }
        }
        /* then the rest, in file order - but not sparse subtitle tracks (forced-only: a few samples, each lasting
         * many minutes). After a seek the demuxer reads from the file position of every kept track's current
         * sample, so one such track makes each seek and chapter jump read up to ~20 minutes of the file first. */
        for (u32 i = 0; i < n && used < SLOTS; i++)
            if (t[i].kind == kind && !keep[i] && !t[i].sparse) { keep[i] = 1; used++; }
        for (u32 i = 0; i < n; i++)
            if (t[i].kind == kind && !keep[i] && t[i].tid < MAXTRK) { s->skip[t[i].tid >> 5] |= 1u << (t[i].tid & 31); s->scan[3]++; }
    }
}

/* hdlr box (b = its first bytes; handler type at +0x10): 0 = leave this track out (the parser then ignores it, as it
 * does tracks of unknown types), 1 = stock handler. */
u32 mp4_hdlr(u8 *p, const u8 *b, u32 base) {
    volatile struct st *s = ST(base);
    u32 tid = *(u32 *)(p + P_TRACKID), h = be32(b + 0x10);
    if ((u32)p == s->parser && h == FCC('s','u','b','p') && tid == s->tk_tid && s->tk_w && s->tk_h && s->nsd < SLOTS) {
        u32 k = s->nsd++; s->sd[k].tid = tid; s->sd[k].w = s->tk_w; s->sd[k].h = s->tk_h;
    }
    if ((u32)p != s->parser || tid >= MAXTRK || !(s->skip[tid >> 5] & (1u << (tid & 31)))) return 1;
    return !(h == FCC('s','o','u','n') || h == FCC('s','b','t','l') || h == FCC('s','u','b','p') || h == FCC('s','u','b','t'));
}

/* mdhd box (b = its first 0x28 bytes): keep the language under the current track_ID. A version 1 box has it at +0x28,
 * past the bytes the parser hands its handler, so it is read from the file at the walker's position. */
void mp4_mdhd(u8 *p, const u8 *b, u32 base) {
    volatile struct st *s = ST(base);
    u32 tid = *(u32 *)(p + P_TRACKID), l;
    u8 t[0x2c];
    if (tid >= MAXTRK) return;
    if (b[8] == 0) l = (b[0x1c] << 8) | b[0x1d];
    else if (b[8] == 1) {
        if (((read_t)(base + VA_READ))(p, *(u64 *)(p + P_BOXPOS) - 4, t, 0x2c)) return;
        if (be32(t + 4) != FCC('m','d','h','d')) return;
        l = (t[0x28] << 8) | t[0x29];
    } else return;
    s->lang[tid] = l & 0x7fff; s->hits[1]++;
}

/* STSD: the current subtitle track has a tx3g sample entry. 1 = keep it, 0 = drop it (stock path). Only 8 are
 * kept: the demuxer is given no subtitle track at all when the parser counts 9 (vMP4GetCfgCfaParam). */
u32 mp4_tx3g(u8 *p, u32 base) {
    volatile struct st *s = ST(base);
    u32 i = (u32)p[P_NSP] - 1;
    if (i >= SLOTS) return 0;
    s->tx3g |= 1u << i; s->hits[3]++;
    return 1;
}

/* After CLpMp4Parser::mrGetStreamInfo: tx3g tracks are UTF-8 text subtitles. */
void mp4_gsi(u8 *p, u32 base) {
    volatile struct st *s = ST(base);
    if ((u32)p != s->parser) return;
    for (u32 i = 0; i < SLOTS; i++) {
        u32 *r = (u32 *)(p + P_SPSTRM + i * 0x30);
        if (s->tx3g & (1u << i)) {
            u32 w = *(u16 *)(p + P_VIDW), h = *(u16 *)(p + P_VIDW + 2);
            if (r[1] != FMT_TEXT || r[4] != w) { r[1] = FMT_TEXT; r[3] = 1; r[4] = r[7] = w; r[5] = r[8] = h; s->hits[4]++; }
        } else if (r[1] == FMT_DVDSP && i < p[P_NSP]) {
            /* VobSub: its frame size, and the mode the MKV player derives from it (1 up to 480 lines, else 2) */
            u32 tid = *(u32 *)(p + P_SPREC + i * REC_SIZE + REC_TID);
            for (u32 k = 0; k < s->nsd && k < SLOTS; k++)
                if (s->sd[k].tid == tid && (r[7] != s->sd[k].w || r[8] != s->sd[k].h)) {
                    r[7] = s->sd[k].w; r[8] = s->sd[k].h; r[3] = s->sd[k].h > 480 ? 2 : 1; s->hits[4]++; break;
                }
        }
    }
}

static void send_lang(void *obj, volatile struct st *s, u32 type, u32 idx, u8 *rec, u32 base) {
    u32 tid = *(u32 *)(rec + REC_TID), a2 = tid < MAXTRK ? iso2(s->lang[tid], base) : 0;
    if (!a2) return;
    struct langmsg m; m.type = type; m.idx = (u16)idx; m.c[0] = a2 >> 8; m.c[1] = a2 & 0xff; m.c[2] = 0;
    (*(send_t *)(*(u32 *)obj + 0x14))(obj, MSG_LANG, &m);
    s->hits[5]++;
}

/* After the proxy's vSetExtSubLanguage (it sends empty languages for the file's own subtitles):
 * send the audio and internal subtitle languages, the way the MKV proxy does. */
void mp4_lang(u8 *x, u32 base) {
    volatile struct st *s = ST(base);
    u8 *p = *(u8 **)(x + X_PARSER);
    void *obj = **(void ***)(x + X_MSG);
    s->hits[2]++;
    if (!p || !obj || (u32)p != s->parser) return;
    u32 na = p[P_NAUD], ns = x[X_NSPINT];
    for (u32 i = 0; i < na && i < 9; i++) send_lang(obj, s, 1, i + 1, p + P_AUDREC + i * REC_SIZE, base);
    for (u32 i = 0; i < ns && i < SLOTS; i++) send_lang(obj, s, 5, i + 1, p + P_SPREC + i * REC_SIZE, base);
}

/* After mrSpsDecStringByFmtSRT (text = the sample as is): a tx3g sample is a 2-byte big-endian text
 * length, the UTF-8 text, then optional style boxes. MKV/SRT text never starts with a byte < 8. */
void mp4_srt(u8 *d, u32 base) {
    volatile struct st *s = ST(base);
    u8 *t = *(u8 **)(d + SPS_TXT); u32 n = *(u32 *)(d + SPS_LEN);
    if (!t || n < 2 || t[0] >= 8) return;
    u32 l = (t[0] << 8) | t[1];
    if (l > n - 2) return;
    s->hits[6]++;
    if (!l) { s->space = ' '; *(u8 **)(d + SPS_TXT) = (u8 *)&s->space; *(u32 *)(d + SPS_LEN) = 1; return; }   /* end of a line */
    *(u8 **)(d + SPS_TXT) = t + 2; *(u32 *)(d + SPS_LEN) = l;
    /* The MP4 demuxer gives text AUs no end time (-1), which the display shows only for a moment, and a line
     * shown with an end time holds the screen until then. A tx3g line ends where the track's next sample
     * starts: take that from the next AU already queued (the demuxer reads ahead), else allow 3 s. */
    volatile u32 *q = (volatile u32 *)(d + SPS_AUQ);
    u64 st = *(u64 *)(d + SPS_STARTPTS), en = st + 3 * 90000;
    u32 h = q[0], c = q[2];
    if (c && h < 0x80) {
        u64 nx = *(volatile u64 *)(d + SPS_AUQ + 0x10 + h * 0x60 + 0x10);
        if (nx > st && nx < st + 30 * 90000) { en = nx; s->hits[7]++; }
    }
    *(u64 *)(d + SPS_ENDPTS) = en;
}
