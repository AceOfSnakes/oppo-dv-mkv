#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <arm_neon.h>
#include "hwcrypto.h"
int hw_force_off;
static sigjmp_buf jb;
static void ill(int s) { (void)s; siglongjmp(jb, 1); }
static int probe(void) {
    struct sigaction a, o; int ok = 0;
    memset(&a, 0, sizeof a); a.sa_handler = ill;
    if (sigaction(SIGILL, &a, &o)) return 0;
    if (!sigsetjmp(jb, 1)) {
        uint8_t z[16];
        vst1q_u8(z, vaeseq_u8(vdupq_n_u8(0), vdupq_n_u8(0)));
        volatile uint32_t x = vgetq_lane_u32(vsha256hq_u32(vdupq_n_u32(1), vdupq_n_u32(2), vdupq_n_u32(3)), 0);
        (void)x; ok = z[0] == 0x63;
    }
    sigaction(SIGILL, &o, 0);
    return ok;
}
int hw_ok(void) { static int st = -1; if (st < 0) st = probe(); return st && !hw_force_off; }

typedef struct { uint8x16_t k[11]; } ks;
static uint32_t subw(uint32_t w) {
    uint8x16_t r = vaeseq_u8(vreinterpretq_u8_u32(vdupq_n_u32(w)), vdupq_n_u8(0));   /* all columns equal: ShiftRows changes nothing */
    return vgetq_lane_u32(vreinterpretq_u32_u8(r), 0);
}
static void expand(ks *s, const uint8_t *key) {
    uint32_t w[44]; uint8_t rc = 1;
    memcpy(w, key, 16);
    for (int i = 4; i < 44; i++) {
        uint32_t t = w[i - 1];
        if (!(i & 3)) { t = subw(t >> 8 | t << 24) ^ rc; rc = (uint8_t)(rc << 1 ^ (rc & 0x80 ? 0x1b : 0)); }
        w[i] = w[i - 4] ^ t;
    }
    for (int i = 0; i < 11; i++) s->k[i] = vreinterpretq_u8_u32(vld1q_u32(w + 4 * i));
}
static inline __attribute__((always_inline)) uint8x16_t enc(const ks *s, uint8x16_t b) {
    for (int i = 0; i < 9; i++) b = vaesmcq_u8(vaeseq_u8(b, s->k[i]));
    return veorq_u8(vaeseq_u8(b, s->k[9]), s->k[10]);
}
static void dbl(uint8_t *d) { int c = d[0] >> 7; for (int i = 0; i < 15; i++) d[i] = (uint8_t)(d[i] << 1 | d[i + 1] >> 7); d[15] = (uint8_t)(d[15] << 1 ^ (c ? 0x87 : 0)); }
void hw_cmac(const uint8_t key[16], const struct hw_iov *v, int nv, uint8_t mac[16]) {
    ks s; uint8_t k1[16], buf[16]; size_t bn = 0;       /* buf: last (possibly full) block not yet processed */
    uint8x16_t m = vdupq_n_u8(0);
    expand(&s, key);
    for (int i = 0; i < nv; i++) {
        const uint8_t *p = v[i].p; size_t n = v[i].n;
        while (n) {
            if (bn == 16) { m = enc(&s, veorq_u8(m, vld1q_u8(buf))); bn = 0; }
            if (!bn) while (n > 16) { m = enc(&s, veorq_u8(m, vld1q_u8(p))); p += 16; n -= 16; }
            size_t l = 16 - bn; if (l > n) l = n;
            memcpy(buf + bn, p, l); bn += l; p += l; n -= l;
        }
    }
    vst1q_u8(k1, enc(&s, vdupq_n_u8(0))); dbl(k1);
    if (bn < 16) { buf[bn] = 0x80; memset(buf + bn + 1, 0, 15 - bn); dbl(k1); }
    vst1q_u8(mac, enc(&s, veorq_u8(veorq_u8(m, vld1q_u8(buf)), vld1q_u8(k1))));
}
/* 4 counter blocks at once, and the (serial) CBC-MAC in the same rounds where possible: the A53 runs two NEON pipes */
static inline __attribute__((always_inline)) void enc4(const ks *s, uint8x16_t *b) {
    uint8x16_t b0 = b[0], b1 = b[1], b2 = b[2], b3 = b[3];
    for (int i = 0; i < 9; i++) {
        uint8x16_t k = s->k[i];
        b0 = vaesmcq_u8(vaeseq_u8(b0, k)); b1 = vaesmcq_u8(vaeseq_u8(b1, k));
        b2 = vaesmcq_u8(vaeseq_u8(b2, k)); b3 = vaesmcq_u8(vaeseq_u8(b3, k));
    }
    b[0] = veorq_u8(vaeseq_u8(b0, s->k[9]), s->k[10]); b[1] = veorq_u8(vaeseq_u8(b1, s->k[9]), s->k[10]);
    b[2] = veorq_u8(vaeseq_u8(b2, s->k[9]), s->k[10]); b[3] = veorq_u8(vaeseq_u8(b3, s->k[9]), s->k[10]);
}
void hw_ccm_crypt(const uint8_t key[16], const uint8_t *nonce, size_t nlen, const uint8_t *aad, size_t alen,
                  uint8_t *p, size_t plen, uint8_t tag[16], int dec) {
    ks s; uint8_t b[16], c[16]; uint32_t ctr = 1, L = (uint32_t)(15 - nlen);
    expand(&s, key);
    memset(b, 0, 16); b[0] = (uint8_t)((alen ? 0x40 : 0) | 7 << 3 | (L - 1)); memcpy(b + 1, nonce, nlen);
    for (uint32_t i = 0; i < 4; i++) b[15 - i] = (uint8_t)(plen >> (8 * i));
    uint8x16_t m = enc(&s, vld1q_u8(b));
    if (alen) {                                         /* alen < 0xff00 */
        memset(b, 0, 16); b[0] = (uint8_t)(alen >> 8); b[1] = (uint8_t)alen;
        size_t o = 2;
        for (size_t i = 0; i < alen; i++) { b[o++] = aad[i]; if (o == 16) { m = enc(&s, veorq_u8(m, vld1q_u8(b))); memset(b, 0, 16); o = 0; } }
        if (o) m = enc(&s, veorq_u8(m, vld1q_u8(b)));
    }
    memset(c, 0, 16); c[0] = (uint8_t)(L - 1); memcpy(c + 1, nonce, nlen);
    uint32x4_t cb = vreinterpretq_u32_u8(vld1q_u8(c));
#define CTR(n) vreinterpretq_u8_u32(vsetq_lane_u32(__builtin_bswap32(n), cb, 3))
    while (plen >= 64) {
        uint8x16_t k[4] = { CTR(ctr), CTR(ctr + 1), CTR(ctr + 2), CTR(ctr + 3) };
        ctr += 4;
        enc4(&s, k);
        for (int i = 0; i < 4; i++) {
            uint8x16_t x = vld1q_u8(p + 16 * i), y = veorq_u8(x, k[i]);
            m = enc(&s, veorq_u8(m, dec ? y : x));
            vst1q_u8(p + 16 * i, y);
        }
        p += 64; plen -= 64;
    }
    while (plen) {
        uint8_t x[16] = { 0 }, y[16], kk[16]; size_t l = plen < 16 ? plen : 16;
        memcpy(x, p, l); vst1q_u8(kk, enc(&s, CTR(ctr))); ctr++;
        memset(y, 0, 16); for (size_t i = 0; i < l; i++) y[i] = x[i] ^ kk[i];
        m = enc(&s, veorq_u8(m, vld1q_u8(dec ? y : x)));
        memcpy(p, y, l); p += l; plen -= l;
    }
    vst1q_u8(tag, veorq_u8(m, enc(&s, CTR(0))));
}
static const uint32_t K[64] = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
void hw_sha256(uint32_t h[8], const uint8_t *p, size_t n) {
    uint32x4_t a = vld1q_u32(h), e = vld1q_u32(h + 4);
    while (n--) {
        uint32x4_t w[4], a0 = a, e0 = e;
        for (int i = 0; i < 4; i++) w[i] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(p + 16 * i)));
        for (int i = 0; i < 16; i++) {
            uint32x4_t t = vaddq_u32(w[i & 3], vld1q_u32(K + 4 * i)), o = a;
            a = vsha256hq_u32(a, e, t); e = vsha256h2q_u32(e, o, t);
            if (i < 12) w[i & 3] = vsha256su1q_u32(vsha256su0q_u32(w[i & 3], w[(i + 1) & 3]), w[(i + 2) & 3], w[(i + 3) & 3]);
        }
        a = vaddq_u32(a, a0); e = vaddq_u32(e, e0); p += 64;
    }
    vst1q_u32(h, a); vst1q_u32(h + 4, e);
}
