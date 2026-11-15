// SPDX-License-Identifier: MIT
#include "../source/module_scan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t image[4096];
static const uint64_t base = 0x123000000;
static bool readImage(uint64_t at, void *out, size_t n, void *unused) {
    (void)unused;
    if (at < base || at-base > sizeof(image) || n > sizeof(image)-(at-base)) return false;
    memcpy(out, image+(size_t)(at-base), n); return true;
}
static void u32at(size_t at, uint32_t value) { memcpy(image+at, &value, 4); }
static void u64at(size_t at, uint64_t value) { memcpy(image+at, &value, 8); }
static void fixture(void) {
    memset(image, 0, sizeof(image));
    u32at(4, 0x80); u32at(0x80, 0x30444f4d); u32at(0x84, 0x80);
    u64at(0x100, 4); u64at(0x108, 0x200);
    u64at(0x110, 5); u64at(0x118, 0x400);
    u64at(0x120, 6); u64at(0x128, 0x300);
    u64at(0x130, 10); u64at(0x138, 128);
    u64at(0x140, 11); u64at(0x148, 24);
    u32at(0x200, 1); u32at(0x204, 3);
    // One exported function and one undefined import with the same search prefix.
    const char names[] = "\0_ZN2nn10streamplay10InitializeEv\0_ZN2nn10streamplay8ImportedEv\0";
    memcpy(image+0x400, names, sizeof(names));
    u32at(0x318, 1); image[0x31c] = 0x12; image[0x31e] = 1;
    u64at(0x320, 0x500); u64at(0x328, 16);
    u32at(0x330, 34); image[0x334] = 0x12;
}
typedef struct { uint8_t *bytes; size_t size; } FileImage;
static bool readFileImage(uint64_t at, void *out, size_t n, void *context) {
    FileImage *file = context;
    if (at < base || at-base > file->size || n > file->size-(at-base)) return false;
    memcpy(out, file->bytes+(size_t)(at-base), n); return true;
}
int main(int argc, char **argv) {
    ProbeInventory inv = {0};
    fixture();
    assert(probeScanModule(&inv, base, readImage, NULL));
    assert(inv.module_count == 1 && inv.modules[0].symbols_complete);
    assert(inv.symbol_count == 2);
    assert(inv.symbols[0].address == base+0x500 && inv.symbols[0].section == 1);
    assert(inv.symbols[1].address == 0 && !inv.symbols[1].executable);
    memset(&inv, 0, sizeof(inv)); u64at(0x100, 0x6ffffef5);
    assert(probeScanModule(&inv, base, readImage, NULL));
    assert(!inv.modules[0].symbols_complete && !strcmp(inv.modules[0].status, "no_sysv_hash_count"));
    fixture(); memset(&inv, 0, sizeof(inv)); u32at(0x204, 300001);
    assert(probeScanModule(&inv, base, readImage, NULL));
    assert(!strcmp(inv.modules[0].status, "table_size_limit"));
    fixture(); memset(&inv, 0, sizeof(inv)); u64at(0x128, sizeof(image)-8);
    assert(probeScanModule(&inv, base, readImage, NULL));
    assert(!strcmp(inv.modules[0].status, "unreadable_symbol_table"));
    fixture(); memset(&inv, 0, sizeof(inv)); u32at(0x318, 128);
    assert(probeScanModule(&inv, base, readImage, NULL));
    assert(!strcmp(inv.modules[0].status, "invalid_symbol_name_offset"));
    fixture(); memset(&inv, 0, sizeof(inv)); u64at(0x118, UINT64_MAX);
    assert(probeScanModule(&inv, base, readImage, NULL));
    assert(!strcmp(inv.modules[0].status, "strtab_overflow"));
    memset(&inv, 0, sizeof(inv));
    assert(!probeScanModule(&inv, UINT64_MAX-1, readImage, NULL));
    assert(!inv.module_count);
    puts("module parser: defined/imported symbols, missing hash, bounds and overflow checks passed");
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb"); assert(file);
        assert(!fseek(file, 0, SEEK_END)); long length = ftell(file);
        assert(length > 0 && length < 32*1024*1024); rewind(file);
        FileImage real = {.bytes=malloc((size_t)length), .size=(size_t)length}; assert(real.bytes);
        assert(fread(real.bytes, 1, real.size, file) == real.size); fclose(file);
        memset(&inv, 0, sizeof(inv));
        assert(probeScanModule(&inv, base, readFileImage, &real));
        assert(inv.module_count == 1 && inv.modules[0].symbols_complete);
        printf("actual built NRO: MOD0/dynamic tables parsed; %u symbols, %zu matches\n",
               inv.modules[0].symbol_count, inv.symbol_count);
        free(real.bytes);
    }
    return 0;
}
