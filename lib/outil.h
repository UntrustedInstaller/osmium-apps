// outil.h - tiny shared helpers for animation apps: a BIOS key peek and a
// xorshift random-number generator seeded from the real-time clock.
//
// Everything is `static inline`/`static` so an app that pulls in the whole
// header but uses only one function pays only for what it calls.

#ifndef OSM_OUTIL_H
#define OSM_OUTIL_H

#include "api.h"

/* Nonzero if a key is waiting in the BIOS buffer. Does not consume it. */
static inline int o_kbhit(void) {
    uint8_t v;
    __asm__ __volatile__("movb $0x01, %%ah\n\tint $0x16\n\tsetnz %0"
                         : "=q"(v) : : "eax");
    return v;
}

static uint16_t o_rng_state = 0xBEEF;

/* Seed from the RTC tick counter so each launch differs. */
static inline void o_seed_rng(void) {
    uint16_t cx, dx;
    __asm__ __volatile__("movb $0x00, %%ah\n\tint $0x1a"
                         : "=c"(cx), "=d"(dx) : : "eax");
    o_rng_state = (uint16_t)(dx ^ cx ^ 0xACE1u);
    if (!o_rng_state) o_rng_state = 0xBEEF;
}

static inline uint16_t o_rng(void) {
    o_rng_state ^= (uint16_t)(o_rng_state << 7);
    o_rng_state ^= (uint16_t)(o_rng_state >> 9);
    o_rng_state ^= (uint16_t)(o_rng_state << 8);
    return o_rng_state;
}

/* Uniform value in [0, n). */
static inline uint16_t o_randn(uint16_t n) {
    return n ? (uint16_t)(o_rng() % n) : 0;
}

#endif
