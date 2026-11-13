/* Dolby Vision from MKV for Oppo UDP-203 liblp.so (md5 e39b358c...).
 * Linked at liblp VA 0x6cdc00 (zero padding at end of the RX segment); all liblp
 * addresses below are link-time VAs, made absolute with the runtime base passed in. */
typedef unsigned char u8; typedef unsigned int u32; typedef unsigned long long u64;

#define ST_VA          0x757e00u   /* RW bss slack (bss ends 0x757cd4, page ends 0x758000) */
#define VA_GETLEN      0x3b4014u   /* CLpMkvParser::u8GetLength(uchar*) */
#define VA_LOADBYTE    0x3b3dd8u   /* CLpMkvParser::ucLoadByte() */
#define P_POS          0x21f8      /* parser: u64 file position */
#define P_LEFT         0x2944      /* parser: bytes left in read buffer */
#define P_MINFO        0x2e00      /* parser: media info (video fields) */
#define MI_HEVC        0x105       /* media info: 1 = HEVC codec private parsed */
#define MI_CPLEN       0xb0        /* media info: CodecPrivate length */
#define FOURCC(a,b,c,d) (((u32)(a)<<24)|((u32)(b)<<16)|((u32)(c)<<8)|(u32)(d))

struct st {
    u32 parser;      /* parser that produced rec */
    u8 have, active, profile, level;
    u8 rec[24];      /* dvcC/dvvC record */
    u8 mmed[0x1c];   /* MPC_DOVI_INFO_T handed to mrSetMediaInfo */
    u32 hits;        /* debug counters */
    u8 hvcc;         /* 1 = splitter fix on (hvccfix.sh): DV also for a 23-byte hvcC (no VPS/SPS/PPS) */
};
#define ST(base) ((volatile struct st *)((base) + ST_VA))

typedef u64 (*getlen_t)(void *, u8 *);
typedef u32 (*loadbyte_t)(void *);

static u32 rd_vint(const u8 *b, u32 n, u32 *i, int keep_marker) {
    if (*i >= n) return 0xffffffffu;
    u8 c = b[*i]; u32 len = 1, m = 0x80;
    while (len <= 4 && !(c & m)) { m >>= 1; len++; }
    if (len > 4 || *i + len > n) return 0xffffffffu;
    u32 v = keep_marker ? c : (c & (m - 1));
    for (u32 k = 1; k < len; k++) v = (v << 8) | b[*i + k];
    *i += len;
    return v;
}

/* H0: start of Tracks parsing -> forget any previous record */
void dv_clear(u32 base) { ST(base)->have = 0; }

/* H1: TrackEntry child about to be skipped; peek into BlockAdditionMapping (0x41E4) */
void dv_peek(u8 *p, u32 base) {
    volatile struct st *s = ST(base);
    u64 pos = *(u64 *)(p + P_POS); u32 left = *(u32 *)(p + P_LEFT);
    u8 ls = 0, buf[128];
    u64 len = ((getlen_t)(base + VA_GETLEN))(p, &ls);
    if (len < sizeof buf) {
        u32 n = (u32)len, i = 0, type = 0, got = 0; u8 rec[24];
        loadbyte_t lb = (loadbyte_t)(base + VA_LOADBYTE);
        for (u32 k = 0; k < n; k++) buf[k] = (u8)lb(p);
        while (i < n) {
            u32 id = rd_vint(buf, n, &i, 1), sz = rd_vint(buf, n, &i, 0);
            if (id == 0xffffffffu || sz == 0xffffffffu || i + sz > n) break;
            if (id == 0x41e7 && sz <= 4) { type = 0; for (u32 k = 0; k < sz; k++) type = (type << 8) | buf[i + k]; }
            if (id == 0x41ed && sz >= 4) { for (u32 k = 0; k < 24; k++) rec[k] = k < sz ? buf[i + k] : 0; got = 1; }
            i += sz;
        }
        /* only the Dolby Vision record: another mapping of the track (mkvmerge writes the EL's 'hvcE' after 'dvcC')
         * must not replace it */
        if (got && (type == FOURCC('d','v','c','C') || type == FOURCC('d','v','v','C'))) {
            for (u32 k = 0; k < 24; k++) s->rec[k] = rec[k];
            s->parser = (u32)p; s->have = 1;
        }
    }
    *(u64 *)(p + P_POS) = pos; *(u32 *)(p + P_LEFT) = left;
}

/* H2: mrMkvSetFileInf -> mrSetMediaInfo; return DOVI info pointer or 0 (stock) */
u32 dv_decide(u8 *proxy, u32 base) {
    volatile struct st *s = ST(base);
    u8 *p = *(u8 **)(proxy + 0x114);
    s->active = 0;
    s->hits++;
    if (!s->have || s->parser != (u32)p) return 0;
    u8 prof = s->rec[2] >> 1, lvl = ((s->rec[2] & 1) << 5) | (s->rec[3] >> 3);
    u8 rpu = (s->rec[3] >> 2) & 1, el = (s->rec[3] >> 1) & 1, bl = s->rec[3] & 1;
    u8 *mi = *(u8 **)(p + P_MINFO);
    if (!mi || mi[MI_HEVC] != 1) return 0;
    u32 cpl = *(u32 *)(mi + MI_CPLEN);   /* 23 = hvcC header only, VPS/SPS/PPS just in the keyframes (hev1 MP4 remux): */
    if (cpl < 23 || (cpl == 23 && !s->hvcc)) return 0;   /* stock splitter + DV hard-locks; fine with hvccfix.sh */
    if (!rpu || !bl) return 0;
    if (!(((prof == 5 || prof == 8) && !el) || (prof == 7 && el))) return 0; /* single layer, or P7 BL+EL in one track */
    for (u32 k = 0; k < sizeof s->mmed; k++) s->mmed[k] = 0;
    s->mmed[0] = 1; s->mmed[1] = 1; s->mmed[3] = 1; s->mmed[4] = el; s->mmed[5] = 1; /* dovi, single, BL, EL, RPU */
    s->rec[23] = el;
    s->profile = prof; s->level = lvl; s->active = 1;
    return base + ST_VA + __builtin_offsetof(struct st, mmed);
}

/* H3: mrMkvSetVideoStreamInf -> mrSetStreamInfo; vidinfo DV fields */
void dv_vinfo(u8 *v, u32 base) {
    volatile struct st *s = ST(base);
    if (!s->active || !v) return;
    v[0x124] = 1; v[0x125] = 1; v[0x126] = 0; v[0x127] = 1; v[0x128] = s->rec[23]; v[0x129] = 1;
    v[0x12c] = s->profile; v[0x12d] = s->level;
}
