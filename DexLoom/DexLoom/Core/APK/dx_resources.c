#include "../Include/dx_resources.h"
#include "../Include/dx_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "Resources"

#include "../Include/dx_memory.h"

// resources.arsc chunk types
#define RES_TABLE_TYPE           0x0002
#define RES_STRING_POOL_TYPE     0x0001
#define RES_TABLE_PACKAGE_TYPE   0x0200
#define RES_TABLE_TYPE_TYPE      0x0201
#define RES_TABLE_TYPE_SPEC_TYPE 0x0202

// Resource value types (Android format)
#define RES_VALUE_TYPE_NULL      0x00
#define RES_VALUE_TYPE_REF       0x01
#define RES_VALUE_TYPE_STRING    0x03
#define RES_VALUE_TYPE_FLOAT     0x04
#define RES_VALUE_TYPE_DIMENSION 0x05
#define RES_VALUE_TYPE_FRACTION  0x06
#define RES_VALUE_TYPE_INT_DEC   0x10
#define RES_VALUE_TYPE_INT_HEX   0x11
#define RES_VALUE_TYPE_INT_BOOL  0x12
#define RES_VALUE_TYPE_INT_COLOR_ARGB8 0x1c
#define RES_VALUE_TYPE_INT_COLOR_RGB8  0x1d
#define RES_VALUE_TYPE_INT_COLOR_ARGB4 0x1e
#define RES_VALUE_TYPE_INT_COLOR_RGB4  0x1f

#define INITIAL_ENTRY_CAPACITY 256

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

// Decode a string from a string pool (UTF-8 or UTF-16)
static char *decode_res_string(const uint8_t *pool_data, uint32_t pool_size,
                                uint32_t offset, bool is_utf8) {
    if (offset >= pool_size) return dx_strdup("");
    const uint8_t *p = pool_data + offset;

    if (is_utf8) {
        uint32_t char_count = *p++;
        if (char_count > 0x7F) {
            char_count = ((char_count & 0x7F) << 8) | *p++;
        }
        uint32_t byte_count = *p++;
        if (byte_count > 0x7F) {
            byte_count = ((byte_count & 0x7F) << 8) | *p++;
        }
        char *s = (char *)dx_malloc(byte_count + 1);
        if (!s) return NULL;
        memcpy(s, p, byte_count);
        s[byte_count] = '\0';
        return s;
    } else {
        uint16_t char_count = read_u16(p);
        p += 2;
        char *s = (char *)dx_malloc(char_count + 1);
        if (!s) return NULL;
        for (uint32_t i = 0; i < char_count; i++) {
            uint16_t c = read_u16(p + i * 2);
            s[i] = (c < 128) ? (char)c : '?';
        }
        s[char_count] = '\0';
        return s;
    }
}

// Parse a string pool chunk, returning the decoded strings array
static DxResult parse_string_pool(const uint8_t *data, uint32_t size, uint32_t pos,
                                    char ***out_strings, uint32_t *out_count) {
    if (pos + 28 > size) return DX_ERR_INVALID_FORMAT;

    uint32_t string_count = read_u32(data + pos + 8);
    uint32_t flags = read_u32(data + pos + 16);
    uint32_t strings_start = read_u32(data + pos + 20);
    bool is_utf8 = (flags & (1 << 8)) != 0;

    char **strings = (char **)dx_malloc(sizeof(char *) * (string_count + 1));
    if (!strings) return DX_ERR_OUT_OF_MEMORY;

    uint32_t offsets_start = pos + 28;
    uint32_t pool_data_start = pos + strings_start;

    for (uint32_t i = 0; i < string_count; i++) {
        if (offsets_start + i * 4 + 4 > size) break;
        uint32_t str_offset = read_u32(data + offsets_start + i * 4);
        if (pool_data_start + str_offset < size) {
            strings[i] = decode_res_string(
                data + pool_data_start,
                size - pool_data_start,
                str_offset,
                is_utf8
            );
        } else {
            strings[i] = dx_strdup("");
        }
    }

    *out_strings = strings;
    *out_count = string_count;
    return DX_OK;
}

