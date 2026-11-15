// SPDX-License-Identifier: MIT
#include "module_scan.h"
#include <stdio.h>
#include <string.h>

typedef struct { int64_t tag; uint64_t value; } Dynamic;
typedef struct {
    uint32_t name; uint8_t info, other; uint16_t section;
    uint64_t value, size;
} Symbol;
_Static_assert(sizeof(Symbol) == 24, "ELF64 symbol ABI");
_Static_assert(sizeof(Dynamic) == 16, "ELF64 dynamic ABI");

static bool add(uint64_t a, uint64_t b, uint64_t *out) {
    if (UINT64_MAX - a < b) return false;
    *out = a + b; return true;
}
static bool interesting(const char *s) {
    const char *terms[] = {"streamplay", "shareplay", "StreamPlay", "SharePlay",
        "IndividualDisplay", "GetNpad", "lrp", "Lrp", "GameShare"};
    for (size_t i = 0; i < sizeof(terms)/sizeof(terms[0]); i++)
        if (strstr(s, terms[i])) return true;
    return false;
}
static bool stringAt(ProbeRead read, void *ctx, uint64_t at, size_t limit,
                     char *out, size_t capacity) {
    size_t n = limit < capacity ? limit : capacity;
    for (size_t i = 0; i < n; i++) {
        uint64_t address;
        if (!add(at, i, &address) || !read(address, out+i, 1, ctx)) return false;
        if (!out[i]) return true;
    }
    return false;
}

bool probeScanModule(ProbeInventory *inv, uint64_t base, ProbeRead read, void *ctx) {
    uint32_t offset, magic;
    uint64_t mod0, offset_address;
    if (!add(base, 4, &offset_address) || !read(offset_address, &offset, 4, ctx) || offset > 0x10000000 ||
        !add(base, offset, &mod0) || !read(mod0, &magic, 4, ctx) || magic != 0x30444f4d)
        return false;
    if (inv->module_count == PROBE_MAX_MODULES) { inv->module_limit = true; return true; }
    ProbeModule *m = &inv->modules[inv->module_count++];
    m->base = base;
#define FAIL(message) do { snprintf(m->status, sizeof(m->status), "%s", message); return true; } while (0)
    int32_t dynamic_offset;
    if (!add(mod0, 4, &offset_address) || !read(offset_address, &dynamic_offset, 4, ctx) || dynamic_offset < 0 ||
        !add(mod0, (uint32_t)dynamic_offset, &m->dynamic)) FAIL("invalid_dynamic_offset");
    uint64_t strtab = 0, symtab = 0, hash = 0, strsize = 0, syment = 0;
    bool terminated = false;
    for (size_t i = 0; i < 512; i++) {
        Dynamic d;
        if (!add(m->dynamic, i*sizeof(d), &offset_address) ||
            !read(offset_address, &d, sizeof(d), ctx)) FAIL("unreadable_dynamic_table");
        if (!d.tag) { terminated = true; break; }
        switch (d.tag) {
            // Switch MOD0 dynamic pointer tags are offsets relative to module base.
            case 4: if (!add(base, d.value, &hash)) FAIL("hash_overflow"); break;
            case 5: if (!add(base, d.value, &strtab)) FAIL("strtab_overflow"); break;
            case 6: if (!add(base, d.value, &symtab)) FAIL("symtab_overflow"); break;
            case 10: strsize = d.value; break;
            case 11: syment = d.value; break;
        }
    }
    if (!terminated) FAIL("dynamic_table_limit");
    if (!strtab || !symtab || !strsize) FAIL("no_dynamic_symbols");
    if (syment != sizeof(Symbol)) FAIL("unsupported_symbol_size");
    // No inferred symbol count: GNU-only/stripped tables remain explicitly unknown.
    if (!hash) FAIL("no_sysv_hash_count");
    uint32_t hashes[2];
    if (!read(hash, hashes, sizeof(hashes), ctx)) FAIL("unreadable_hash");
    if (hashes[1] > 300000 || strsize > 64*1024*1024) FAIL("table_size_limit");
    m->symbol_count = hashes[1];
    for (uint32_t i = 0; i < m->symbol_count; i++) {
        Symbol s;
        uint64_t address, name_at;
        if (!add(symtab, (uint64_t)i*sizeof(s), &address) ||
            !read(address, &s, sizeof(s), ctx)) FAIL("unreadable_symbol_table");
        if (s.name >= strsize) FAIL("invalid_symbol_name_offset");
        char name[256];
        if (!add(strtab, s.name, &name_at) ||
            !stringAt(read, ctx, name_at, strsize-s.name, name, sizeof(name))) {
            // A long/unreadable name prevents a negative conclusion about this module.
            snprintf(m->status, sizeof(m->status), "partial_symbol_names");
            continue;
        }
        if (!interesting(name)) continue;
        m->matched_count++;
        if (inv->symbol_count == PROBE_MAX_SYMBOLS) { inv->symbol_limit = true; continue; }
        ProbeSymbol *out = &inv->symbols[inv->symbol_count++];
        memcpy(out->name, name, strlen(name)+1);
        out->section = s.section; out->info = s.info; out->size = s.size;
        // An undefined/imported name is never reported as an available function.
        if (s.section && s.value && add(s.section == 0xfff1 ? 0 : base, s.value, &out->address)) {
            uint32_t instruction;
            out->executable = (s.info & 15) == 2 && read(out->address, &instruction, 4, ctx);
        }
    }
    m->symbols_complete = !m->status[0] && !inv->symbol_limit;
    if (!m->status[0]) snprintf(m->status, sizeof(m->status), "%s",
                               m->symbols_complete ? "scanned" : "symbol_output_limit");
    return true;
#undef FAIL
}
