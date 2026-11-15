// SPDX-License-Identifier: MIT
// Freestanding helpers for compiler-generated structure copies/zeroing.
#include <stddef.h>
void *memset(void *dest, int value, size_t size) {
    unsigned char *p=dest; while (size--) *p++=(unsigned char)value; return dest;
}
void *memcpy(void *dest, const void *src, size_t size) {
    unsigned char *d=dest; const unsigned char *s=src; while (size--) *d++=*s++; return dest;
}
void *memmove(void *dest, const void *src, size_t size) {
    unsigned char *d=dest; const unsigned char *s=src;
    if (d<s) return memcpy(dest,src,size);
    while (size) { size--; d[size]=s[size]; } return dest;
}
size_t strlen(const char *text) { size_t n=0; while (text[n]) n++; return n; }
size_t strnlen(const char *text, size_t limit) { size_t n=0; while (n<limit && text[n]) n++; return n; }
int memcmp(const void *left, const void *right, size_t size) {
    const unsigned char *a=left,*b=right;
    while (size--) { if (*a!=*b) return (int)*a-(int)*b; a++; b++; } return 0;
}
int strcmp(const char *left, const char *right) {
    while (*left && *left==*right) { left++; right++; }
    return (unsigned char)*left-(unsigned char)*right;
}