// Decode Android's dimension value format
float dx_resources_decode_dimen(uint32_t raw_data, uint8_t *out_unit) {
    // Low 4 bits = unit, next 4 bits = radix, top 24 bits = mantissa
    uint8_t unit = raw_data & 0x0F;
    uint8_t radix = (raw_data >> 4) & 0x03;
    int32_t mantissa = (int32_t)(raw_data & 0xFFFFFF00);

    // Android uses fixed-point with different radix positions
    static const float radix_mults[] = {
        1.0f / (1 << 0),   // 23p0
        1.0f / (1 << 7),   // 16p7
        1.0f / (1 << 15),  // 8p15
        1.0f / (1 << 23),  // 0p23
    };

    float value = (float)mantissa * radix_mults[radix];

    if (out_unit) *out_unit = unit;
    return value;
}

// Expand color formats to full ARGB8
static uint32_t normalize_color(uint8_t val_type, uint32_t raw) {
    switch (val_type) {
        case RES_VALUE_TYPE_INT_COLOR_ARGB8:
            return raw;
        case RES_VALUE_TYPE_INT_COLOR_RGB8:
            return 0xFF000000 | (raw & 0x00FFFFFF);
        case RES_VALUE_TYPE_INT_COLOR_ARGB4: {
            uint8_t a = (raw >> 12) & 0xF;
            uint8_t r = (raw >>  8) & 0xF;
            uint8_t g = (raw >>  4) & 0xF;
            uint8_t b = (raw >>  0) & 0xF;
            return ((uint32_t)(a | (a << 4)) << 24) |
                   ((uint32_t)(r | (r << 4)) << 16) |
                   ((uint32_t)(g | (g << 4)) <<  8) |
                   ((uint32_t)(b | (b << 4)));
        }
        case RES_VALUE_TYPE_INT_COLOR_RGB4: {
            uint8_t r = (raw >> 8) & 0xF;
            uint8_t g = (raw >> 4) & 0xF;
            uint8_t b = (raw >> 0) & 0xF;
            return 0xFF000000 |
                   ((uint32_t)(r | (r << 4)) << 16) |
                   ((uint32_t)(g | (g << 4)) <<  8) |
                   ((uint32_t)(b | (b << 4)));
        }
        default:
            return raw;
    }
}

// Add a resource entry to the table, growing if needed
static bool add_resource_entry(DxResources *res, const DxResourceEntry *entry) {
    if (res->entry_count >= res->entry_capacity) {
        uint32_t new_cap = res->entry_capacity == 0 ? INITIAL_ENTRY_CAPACITY : res->entry_capacity * 2;
        DxResourceEntry *new_entries = (DxResourceEntry *)dx_realloc(
            res->entries, sizeof(DxResourceEntry) * new_cap);
        if (!new_entries) return false;
        res->entries = new_entries;
        res->entry_capacity = new_cap;
    }
    res->entries[res->entry_count++] = *entry;
    return true;
}

// Decode a float from raw uint32 bits
static float decode_float(uint32_t raw) {
    union { uint32_t u; float f; } conv;
    conv.u = raw;
    return conv.f;
}

