#include "../Include/dx_dex.h"
#include "../Include/dx_vm.h"
#include "../Include/dx_log.h"
#include <stdlib.h>
#include <string.h>

#define TAG "DEX"

#include "../Include/dx_memory.h"

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

// Read ULEB128 (unsigned LEB128)
static uint32_t read_uleb128(const uint8_t **pp) {
    const uint8_t *p = *pp;
    uint32_t result = 0;
    int shift = 0;
    uint8_t byte;
    do {
        byte = *p++;
        result |= (uint32_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    *pp = p;
    return result;
}

// Decode MUTF-8 string from DEX string_data section
static char *decode_mutf8(const uint8_t *data, uint32_t offset, uint32_t file_size) {
    if (offset >= file_size) return dx_strdup("");

    const uint8_t *p = data + offset;
    const uint8_t *end = data + file_size;
    // First: ULEB128 utf16_size (number of UTF-16 code units)
    uint32_t utf16_size = read_uleb128(&p);

    // Allocate output buffer (max 3 bytes per UTF-16 code unit + null)
    size_t max_len = (size_t)utf16_size * 3 + 1;
    char *s = (char *)dx_malloc(max_len);
    if (!s) return NULL;

    size_t out = 0;
    while (p < end && out < max_len - 1) {
        uint8_t b0 = *p;
        if (b0 == 0) break; // MUTF-8 null terminator

        if ((b0 & 0x80) == 0) {
            // Single byte: 0xxxxxxx
            s[out++] = (char)b0;
            p++;
        } else if ((b0 & 0xE0) == 0xC0) {
            // Two bytes: 110xxxxx 10xxxxxx
            if (p + 1 >= end) break;
            uint8_t b1 = p[1];
            if (b0 == 0xC0 && b1 == 0x80) {
                // MUTF-8 encoded null (U+0000)
                s[out++] = '\0';
            } else {
                s[out++] = (char)b0;
                s[out++] = (char)b1;
            }
            p += 2;
        } else if ((b0 & 0xF0) == 0xE0) {
            // Three bytes: 1110xxxx 10xxxxxx 10xxxxxx
            if (p + 2 >= end) break;
            s[out++] = (char)b0;
            s[out++] = (char)p[1];
            s[out++] = (char)p[2];
            p += 3;
        } else {
            // Skip invalid byte
            s[out++] = '?';
            p++;
        }
    }
    s[out] = '\0';
    return s;
}

DxResult dx_dex_parse(const uint8_t *data, uint32_t size, DxDexFile **out) {
    if (!data || !out) return DX_ERR_NULL_PTR;
    if (size < sizeof(DxDexHeader)) {
        DX_ERROR(TAG, "File too small: %u bytes", size);
        return DX_ERR_INVALID_FORMAT;
    }

    // Validate magic
    if (memcmp(data, DEX_MAGIC, 4) != 0) {
        DX_ERROR(TAG, "Invalid DEX magic");
        return DX_ERR_INVALID_MAGIC;
    }

    // Check version (035, 037, 038, 039)
    char version[4] = {(char)data[4], (char)data[5], (char)data[6], 0};
    DX_INFO(TAG, "DEX version: %s", version);

    DxDexFile *dex = (DxDexFile *)dx_malloc(sizeof(DxDexFile));
    if (!dex) return DX_ERR_OUT_OF_MEMORY;

    dex->raw_data = data;
    dex->raw_size = size;

    // Parse header
    memcpy(&dex->header, data, sizeof(DxDexHeader));

    DX_INFO(TAG, "DEX: %u strings, %u types, %u protos, %u fields, %u methods, %u classes",
            dex->header.string_ids_size, dex->header.type_ids_size,
            dex->header.proto_ids_size, dex->header.field_ids_size,
            dex->header.method_ids_size, dex->header.class_defs_size);

    // Validate endian tag
    if (dex->header.endian_tag != 0x12345678) {
        DX_WARN(TAG, "Invalid endian_tag: 0x%08x (expected 0x12345678)", dex->header.endian_tag);
        dx_free(dex);
        return DX_ERR_INVALID_FORMAT;
    }

    // Validate file size
    if (dex->header.file_size != size) {
        DX_WARN(TAG, "Header file_size (%u) != actual size (%u)",
                dex->header.file_size, size);
    }

    // Validate table offsets are within file bounds
    if (dex->header.string_ids_size > 0 &&
        (dex->header.string_ids_off >= size ||
         dex->header.string_ids_off + (uint64_t)dex->header.string_ids_size * 4 > size)) {
        DX_WARN(TAG, "string_ids table (off=0x%x, count=%u) out of bounds",
                dex->header.string_ids_off, dex->header.string_ids_size);
        dx_free(dex);
        return DX_ERR_INVALID_FORMAT;
    }
    if (dex->header.type_ids_size > 0 &&
        (dex->header.type_ids_off >= size ||
         dex->header.type_ids_off + (uint64_t)dex->header.type_ids_size * 4 > size)) {
        DX_WARN(TAG, "type_ids table (off=0x%x, count=%u) out of bounds",
                dex->header.type_ids_off, dex->header.type_ids_size);
        dx_free(dex);
        return DX_ERR_INVALID_FORMAT;
    }
    if (dex->header.proto_ids_size > 0 &&
        (dex->header.proto_ids_off >= size ||
         dex->header.proto_ids_off + (uint64_t)dex->header.proto_ids_size * 12 > size)) {
        DX_WARN(TAG, "proto_ids table (off=0x%x, count=%u) out of bounds",
                dex->header.proto_ids_off, dex->header.proto_ids_size);
        dx_free(dex);
        return DX_ERR_INVALID_FORMAT;
    }
    if (dex->header.field_ids_size > 0 &&
        (dex->header.field_ids_off >= size ||
         dex->header.field_ids_off + (uint64_t)dex->header.field_ids_size * 8 > size)) {
        DX_WARN(TAG, "field_ids table (off=0x%x, count=%u) out of bounds",
                dex->header.field_ids_off, dex->header.field_ids_size);
        dx_free(dex);
        return DX_ERR_INVALID_FORMAT;
    }
    if (dex->header.method_ids_size > 0 &&
        (dex->header.method_ids_off >= size ||
         dex->header.method_ids_off + (uint64_t)dex->header.method_ids_size * 8 > size)) {
        DX_WARN(TAG, "method_ids table (off=0x%x, count=%u) out of bounds",
                dex->header.method_ids_off, dex->header.method_ids_size);
        dx_free(dex);
        return DX_ERR_INVALID_FORMAT;
    }
    if (dex->header.class_defs_size > 0 &&
        (dex->header.class_defs_off >= size ||
         dex->header.class_defs_off + (uint64_t)dex->header.class_defs_size * 32 > size)) {
        DX_WARN(TAG, "class_defs table (off=0x%x, count=%u) out of bounds",
                dex->header.class_defs_off, dex->header.class_defs_size);
        dx_free(dex);
        return DX_ERR_INVALID_FORMAT;
    }

    // Parse string table
    dex->string_count = dex->header.string_ids_size;
    dex->strings = (char **)dx_malloc(sizeof(char *) * dex->string_count);
    if (!dex->strings) goto fail;

    for (uint32_t i = 0; i < dex->string_count; i++) {
        uint32_t string_id_off = dex->header.string_ids_off + i * 4;
        if (string_id_off + 4 > size) break;
        uint32_t string_data_off = read_u32(data + string_id_off);
        dex->strings[i] = decode_mutf8(data, string_data_off, size);
    }

    // Parse type IDs
    dex->type_count = dex->header.type_ids_size;
    dex->type_ids = (DxDexTypeId *)dx_malloc(sizeof(DxDexTypeId) * dex->type_count);
    if (!dex->type_ids) goto fail;
    for (uint32_t i = 0; i < dex->type_count; i++) {
        uint32_t off = dex->header.type_ids_off + i * 4;
        if (off + 4 > size) break;
        dex->type_ids[i].descriptor_idx = read_u32(data + off);
    }

    // Parse proto IDs
    dex->proto_count = dex->header.proto_ids_size;
    dex->proto_ids = (DxDexProtoId *)dx_malloc(sizeof(DxDexProtoId) * dex->proto_count);
    if (!dex->proto_ids) goto fail;
    for (uint32_t i = 0; i < dex->proto_count; i++) {
        uint32_t off = dex->header.proto_ids_off + i * 12;
        if (off + 12 > size) break;
        dex->proto_ids[i].shorty_idx = read_u32(data + off);
        dex->proto_ids[i].return_type_idx = read_u32(data + off + 4);
        dex->proto_ids[i].parameters_off = read_u32(data + off + 8);
    }

    // Parse field IDs
    dex->field_count = dex->header.field_ids_size;
    dex->field_ids = (DxDexFieldId *)dx_malloc(sizeof(DxDexFieldId) * dex->field_count);
    if (!dex->field_ids) goto fail;
    for (uint32_t i = 0; i < dex->field_count; i++) {
        uint32_t off = dex->header.field_ids_off + i * 8;
        if (off + 8 > size) break;
        dex->field_ids[i].class_idx = read_u16(data + off);
        dex->field_ids[i].type_idx = read_u16(data + off + 2);
        dex->field_ids[i].name_idx = read_u32(data + off + 4);
    }

    // Parse method IDs
    dex->method_count = dex->header.method_ids_size;
    dex->method_ids = (DxDexMethodId *)dx_malloc(sizeof(DxDexMethodId) * dex->method_count);
    if (!dex->method_ids) goto fail;
    for (uint32_t i = 0; i < dex->method_count; i++) {
        uint32_t off = dex->header.method_ids_off + i * 8;
        if (off + 8 > size) break;
        dex->method_ids[i].class_idx = read_u16(data + off);
        dex->method_ids[i].proto_idx = read_u16(data + off + 2);
        dex->method_ids[i].name_idx = read_u32(data + off + 4);
    }

    // Parse class definitions
    dex->class_count = dex->header.class_defs_size;
    dex->class_defs = (DxDexClassDef *)dx_malloc(sizeof(DxDexClassDef) * dex->class_count);
    if (!dex->class_defs) goto fail;
    for (uint32_t i = 0; i < dex->class_count; i++) {
        uint32_t off = dex->header.class_defs_off + i * 32;
        if (off + 32 > size) break;
        dex->class_defs[i].class_idx = read_u32(data + off);
        dex->class_defs[i].access_flags = read_u32(data + off + 4);
        dex->class_defs[i].superclass_idx = read_u32(data + off + 8);
        dex->class_defs[i].interfaces_off = read_u32(data + off + 12);
        dex->class_defs[i].source_file_idx = read_u32(data + off + 16);
        dex->class_defs[i].annotations_off = read_u32(data + off + 20);
        dex->class_defs[i].class_data_off = read_u32(data + off + 24);
        dex->class_defs[i].static_values_off = read_u32(data + off + 28);
    }

    // Allocate class_data array (lazy parsing)
    dex->class_data = (DxDexClassData **)dx_malloc(sizeof(DxDexClassData *) * dex->class_count);

    DX_INFO(TAG, "DEX parsed successfully");
    *out = dex;
    return DX_OK;

fail:
    dx_dex_free(dex);
    return DX_ERR_OUT_OF_MEMORY;
}

void dx_dex_free(DxDexFile *dex) {
    if (!dex) return;

    for (uint32_t i = 0; i < dex->string_count; i++) {
        dx_free(dex->strings[i]);
    }
    dx_free(dex->strings);
    dx_free(dex->type_ids);
    dx_free(dex->proto_ids);
    dx_free(dex->field_ids);
    dx_free(dex->method_ids);
    dx_free(dex->class_defs);

    if (dex->class_data) {
        for (uint32_t i = 0; i < dex->class_count; i++) {
            if (dex->class_data[i]) {
                dx_free(dex->class_data[i]->static_fields);
                dx_free(dex->class_data[i]->instance_fields);
                dx_free(dex->class_data[i]->direct_methods);
                dx_free(dex->class_data[i]->virtual_methods);
                dx_free(dex->class_data[i]);
            }
        }
        dx_free(dex->class_data);
    }

    dx_free(dex);
}

const char *dx_dex_get_string(const DxDexFile *dex, uint32_t idx) {
    if (!dex || idx >= dex->string_count) return NULL;
    return dex->strings[idx];
}

const char *dx_dex_get_type(const DxDexFile *dex, uint32_t type_idx) {
    if (!dex || type_idx >= dex->type_count) return NULL;
    return dx_dex_get_string(dex, dex->type_ids[type_idx].descriptor_idx);
}

DxResult dx_dex_parse_class_data(DxDexFile *dex, uint32_t class_def_idx) {
    if (!dex || class_def_idx >= dex->class_count) return DX_ERR_NULL_PTR;
    if (dex->class_data[class_def_idx]) return DX_OK; // already parsed

    uint32_t off = dex->class_defs[class_def_idx].class_data_off;
    if (off == 0) {
        // No class data (interface or annotation-only)
        DxDexClassData *cd = (DxDexClassData *)dx_malloc(sizeof(DxDexClassData));
        if (!cd) return DX_ERR_OUT_OF_MEMORY;
        dex->class_data[class_def_idx] = cd;
        return DX_OK;
    }

    if (off >= dex->raw_size) return DX_ERR_INVALID_FORMAT;

    const uint8_t *p = dex->raw_data + off;
    uint32_t static_fields_count = read_uleb128(&p);
    uint32_t instance_fields_count = read_uleb128(&p);
    uint32_t direct_methods_count = read_uleb128(&p);
    uint32_t virtual_methods_count = read_uleb128(&p);

    DxDexClassData *cd = (DxDexClassData *)dx_malloc(sizeof(DxDexClassData));
    if (!cd) return DX_ERR_OUT_OF_MEMORY;

    cd->static_fields_count = static_fields_count;
    cd->instance_fields_count = instance_fields_count;
    cd->direct_methods_count = direct_methods_count;
    cd->virtual_methods_count = virtual_methods_count;

    // Parse static fields
    if (static_fields_count > 0) {
        cd->static_fields = (DxDexEncodedField *)dx_malloc(sizeof(DxDexEncodedField) * static_fields_count);
        if (!cd->static_fields) { dx_free(cd); return DX_ERR_OUT_OF_MEMORY; }
        uint32_t field_idx = 0;
        for (uint32_t i = 0; i < static_fields_count; i++) {
            field_idx += read_uleb128(&p);
            cd->static_fields[i].field_idx = field_idx;
            cd->static_fields[i].access_flags = read_uleb128(&p);
        }
    }

    // Parse instance fields
    if (instance_fields_count > 0) {
        cd->instance_fields = (DxDexEncodedField *)dx_malloc(sizeof(DxDexEncodedField) * instance_fields_count);
        if (!cd->instance_fields) { dx_free(cd->static_fields); dx_free(cd); return DX_ERR_OUT_OF_MEMORY; }
        uint32_t field_idx = 0;
        for (uint32_t i = 0; i < instance_fields_count; i++) {
            field_idx += read_uleb128(&p);
            cd->instance_fields[i].field_idx = field_idx;
            cd->instance_fields[i].access_flags = read_uleb128(&p);
        }
    }

    // Parse direct methods
    if (direct_methods_count > 0) {
        cd->direct_methods = (DxDexEncodedMethod *)dx_malloc(sizeof(DxDexEncodedMethod) * direct_methods_count);
        if (!cd->direct_methods) goto fail_methods;
        uint32_t method_idx = 0;
        for (uint32_t i = 0; i < direct_methods_count; i++) {
            method_idx += read_uleb128(&p);
            cd->direct_methods[i].method_idx = method_idx;
            cd->direct_methods[i].access_flags = read_uleb128(&p);
            cd->direct_methods[i].code_off = read_uleb128(&p);
        }
    }

    // Parse virtual methods
    if (virtual_methods_count > 0) {
        cd->virtual_methods = (DxDexEncodedMethod *)dx_malloc(sizeof(DxDexEncodedMethod) * virtual_methods_count);
        if (!cd->virtual_methods) goto fail_methods;
        uint32_t method_idx = 0;
        for (uint32_t i = 0; i < virtual_methods_count; i++) {
            method_idx += read_uleb128(&p);
            cd->virtual_methods[i].method_idx = method_idx;
            cd->virtual_methods[i].access_flags = read_uleb128(&p);
            cd->virtual_methods[i].code_off = read_uleb128(&p);
        }
    }

    dex->class_data[class_def_idx] = cd;

    DX_DEBUG(TAG, "Class data[%u]: %u sfields, %u ifields, %u dmethods, %u vmethods",
             class_def_idx, static_fields_count, instance_fields_count,
             direct_methods_count, virtual_methods_count);

    return DX_OK;

fail_methods:
    dx_free(cd->static_fields);
    dx_free(cd->instance_fields);
    dx_free(cd->direct_methods);
    dx_free(cd);
    return DX_ERR_OUT_OF_MEMORY;
}

DxResult dx_dex_parse_code_item(const DxDexFile *dex, uint32_t offset, DxDexCodeItem *out) {
    if (!dex || !out || offset == 0) return DX_ERR_NULL_PTR;
    if (offset + 16 > dex->raw_size) return DX_ERR_INVALID_FORMAT;

    const uint8_t *p = dex->raw_data + offset;
    out->registers_size = read_u16(p);
    out->ins_size = read_u16(p + 2);
    out->outs_size = read_u16(p + 4);
    out->tries_size = read_u16(p + 6);
    out->debug_info_off = read_u32(p + 8);
    out->insns_size = read_u32(p + 12);
    out->insns = (uint16_t *)(p + 16);

    // Validate insns_size doesn't exceed remaining file space
    uint32_t remaining = dex->raw_size - (offset + 16);
    if (out->insns_size > remaining / 2) {
        DX_WARN(TAG, "Code item insns_size (%u) exceeds remaining file (%u bytes at offset 0x%x)",
                out->insns_size, remaining, offset);
        return DX_ERR_INVALID_FORMAT;
    }

    // Initialize line table to empty
    out->line_table = NULL;
    out->line_count = 0;

    return DX_OK;
}

// Read signed LEB128
static int32_t read_sleb128(const uint8_t **pp) {
    const uint8_t *p = *pp;
    int32_t result = 0;
    int shift = 0;
    uint8_t byte;
    do {
        byte = *p++;
        result |= (int32_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    // Sign extend
    if (shift < 32 && (byte & 0x40)) {
        result |= -(1 << shift);
    }
    *pp = p;
    return result;
}

DxResult dx_dex_parse_debug_info(const DxDexFile *dex, DxDexCodeItem *code) {
    if (!dex || !code) return DX_ERR_NULL_PTR;
    if (code->debug_info_off == 0 || code->debug_info_off >= dex->raw_size) return DX_OK;

    const uint8_t *p = dex->raw_data + code->debug_info_off;
    const uint8_t *end = dex->raw_data + dex->raw_size;

    // Read line_start and parameters_size
    uint32_t line_start = read_uleb128(&p);
    if (p >= end) return DX_OK;
    uint32_t parameters_size = read_uleb128(&p);
    if (p >= end) return DX_OK;

    // Skip parameter names (each is ULEB128p1: value+1, where 0 means no name)
    for (uint32_t i = 0; i < parameters_size && p < end; i++) {
        read_uleb128(&p); // name_idx + 1
    }

    // Allocate line table (we'll grow up to DX_MAX_LINE_ENTRIES)
    DxLineEntry *entries = (DxLineEntry *)dx_malloc(sizeof(DxLineEntry) * DX_MAX_LINE_ENTRIES);
    if (!entries) return DX_ERR_OUT_OF_MEMORY;

    uint32_t count = 0;
    uint32_t address = 0;
    int32_t line = (int32_t)line_start;

    // Emit the initial position
    if (count < DX_MAX_LINE_ENTRIES) {
        entries[count].address = address;
        entries[count].line = line;
        count++;
    }

    // Process state machine bytecodes
    while (p < end) {
        uint8_t opcode = *p++;

        switch (opcode) {
            case 0x00: // DBG_END_SEQUENCE
                goto done;

            case 0x01: { // DBG_ADVANCE_PC
                if (p >= end) goto done;
                uint32_t addr_diff = read_uleb128(&p);
                address += addr_diff;
                break;
            }

            case 0x02: { // DBG_ADVANCE_LINE
                if (p >= end) goto done;
                int32_t line_diff = read_sleb128(&p);
                line += line_diff;
                break;
            }

            case 0x03: { // DBG_START_LOCAL
                if (p >= end) goto done;
                read_uleb128(&p); // register_num
                if (p >= end) goto done;
                read_uleb128(&p); // name_idx + 1
                if (p >= end) goto done;
                read_uleb128(&p); // type_idx + 1
                break;
            }

            case 0x04: { // DBG_START_LOCAL_EXTENDED
                if (p >= end) goto done;
                read_uleb128(&p); // register_num
                if (p >= end) goto done;
                read_uleb128(&p); // name_idx + 1
                if (p >= end) goto done;
                read_uleb128(&p); // type_idx + 1
                if (p >= end) goto done;
                read_uleb128(&p); // sig_idx + 1
                break;
            }

            case 0x05: // DBG_END_LOCAL
            case 0x06: // DBG_RESTART_LOCAL
                if (p >= end) goto done;
                read_uleb128(&p); // register_num
                break;

            case 0x07: // DBG_SET_PROLOGUE_END
            case 0x08: // DBG_SET_EPILOGUE_BEGIN
                // No arguments
                break;

            case 0x09: // DBG_SET_FILE
                if (p >= end) goto done;
                read_uleb128(&p); // name_idx + 1
                break;

            default: {
                // Special opcode (0x0A - 0xFF)
                int adjusted = opcode - 0x0A;
                line += -4 + (adjusted % 15);
                address += adjusted / 15;

                // Emit a line entry
                if (count < DX_MAX_LINE_ENTRIES) {
                    entries[count].address = address;
                    entries[count].line = line;
                    count++;
                }
                break;
            }
        }
    }

done:
    if (count == 0) {
        dx_free(entries);
        return DX_OK;
    }

    // Shrink allocation to actual size
    if (count < DX_MAX_LINE_ENTRIES) {
        DxLineEntry *shrunk = (DxLineEntry *)dx_malloc(sizeof(DxLineEntry) * count);
        if (shrunk) {
            memcpy(shrunk, entries, sizeof(DxLineEntry) * count);
            dx_free(entries);
            entries = shrunk;
        }
        // If realloc fails, just keep the oversized buffer
    }

    code->line_table = entries;
    code->line_count = count;

    DX_DEBUG(TAG, "Parsed %u line entries (lines %d-%d) from debug_info at 0x%x",
             count, entries[0].line, entries[count - 1].line, code->debug_info_off);

    return DX_OK;
}

int dx_method_get_line(const DxMethod *method, uint32_t pc) {
    if (!method || !method->has_code) return -1;
    const DxDexCodeItem *code = &method->code;
    if (!code->line_table || code->line_count == 0) return -1;

    // Binary search: find the last entry with address <= pc
    int32_t lo = 0;
    int32_t hi = (int32_t)code->line_count - 1;
    int32_t best = -1;

    while (lo <= hi) {
        int32_t mid = lo + (hi - lo) / 2;
        if (code->line_table[mid].address <= pc) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    return best >= 0 ? code->line_table[best].line : -1;
}

void dx_dex_free_code_item(DxDexCodeItem *code) {
    if (!code) return;
    if (code->line_table) {
        dx_free(code->line_table);
        code->line_table = NULL;
        code->line_count = 0;
    }
}

const char *dx_dex_get_method_name(const DxDexFile *dex, uint32_t method_idx) {
    if (!dex || method_idx >= dex->method_count) return NULL;
    return dx_dex_get_string(dex, dex->method_ids[method_idx].name_idx);
}

const char *dx_dex_get_method_class(const DxDexFile *dex, uint32_t method_idx) {
    if (!dex || method_idx >= dex->method_count) return NULL;
    return dx_dex_get_type(dex, dex->method_ids[method_idx].class_idx);
}

const char *dx_dex_get_method_shorty(const DxDexFile *dex, uint32_t method_idx) {
    if (!dex || method_idx >= dex->method_count) return NULL;
    uint16_t proto_idx = dex->method_ids[method_idx].proto_idx;
    if (proto_idx >= dex->proto_count) return NULL;
    return dx_dex_get_string(dex, dex->proto_ids[proto_idx].shorty_idx);
}

const char *dx_dex_get_field_name(const DxDexFile *dex, uint32_t field_idx) {
    if (!dex || field_idx >= dex->field_count) return NULL;
    return dx_dex_get_string(dex, dex->field_ids[field_idx].name_idx);
}

const char *dx_dex_get_field_class(const DxDexFile *dex, uint32_t field_idx) {
    if (!dex || field_idx >= dex->field_count) return NULL;
    return dx_dex_get_type(dex, dex->field_ids[field_idx].class_idx);
}

uint32_t dx_dex_get_method_param_count(const DxDexFile *dex, uint32_t method_idx) {
    if (!dex || method_idx >= dex->method_count) return 0;
    uint16_t proto_idx = dex->method_ids[method_idx].proto_idx;
    if (proto_idx >= dex->proto_count) return 0;
    uint32_t params_off = dex->proto_ids[proto_idx].parameters_off;
    if (params_off == 0) return 0;
    if (params_off + 4 > dex->raw_size) return 0;
    return read_u32(dex->raw_data + params_off);
}

const char *dx_dex_get_method_param_type(const DxDexFile *dex, uint32_t method_idx, uint32_t param_idx) {
    if (!dex || method_idx >= dex->method_count) return NULL;
    uint16_t proto_idx = dex->method_ids[method_idx].proto_idx;
    if (proto_idx >= dex->proto_count) return NULL;
    uint32_t params_off = dex->proto_ids[proto_idx].parameters_off;
    if (params_off == 0) return NULL;
    if (params_off + 4 > dex->raw_size) return NULL;
    uint32_t count = read_u32(dex->raw_data + params_off);
    if (param_idx >= count) return NULL;
    if (params_off + 4 + (param_idx + 1) * 2 > dex->raw_size) return NULL;
    uint16_t type_idx = read_u16(dex->raw_data + params_off + 4 + param_idx * 2);
    return dx_dex_get_type(dex, type_idx);
}

const char *dx_dex_get_method_return_type(const DxDexFile *dex, uint32_t method_idx) {
    if (!dex || method_idx >= dex->method_count) return NULL;
    uint16_t proto_idx = dex->method_ids[method_idx].proto_idx;
    if (proto_idx >= dex->proto_count) return NULL;
    return dx_dex_get_type(dex, dex->proto_ids[proto_idx].return_type_idx);
}

// Read a sign-extended integer of (size) bytes from ptr
static int64_t read_signed(const uint8_t *ptr, uint32_t size) {
    int64_t result = 0;
    for (uint32_t i = 0; i < size; i++) {
        result |= (int64_t)ptr[i] << (i * 8);
    }
    // Sign-extend from the top bit of the last byte
    if (ptr[size - 1] & 0x80) {
        for (uint32_t i = size; i < 8; i++) {
            result |= (int64_t)0xFF << (i * 8);
        }
    }
    return result;
}

// Read a zero-extended integer of (size) bytes from ptr
static uint64_t read_unsigned(const uint8_t *ptr, uint32_t size) {
    uint64_t result = 0;
    for (uint32_t i = 0; i < size; i++) {
        result |= (uint64_t)ptr[i] << (i * 8);
    }
    return result;
}

// ── Annotation parsing ──

// Skip an encoded_value (recursive for arrays/annotations)
static void skip_encoded_value(const uint8_t **pp, const uint8_t *end) {
    if (*pp >= end) return;
    uint8_t type_and_arg = *(*pp)++;
    uint8_t value_type = type_and_arg & 0x1F;
    uint8_t value_arg = (type_and_arg >> 5) & 0x07;

    switch (value_type) {
        case 0x00: // BYTE
            *pp += 1;
            break;
        case 0x02: // SHORT
        case 0x03: // CHAR
        case 0x04: // INT
        case 0x06: // LONG
        case 0x10: // FLOAT
        case 0x11: // DOUBLE
        case 0x17: // STRING
        case 0x18: // TYPE
        case 0x19: // FIELD
        case 0x1a: // METHOD_TYPE (DEX 039+)
        case 0x1b: // METHOD
        case 0x1c: // ENUM
            *pp += (value_arg + 1);
            break;
        case 0x1d: { // ARRAY
            uint32_t arr_size = read_uleb128(pp);
            for (uint32_t a = 0; a < arr_size && *pp < end; a++) {
                skip_encoded_value(pp, end);
            }
            break;
        }
        case 0x1e: // NULL - no data
        case 0x1f: // BOOLEAN - no data (value in arg)
            break;
        case 0x20: { // ANNOTATION (sub-annotation)
            read_uleb128(pp); // type_idx
            uint32_t sub_size = read_uleb128(pp);
            for (uint32_t s = 0; s < sub_size && *pp < end; s++) {
                read_uleb128(pp); // name_idx
                skip_encoded_value(pp, end);
            }
            break;
        }
        default:
            // Unknown type, can't skip reliably
            break;
    }
}

// Parse an annotation_set_item at the given offset, returning type descriptors
static DxResult parse_annotation_set(const DxDexFile *dex, uint32_t set_off,
                                      DxAnnotationEntry **out_entries, uint32_t *out_count) {
    *out_entries = NULL;
    *out_count = 0;

    if (set_off == 0 || set_off + 4 > dex->raw_size) return DX_OK;

    const uint8_t *base = dex->raw_data;
    uint32_t size = read_u32(base + set_off);
    if (size == 0) return DX_OK;
    if (set_off + 4 + (uint64_t)size * 4 > dex->raw_size) return DX_ERR_INVALID_FORMAT;

    DxAnnotationEntry *entries = (DxAnnotationEntry *)dx_malloc(sizeof(DxAnnotationEntry) * size);
    if (!entries) return DX_ERR_OUT_OF_MEMORY;

    uint32_t count = 0;
    for (uint32_t i = 0; i < size; i++) {
        uint32_t ann_off = read_u32(base + set_off + 4 + i * 4);
        if (ann_off == 0 || ann_off >= dex->raw_size) continue;

        const uint8_t *p = base + ann_off;
        const uint8_t *end = base + dex->raw_size;
        if (p >= end) continue;

        uint8_t visibility = *p++;
        // encoded_annotation: type_idx (uleb128), size (uleb128), then elements
        uint32_t type_idx = read_uleb128(&p);
        uint32_t elem_size = read_uleb128(&p);

        // Skip annotation elements (name-value pairs)
        for (uint32_t e = 0; e < elem_size && p < end; e++) {
            read_uleb128(&p); // name_idx
            skip_encoded_value(&p, end);
        }

        const char *type_desc = dx_dex_get_type(dex, type_idx);
        if (type_desc) {
            entries[count].type = type_desc;
            entries[count].visibility = visibility;
            count++;
        }
    }

    if (count == 0) {
        dx_free(entries);
        return DX_OK;
    }

    *out_entries = entries;
    *out_count = count;
    return DX_OK;
}

DxResult dx_dex_parse_annotations(const DxDexFile *dex, uint32_t annotations_off,
                                   DxAnnotationsDirectory *out) {
    if (!dex || !out) return DX_ERR_NULL_PTR;
    memset(out, 0, sizeof(*out));

    if (annotations_off == 0 || annotations_off + 16 > dex->raw_size)
        return DX_OK; // no annotations

    const uint8_t *base = dex->raw_data;
    const uint8_t *p = base + annotations_off;

    uint32_t class_annotations_off = read_u32(p);
    uint32_t fields_size = read_u32(p + 4);
    uint32_t annotated_methods_size = read_u32(p + 8);
    // uint32_t annotated_parameters_size = read_u32(p + 12); // not used yet

    // Validate the directory doesn't exceed file bounds
    uint32_t dir_data_size = 16 + fields_size * 8 + annotated_methods_size * 8;
    if (annotations_off + (uint64_t)dir_data_size > dex->raw_size) {
        DX_WARN(TAG, "annotations_directory_item at 0x%x exceeds file bounds", annotations_off);
        return DX_ERR_INVALID_FORMAT;
    }

    // Parse class-level annotations
    if (class_annotations_off != 0) {
        DxResult res = parse_annotation_set(dex, class_annotations_off,
                                             &out->class_annotations, &out->class_annotation_count);
        if (res != DX_OK) return res;
        DX_DEBUG(TAG, "Parsed %u class annotations", out->class_annotation_count);
    }

    // Skip field_annotations (fields_size entries of 8 bytes each)
    const uint8_t *method_ann_start = p + 16 + fields_size * 8;

    // Parse method annotations
    if (annotated_methods_size > 0) {
        out->method_idxs = (uint32_t *)dx_malloc(sizeof(uint32_t) * annotated_methods_size);
        out->method_annotations = (DxAnnotationEntry **)dx_malloc(sizeof(DxAnnotationEntry *) * annotated_methods_size);
        out->method_annotation_counts = (uint32_t *)dx_malloc(sizeof(uint32_t) * annotated_methods_size);
        if (!out->method_idxs || !out->method_annotations || !out->method_annotation_counts) {
            dx_dex_free_annotations(out);
            return DX_ERR_OUT_OF_MEMORY;
        }
        memset(out->method_annotations, 0, sizeof(DxAnnotationEntry *) * annotated_methods_size);
        memset(out->method_annotation_counts, 0, sizeof(uint32_t) * annotated_methods_size);

        uint32_t valid_count = 0;
        for (uint32_t i = 0; i < annotated_methods_size; i++) {
            const uint8_t *entry = method_ann_start + i * 8;
            uint32_t method_idx = read_u32(entry);
            uint32_t ann_set_off = read_u32(entry + 4);

            DxAnnotationEntry *entries = NULL;
            uint32_t entry_count = 0;
            DxResult res = parse_annotation_set(dex, ann_set_off, &entries, &entry_count);
            if (res != DX_OK) continue;
            if (entry_count == 0) continue;

            out->method_idxs[valid_count] = method_idx;
            out->method_annotations[valid_count] = entries;
            out->method_annotation_counts[valid_count] = entry_count;
            valid_count++;
        }
        out->annotated_method_count = valid_count;
        DX_DEBUG(TAG, "Parsed annotations for %u methods", valid_count);
    }

    return DX_OK;
}

void dx_dex_free_annotations(DxAnnotationsDirectory *dir) {
    if (!dir) return;
    dx_free(dir->class_annotations);
    if (dir->method_annotations) {
        for (uint32_t i = 0; i < dir->annotated_method_count; i++) {
            dx_free(dir->method_annotations[i]);
        }
        dx_free(dir->method_annotations);
    }
    dx_free(dir->method_idxs);
    dx_free(dir->method_annotation_counts);
    memset(dir, 0, sizeof(*dir));
}

DxResult dx_dex_parse_static_values(const DxDexFile *dex, uint32_t offset,
                                     DxValue *out_values, uint32_t max_count) {
    if (!dex || !out_values) return DX_ERR_NULL_PTR;
    if (offset == 0 || offset >= dex->raw_size) return DX_ERR_INVALID_FORMAT;

    const uint8_t *p = dex->raw_data + offset;
    const uint8_t *end = dex->raw_data + dex->raw_size;

    uint32_t size = read_uleb128(&p);
    if (size > max_count) size = max_count;

    DX_DEBUG(TAG, "Parsing %u static values at offset 0x%x", size, offset);

    for (uint32_t i = 0; i < size; i++) {
        if (p >= end) break;

        uint8_t type_and_arg = *p++;
        uint8_t value_type = type_and_arg & 0x1F;
        uint8_t value_arg = (type_and_arg >> 5) & 0x07;
        uint32_t byte_count = value_arg + 1;  // number of bytes for most types

        switch (value_type) {
            case 0x00: { // VALUE_BYTE
                if (p >= end) break;
                out_values[i].tag = DX_VAL_INT;
                out_values[i].i = (int8_t)*p++;
                break;
            }
            case 0x02: { // VALUE_SHORT (sign-extended)
                if (p + byte_count > end) break;
                out_values[i].tag = DX_VAL_INT;
                out_values[i].i = (int32_t)read_signed(p, byte_count);
                p += byte_count;
                break;
            }
            case 0x03: { // VALUE_CHAR (zero-extended)
                if (p + byte_count > end) break;
                out_values[i].tag = DX_VAL_INT;
                out_values[i].i = (int32_t)read_unsigned(p, byte_count);
                p += byte_count;
                break;
            }
            case 0x04: { // VALUE_INT (sign-extended)
                if (p + byte_count > end) break;
                out_values[i].tag = DX_VAL_INT;
                out_values[i].i = (int32_t)read_signed(p, byte_count);
                p += byte_count;
                break;
            }
            case 0x06: { // VALUE_LONG (sign-extended)
                if (p + byte_count > end) break;
                out_values[i].tag = DX_VAL_LONG;
                out_values[i].l = read_signed(p, byte_count);
                p += byte_count;
                break;
            }
            case 0x10: { // VALUE_FLOAT (zero-extended right)
                if (p + byte_count > end) break;
                uint32_t raw = 0;
                for (uint32_t b = 0; b < byte_count; b++) {
                    raw |= (uint32_t)p[b] << ((4 - byte_count + b) * 8);
                }
                out_values[i].tag = DX_VAL_FLOAT;
                memcpy(&out_values[i].f, &raw, sizeof(float));
                p += byte_count;
                break;
            }
            case 0x11: { // VALUE_DOUBLE (zero-extended right)
                if (p + byte_count > end) break;
                uint64_t raw = 0;
                for (uint32_t b = 0; b < byte_count; b++) {
                    raw |= (uint64_t)p[b] << ((8 - byte_count + b) * 8);
                }
                out_values[i].tag = DX_VAL_DOUBLE;
                memcpy(&out_values[i].d, &raw, sizeof(double));
                p += byte_count;
                break;
            }
            case 0x17: { // VALUE_STRING (string_idx)
                if (p + byte_count > end) break;
                uint32_t str_idx = (uint32_t)read_unsigned(p, byte_count);
                out_values[i].tag = DX_VAL_INT;  // store string index as int for now
                out_values[i].i = (int32_t)str_idx;
                // Mark as a string index by using a negative tag convention:
                // The caller should interpret this based on field type context.
                // For now, store the string index - the VM will create string objects as needed.
                p += byte_count;
                DX_DEBUG(TAG, "  static[%u] = string_idx %u", i, str_idx);
                break;
            }
            case 0x18: // VALUE_TYPE (type_idx)
            case 0x19: // VALUE_FIELD (field_idx)
            case 0x1a: // VALUE_METHOD (method_idx)
            case 0x1b: // VALUE_ENUM (field_idx)
            {
                // Index types - store as int
                if (p + byte_count > end) break;
                out_values[i].tag = DX_VAL_INT;
                out_values[i].i = (int32_t)read_unsigned(p, byte_count);
                p += byte_count;
                break;
            }
            case 0x1c: { // VALUE_ARRAY — encoded_array
                // Format: ULEB128 size, then 'size' encoded_values
                // We skip the contained values but must advance p correctly
                if (p >= end) break;
                uint32_t arr_size = 0;
                uint32_t shift = 0;
                while (p < end) {
                    uint8_t b = *p++;
                    arr_size |= (uint32_t)(b & 0x7F) << shift;
                    if ((b & 0x80) == 0) break;
                    shift += 7;
                    if (shift >= 35) break;
                }
                // Skip each element by recursively parsing its encoded_value
                for (uint32_t j = 0; j < arr_size && p < end; j++) {
                    uint8_t elem_header = *p++;
                    uint8_t elem_type = elem_header & 0x1F;
                    uint8_t elem_arg = (elem_header >> 5) & 0x07;
                    if (elem_type == 0x1e || elem_type == 0x1f) {
                        // VALUE_NULL / VALUE_BOOLEAN: no data bytes
                        continue;
                    }
                    if (elem_type == 0x1c || elem_type == 0x1d) {
                        // Nested array/annotation - too complex, bail
                        DX_DEBUG(TAG, "  static[%u] nested VALUE_ARRAY/ANNOTATION, skipping rest", i);
                        out_values[i].tag = DX_VAL_VOID;
                        return DX_OK;
                    }
                    uint32_t elem_bytes = (uint32_t)(elem_arg + 1);
                    if (p + elem_bytes > end) break;
                    p += elem_bytes;
                }
                out_values[i].tag = DX_VAL_VOID; // Array values not stored yet
                DX_DEBUG(TAG, "  static[%u] = VALUE_ARRAY (size=%u, skipped)", i, arr_size);
                break;
            }
            case 0x1d: { // VALUE_ANNOTATION — encoded_annotation
                // Format: ULEB128 type_idx, ULEB128 size, then size (name_idx, encoded_value) pairs
                if (p >= end) break;
                // Skip type_idx ULEB128
                while (p < end && (*p & 0x80)) p++;
                if (p < end) p++;
                // Skip size ULEB128
                uint32_t ann_size = 0;
                uint32_t ann_shift = 0;
                while (p < end) {
                    uint8_t b = *p++;
                    ann_size |= (uint32_t)(b & 0x7F) << ann_shift;
                    if ((b & 0x80) == 0) break;
                    ann_shift += 7;
                    if (ann_shift >= 35) break;
                }
                // Skip each name-value pair
                for (uint32_t j = 0; j < ann_size && p < end; j++) {
                    // Skip name_idx ULEB128
                    while (p < end && (*p & 0x80)) p++;
                    if (p < end) p++;
                    // Skip the encoded_value
                    if (p >= end) break;
                    uint8_t elem_header = *p++;
                    uint8_t elem_type = elem_header & 0x1F;
                    uint8_t elem_arg = (elem_header >> 5) & 0x07;
                    if (elem_type == 0x1e || elem_type == 0x1f) continue;
                    if (elem_type == 0x1c || elem_type == 0x1d) {
                        // Nested - bail
                        out_values[i].tag = DX_VAL_VOID;
                        return DX_OK;
                    }
                    uint32_t elem_bytes = (uint32_t)(elem_arg + 1);
                    if (p + elem_bytes > end) break;
                    p += elem_bytes;
                }
                out_values[i].tag = DX_VAL_VOID;
                DX_DEBUG(TAG, "  static[%u] = VALUE_ANNOTATION (size=%u, skipped)", i, ann_size);
                break;
            }
            case 0x1e: { // VALUE_NULL (no data, value_arg must be 0)
                out_values[i].tag = DX_VAL_OBJ;
                out_values[i].obj = NULL;
                // No bytes to consume
                break;
            }
            case 0x1f: { // VALUE_BOOLEAN (value is in arg: 0=false, 1=true)
                out_values[i].tag = DX_VAL_INT;
                out_values[i].i = value_arg;
                // No bytes to consume
                break;
            }
            default: {
                // Unknown type - skip based on byte_count
                DX_WARN(TAG, "Unknown encoded_value type 0x%02x at static[%u]", value_type, i);
                if (p + byte_count <= end) {
                    p += byte_count;
                }
                out_values[i].tag = DX_VAL_VOID;
                break;
            }
        }
    }

    return DX_OK;
}
