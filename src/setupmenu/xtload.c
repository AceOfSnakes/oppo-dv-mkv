// oppo-dv-mkv Setup "Extras" page: loader stub. Lives in the jailbreak's free .text1 tail at 0x1c07b00 (RWX), after
// the boot message stub (0x1c07800..0x1c07a30). While xtmenu.sh has a request pending, the player program's usleep
// import (GOT) points at xl_usleep: the first thread that sleeps (the RS-232 thread polls every 100 ms) runs the
// requested function once, stores its result and continues into the real usleep. Used twice per boot: xl_mmap (a
// fixed RWX region for the Extras code, which is too big for this tail), then the Extras code's init.
typedef unsigned u32;
struct hdr {
    u32 magic;                      // "XTLD"
    volatile u32 state;             // 0 idle; 1 call requested -> 2 running -> 3 done
    int (*orig)(u32);               // the real usleep
    u32 fn, a0, a1, a2, a3;         // the call
    volatile int ret;
    volatile u32 calls;
    int (*hook)(u32);               // xl_usleep (for the script)
    u32 (*mmap)(u32, u32, u32, u32);// xl_mmap (for the script)
};
int xl_usleep(u32 us);
u32 xl_mmap(u32 addr, u32 len, u32 prot, u32 flags);
__attribute__((section(".hdr"), used)) struct hdr xl = { 0x444c5458, 0, 0, 0, 0, 0, 0, 0, 0, 0, xl_usleep, xl_mmap };

u32 xl_mmap(u32 addr, u32 len, u32 prot, u32 flags)   // mmap2(addr, len, prot, flags, -1, 0)
{
    register u32 r0 __asm__("r0") = addr;
    register u32 r1 __asm__("r1") = len;
    register u32 r2 __asm__("r2") = prot;
    register u32 r3 __asm__("r3") = flags;
    register int r4 __asm__("r4") = -1;
    register u32 r5 __asm__("r5") = 0;
    register u32 r7 __asm__("r7") = 192;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5), "r"(r7) : "memory");
    return r0;
}

int xl_usleep(u32 us)
{
    u32 s = 1;
    if (xl.state == 1 && __atomic_compare_exchange_n(&xl.state, &s, 2, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
        xl.ret = ((int (*)(u32, u32, u32, u32))xl.fn)(xl.a0, xl.a1, xl.a2, xl.a3);
        xl.calls++;
        __atomic_store_n(&xl.state, 3, __ATOMIC_RELEASE);
    }
    return xl.orig(us);
}