DxResult dx_resources_parse(const uint8_t *data, uint32_t size, DxResources **out) {
    if (!data || !out) return DX_ERR_NULL_PTR;
    if (size < 12) return DX_ERR_INVALID_FORMAT;

    uint16_t type = read_u16(data);
    if (type != RES_TABLE_TYPE) {
        DX_ERROR(TAG, "Invalid resource table type: 0x%04x", type);
        return DX_ERR_INVALID_FORMAT;
    }

    DxResources *res = (DxResources *)dx_malloc(sizeof(DxResources));
    if (!res) return DX_ERR_OUT_OF_MEMORY;

    // Parse global string pool (first chunk after 12-byte table header)
    uint32_t pos = 12;
    if (pos + 8 > size) goto fail;

    uint16_t sp_type = read_u16(data + pos);
    uint32_t sp_size = read_u32(data + pos + 4);

    if (sp_type == RES_STRING_POOL_TYPE && pos + sp_size <= size) {
        DxResult pr = parse_string_pool(data, size, pos, &res->strings, &res->string_count);
        if (pr != DX_OK) goto fail;
        DX_INFO(TAG, "Global string pool: %u strings", res->string_count);
        pos += sp_size;
    }

    // Parse package chunks to extract resource type entries
    while (pos + 8 <= size) {
        uint16_t chunk_type = read_u16(data + pos);
        uint32_t chunk_size = read_u32(data + pos + 4);
        if (chunk_size < 8 || pos + chunk_size > size) break;

        if (chunk_type == RES_TABLE_PACKAGE_TYPE) {
            // Package chunk header: read actual header size
            uint16_t pkg_header_size = read_u16(data + pos + 2);
            if (pkg_header_size < 44 || pos + pkg_header_size > size) {
                pos += chunk_size; continue;
            }

            uint32_t pkg_id = read_u32(data + pos + 8);
            // Package name is at offset 12, 128 UTF-16 chars - skip for now
            uint32_t type_strings_off = read_u32(data + pos + 268);
            uint32_t key_strings_off = read_u32(data + pos + 276);

            // Parse key string pool (contains field/resource names)
            char **key_strings = NULL;
            uint32_t key_count = 0;
            uint32_t key_pool_pos = pos + key_strings_off;
            if (key_pool_pos + 28 <= size &&
                read_u16(data + key_pool_pos) == RES_STRING_POOL_TYPE) {
                parse_string_pool(data, size, key_pool_pos, &key_strings, &key_count);
            }

            // Parse type string pool (contains type names like "string", "layout", etc.)
            char **type_strings = NULL;
            uint32_t type_str_count = 0;
            uint32_t type_pool_pos = pos + type_strings_off;
            if (type_pool_pos + 28 <= size &&
                read_u16(data + type_pool_pos) == RES_STRING_POOL_TYPE) {
                parse_string_pool(data, size, type_pool_pos, &type_strings, &type_str_count);
            }

            DX_DEBUG(TAG, "Package 0x%02x: header=%u, typeStrOff=%u, keyStrOff=%u, typeStrs=%u, keyStrs=%u",
                     pkg_id, pkg_header_size, type_strings_off, key_strings_off,
                     type_str_count, key_count);

            // Walk ALL sub-chunks within the package sequentially.
            // Start right after the package header and iterate through all chunks.
            uint32_t sub_pos = pos + pkg_header_size;
            // Some packages have header_size pointing past the string pools;
            // use the minimum of header_size and type_strings_off as the start.
            if (type_strings_off > 0 && pos + type_strings_off < sub_pos) {
                sub_pos = pos + type_strings_off;
            }

            while (sub_pos + 8 <= pos + chunk_size && sub_pos + 8 <= size) {
                uint16_t sub_type = read_u16(data + sub_pos);
                uint32_t sub_size = read_u32(data + sub_pos + 4);
                if (sub_size < 8 || sub_pos + sub_size > size) break;

                if (sub_type == RES_TABLE_TYPE_TYPE) {
                    // ResTable_type: extract entries
                    if (sub_pos + 76 > size) { sub_pos += sub_size; continue; }

                    uint8_t type_id = data[sub_pos + 8]; // 1-based
                    uint32_t entry_count = read_u32(data + sub_pos + 48);
                    uint32_t entries_start = read_u32(data + sub_pos + 52);

                    const char *type_name_str = "";
                    if (type_strings && type_id > 0 && (type_id - 1) < type_str_count) {
                        type_name_str = type_strings[type_id - 1];
                    }

                    bool is_string_type = (strcmp(type_name_str, "string") == 0);
                    bool is_layout_type = (strcmp(type_name_str, "layout") == 0);

                    // Offset array starts at sub_pos + header_size (varies, typically 76 or 84)
                    uint16_t header_size = read_u16(data + sub_pos + 2);
                    uint32_t offsets_base = sub_pos + header_size;
                    uint32_t entries_base = sub_pos + entries_start;

                    for (uint32_t e = 0; e < entry_count; e++) {
                        if (offsets_base + e * 4 + 4 > size) break;
                        uint32_t entry_off = read_u32(data + offsets_base + e * 4);
                        if (entry_off == 0xFFFFFFFF) continue; // NO_ENTRY

                        uint32_t entry_pos = entries_base + entry_off;
                        if (entry_pos + 8 > size) continue;

                        uint16_t entry_size = read_u16(data + entry_pos);
                        uint16_t entry_flags = read_u16(data + entry_pos + 2);
                        uint32_t key_idx = read_u32(data + entry_pos + 4);
                        (void)entry_flags;

                        if (entry_pos + entry_size + 8 > size) continue;

                        // Value is at entry_pos + 8 (after ResTable_entry header)
                        uint32_t val_pos = entry_pos + 8;
                        if (val_pos + 8 > size) continue;

                        uint8_t val_type = data[val_pos + 3];
                        uint32_t val_data = read_u32(data + val_pos + 4);

                        uint32_t res_id = (pkg_id << 24) | ((uint32_t)type_id << 16) | e;

                        // Get the key name for this entry
                        const char *key_name = (key_strings && key_idx < key_count) ?
                                                key_strings[key_idx] : NULL;

                        // --- Populate the general resource entry table ---
                        DxResourceEntry re;
                        memset(&re, 0, sizeof(re));
                        re.id = res_id;
                        re.value_type = val_type;
                        re.entry_name = key_name ? dx_strdup(key_name) : NULL;
                        re.type_name = dx_strdup(type_name_str);

                        switch (val_type) {
                            case RES_VALUE_TYPE_STRING:
                                if (val_data < res->string_count && res->strings) {
                                    re.str_val = dx_strdup(res->strings[val_data]);
                                } else {
                                    re.str_val = dx_strdup("");
                                }
                                break;

                            case RES_VALUE_TYPE_INT_DEC:
                            case RES_VALUE_TYPE_INT_HEX:
                                re.int_val = (int32_t)val_data;
                                break;

                            case RES_VALUE_TYPE_INT_BOOL:
                                re.bool_val = (val_data != 0);
                                break;

                            case RES_VALUE_TYPE_FLOAT:
                                re.float_val = decode_float(val_data);
                                break;

                            case RES_VALUE_TYPE_DIMENSION: {
                                uint8_t unit = 0;
                                float dim_val = dx_resources_decode_dimen(val_data, &unit);
                                re.dimen.value = dim_val;
                                re.dimen.unit = unit;
                                break;
                            }

                            case RES_VALUE_TYPE_INT_COLOR_ARGB8:
                            case RES_VALUE_TYPE_INT_COLOR_RGB8:
                            case RES_VALUE_TYPE_INT_COLOR_ARGB4:
                            case RES_VALUE_TYPE_INT_COLOR_RGB4:
                                re.color_val = normalize_color(val_type, val_data);
                                break;

                            case RES_VALUE_TYPE_REF:
                                re.ref_id = val_data;
                                break;

                            default:
                                // Store raw int value for unknown types
                                re.int_val = (int32_t)val_data;
                                break;
                        }

                        add_resource_entry(res, &re);

                        // --- Also populate legacy string_entries / layout_entries ---
                        if (is_string_type && val_type == RES_VALUE_TYPE_STRING) {
                            // String resource - val_data is index into global string pool
                            uint32_t idx = res->string_entry_count++;
                            res->string_entries = (typeof(res->string_entries))dx_realloc(
                                res->string_entries,
                                sizeof(*res->string_entries) * res->string_entry_count);
                            if (res->string_entries) {
                                res->string_entries[idx].id = res_id;
                                if (val_data < res->string_count && res->strings) {
                                    res->string_entries[idx].value = dx_strdup(res->strings[val_data]);
                                } else {
                                    res->string_entries[idx].value = dx_strdup("");
                                }
                                DX_TRACE(TAG, "String 0x%08x (%s) = \"%s\"",
                                         res_id, key_name ? key_name : "?",
                                         res->string_entries[idx].value);
                            }
                        } else if (is_layout_type && val_type == RES_VALUE_TYPE_STRING) {
                            // Layout resource - val_data is index into global string pool
                            // Validate the filename looks like a path (contains '/' or '.xml')
                            const char *fn = NULL;
                            if (val_data < res->string_count && res->strings) {
                                fn = res->strings[val_data];
                            }
                            if (fn && (strchr(fn, '/') != NULL || strstr(fn, ".xml") != NULL)) {
                                uint32_t idx = res->layout_entry_count++;
                                res->layout_entries = (typeof(res->layout_entries))dx_realloc(
                                    res->layout_entries,
                                    sizeof(*res->layout_entries) * res->layout_entry_count);
                                if (res->layout_entries) {
                                    res->layout_entries[idx].id = res_id;
                                    res->layout_entries[idx].filename = dx_strdup(fn);
                                    DX_DEBUG(TAG, "Layout 0x%08x = %s",
                                             res_id, res->layout_entries[idx].filename);
                                }
                            }
                        }
                    }
                }

                sub_pos += sub_size;
            }

            // Free local string pools
            if (key_strings) {
                for (uint32_t i = 0; i < key_count; i++) dx_free(key_strings[i]);
                dx_free(key_strings);
            }
            if (type_strings) {
                for (uint32_t i = 0; i < type_str_count; i++) dx_free(type_strings[i]);
                dx_free(type_strings);
            }
        }

        pos += chunk_size;
    }

    DX_INFO(TAG, "Resources parsed: %u strings, %u string entries, %u layout entries, %u total entries",
            res->string_count, res->string_entry_count, res->layout_entry_count, res->entry_count);
    *out = res;
    return DX_OK;

fail:
    dx_resources_free(res);
    return DX_ERR_INVALID_FORMAT;
}

