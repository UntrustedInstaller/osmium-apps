#ifndef OSM_OSTR_H
#define OSM_OSTR_H

#include "api.h"

static inline uint16_t o_strlen(const char* s) {
    uint16_t n = 0;
    while (s[n]) n++;
    return n;
}

static inline void o_memset(void* dst, int c, uint16_t n) {
    uint8_t* p = (uint8_t*)dst;
    while (n--) *p++ = (uint8_t)c;
}

static inline void o_memcpy(void* dst, const void* src, uint16_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
}

static inline int o_strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

static inline char o_upper(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }
static inline char o_lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

#endif
