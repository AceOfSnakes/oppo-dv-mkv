#include <stdint.h>
#include <stddef.h>
struct hw_iov { const uint8_t *p; size_t n; };
extern int hw_force_off;
int hw_ok(void);   /* the CPU runs the ARMv8 AES / SHA-256 instructions (Cortex-A53 in 32-bit mode: the kernel does not list them) */
void hw_cmac(const uint8_t key[16], const struct hw_iov *v, int nv, uint8_t mac[16]);
void hw_ccm_crypt(const uint8_t key[16], const uint8_t *nonce, size_t nlen, const uint8_t *aad, size_t alen,
                  uint8_t *p, size_t plen, uint8_t tag[16], int dec);   /* in place; tag: computed, 16 bytes */
void hw_sha256(uint32_t h[8], const uint8_t *p, size_t blocks);