void dx_resources_free(DxResources *res) {
    if (!res) return;
    for (uint32_t i = 0; i < res->string_count; i++) {
        dx_free(res->strings[i]);
    }
    dx_free(res->strings);
    for (uint32_t i = 0; i < res->string_entry_count; i++) {
        dx_free(res->string_entries[i].value);
    }
    dx_free(res->string_entries);
    for (uint32_t i = 0; i < res->layout_entry_count; i++) {
        dx_free(res->layout_entries[i].filename);
    }
    dx_free(res->layout_entries);

    // Free general entry table
    for (uint32_t i = 0; i < res->entry_count; i++) {
        DxResourceEntry *e = &res->entries[i];
        dx_free(e->entry_name);
        dx_free(e->type_name);
        // Free string values
        if (e->value_type == RES_VALUE_TYPE_STRING) {
            dx_free(e->str_val);
        }
    }
    dx_free(res->entries);

    dx_free(res);
}

const char *dx_resources_get_string(const DxResources *res, uint32_t id) {
    if (!res) return NULL;
    // Search indexed entries first (proper resource ID -> value mapping)
    for (uint32_t i = 0; i < res->string_entry_count; i++) {
        if (res->string_entries[i].id == id) {
            return res->string_entries[i].value;
        }
    }
    // Fallback: use entry index into global string pool
    uint32_t entry = id & 0xFFFF;
    if (entry < res->string_count) {
        return res->strings[entry];
    }
    return NULL;
}

