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
#define CAVE5 __attribute__((section(".text.cave5"), noinline))   /* progressive/interlaced: in vMPCTestDvdRwdThr (link.ld) */
#define CAVE6 __attribute__((section(".subtext"), noinline))   /* text subtitles: in vMPCMpgFilePbTest (link.ld) */
#define CAVE7 __attribute__((section(".subtx3g"), noinline))   /* MP4 tx3g styles: in vMPCTestPauseWithAThr (link.ld) */
#include "iso639.h"

#define ST_VA       0x757e80u   /* RW bss slack after dvhook's state (0x757e00..0x757e4c) */
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
#define P_VIDSTRM   0x7400      /* parser: MPCVidElmStrmInf[], 0x140 each (+0x2c scan type: 0 unknown, 1 progressive, 2 interlaced) */
#define DV_PROG     0x757e41u   /* dvhook.c's state: u8 prog, u8 scanon (+1), u32 progp (+3) */
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
    u16 tk_w, tk_h;  /* last tkhd's width and height (integer pixels) */
    u8 pgmix;        /* the MKV being played has PGS and other subtitle formats (pg_mode; ST_PGMIX in shims.S) */
    u8 pgsync;       /* the PG decoder has been given a display set it can start from (pg_feed; 0 again in p_start) */
    struct { u8 tid, rsv; u16 w, h, rsv2; } sd[SLOTS];   /* 'subp' (VobSub) tracks: frame size from their tkhd */
    u32 sps;         /* the subtitle decoder whose PG decoder is running (p_start / p_tset; ST_SPS in shims.S) */
    short pgdy;      /* lines the current PGS epoch is moved down (Subtitle Shift; pg_feed) */
    u8 vprog;        /* the MP4's video is progressive (pre-scan: its avcC / hvcC; vid_trak) */
    u32 prov;        /* end time given to the tx3g line on screen for want of a next sample (mp4_srt / tx_end); 0 = none */
    u8 top;          /* the text line being decoded asked for the top of the screen ({\an7..9}, {\a5..7}; txt_dec) */
    u8 wlim;         /* word wrap width in narrow characters (0 = WRAP_DEF; dvhook.sh can set it) */
    u8 rsv3[2];
    u32 thits[2];    /* text lines cleaned, lines drawn at the top */
    u32 tdec;        /* the subtitle decoder of the last text line (for reading its fields while testing) */
};
#define ST(base) ((volatile struct st *)((base) + ST_VA))
_Static_assert(__builtin_offsetof(struct st, pgmix) == 0xe6 && __builtin_offsetof(struct st, pgsync) == 0xe7 && __builtin_offsetof(struct st, sps) == 0x128 && __builtin_offsetof(struct st, pgdy) == 0x12c && __builtin_offsetof(struct st, prov) == 0x130 && __builtin_offsetof(struct st, top) == 0x134 && sizeof(struct st) == 0x144, "struct st layout (shims.S, mkinstall.py)");

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

