#ifndef OSM_OSND_H
#define OSM_OSND_H

#include "api.h"

static inline void o_outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t o_inb(uint16_t port) {
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* Divide the 1.19318 MHz PIT base into a channel-2 divisor using a 16-bit
 * `div`, so we never pull in libgcc's 32-bit division helpers. */
static inline uint16_t o_pit_divisor(uint16_t freq) {
    uint16_t q;
    __asm__ __volatile__(
        "movw $0x12, %%dx\n\t"
        "movw $0x34DC, %%ax\n\t"
        "divw %1\n\t"
        "movw %%ax, %0"
        : "=r"(q) : "r"(freq) : "ax", "dx"
    );
    return q;
}

/* Start a tone on the PC speaker without blocking, so callers can animate
 * while it sounds. Frequency below 20 Hz (or 0) silences it. */
static inline void o_tone_off(void) {
    o_outb(0x61, (uint8_t)(o_inb(0x61) & 0xFC));
}

static inline void o_tone(uint16_t freq) {
    if (freq < 20) { o_tone_off(); return; }
    uint16_t div = o_pit_divisor(freq);
    o_outb(0x43, 0xB6);
    o_outb(0x42, (uint8_t)(div & 0xFF));
    o_outb(0x42, (uint8_t)(div >> 8));
    o_outb(0x61, (uint8_t)(o_inb(0x61) | 0x03));
}

/* BIOS busy-wait. Blocks for about `ms` milliseconds. */
static inline void o_delay_ms(uint16_t ms) {
    uint32_t us = (uint32_t)ms * 1000u;
    __asm__ __volatile__("movb $0x86, %%ah\n\tint $0x15"
        : : "c"((uint16_t)(us >> 16)), "d"((uint16_t)(us & 0xFFFF)) : "eax");
}

#endif