const char *dx_resources_get_layout_filename(const DxResources *res, uint32_t id) {
    if (!res) return NULL;
    for (uint32_t i = 0; i < res->layout_entry_count; i++) {
        if (res->layout_entries[i].id == id) {
            return res->layout_entries[i].filename;
        }
    }
    return NULL;
}

const DxResourceEntry *dx_resources_find_by_id(const DxResources *res, uint32_t id) {
    if (!res) return NULL;
    for (uint32_t i = 0; i < res->entry_count; i++) {
        if (res->entries[i].id == id) {
            return &res->entries[i];
        }
    }
    return NULL;
}

const char *dx_resources_get_string_by_id(const DxResources *res, uint32_t id) {
    if (!res) return NULL;

    // Check legacy string entries first (faster, string-type only)
    const char *s = dx_resources_get_string(res, id);
    if (s) return s;

    // Fall back to general table
    const DxResourceEntry *e = dx_resources_find_by_id(res, id);
    if (e && e->value_type == RES_VALUE_TYPE_STRING) {
        return e->str_val;
    }
    return NULL;
}

const DxResourceEntry *dx_resources_find_by_name(const DxResources *res,
                                                   const char *type_name,
                                                   const char *entry_name) {
    if (!res || !entry_name) return NULL;
    for (uint32_t i = 0; i < res->entry_count; i++) {
        const DxResourceEntry *e = &res->entries[i];
        if (!e->entry_name) continue;
        if (strcmp(e->entry_name, entry_name) != 0) continue;
        if (type_name && e->type_name && strcmp(e->type_name, type_name) != 0) continue;
        return e;
    }
    return NULL;
}

char *dx_resources_format_dimen(float value, uint8_t unit) {
    const char *suffix;
    switch (unit) {
        case DX_DIMEN_UNIT_PX:  suffix = "px"; break;
        case DX_DIMEN_UNIT_DIP: suffix = "dp"; break;
        case DX_DIMEN_UNIT_SP:  suffix = "sp"; break;
        case DX_DIMEN_UNIT_PT:  suffix = "pt"; break;
        case DX_DIMEN_UNIT_IN:  suffix = "in"; break;
        case DX_DIMEN_UNIT_MM:  suffix = "mm"; break;
        default:                suffix = "??"; break;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f%s", value, suffix);
    return dx_strdup(buf);
}

char *dx_resources_format_color(uint32_t argb) {
    char buf[16];
    snprintf(buf, sizeof(buf), "#%08X", argb);
    return dx_strdup(buf);
}