static CAVE7 u32 lc(u32 c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
/* 3 letters -> 'xy' (menu code), 0 = unknown */
static CAVE7 u32 key3(const u8 *v, u32 base) {
    u32 a = lc(v[0]), b = lc(v[1]), c = lc(v[2]);
    if (a < 'a' || a > 'z' || b < 'a' || b > 'z' || c < 'a' || c > 'z') return 0;
    return iso2((a - 0x60) << 10 | (b - 0x60) << 5 | (c - 0x60), base);
}
/* LanguageIETF (BCP 47) -> 'xy'; Chinese with a script or region: 'zs' Simplified (Hans, CN, SG), 'zt' Traditional
 * (Hant, TW, HK, MO). Untagged Chinese stays 'zh'. */
static CAVE7 u32 ietf(const u8 *v, u32 sz, u32 base) {
    u32 i = 0, k = 0;
    while (i < sz && v[i] && v[i] != '-' && v[i] != '_') i++;
    if (i == 2) k = lc(v[0]) << 8 | lc(v[1]);
    else if (i == 3) k = key3(v, base);
    if (k != ('z' << 8 | 'h')) return k;
    while (i < sz && v[i]) {
        u32 j = ++i, t = 0;
        while (i < sz && v[i] && v[i] != '-' && v[i] != '_') { if (i - j < 4) t = t << 8 | lc(v[i]); i++; }
        if (i - j > 4) continue;
        if (t == FCC('h','a','n','s') || t == ('c' << 8 | 'n') || t == ('s' << 8 | 'g')) return 'z' << 8 | 's';
        if (t == FCC('h','a','n','t') || t == ('t' << 8 | 'w') || t == ('h' << 8 | 'k') || t == ('m' << 8 | 'o')) return 'z' << 8 | 't';
    }
    return k;
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
    if (k->a2 == ('z' << 8 | 'h') && child(c, ma, me, FCC('e','l','n','g'), &a, &e) && e - a > 12 && e - a <= 0x1c
        && !rd(c, a, b, e - a))   /* Apple extended language (BCP 47): Simplified / Traditional Chinese */
        k->a2 = ietf(b + 12, e - a - 12, c->base);
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

static CAVE7 u32 pref_a2(volatile struct st *s, u32 i, u32 base) {   /* list entry -> 'xy' */
    u8 a = s->pref[i][0], b = s->pref[i][1], c = s->pref[i][2];
    if (!a || !b) return 0;
    if (!c) return a == 'h' && b == 'e' ? ('i' << 8 | 'w') : (u32)a << 8 | b;   /* the menus know Hebrew as 'iw' */
    if (a < 'a' || a > 'z' || b < 'a' || b > 'z' || c < 'a' || c > 'z') return 0;
    return iso2((u32)(a - 0x60) << 10 | (u32)(b - 0x60) << 5 | (c - 0x60), base);
}

/* ---- progressive or interlaced? ----
 * Neither parser tells the player (MPCVidElmStrmInf +0x2c stays 0 = unknown), and the player program then takes HD
 * video at 25, 29.97 or 30 frames/s for interlaced and plays it at 50/60 Hz. The answer is in the codec headers the
 * parsers already hold: H.264 SPS frame_mbs_only_flag, HEVC general_progressive/interlaced_source_flag. Only
 * "progressive" is passed on; anything else (interlaced, other codecs, a header that does not parse) stays unknown. */
struct bits { const u8 *b; u32 n, pos; };   /* n = length in bits; pos past n = ran out (reads give 0) */
static CAVE5 u32 gb(struct bits *s, u32 k) {
    u32 v = 0;
    while (k--) { u32 p = s->pos++; v = v << 1 | (p < s->n ? (s->b[p >> 3] >> (7 - (p & 7))) & 1 : 0); }
    return v;
}
static CAVE5 u32 ue(struct bits *s) {
    u32 z = 0;
    while (s->pos < s->n && !gb(s, 1)) z++;
    return z > 31 ? 0 : (1u << z) - 1 + gb(s, z);
}
/* avcC (version, profile, compat, level, length size, SPS count, SPS length, SPS NAL): 1 = frame_mbs_only_flag set */
static CAVE5 u32 avc_prog(const u8 *c, u32 len) {
    u8 r[192]; u32 n = 0, i, k, z = 0;
    if (len < 12 || c[0] != 1 || !(c[5] & 0x1f) || (c[8] & 0x1f) != 7) return 0;
    k = (u32)c[6] << 8 | c[7];
    if (k > len - 8) k = len - 8;
    for (i = 1; i < k && n < sizeof r; i++) {            /* the NAL without its header byte and emulation prevention */
        u8 x = c[8 + i];
        if (z >= 2 && x == 3) { z = 0; continue; }
        z = x ? 0 : z + 1; r[n++] = x;
    }
    struct bits s; s.b = r; s.n = n * 8; s.pos = 24;     /* profile_idc, constraint flags, level_idc */
    u32 pr = r[0];
    ue(&s);                                              /* seq_parameter_set_id */
    if (pr == 100 || pr == 110 || pr == 122 || pr == 244 || pr == 44 || pr == 83 || pr == 86 || pr == 118 || pr == 128
        || pr == 138 || pr == 139 || pr == 134 || pr == 135) {
        u32 cf = ue(&s);
        if (cf == 3) gb(&s, 1);
        ue(&s); ue(&s); gb(&s, 1);                       /* bit depths, transform bypass */
        if (gb(&s, 1))                                   /* scaling matrix */
            for (i = 0; i < (cf != 3 ? 8u : 12u); i++)
                if (gb(&s, 1)) {
                    u32 last = 8, next = 8;
                    for (k = 0; k < (i < 6 ? 16u : 64u); k++) {
                        if (next) { u32 d = ue(&s); next = (last + (d & 1 ? (d + 1) >> 1 : 0u - (d >> 1))) & 255; }
                        if (next) last = next;
                    }
                }
    }
    ue(&s);                                              /* log2_max_frame_num */
    k = ue(&s);                                          /* pic_order_cnt_type */
    if (k == 0) ue(&s);
    else if (k == 1) { gb(&s, 1); ue(&s); ue(&s); k = ue(&s); if (k > 255) return 0; while (k--) ue(&s); }
    ue(&s); gb(&s, 1); ue(&s); ue(&s);                   /* reference frames, gaps, width, height in macroblocks */
    k = gb(&s, 1);                                       /* frame_mbs_only_flag */
    return s.pos <= s.n ? k : 0;
}
/* hvcC: byte 6 = general_progressive_source_flag (0x80), general_interlaced_source_flag (0x40) */
static CAVE5 u32 hevc_prog(const u8 *c, u32 len) { return len >= 23 && c[0] == 1 && (c[6] & 0xc0) == 0x80; }
static CAVE5 void mkv_prog(u32 v, u32 parser, u32 base) {
    volatile u8 *d = (volatile u8 *)(base + DV_PROG);
    d[0] = v; *(volatile u32 *)(d + 3) = parser;
}
/* MKV: the video track's CodecPrivate while CLpMkvParser::mrParseTrackEntry holds it (k_avc / k_hevc in shims.S).
 * dvhook.c forgets it when the next Tracks element is parsed and hands it to the player when the file starts. */
CAVE5 void mkv_avc(const u8 *c, u32 len, u32 parser, u32 base) { mkv_prog(avc_prog(c, len), parser, base); }
CAVE5 void mkv_hevc(const u8 *c, u32 len, u32 parser, u32 base) { mkv_prog(hevc_prog(c, len), parser, base); }
/* MP4, a trak of the moov pre-scan: 1 = a video track whose sample entry has a progressive avcC / hvcC.
 * stsd: box header 8, version/flags 4, count 4; the first entry: size 4, type 4, 78 bytes, then its boxes. */
static CAVE5 u32 vid_trak(struct scan *c, u64 pos, u64 end) {
    u8 b[0x1c0]; u64 a, e; u32 hl, n, i;
    if (!(hl = child(c, pos, end, FCC('m','d','i','a'), &pos, &end))) return 0;
    pos += hl;
    if (!child(c, pos, end, FCC('h','d','l','r'), &a, &e) || e - a < 0x14 || rd(c, a, b, 0x14)) return 0;
    if (be32(b + 0x10) != FCC('v','i','d','e')) return 0;
    if (!(hl = child(c, pos, end, FCC('m','i','n','f'), &a, &e))) return 0;
    if (!(hl = child(c, a + hl, e, FCC('s','t','b','l'), &a, &e))) return 0;
    if (!child(c, a + hl, e, FCC('s','t','s','d'), &a, &e)) return 0;
    n = e - a > sizeof b ? sizeof b : (u32)(e - a);
    if (n < 0x66 + 8 || rd(c, a, b, n)) return 0;
    for (i = 0x66; i + 8 <= n; ) {
        u32 sz = be32(b + i), ty = be32(b + i + 4), m = n - i - 8;
        if (sz < 8) break;
        if (m > sz - 8) m = sz - 8;
        if (ty == FCC('a','v','c','C')) return avc_prog(b + i + 8, m);
        if (ty == FCC('h','v','c','C')) return hevc_prog(b + i + 8, m);
        if (sz > n) break;
        i += sz;
    }
    return 0;
}
static CAVE5 u32 scan_on(u32 base) { return ((volatile u8 *)(base + DV_PROG))[1]; }
/* MP4, after CLpMp4Parser::mrGetStreamInfo (mp4_gsi): the video's scan type, HD only (SD keeps the stock way) */
static CAVE5 void vid_gsi(u8 *p, volatile struct st *s, u32 base) {
    if (scan_on(base)) *(u32 *)(p + P_VIDSTRM + 0x2c) = s->vprog && *(u16 *)(p + P_VIDW + 2) >= 720;
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

/* MKV PGS subtitles chosen after playback started on a text or VobSub track (Oppo bug: nothing was drawn).
 * After such a switch the subtitle decoder (SPS) hands the track's data to the player program's Blu-ray PG decoder
 * (TSFile_ConsumeData), which parses every packet as an MPEG PES packet (00 00 01 BD, PES header with PTS/DTS) holding
 * ONE PGS segment, as in a transport stream. A Matroska block is a whole display set of bare segments, so the decoder
 * read no time stamps and dropped everything ("segment ignored; is not a PCS"). pg_feed gives it one packet per
 * segment: a PES header fragment chained to the segment (the decoder walks fragment lists). The packets live in two
 * heap blocks, freed when the decoder has reported every one of them consumed; only then the real packet is released. */
#define VA_TSCONSUME 0x1512ccu  /* .plt TSFile_ConsumeData(packet) */
#define VA_ALLOC     0x156198u  /* .plt x_mem_alloc(size) */
#define VA_FREE      0x166398u  /* .plt x_mem_free(ptr) */
#define ESH_PTS      0x18       /* SPS packet (ESH): u64 PTS, 90 kHz; [0] = fragment list, [1] = next packet */
#define PG_MAGIC     0x53475050u
struct frag { u8 *p; u32 n; struct frag *next; };
struct pgblk { u32 magic; void *esh; u32 left; void *pk; };
struct pgpk { struct frag *f; void *next; u32 magic; struct pgblk *b; struct frag f1, f2; u8 h[20]; };
typedef void *(*alloc_t)(u32); typedef void (*free_t)(void *); typedef u32 (*cons_t)(void *);

static void pes_ts(u8 *h, u32 mark, u64 t) {
    h[0] = mark | ((u32)(t >> 30) & 7) << 1 | 1; h[1] = t >> 22; h[2] = ((u32)(t >> 15) & 0x7f) << 1 | 1;
    h[3] = t >> 7; h[4] = ((u32)t & 0x7f) << 1 | 1;
}

/* Subtitle Shift (Setup / hold the Subtitle key). The subtitle decoder only keeps the setting (SPS_SHDN / SPS_SHUP,
 * lines down / up on the 1080-line plane) and moves its own VobSub and text planes; the PG decoder is started with a
 * fixed offset of 0 and draws where the disc says (Oppo gap: transport streams have no Subtitle Shift at all). Here
 * every window and object position of a display set is moved instead. The amount is fixed per epoch (windows must
 * not move inside one) and limited so that every window stays inside the picture, as the setting does elsewhere.
 * apply = 0: returns dy limited by the set's windows; 1: moves the set. */
#define SPS_SHDN     0x9cdc
#define SPS_SHUP     0x9ce4
CAVE4 static int pg_dy(u8 *d, u32 total, int H, int dy, u32 apply) {
    u32 o, l;
    for (o = 0; o + 3 <= total && (l = 3 + (d[o + 1] << 8 | d[o + 2])) <= total - o; o += l) {
        u8 *p = d + o, *e = p + l; u32 win = *p == 0x17, k;
        if (win && l > 3) { k = p[3]; p += 4; }                     /* WDS: id, x, y, w, h */
        else if (*p == 0x16 && l >= 14) { k = p[13]; p += 14; }      /* PCS: object id, window id, flags, x, y [, crop] */
        else continue;
        for (; k; k--) {
            u32 sz = win ? 9 : 8;
            if (p + sz > e) break;
            u8 *yp = p + (win ? 3 : 6); int y = yp[0] << 8 | yp[1];
            if (!apply) {
                if (win) { int r = H - (p[7] << 8 | p[8]) - y; if (dy > r) dy = r; if (dy < -y) dy = -y; }
            } else if (y + dy >= 0 && y + dy < H) { y += dy; yp[0] = y >> 8; yp[1] = y; }
            if (!win && (p[3] & 0x80)) sz = 16;
            p += sz;
        }
    }
    return dy;
}

CAVE4 void pg_feed(void **esh, u32 base) {
    cons_t consume = (cons_t)(base + VA_TSCONSUME);
    struct frag *f = esh[0], *g; u32 total = 0, n = 0, o, l;
    for (g = f; g; g = g->next) total += g->n;
    for (g = f; g && !g->n; g = g->next) ;
    if (!g || total < 3 || !g->p[0]) goto raw;            /* 00 00 01: already a PES packet (transport stream) */
    struct pgblk *b = ((alloc_t)(base + VA_ALLOC))(sizeof *b + total);
    if (!b) goto raw;
    u8 *d = (u8 *)(b + 1);
    for (g = f, o = 0; g; g = g->next) for (l = 0; l < g->n; l++) d[o++] = g->p[l];
    for (o = 0; o + 3 <= total && (l = 3 + (d[o + 1] << 8 | d[o + 2])) <= total - o; o += l) n++;
    /* After a start or a seek the PG decoder skips display sets until one it can start from (epoch start or
     * acquisition point), and after skipping one it sets no timer for the sets it already holds until more data
     * arrives (Oppo bug; a transport stream always brings more): in an MKV that is up to 6 s later, and every line in
     * between was lost. Half of all seeks land on such a set (the "clear" after a line). Give the decoder an empty
     * epoch start instead: it clears the screen, which is all such a set can do without the sets before it. */
    if (n && d[0] == 0x16 && (d[1] << 8 | d[2]) >= 11) {
        volatile struct st *s = ST(base);
        u32 first = !s->pgsync; int H = d[5] << 8 | d[6], dy = s->pgdy;
        if (d[10] & 0xc0) s->pgsync = 1;
        else if (first) { d[10] = 0x80; d[11] = 0; d[13] = 0; }
        if (s->sps && ((d[10] & 0x80) || first)) {            /* a new epoch, or the first set the decoder takes */
            dy = *(int *)(s->sps + SPS_SHDN) - *(int *)(s->sps + SPS_SHUP);
            if (H != 1080) dy = dy * H >> 10;
            s->pgdy = dy = pg_dy(d, total, H, dy, 0);
        }
        if (dy) pg_dy(d, total, H, dy, 1);
    }
    struct pgpk *k = n ? ((alloc_t)(base + VA_ALLOC))(n * sizeof *k) : 0;
    if (!k) { ((free_t)(base + VA_FREE))(b); goto raw; }
    b->magic = PG_MAGIC; b->esh = esh; b->left = n; b->pk = k;
    u64 pts = *(u64 *)((u8 *)esh + ESH_PTS);
    for (o = 0, l = 0; l < n; l++, k++) {
        u32 sl = 3 + (d[o + 1] << 8 | d[o + 2]), fl = sl;
        if (d[o] == 0x16 && sl >= 14 && !d[o + 13]) { fl = 14; d[o + 1] = 0; d[o + 2] = 11; }   /* no objects (see above) */
        k->f = &k->f1; k->next = 0; k->magic = PG_MAGIC; k->b = b;
        k->f1.p = k->h; k->f1.n = 19; k->f1.next = &k->f2;
        k->f2.p = d + o; k->f2.n = fl; k->f2.next = 0;
        k->h[0] = 0; k->h[1] = 0; k->h[2] = 1; k->h[3] = 0xbd; k->h[4] = 0; k->h[5] = 0;
        k->h[6] = 0x80; k->h[7] = 0xc0; k->h[8] = 10;
        pes_ts(k->h + 9, 0x30, pts); pes_ts(k->h + 14, 0x10, pts);
        o += sl;
    }
    for (k = b->pk, l = 0; l < n; l++) consume(k + l);    /* b may be gone after the last one */
    return;
raw:
    consume(esh);
}

/* MKV file open: 1 = keep the stock choice for a file that starts on a PGS track (the player program draws PGS itself
 * and no subtitle decoder exists: the other formats could never be drawn after a switch, and the last PGS line stayed
 * on screen), 0 = use the subtitle decoder, as for every other start track. 0 only when the file also has subtitles in
 * another format or external subtitles; a PGS-only file plays as before. */
#define X_MKVNSP     0x125      /* MKV proxy: u8 subtitle tracks in the file (u4GetIntSpCount's byte is set later) */
#define VA_NEXTSP    0x3cbd98u  /* CLpMkvMediaManagerProxy::u4GetExtSpCount() */
#define VA_SPINF     0x3ba798u  /* CLpMkvParser::vGetSpStrmInf(MPCSpElmStrmInf&, uchar): format at +4 */
#define X_MKVPARSER  0x114      /* MKV proxy: CLpMkvParser* */
#define FMT_PGS      0xc
typedef u32 (*cnt_t)(void *); typedef void (*spinf_t)(void *, u32 *, u32);
CAVE4 u32 pg_mode(u8 *x, u32 base) {
    u32 inf[12], i, n = x[X_MKVNSP], mix = ((cnt_t)(base + VA_NEXTSP))(x) != 0;
    for (i = 0; i < n && !mix; i++) {
        inf[1] = FMT_PGS;
        ((spinf_t)(base + VA_SPINF))(*(void **)(x + X_MKVPARSER), inf, i);
        mix = inf[1] != FMT_PGS;
    }
    ST(base)->pgmix = mix;
    return !mix;
}

/* The PG decoder consumed a packet: one of ours -> 0 until the last of its block, then the real packet to release. */
CAVE4 void *pg_done(struct pgpk *k, u32 base) {
    if (k->magic != PG_MAGIC || k->b->magic != PG_MAGIC) return k;
    struct pgblk *b = k->b;
    if (--b->left) return 0;
    void *esh = b->esh;
    b->magic = 0;
    ((free_t)(base + VA_FREE))(b->pk); ((free_t)(base + VA_FREE))(b);
    return esh;
}

CAVE3 void mp4_moov(u8 *p, const u8 *b, u32 base) {
    volatile struct st *s = ST(base);
    for (u32 i = 0; i < MAXTRK; i++) s->lang[i] = 0;
    s->tx3g = 0; s->parser = (u32)p; s->skip[0] = s->skip[1] = 0; s->hits[0]++;
    s->nsd = 0; s->tk_tid = 0xff; s->pgmix = 0; s->vprog = 0;
    u32 sel = s->pref[0][0] != 0;   /* track selection only with a language list; chapters always */
    u32 vs = scan_on(base), vp = 0; /* progressive video? only when the player is to be told */
    struct scan c; c.p = p; c.base = base; c.reads = 0;
    struct trk t[MAXTRK]; u32 n = 0, na = 0, ns = 0;
    u64 pos = *(u64 *)(p + P_BOXPOS) - 4, sz = be32(b), end;
    u32 hl = 8, ty;
    if (sz == 1) { sz = (u64)be32(b + 8) << 32 | be32(b + 12); hl = 16; }
    if (sz < hl) return;
    end = pos + sz; pos += hl;
    while ((hl = box(&c, pos, end, &sz, &ty))) {
        if (ty == FCC('u','d','t','a')) scan_chpl(&c, pos + hl, pos + sz, p);
        else if (ty == FCC('t','r','a','k')) {
            if (vs && !vp) vp = vid_trak(&c, pos + hl, pos + sz);
            if (sel && n < MAXTRK) {
                scan_trak(&c, pos + hl, pos + sz, &t[n]);
                if (t[n].kind == 1) na++; else if (t[n].kind == 2) ns++;
                n++;
            }
        }
        pos += sz;
    }
    s->vprog = vp;
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
    vid_gsi(p, s, base);
    for (u32 i = 0; i < SLOTS; i++) {
        u32 *r = (u32 *)(p + P_SPSTRM + i * 0x30);
        if (s->tx3g & (1u << i)) {
            u32 w = *(u16 *)(p + P_VIDW), h = *(u16 *)(p + P_VIDW + 2);
            if (r[1] != FMT_TEXT || r[4] != w) { r[1] = FMT_TEXT; r[3] = 1; r[4] = r[7] = w; r[5] = r[8] = h; s->hits[4]++; }
        } else if (r[1] == FMT_DVDSP && i < p[P_NSP]) {
            /* VobSub: its frame size, and the mode the MKV player derives from it (1 up to 480 lines, else 2) */
            u32 tid = *(u32 *)(p + P_SPREC + i * REC_SIZE + REC_TID);
            for (u32 k = 0; k < s->nsd && k < SLOTS; k++)
                if (s->sd[k].tid == tid) {   /* every call: the parser sets the mode again, not the size */
                    u32 m = s->sd[k].h > 480 ? 2 : 1;
                    if (r[3] != m || r[7] != s->sd[k].w || r[8] != s->sd[k].h) {
                        r[7] = s->sd[k].w; r[8] = s->sd[k].h; r[3] = m; s->hits[4]++;
                    }
                    break;
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

/* PTS of the next subtitle AU already queued (the demuxer reads ahead) if it lies between st and lim, else 0.
 * 90 kHz in 32 bits: good for 13 hours. */
CAVE3 static u32 nx_au(u8 *d, u32 st, u32 lim) {
    volatile u32 *q = (volatile u32 *)(d + SPS_AUQ); u32 h = q[0];
    if (!q[2] || h >= 0x80) return 0;
    q += 4 + h * (0x60 / 4) + 4;                           /* AU h: PTS at +0x10 */
    return !q[1] && q[0] > st && q[0] < lim ? q[0] : 0;
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
     * starts: take that from the next AU if it is queued already, else allow 3 s for now (tx_end). */
    u32 st = *(u32 *)(d + SPS_STARTPTS), en = nx_au(d, st, st + 30 * 90000);
    s->prov = 0;
    if (en) s->hits[7]++; else s->prov = en = st + 3 * 90000;
    *(u64 *)(d + SPS_ENDPTS) = en;
}

/* Every round of the subtitle decoder's thread (p_need): the next sample of a tx3g line that got the 3 s allowance
 * has arrived: the line ends there. Before, a line shorter than 3 s whose end was not read yet stayed too long. */
CAVE3 void tx_end(u8 *d, u32 base) {
    volatile struct st *s = ST(base); u32 pv = s->prov, nx;
    if (!pv) return;
    volatile u32 *e = (volatile u32 *)(d + SPS_ENDPTS);
    if (e[0] != pv || e[1]) s->prov = 0;                   /* another AU by now */
    else if ((nx = nx_au(d, *(u32 *)(d + SPS_STARTPTS), pv))) { e[0] = nx; s->prov = 0; }
}

/* ---- text subtitles (MKV S_TEXT/UTF8 and S_TEXT/ASS, MP4 tx3g): the subtitle decoder's text line, cleaned ----
 * Stock (mrSpsFillShowDataInfo) knows <b> <u> <i> and their end tags (exact lower case), line breaks (\N \n LF; and
 * '|', switched off in mkinstall.py) and deletes {...}. Anything else is drawn as text, so <font color="..."> showed
 * on screen. In place, never longer than the input:
 *  - <i> <b> <u> (any case) are kept in lower case; <br> becomes a line break; any other <tag ...> or </tag> goes.
 *    A '<' not followed by a letter (or '/' and a letter) and closed by '>' on the same line stays text ("<3", "< >").
 *  - {\...} override blocks: \i1 \b1 \u1 (\..0) become <i> <b> <u> (</..>); \an7..9 or SSA \a5..7 ask for the top of
 *    the screen; the rest goes. A '{' not followed by '\' stays text (the stock {...} deletion is switched off). */
static CAVE6 u32 txt_fix(u8 *t, u32 n, volatile u8 *top) {
    u32 i = 0, o = 0;
    while (i < n) {
        u8 c = t[i];
        if (c == '<') {
            u32 j = i + 1, cl = 0;
            if (j < n && t[j] == '/') { cl = 1; j++; }
            u8 a = j < n ? t[j] | 0x20 : 0;
            if (a >= 'a' && a <= 'z') {
                u32 k = j;
                while (k < n && t[k] != '>' && t[k] != '<' && t[k] != '\n' && t[k] != '\r') k++;
                if (k < n && t[k] == '>') {
                    if (k == j + 1 && (a == 'i' || a == 'b' || a == 'u')) {
                        t[o++] = '<'; if (cl) t[o++] = '/'; t[o++] = a; t[o++] = '>';
                    } else if (!cl && a == 'b' && (t[j + 1] | 0x20) == 'r' && (k == j + 2 || t[j + 2] == '/' || t[j + 2] == ' '))
                        t[o++] = '\n';
                    i = k + 1; continue;
                }
            }
        } else if (c == '{' && i + 1 < n && t[i + 1] == '\\') {
            u32 k = i + 1;
            while (k < n && t[k] != '}' && t[k] != '\n') k++;
            if (k < n) {
                u8 b[16]; u32 m = 0;
                for (u32 p = i + 1; p + 1 < k; p++) {
                    if (t[p] != '\\') continue;
                    u8 x = t[p + 1], y = p + 2 < k ? t[p + 2] : 0, z = p + 3 < k ? t[p + 3] : 0;
                    if (x == 'a' && y == 'n') { if (z >= '7' && z <= '9') *top = 1; }
                    else if (x == 'a' && y >= '5' && y <= '7' && !(z >= '0' && z <= '9')) *top = 1;
                    else if ((x == 'i' || x == 'b' || x == 'u') && (y == '0' || y == '1') && !(z >= '0' && z <= '9')
                             && o + m + 4 <= k + 1 && m + 4 <= sizeof b) {
                        b[m++] = '<'; if (y == '0') b[m++] = '/'; b[m++] = x; b[m++] = '>';
                    }
                }
                for (u32 q = 0; q < m; q++) t[o++] = b[q];
                i = k + 1; continue;
            }
        }
        t[o++] = c; i++;
    }
    return o;
}

/* UTF-8 code point at t (n bytes left): returns its length, the code point in *cp (a stray byte counts as itself). */
static CAVE6 u32 u8cp(const u8 *t, u32 n, u32 *cp) {
    u8 c = t[0]; u32 k = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0, v = k ? c & (0x3f >> k) : c;
    if (k >= n) { *cp = c; return 1; }
    for (u32 j = 1; j <= k; j++) { if ((t[j] & 0xc0) != 0x80) { *cp = c; return 1; } v = v << 6 | (t[j] & 0x3f); }
    *cp = v; return k + 1;
}
static CAVE6 u32 cpw(u32 c) {   /* display width: 2 for East Asian wide / full-width, else 1 */
    return (c >= 0x1100 && c <= 0x115f) || (c >= 0x2e80 && c <= 0xa4cf) || (c >= 0xac00 && c <= 0xd7a3) ||
           (c >= 0xf900 && c <= 0xfaff) || (c >= 0xfe30 && c <= 0xfe4f) || (c >= 0xff00 && c <= 0xff60) ||
           (c >= 0xffe0 && c <= 0xffe6) || c >= 0x20000 ? 2 : 1;
}
/* Width of t[i..) up to the line end (LF, CR, \N, \n); tags <..> count 0. *e = where the line ends. */
static CAVE6 u32 lwidth(const u8 *t, u32 n, u32 i, u32 *e) {
    u32 w = 0, cp;
    while (i < n) {
        u8 c = t[i];
        if (c == '\n' || c == '\r' || (c == '\\' && i + 1 < n && (t[i + 1] | 0x20) == 'n')) break;
        if (c == '<') { u32 k = i + 1; while (k < n && k < i + 6 && t[k] != '>') k++; if (k < n && t[k] == '>') { i = k + 1; continue; } }
        i += u8cp(t + i, n - i, &cp); w += cpw(cp);
    }
    *e = i; return w;
}
static CAVE6 u32 cdiv(u32 a, u32 b) { u32 q = 0; while (q * b < a) q++; return q; }   /* ceil(a / b), small values */
/* Word wrap (the stock wrap skips MKV SRT / MP4 tx3g, and counts bytes): each line wider than lim is cut into the
 * fewest lines that fit, of even width, at spaces (a space becomes LF; in place). Text without spaces (CJK) stays. */
static CAVE6 void txt_wrap(u8 *t, u32 n, u32 lim) {
    for (u32 i = 0; i < n; ) {
        u32 e, w = lwidth(t, n, i, &e);
        if (w > lim) {
            u32 k = cdiv(w, lim), tgt = cdiv(w, k), x = 0, sp = 0, xs = 0, have = 0, cp;
            for (u32 j = i; j < e; ) {
                if (t[j] == '<') { u32 q = j + 1; while (q < e && q < j + 6 && t[q] != '>') q++; if (q < e && t[q] == '>') { j = q + 1; continue; } }
                if (t[j] == ' ') {
                    if (x < tgt) { sp = j; xs = x; have = 1; }
                    else if (have && (tgt - xs < x - tgt || x > lim)) { t[sp] = '\n'; x = x - xs - 1; sp = j; xs = x; }
                    else { t[j] = '\n'; x = 0; have = 0; j++; continue; }
                }
                j += u8cp(t + j, e - j, &cp); x += cpw(cp);
            }
        }
        i = e + 1;
    }
}

#define WRAP_DEF 48     /* narrow characters per line when the box below is not known */
#define SPS_BOXW 0x9d3c  /* subtitle decoder: text box width (1520 on the 1920x1080 plane) */
#define SPS_FONT 0x9d44  /* subtitle decoder: font size (66 at the default Subtitle Size); about 2 x width / size
                            average narrow characters fit (49 at 1520 / 66) */
/* After mrSpsDecStringByFmtSRT (+ mp4_srt) or mrSpsDecStringByFmtASS: the text line at SPS_TXT / SPS_LEN. */
CAVE6 void txt_dec(u8 *d, u32 base) {
    volatile struct st *s = ST(base);
    u8 *t = *(u8 **)(d + SPS_TXT); u32 n = *(u32 *)(d + SPS_LEN);
    s->top = 0; s->tdec = (u32)d;
    if (!t || !n || n > 0x4000 || t == (u8 *)&s->space) return;
    u32 m = txt_fix(t, n, &s->top);
    u32 lim = s->wlim, bw = *(u32 *)(d + SPS_BOXW), fs = *(u32 *)(d + SPS_FONT);
    if (!lim) lim = bw >= 200 && bw <= 1920 && fs >= 16 && fs <= 200 ? cdiv(2 * bw, fs) + 2 : WRAP_DEF;
    txt_wrap(t, m, lim);
    if (m == n) return;
    s->thits[0]++;
    if (!m) { s->space = ' '; *(u8 **)(d + SPS_TXT) = (u8 *)&s->space; m = 1; }
    else t[m] = 0;                                 /* the stock parser searches the line with strstr / strchr */
    *(u32 *)(d + SPS_LEN) = m;
}

/* mrSpsUpdateDecBufByTextString: bl mrSpsRenderTextOnSurf. The text box from the player program's style (x SPS_BOXX,
 * bottom SPS_BOXY, height SPS_BOXH = 280 on the 1920x1080 subtitle plane) holds 3 lines of the larger font sizes; the
 * drawer skips lines outside it, so the top line of a 4-line subtitle (long lines wrapped) was lost. The box is made
 * TALL_BOX high, growing upwards (lines stay bottom-aligned). A line that asked for the top is drawn in a box at the
 * top of the plane, its lines from the top of the box (vertical alignment SPS_VALIGN 1 = top). The box starts as far
 * from the top as the bottom box ends from the bottom, so the menu's Subtitle Shift moves both the same way in or out.
 * A line or segment with tags got its tags' style only, without the menu style (the outline): put it back (SPS_STY
 * -> x_fe style bits: 1 regular, 2 italic, 4 bold, 8 underline, 0x20 and up edges). */
#define VA_RENDER  0x5847b0u   /* mrSpsRenderTextOnSurf */
#define SPS_BOXY   0x9d38
#define SPS_BOXH   0x9d40
#define SPS_VALIGN 0x9d54
#define TALL_BOX   600
#define VA_STYLE   0x584008u   /* (u8, u8, u8 of the menu's font style) -> x_fe style of plain text */
#define SPS_STY    0x9d28
typedef u32 (*style_t)(u32 a, u32 b, u32 c);
struct seg { u32 text, len, sty; };
struct ln { u32 text, len, sty, nseg; struct seg *seg; };
typedef u32 (*rend_t)(u8 *d, u32 surf, void *info);
CAVE6 u32 txt_rend(u8 *d, u32 surf, void *info, u32 base) {
    volatile struct st *s = ST(base);
    rend_t f = (rend_t)(base + VA_RENDER);
    u32 y = *(u32 *)(d + SPS_BOXY), h = *(u32 *)(d + SPS_BOXH), va = *(u32 *)(d + SPS_VALIGN);
    if (y > 1080 || h > 1080) return f(d, surf, info);      /* not the layout we know */
    struct { struct ln *l; u32 n; } *li = info;
    u32 ps = ((style_t)(base + VA_STYLE))(d[SPS_STY], d[SPS_STY + 1], d[SPS_STY + 2]) & ~0xfu;
    for (u32 i = 0; li->l && i < li->n && i < 8; i++) {
        struct ln *l = &li->l[i];
        if (l->sty & 0xe) l->sty |= ps;
        for (u32 k = 0; l->nseg > 1 && l->seg && k < l->nseg; k++) if (l->seg[k].sty & 0xe) l->seg[k].sty |= ps;
    }
    u32 nh = h < TALL_BOX ? TALL_BOX : h;
    if (nh > y) nh = y;
    *(u32 *)(d + SPS_BOXH) = nh;
    if (s->top) { s->thits[1]++; *(u32 *)(d + SPS_BOXY) = nh + (1080 - y); *(u32 *)(d + SPS_VALIGN) = 1; }
    u32 r = f(d, surf, info);
    *(u32 *)(d + SPS_BOXY) = y; *(u32 *)(d + SPS_BOXH) = h; *(u32 *)(d + SPS_VALIGN) = va;
    return r;
}

/* ---- external text subtitle files (.srt .ass .ssa .smi .sub .txt ...): UTF-8 without a BOM ----
 * CLpSpParser::vAnalyzeFileFormat takes a file as UTF-8 (type 3, code page 0) or UTF-16 (1, 2) only from its BOM; any
 * other file (type 0) is read in the menu's Text Subtitle Charset (Latin 1 / Latin 2 / Cyrillic / Other), so a UTF-8
 * file without a BOM - most of them - came out garbled with any setting but Other. After it: a type 0 file whose start
 * (the parser's read buffer) is valid UTF-8 with at least one non-ASCII character is UTF-8. A legacy code page file is
 * practically never valid UTF-8, so the menu setting still applies to those. Reached through liblp's GOT slot of
 * vAnalyzeFileFormat (every external text format's mrInit calls it). */
#define VA_ANAL     0x3a6318u  /* CLpSpParser::vAnalyzeFileFormat() */
#define VA_BUFSZ    0x3a4018u  /* CLpSpParser::u4GetSpParserBufSize() */
#define GOT_SPBUF   0x6ecbfcu  /* GOT word: address of the global holding the parser's read buffer */
#define SP_ENC      0x14       /* parser: 0 code page, 1 UTF-16 BE, 2 UTF-16 LE, 3 UTF-8 */
CAVE6 void ext_utf8(u8 *p, u32 base) {
    volatile struct st *s = ST(base);
    if (*(u32 *)(p + SP_ENC) != 0) return;
    u8 **bv = *(u8 ***)(base + GOT_SPBUF);
    u8 *b = bv ? *bv : 0;
    u32 n = ((u32 (*)(u8 *))(base + VA_BUFSZ))(p), hi = 0;
    if (!b || n < 4 || n > 0x200000) return;
    for (u32 i = 0; i < n; ) {
        u8 c = b[i];
        if (!c) break;
        if (c < 0x80) { i++; continue; }
        u32 k, cp;
        if (c >= 0xc2 && c <= 0xdf) { k = 1; cp = c & 0x1f; }
        else if (c >= 0xe0 && c <= 0xef) { k = 2; cp = c & 0x0f; }
        else if (c >= 0xf0 && c <= 0xf4) { k = 3; cp = c & 0x07; }
        else return;                                        /* not UTF-8 */
        if (i + k >= n) break;                              /* cut off by the end of the buffer */
        for (u32 j = 1; j <= k; j++) {
            if ((b[i + j] & 0xc0) != 0x80) return;
            cp = cp << 6 | (b[i + j] & 0x3f);
        }
        if ((k == 2 && (cp < 0x800 || (cp >= 0xd800 && cp <= 0xdfff))) || (k == 3 && (cp < 0x10000 || cp > 0x10ffff)))
            return;                                         /* overlong or surrogate */
        hi++; i += k + 1;
    }
    if (!hi) return;
    *(u32 *)(p + SP_ENC) = 3;
    s->thits[1] += 0x10000;
}

/* MP4 tx3g text styles: a sample is a 2-byte length, the UTF-8 text, then boxes; a 'styl' box lists ranges (start and
 * end character, font id, face flags 1 bold / 2 italic / 4 underline, size, colour, 12 bytes each). They become
 * <b> <i> <u> tags for the text decoder (stock ignored them: no italics in MP4 subtitles). The text is rewritten in
 * place into the sample, whose 'styl' box (12 bytes per range, at most 21 bytes of tags) leaves room. */
static CAVE7 void tx3g_styl(u8 *t, u32 l, u32 cap, u8 *d) {
    struct { u16 s, e; u8 f; } sty[8]; u32 ns = 0, p = l;
    while (p + 8 <= cap) {
        u32 sz = be32(t + p), ty = be32(t + p + 4);
        if (sz < 8 || sz > cap - p) break;
        if (ty == FCC('s','t','y','l') && sz >= 10) {
            u32 cnt = t[p + 8] << 8 | t[p + 9];
            for (u32 k = 0; k < cnt && ns < 8 && 10 + 12 * k + 12 <= sz; k++) {
                u8 *e = t + p + 10 + 12 * k;
                sty[ns].s = e[0] << 8 | e[1]; sty[ns].e = e[2] << 8 | e[3]; sty[ns].f = e[6] & 7;
                if (sty[ns].f && sty[ns].e > sty[ns].s) ns++;
            }
        }
        p += sz;
    }
    u8 src[600];
    if (!ns || l > sizeof src) return;
    for (u32 i = 0; i < l; i++) src[i] = t[i];
    u32 o = 0, ci = 0, lim = cap - 1, cp;
    for (u32 i = 0; ; ci++) {
        for (u32 k = 0; k < ns; k++)                       /* ends first, then starts */
            if (sty[k].e == ci) for (int b = 2; b >= 0; b--)
                if (sty[k].f >> b & 1 && o + 4 <= lim) { t[o++] = '<'; t[o++] = '/'; t[o++] = b == 0 ? 'b' : b == 1 ? 'i' : 'u'; t[o++] = '>'; }
        if (i >= l) break;
        for (u32 k = 0; k < ns; k++)
            if (sty[k].s == ci) for (int b = 0; b < 3; b++)
                if (sty[k].f >> b & 1 && o + 3 <= lim) { t[o++] = '<'; t[o++] = b == 0 ? 'b' : b == 1 ? 'i' : 'u'; t[o++] = '>'; }
        u32 m = u8cp(src + i, l - i, &cp);
        if (o + m > lim) break;
        for (u32 j = 0; j < m; j++) t[o++] = src[i + j];
        i += m;
    }
    t[o] = 0;
    *(u32 *)(d + SPS_LEN) = o;
}

/* mrSpsDecTextStringByFormat: after mrSpsDecStringByFmtSRT. MP4 tx3g samples (mp4_srt), their styles, then the text
 * pass for every text line. */
CAVE7 void txt_srt(u8 *d, u32 base) {
    u8 *t0 = *(u8 **)(d + SPS_TXT); u32 n0 = *(u32 *)(d + SPS_LEN);
    mp4_srt(d, base);
    u8 *t = *(u8 **)(d + SPS_TXT); u32 l = *(u32 *)(d + SPS_LEN);
    if (t0 && t == t0 + 2 && l + 2 < n0) tx3g_styl(t, l, n0 - 2, d);
    txt_dec(d, base);
}

/* ---- MKV language list (B1/B2) and forced subtitles (B3) ----
 * CLpMkvParser::mrParseTrackEntry keeps at most 10 audio and 32 subtitle tracks, the first ones in the file (an 11th
 * audio record would overlap the subtitle records). mkv_scan reads the whole Tracks element ahead (from the H0 hook,
 * before the parser walks it) and, with a language list and more tracks than fit, picks them like the MP4 pre-scan:
 * the default audio track, then every track of the listed languages in list order, then the rest in file order but
 * not forced subtitle tracks of other languages. mkv_keep (at the parser's "too many tracks" checks) then leaves the
 * others out by track number. FlagForced and LanguageIETF are not read by the parser at all; the scan keeps both. */
#define CAVE8 __attribute__((section(".langsel"), noinline))   /* in vMPCTestSendSetFileInf2Pipe (link.ld) */
#define ML_VA       0x757d00u   /* RW bss slack (bss ends 0x757cd4; dvhook's state starts 0x757e00) */
#define VA_MKVREAD  0x3b3a40u   /* CLpMkvParser::mrMkvReadDataFromBuffer(u8 *buf, u32 len): reads at +0x21f8, 0 = ok */
#define K_POS       0x21f8      /* MKV parser: u64 file position of the next read */
#define K_BUFLEFT   0x2944      /* MKV parser: bytes left in its read buffer at that position (0 = refill) */
#define K_TRK       0x2e00      /* MKV parser: -> track info: +0x34 audio count, +0x36 subtitle count, audio records
                                   0xa0 at +400 (language +0x88), subtitle records 0x70 at +2000 (language +0x38) */
#define MKT         64          /* tracks the scan looks at */
#define MKMAX       0x10000     /* largest Tracks element read ahead */
struct ml {
    u32 parser;      /* parser whose Tracks the scan read */
    u32 skip[4];     /* track numbers (< 128) left out */
    u32 hits[3];     /* scans, scans that chose, tracks left out */
    u32 sublang;     /* last file start: Subtitle Language read << 16 | OSD language (0xffff = not read) */
    u8 na, ns, sel, rd;    /* last scan: audio, subtitle tracks; 1 = chose; 2 = read, 3 = read failed */
    u8 arm;          /* 1: the next subtitle switch-on is replaced; then 0x80 | the number chosen (0 = off / stock) */
    u8 fopt;         /* option forced_subs (Setup > Extras, dvhook.sh forced on|off): forced subtitles only */
    u8 nst;
    u8 h10;          /* bit 0: the file's video has HDR10+ data; bit 1: it plays in HDR10+, not in Dolby Vision */
    u32 fn_sub, fn_osd, tab;   /* player program: a_cfg_get_subtitlelanguage, a_cfg_get_osdlanguage (Thumb), the
                                  10-byte ISO 639 table; set by dvhook.sh on after checking them, 0 = not this program */
    u8 snum[32];     /* track number of subtitle record i (mkv_keep) */
    struct { u8 num, fl; u16 key; } st[36];   /* the scan's subtitle tracks: fl 1 forced, 2 default, codec << 4 */
    u32 atmos[4];    /* track numbers (< 128) of TrueHD tracks with Atmos (thd_scan) */
    u32 proxy;       /* (+0xf8) the MKV proxy of the file playing (mkv_start), 0 once it is stopped or destroyed (mkv_dtor) */
    u8 hopt;         /* (+0xfc) options: bit 0 hdr10plus = off, bit 1 dynamic_hdr = hdr10plus (dvhook.sh h10, Setup > Extras) */
};
#define ML(base) ((volatile struct ml *)((base) + ML_VA))
_Static_assert(sizeof(struct ml) <= 0x100 && __builtin_offsetof(struct ml, fopt) == 0x29 && __builtin_offsetof(struct ml, hopt) == 0xfc && __builtin_offsetof(struct ml, fn_sub) == 0x2c, "struct ml: fits before dvhook's state; mkinstall.py");
struct mt { u8 num, type, def, forced; u16 key; u8 thd, rsv; };

/* EBML element header at b[*i] (n bytes): ID with its marker, size; 0 = bad / unknown size / past the end */
static CAVE8 u32 ebml(const u8 *b, u32 n, u32 *i, u32 *id, u32 *sz) {
    for (int k = 0; k < 2; k++) {
        if (*i >= n) return 0;
        u32 c = b[*i], len = 1, m = 0x80;
        while (len <= 8 && !(c & m)) { len++; m >>= 1; }
        if (len > (k ? 8u : 4u) || *i + len > n) return 0;
        u32 x = k ? c & (m - 1) : c, ones = (c & (m - 1)) == m - 1;
        for (u32 j = 1; j < len; j++) {
            u32 d = b[*i + j];
            if (k && x >> 24) return 0;   /* sizes over 16 MB: not in a Tracks element we read */
            x = x << 8 | d; if (d != 0xff) ones = 0;
        }
        if (k && ones) return 0;
        *i += len;
        if (k) *sz = x; else *id = x;
    }
    return *i + *sz <= n;
}
static CAVE7 u32 euint(const u8 *v, u32 sz) { u32 x = 0; for (u32 j = 0; j < sz; j++) x = x << 8 | v[j]; return sz > 4 ? 0xffffffff : x; }
/* CodecID: 1 A_TRUEHD, 2 A_EAC3; 3 V_MPEGH (HEVC); subtitles 4 SRT, 5 ASS / SSA, 6 WebVTT, 7 PGS, 8 VobSub; else 0 */
static __attribute__((section(".ddp"), noinline)) u32 codec(const u8 *v, u32 sz) {
    if (sz < 5 || v[1] != '_') return 0;
    if (v[0] == 'V') return sz >= 15 && v[2] == 'M' && v[6] == 'H' ? 3 : 0;
    if (v[0] == 'A') return sz >= 8 && v[2] == 'T' && v[3] == 'R' && v[7] == 'D' ? 1 : sz >= 6 && v[2] == 'E' && v[3] == 'A' ? 2 : 0;
    u32 c = v[2];
    if (c == 'T' && sz >= 10) c = v[7];                      /* S_TEXT/... */
    return c == 'U' ? 4 : c == 'A' || c == 'S' ? 5 : c == 'W' ? 6 : c == 'H' && sz >= 10 && v[7] == 'P' ? 7 : c == 'V' ? 8 : 0;
}
static CAVE7 void trk(const u8 *b, u32 e, u32 i, struct mt *t, u32 base) {
    u32 id, sz, kl = 'e' << 8 | 'n', ki = 0;   /* Matroska defaults: Language "eng", FlagDefault 1, FlagForced 0 */
    t->num = 0; t->type = 0; t->def = 1; t->forced = 0; t->thd = 0;
    while (ebml(b, e, &i, &id, &sz)) {
        const u8 *v = b + i;
        if (id == 0xd7) { u32 x = euint(v, sz); t->num = x < 128 ? x : 0; }
        else if (id == 0x83) t->type = euint(v, sz);
        else if (id == 0x88) t->def = euint(v, sz) != 0;
        else if (id == 0x86) t->thd = codec(v, sz);
        else if (id == 0x55aa) t->forced = euint(v, sz) != 0;
        else if (id == 0x22b59 && sz >= 3) kl = key3(v, base);
        else if (id == 0x22b59d) ki = ietf(v, sz, base);
        i += sz;
    }
    t->key = ki ? ki : kl;
}

typedef u32 (*mkrd_t)(void *p, u8 *buf, u32 len);
/* ---- Dolby Atmos in MKV TrueHD tracks ----
 * The info bar names the audio by its PB code; TrueHD is 0x1a ("Dolby TrueHD") and the player has no Atmos name.
 * thd_scan (from mkv_scan, only when the file has TrueHD) walks the Segment from the end of Tracks to the first
 * Cluster, steps through up to TH_ELEMS of its blocks and looks at the first major sync of each TrueHD track: Atmos = 4 substreams
 * and substream_info bit 7 (the 16-channel presentation). A Dolby Digital Plus track is Atmos when its first syncframe
 * has the extension flag for it in addbsi (ddp_joc). mkv_thd then gives such a track code 8 (SDDS, which never occurs
 * on this player), whose text dvhook.sh points at our buffers; atmos_names writes them for each file. */
#define TH_ELEMS 200     /* cluster elements looked at */
static CAVE5 u32 rdat(u8 *p, u64 pos, u8 *b, u32 n, u32 base) {
    *(volatile u64 *)(p + K_POS) = pos; *(volatile u32 *)(p + K_BUFLEFT) = 0;
    return ((mkrd_t)(base + VA_MKVREAD))(p, b, n);
}
/* element header at b (n bytes): ID, size (0xffffffffffffffff = unknown), header length; 0 = bad */
static CAVE5 u32 hdr(const u8 *b, u32 n, u32 *id, u64 *sz) {
    u32 i = 0;
    for (int k = 0; k < 2; k++) {
        if (i >= n) return 0;
        u32 c = b[i], len = 1, m = 0x80;
        while (len <= 8 && !(c & m)) { len++; m >>= 1; }
        if (len > (k ? 8u : 4u) || i + len > n) return 0;
        u64 x = k ? c & (m - 1) : c; u32 ones = (c & (m - 1)) == m - 1;
        for (u32 j = 1; j < len; j++) { x = x << 8 | b[i + j]; if (b[i + j] != 0xff) ones = 0; }
        i += len;
        if (k) *sz = ones ? ~(u64)0 : x; else *id = (u32)x;
    }
    return i;
}
/* E-AC-3 syncframe at d (n bytes): the bit stream information up to addbsi; 1 = extension type A (JOC: Dolby Atmos) */
#define CAVE9 __attribute__((section(".ddp"), noinline))   /* in vMPCTestPlayVcdThr (link.ld) */
struct br { const u8 *d; u32 n, p; };
static CAVE9 u32 dgb(struct br *b, u32 k) {
    u32 v = 0;
    while (k--) { u32 i = b->p >> 3; v = v << 1 | (i < b->n ? b->d[i] >> (7 - (b->p & 7)) & 1 : 0); b->p++; }
    return v;
}
static CAVE9 void dopt(struct br *b, u32 k) { if (dgb(b, 1)) b->p += k; }   /* a flag, and k bits when it is set */
static CAVE9 u32 ddp_joc(const u8 *d, u32 n) {
    if (n < 16 || d[0] != 0x0b || d[1] != 0x77) return 0;
    struct br b = { d, n, 16 };
    u32 typ = dgb(&b, 2); b.p += 14;                          /* strmtyp; substreamid, frmsiz */
    u32 fscod = dgb(&b, 2), blk = 3;
    if (fscod == 3) b.p += 2; else blk = dgb(&b, 2);          /* fscod2 / numblkscod */
    u32 ac = dgb(&b, 3), lfe = dgb(&b, 1), id = dgb(&b, 5);
    if (id < 11 || id > 16) return 0;
    for (u32 k = 0; k < (ac ? 1 : 2); k++) { b.p += 5; dopt(&b, 8); }                         /* dialnorm, compr */
    if (typ == 1) dopt(&b, 16);                               /* chanmap */
    if (dgb(&b, 1)) {                                         /* mixing metadata */
        if (ac > 2) b.p += 2;
        if (ac & 1 && ac > 2) b.p += 6;
        if (ac & 4) b.p += 6;
        if (lfe) dopt(&b, 5);
        if (typ == 0) {
            dopt(&b, 6); if (!ac) dopt(&b, 6);
            dopt(&b, 6);
            u32 mix = dgb(&b, 2);
            if (mix == 1) b.p += 5; else if (mix == 2) b.p += 12; else if (mix == 3) { u32 l = dgb(&b, 5); b.p += 8 * (l + 2); }
            if (ac < 2) { dopt(&b, 14); if (!ac) dopt(&b, 14); }
            if (dgb(&b, 1)) {                                 /* frame mixing configuration */
                if (!blk) b.p += 5; else for (u32 k = 0; k < (blk == 3 ? 6 : blk + 1); k++) dopt(&b, 5);
            }
        }
    }
    if (dgb(&b, 1)) {                                         /* informational metadata */
        b.p += 5;
        if (ac == 2) b.p += 4; else if (ac >= 6) b.p += 2;
        dopt(&b, 8); if (!ac) dopt(&b, 8);
        if (fscod < 3) b.p += 1;
    }
    if (typ == 0 && blk != 3) b.p += 1;                      /* convsync */
    if (typ == 2) { u32 bid = blk == 3 ? 1 : dgb(&b, 1); if (bid) b.p += 6; }
    if (!dgb(&b, 1)) return 0;                                /* addbsi */
    u32 l = dgb(&b, 6); b.p += 7;
    return l > 0 && b.p < n * 8 && dgb(&b, 1);
}
/* the name of audio code 8 for this file: w = 1 TrueHD, 2 Dolby Digital Plus, 3 both kinds of Atmos track */
#define VA_NLONG  0x757cd8u   /* ASCII, 25 bytes */
#define VA_NSHORT 0x757e50u   /* ASCII for the short fields (front display, IP control), 16 bytes */
#define VA_NWIDE  0x757fc8u   /* the long name in UTF-16, 50 bytes */
static CAVE9 void atmos_names(u32 w, u32 base) {
    static const char __attribute__((section(".ddp.rodata"))) T[] = "Dolby TrueHD Atmos\0TrueHD Atmos\0Dolby DD+ Atmos\0DD+ Atmos\0Dolby Atmos\0Dolby Atmos";
    const char *t = T + base;                                /* link-time address -> where liblp is */
    for (u32 k = (w - 1) * 2; k; k--) while (*t++) ;
    volatile char *l = (volatile char *)(base + VA_NLONG), *s = (volatile char *)(base + VA_NSHORT);
    volatile u16 *u = (volatile u16 *)(base + VA_NWIDE);
    do { *l++ = *t; *u++ = (u8)*t; } while (*t++);
    do *s++ = *t; while (*t++);
}
/* ---- HDR10+ (SMPTE ST 2094-40) in HEVC MKV ----
 * The driver reads the HDR10+ SEI of every frame and sends it to the TV (VSIF) when the output is HDR10+, and the player
 * program chooses that by itself when the TV's EDID has HDR10+ - but it must be told that the source is HDR10+, and the
 * MKV code says HDR10. h10_blk looks at the NAL units in front of the first slice of the first video block. */
#define CAVE10 __attribute__((section(".h10"), noinline))   /* in vMPCTestPlayMpgThr (link.ld) */
static CAVE5 u32 rdat(u8 *p, u64 pos, u8 *b, u32 n, u32 base);
static CAVE10 u32 h10_blk(u8 *p, u64 pos, u64 n, u32 base) {
    u8 b[96];
    for (u32 k = 0; k < 32 && n > 8; k++) {
        u32 g = n < sizeof b ? (u32)n : sizeof b;
        if (rdat(p, pos, b, g, base)) return 0;
        u32 l = be32(b), t = b[4] >> 1 & 63, e = l + 4 < g ? l + 4 : g;
        if (t < 32 || l < 2 || l > n) return 0;              /* the picture, or not 4-byte lengths */
        if (t == 39) for (u32 i = 6; i + 8 < e; ) {          /* SEI messages */
            u32 ty = 0, sz = 0;
            while (i < e && b[i] == 0xff) { ty += 255; i++; }
            if (i < e) ty += b[i++];
            while (i < e && b[i] == 0xff) { sz += 255; i++; }
            if (i < e) sz += b[i++];
            if (ty == 4 && sz >= 8 && i + 6 <= e && b[i] == 0xb5 && !b[i + 1] && b[i + 2] == 0x3c && !b[i + 3] && b[i + 4] == 1 && b[i + 5] == 4) return 1;
            i += sz;
        }
        pos += 4 + l; n -= 4 + l;
    }
    return 0;
}
/* eMPCScomMapVideoHdrType, HDR10 source: 2, or 5 (HDR10+) while such an MKV plays */
extern u32 h_scom, h_hand;                                   /* shims.S, written by dvhook.sh (0 = not known) */
typedef int (*sfmt_t)(u32 h, void *fmt);
/* what the TV can: bit 0 HDR10+, bit 1 Dolby Vision (the player program's copy of the EDID, byte 0x94); 3 = not known */
static CAVE10 u32 h10_tv(u32 base) {
    u32 f[0x2f0 / 4], s = *(volatile u32 *)((u32)&h_scom + base), h = *(volatile u32 *)((u32)&h_hand + base);
    if (!s || !h) return 3;
    for (u32 k = 0; k < 0x2f0 / 4; k++) f[k] = 0;
    ((sfmt_t)s)(*(u32 *)h, f);
    u32 b = ((u8 *)f)[0x94];
    return (b >> 4 & 1) | (b >> 1 & 1) << 1;
}
CAVE10 u32 h10_map(u32 base) { volatile struct ml *m = ML(base); return m->h10 & 1 && !(m->hopt & 1) && m->proxy ? 5 : 2; }
/* Before dvhook's dv_decide (mrMkvSetFileInf -> mrSetMediaInfo): a file with Dolby Vision AND HDR10+ on an HDR10 base
 * layer (profile 8.1 / 8.6 or 7). HDR10+ is used when the TV has it and the option prefers it, or the TV has no Dolby
 * Vision: the Dolby Vision record is forgotten, as with dv_mkv = off. */
CAVE10 void h10_pick(u32 base) {
    volatile struct ml *m = ML(base); volatile u8 *dv = (volatile u8 *)(base + 0x757e00);   /* dvhook.c struct st: +4 have, +8 rec */
    if (!(m->h10 & 1) || m->hopt & 1 || !dv[4]) return;
    u32 prof = dv[8 + 2] >> 1, comp = dv[8 + 4] >> 4, tv;
    if (!(prof == 7 || (prof == 8 && (comp == 1 || comp == 6)))) return;
    tv = h10_tv(base);
    if (!(tv & 1) || (tv & 2 && !(m->hopt & 2))) return;
    dv[4] = 0; m->h10 |= 2;
}
static CAVE5 void thd_blk(const u8 *d, u32 n, u32 *thd, volatile struct ml *m) {   /* a (Simple)Block's data */
    if (n < 4 || (d[0] & 0xf0) != 0x80) return;   /* track number as a 1-byte vint (< 128) */
    u32 num = d[0] & 0x7f, w = num >> 5, bit = 1u << (num & 31);
    if (thd[4 + w] & bit) {
        thd[4 + w] &= ~bit;
        u32 o = 4;                                           /* a laced block (mkvmerge packs 8 frames) has sizes first */
        while (o < 4 + 32 && o + 1 < n && !(d[o] == 0x0b && d[o + 1] == 0x77)) o++;
        if (ddp_joc(d + o, n - o)) { m->atmos[w] |= bit; thd[8] |= 2; }
        return;
    }
    if (!(thd[w] & bit)) return;
    for (u32 k = 4; k + 18 <= n && k < 4 + 64; k++)
        if (d[k] == 0xf8 && d[k + 1] == 0x72 && d[k + 2] == 0x6f && d[k + 3] == 0xba && d[k + 8] == 0xb7 && d[k + 9] == 0x52) {
            if (d[k + 16] >> 4 == 4 && d[k + 17] & 0x80) { m->atmos[w] |= bit; thd[8] |= 1; }
            thd[w] &= ~bit;
            return;
        }
}
static CAVE5 void thd_scan(u8 *p, u64 pos, u32 *thd, u32 base) {
    volatile struct ml *m = ML(base);
    u8 h[16]; u32 id, hl; u64 sz;
    for (u32 e = 0; ; e++) {   /* Segment children after Tracks, up to the first Cluster */
        if (e >= 24 || rdat(p, pos, h, 16, base) || !(hl = hdr(h, 16, &id, &sz))) return;
        if (id == 0x1f43b675) break;
        if (sz == ~(u64)0) return;
        pos += hl + sz;
    }
    /* Cluster children, by position: only element headers and the first bytes of a block are read (a video
     * keyframe at the start can be megabytes); a BlockGroup is entered for its Block */
    u8 d[96]; u64 end = sz == ~(u64)0 ? ~(u64)0 : pos + hl + sz;
    pos += hl;
    for (u32 e = 0; e < TH_ELEMS && pos < end && (thd[0] | thd[1] | thd[2] | thd[3] | thd[4] | thd[5] | thd[6] | thd[7] | thd[9]); e++) {
        if (rdat(p, pos, h, 16, base) || !(hl = hdr(h, 16, &id, &sz)) || sz == ~(u64)0) return;
        if (id == 0xa0) { pos += hl; continue; }   /* into the BlockGroup */
        if ((id == 0xa3 || id == 0xa1) && thd[9] && !rdat(p, pos + hl, d, 4, base) && d[0] == (0x80 | thd[9])) {
            thd[9] = 0;
            if (h10_blk(p, pos + hl + 4, sz - 4, base)) m->h10 |= 1;
        }
        if ((id == 0xa3 || id == 0xa1) && !rdat(p, pos + hl, d, sz < sizeof d ? (u32)sz : sizeof d, base))
            thd_blk(d, sz < sizeof d ? (u32)sz : sizeof d, thd, m);
        pos += hl + sz;
    }
}
/* CLpMkvMediaManagerProxy::mrStop, mrUnInit (vtable) and ~ (vtable, PLT): forget it. The proxy outlives a stop. */
CAVE7 void mkv_dtor(u32 x, u32 base) { volatile struct ml *m = ML(base); if (m->proxy == x) m->proxy = 0; }
/* CLpMpcWrapper::vMsgAElmStrmCallback, after mrMapPBACodeMode (the audio the decoder reports; the long-press info
 * panel): decoder type 6 (MLP / TrueHD) gives "MLP". While an MKV plays on this wrapper, its TrueHD track gets the
 * same name as in the menus (mkv_thd: Dolby TrueHD, or Dolby Atmos). Disc MLP (DVD-Audio) keeps its name. */
CAVE7 u32 mkv_thd(u8 *p, u32 idx, u32 base, u32 code);
#define X_WRAPPER 0x80
#define X_MKPARSER  0x114
#define X_AUD       0x1c28
CAVE7 void mkv_aelm(u32 w, u32 *code, u32 dt, u32 base) {
    volatile struct ml *m = ML(base);
    u8 *x = (u8 *)m->proxy;
    u32 c = *code, dd = c == 6 || c == 7 || c == 0x14 || c == 0x18;   /* dd: Dolby Digital Plus */
    if (!(dt == 6 && c == 0xb) && !dd) return;
    if (!x || *(u32 *)(x + X_WRAPPER) != w) return;
    u8 *p = *(u8 **)(x + X_MKPARSER);
    if ((u32)p == m->parser) *code = mkv_thd(p, x[X_AUD], base, dd ? c : 0x1a);
}
/* CLpMkvParser::eMapPBAudioCodec, A_TRUEHD (0x3bac6c) and A_EAC3 (0x3bac44; r5 = parser, r6 = audio record): the
 * code (0x1a, 6), or 8 for Atmos */
CAVE7 u32 mkv_thd(u8 *p, u32 idx, u32 base, u32 code) {
    volatile struct ml *m = ML(base);
    if ((u32)p != m->parser || idx >= 10 || !m->fn_sub) return code;   /* fn_sub 0: not our player program, code 8 is "SDDS" there */
    u32 num = *(u32 *)(*(u8 **)(p + K_TRK) + 400 + idx * 0xa0 + 0x20);
    return num < 128 && m->atmos[num >> 5] >> (num & 31) & 1 ? 8 : code;
}

CAVE8 void mkv_scan(u8 *p, u32 lo, u32 hi, u32 base) {
    volatile struct ml *m = ML(base); volatile struct st *s = ST(base);
    m->parser = 0; m->sel = 0; m->rd = 0; m->nst = 0; m->h10 = 0; m->hits[0]++;
    for (u32 j = 0; j < 4; j++) { m->skip[j] = 0; m->atmos[j] = 0; }
    for (u32 j = 0; j < 32; j++) m->snum[j] = 0;
    if (hi || !lo || lo > MKMAX) return;
    u8 *b = ((alloc_t)(base + VA_ALLOC))(lo);
    if (!b) return;
    u64 pos = *(volatile u64 *)(p + K_POS);
    u32 r = ((mkrd_t)(base + VA_MKVREAD))(p, b, lo);
    *(volatile u64 *)(p + K_POS) = pos; *(volatile u32 *)(p + K_BUFLEFT) = 0;   /* the parser reads from there again */
    struct mt t[MKT]; u32 n = 0, i = 0, id, sz, na = 0, ns = 0;
    if (!r) while (n < MKT && ebml(b, lo, &i, &id, &sz)) {
        if (id == 0xae) {
            trk(b, i + sz, i, &t[n], base);
            if (t[n].type == 2) na++; else if (t[n].type == 0x11) ns++;
            if (t[n].type == 0x11 && t[n].num && m->nst < 36) {
                u32 q = m->nst++; m->st[q].num = t[n].num; m->st[q].fl = t[n].forced | t[n].def << 1 | t[n].thd << 4; m->st[q].key = t[n].key;
            }
            n++;
        }
        i += sz;
    }
    ((free_t)(base + VA_FREE))(b);
    m->rd = r ? 3 : 2;
    if (r) return;
    u32 thd[10], any = 0;                                     /* TrueHD tracks, Dolby Digital Plus tracks; kinds of Atmos found */
    for (u32 k = 0; k < 10; k++) thd[k] = 0;
    for (u32 k = n; k--; ) if (t[k].type == 1 && t[k].thd == 3 && t[k].num) { any = 1; thd[9] = t[k].num; }   /* first HEVC track */
    for (u32 k = 0; k < n; k++) if (t[k].type == 2 && (t[k].thd == 1 || t[k].thd == 2) && t[k].num) { any = 1; thd[(t[k].num >> 5) + (t[k].thd == 2 ? 4 : 0)] |= 1u << (t[k].num & 31); }
    if (any) {
        thd_scan(p, pos + lo, thd, base);
        if (thd[8]) atmos_names(thd[8], base);
        *(volatile u64 *)(p + K_POS) = pos; *(volatile u32 *)(p + K_BUFLEFT) = 0;
    }
    m->na = na; m->ns = ns; m->parser = (u32)p;
    if (!s->pref[0][0]) return;   /* no language list: the parser's own choice */
    for (u32 kind = 2; kind <= 0x11; kind += 0xf) {
        u32 lim = kind == 2 ? 10 : 32;
        if ((kind == 2 ? na : ns) <= lim) continue;
        u8 keep[MKT]; u32 used = 0, d = n;
        for (u32 k = 0; k < n; k++) { keep[k] = 0; if (t[k].type == kind && d == n && (kind != 2 || t[k].def)) d = k; }
        if (kind == 2) {
            if (d == n) for (u32 k = 0; k < n; k++) if (t[k].type == 2) { d = k; break; }
            if (d < n) { keep[d] = 1; used++; }
        }
        for (u32 j = 0; j < 8 && used < lim; j++) {
            u32 a2 = pref_a2(s, j, base);
            if (!a2) continue;
            for (u32 k = 0; k < n && used < lim; k++)
                if (t[k].type == kind && !keep[k] && t[k].key == a2) { keep[k] = 1; used++; }
        }
        for (u32 k = 0; k < n && used < lim; k++)
            if (t[k].type == kind && !keep[k] && !(kind == 0x11 && t[k].forced)) { keep[k] = 1; used++; }
        for (u32 k = 0; k < n; k++)
            if (t[k].type == kind && !keep[k] && t[k].num) { m->skip[t[k].num >> 5] |= 1u << (t[k].num & 31); m->hits[2]++; }
        m->sel = 1;
    }
    if (m->sel) m->hits[1]++;
}

/* mrParseTrackEntry, before an audio (slot 0xff) or subtitle (slot = its record index) record is stored: 0 = leave
 * the track out (the parser then frees it like a track of unknown type). */
CAVE8 u32 mkv_keep(u8 *p, u32 lo, u32 hi, u32 slot, u32 base) {
    volatile struct ml *m = ML(base);
    if ((u32)p != m->parser || hi || lo >= 128) return 1;
    u32 w = lo >> 5, bit = 1u << (lo & 31);
    if (m->skip[w] & bit) return 0;
    if (slot < 32) m->snum[slot] = lo;
    return 1;
}

/* CLpMkvMediaManagerProxy::mrOnStateSettingFileInfo, before mrMkvSetStreamInf: the start tracks are chosen
 * (+0x1c28 audio index, +0x1c29 subtitle 1-based, 0 = off). f = that function's stack frame. */
typedef int (*cfg16_t)(u16 *);
#define SUBLANG_OFF  0x1e4      /* Subtitle Language values: indexes of the 10-byte ISO 639 table (English 0x7e), */
#define SUBLANG_AUTO 0x1e5      /* Off and Auto past its end; OSD language 0x1e6 = Traditional Chinese */
#define X_SUB       0x1c29
CAVE8 void mkv_start(u8 *x, const u8 *f, u32 base) {
    volatile struct ml *m = ML(base);
    u16 v = 0xffff, o = 0xffff;
    m->arm = 0; m->sublang = 0xffffffff; m->proxy = (u32)x;
    if (!m->fn_sub || ((cfg16_t)m->fn_sub)(&v) || ((cfg16_t)m->fn_osd)(&o)) return;
    m->sublang = (u32)v << 16 | o;
    m->arm = v == SUBLANG_AUTO || (m->fopt && v < SUBLANG_OFF);   /* else the player's own choice stays */
}
/* a track record's language (3 letters, none = English) -> 'xy' */
static CAVE7 u32 lkey(const u8 *l, u32 base) { return l[0] ? key3(l, base) : 'e' << 8 | 'n'; }
static CAVE7 u32 zfam(u32 k) { return k == ('z' << 8 | 'h') || k == ('z' << 8 | 's') || k == ('z' << 8 | 't'); }
/* CLpMkvMediaManagerProxy::mrChangeSpStNo(PB_SPSTN_PARAM_T &): +4 subtitle number (1-based), +5 on.
 * Right after a start the player switches a subtitle on: with Subtitle Language = Auto it took the first track. That
 * first switch-on is replaced: the target language is the Subtitle Language, or the player's menu language for Auto;
 * with forced_subs on, the forced track (FlagForced) in that language, else no subtitles; with it off (Auto only),
 * the track in that language with the Default flag, else the first one, else the player's choice. */
CAVE8 void mkv_chg(u8 *x, u8 *prm, u32 base) {
    volatile struct ml *m = ML(base);
    if (m->arm != 1 || !prm[5]) return;
    m->arm = 0x80;
    u8 *p = *(u8 **)(x + X_MKPARSER);
    if ((u32)p != m->parser) return;
    u32 v = m->sublang >> 16, o = m->sublang & 0xffff, want;
    if (v == SUBLANG_AUTO) v = o;
    if (v == 0x1e6) want = 'z' << 8 | 't';
    else if (v < SUBLANG_OFF) { want = key3((const u8 *)(m->tab + v * 10 + 4), base); if (want == ('z' << 8 | 'h')) want = 'z' << 8 | 's'; }
    else return;
    u8 *ti = *(u8 **)(p + K_TRK);
    u32 ns = ti[0x36], best = 0, rank = 0;
    for (u32 j = 0; j < ns && j < 32; j++) {
        u32 k = 0, fl = 0;
        for (u32 q = 0; q < m->nst; q++) if (m->st[q].num == m->snum[j]) { k = m->st[q].key; fl = m->st[q].fl; break; }
        if (!k) k = lkey(ti + 2000 + j * 0x70 + 0x38, base);
        u32 r = k == want ? 4 : zfam(k) && zfam(want) ? 2 : 0;   /* the exact language first, then any Chinese */
        if (!r || (m->fopt && !(fl & 1))) continue;
        if (!m->fopt && fl & 2) r++;
        if (r > rank) { rank = r; best = j + 1; }
    }
    if (best) prm[4] = best;
    else if (m->fopt) { prm[4] = 0; prm[5] = 0; }
    m->arm = 0x80 | best;
}

/* The subtitle menu's rows ("2/5 English", one function of the player program): while an MKV plays, the format and
 * "Forced" (the player's own translated text) behind the language. row = 1-based number, w = the row's text (180). */
CAVE6 void sub_row(u32 row, u16 *w, u32 base, u32 site) {
    volatile struct ml *m = ML(base);
    u32 n = 0;
    while (n < 120 && w[n]) n++;
    w[n++] = ' '; w[n] = 0;                                  /* what the stock code does here */
    if (!m->proxy || !row || row > 32) return;
    u8 *p = *(u8 **)(m->proxy + X_MKPARSER);
    if ((u32)p != m->parser || !m->parser || row > (*(u8 **)(p + K_TRK))[0x36]) return;
    for (u32 q = 0; q < m->nst; q++) if (m->st[q].num == m->snum[row - 1]) {
        u32 f = m->st[q].fl;
        static const char __attribute__((section(".subtext.rodata"))) F[] = "SRT\0ASS\0VTT\0PGS\0VobSub";
        u32 c = f >> 4;
        if (c >= 4 && c <= 8) {
            const char *t = F + base + (c - 4) * 4;
            while (*t) w[n++] = *t++;
            w[n++] = ' ';
        }
        if (f & 1) {                                         /* osdmsg string 1058, in the menu language */
            u8 *lang = *(u8 **)site; u16 **tab = *(u16 ***)(site + 4);
            const u16 *t = tab[1058 * 12 + *lang];
            for (u32 k = 0; t && t[k] && k < 24; k++) w[n++] = t[k];
        }
        w[n] = 0;
        return;
    }
}
