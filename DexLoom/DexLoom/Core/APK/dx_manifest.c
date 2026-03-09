#include "../Include/dx_manifest.h"
#include "../Include/dx_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "Manifest"

#include "../Include/dx_memory.h"

// Append a string to a dynamic array
static void append_string(char ***array, uint32_t *count, const char *str) {
    uint32_t n = *count;
    char **new_arr = (char **)dx_realloc(*array, sizeof(char *) * (n + 1));
    if (!new_arr) return;
    new_arr[n] = dx_strdup(str);
    *array = new_arr;
    *count = n + 1;
}

// Android Binary XML chunk types
#define AXML_CHUNK_STRINGPOOL  0x0001
#define AXML_CHUNK_RESOURCEMAP 0x0180
#define AXML_CHUNK_START_NS    0x0100
#define AXML_CHUNK_END_NS      0x0101
#define AXML_CHUNK_START_TAG   0x0102
#define AXML_CHUNK_END_TAG     0x0103
#define AXML_CHUNK_TEXT        0x0104
#define AXML_FILE_MAGIC        0x00080003

// Well-known Android resource IDs
#define ATTR_NAME       0x01010003
#define ATTR_LABEL      0x01010001
#define ATTR_THEME      0x01010000
#define ATTR_VALUE      0x01010024
#define ATTR_EXPORTED   0x01010010
#define ATTR_MIN_SDK    0x0101020c
#define ATTR_TARGET_SDK 0x01010270

// Attribute value types
#define ATTR_TYPE_STRING    0x03
#define ATTR_TYPE_INT_DEC   0x10
#define ATTR_TYPE_INT_HEX   0x11

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

// Decode a string from AXML string pool
// AXML strings can be UTF-8 or UTF-16
static char *decode_axml_string(const uint8_t *pool_data, uint32_t pool_size,
                                 uint32_t offset, bool is_utf8) {
    if (offset >= pool_size) return dx_strdup("");

    const uint8_t *p = pool_data + offset;

    if (is_utf8) {
        // UTF-8: first byte(s) = char count (ULEB128-ish), then byte count, then data
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
        // UTF-16LE: first 2 bytes = char count, then UTF-16 data
        uint16_t char_count = read_u16(p);
        p += 2;
        if (char_count >= 0x8000) {
            char_count = ((char_count & 0x7FFF) << 16) | read_u16(p);
            p += 2;
        }
        // Simple ASCII extraction from UTF-16 (sufficient for manifest strings)
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

DxResult dx_axml_parse(const uint8_t *data, uint32_t size, DxAxmlParser **out) {
    if (!data || !out) return DX_ERR_NULL_PTR;
    if (size < 8) return DX_ERR_AXML_INVALID;

    uint32_t magic = read_u32(data);
    if (magic != AXML_FILE_MAGIC) {
        DX_ERROR(TAG, "Invalid AXML magic: 0x%08x (expected 0x%08x)", magic, AXML_FILE_MAGIC);
        return DX_ERR_AXML_INVALID;
    }

    DxAxmlParser *parser = (DxAxmlParser *)dx_malloc(sizeof(DxAxmlParser));
    if (!parser) return DX_ERR_OUT_OF_MEMORY;

    // Parse string pool (always first chunk after header)
    uint32_t pos = 8;
    if (pos + 8 > size) goto fail;

    uint16_t chunk_type = read_u16(data + pos);
    // uint16_t header_size = read_u16(data + pos + 2);
    uint32_t chunk_size = read_u32(data + pos + 4);

    if (chunk_type != AXML_CHUNK_STRINGPOOL) {
        DX_ERROR(TAG, "Expected string pool chunk, got 0x%04x", chunk_type);
        goto fail;
    }

    // String pool header
    uint32_t string_count = read_u32(data + pos + 8);
    // uint32_t style_count = read_u32(data + pos + 12);
    uint32_t flags = read_u32(data + pos + 16);
    uint32_t strings_start = read_u32(data + pos + 20);
    // uint32_t styles_start = read_u32(data + pos + 24);
    bool is_utf8 = (flags & (1 << 8)) != 0;

    parser->string_count = string_count;
    parser->strings = (char **)dx_malloc(sizeof(char *) * string_count);
    if (!parser->strings) goto fail;

    // String offsets start at pos + 28
    uint32_t offsets_start = pos + 28;
    uint32_t pool_data_start = pos + strings_start;

    for (uint32_t i = 0; i < string_count; i++) {
        if (offsets_start + i * 4 + 4 > size) break;
        uint32_t str_offset = read_u32(data + offsets_start + i * 4);
        parser->strings[i] = decode_axml_string(
            data + pool_data_start,
            size - pool_data_start,
            str_offset,
            is_utf8
        );
    }

    // Parse resource ID map if present
    pos += chunk_size;
    if (pos + 8 <= size) {
        chunk_type = read_u16(data + pos);
        chunk_size = read_u32(data + pos + 4);
        if (chunk_type == AXML_CHUNK_RESOURCEMAP) {
            uint32_t id_count = (chunk_size - 8) / 4;
            parser->res_id_count = id_count;
            parser->res_ids = (uint32_t *)dx_malloc(sizeof(uint32_t) * id_count);
            if (parser->res_ids) {
                for (uint32_t i = 0; i < id_count; i++) {
                    parser->res_ids[i] = read_u32(data + pos + 8 + i * 4);
                }
            }
        }
    }

    DX_INFO(TAG, "AXML parsed: %u strings", parser->string_count);
    *out = parser;
    return DX_OK;

fail:
    dx_axml_free(parser);
    return DX_ERR_AXML_INVALID;
}

void dx_axml_free(DxAxmlParser *parser) {
    if (!parser) return;
    for (uint32_t i = 0; i < parser->string_count; i++) {
        dx_free(parser->strings[i]);
    }
    dx_free(parser->strings);
    dx_free(parser->res_ids);
    dx_free(parser);
}

DxResult dx_manifest_parse(const uint8_t *data, uint32_t size, DxManifest **out) {
    if (!data || !out) return DX_ERR_NULL_PTR;

    DxAxmlParser *axml = NULL;
    DxResult res = dx_axml_parse(data, size, &axml);
    if (res != DX_OK) return res;

    DxManifest *manifest = (DxManifest *)dx_malloc(sizeof(DxManifest));
    if (!manifest) {
        dx_axml_free(axml);
        return DX_ERR_OUT_OF_MEMORY;
    }

    // Walk the XML tree looking for key elements/attributes
    // We parse the chunk stream after string pool + resource map
    uint32_t pos = 8; // skip file header

    // Skip string pool chunk
    if (pos + 8 <= size) {
        uint32_t chunk_size = read_u32(data + pos + 4);
        pos += chunk_size;
    }
    // Skip resource map chunk if present
    if (pos + 8 <= size && read_u16(data + pos) == AXML_CHUNK_RESOURCEMAP) {
        uint32_t chunk_size = read_u32(data + pos + 4);
        pos += chunk_size;
    }

    bool in_activity = false;
    bool in_intent_filter = false;
    bool found_main = false;
    bool found_launcher = false;
    char *current_activity_name = NULL;

    // Helper: extract android:name string attribute from a tag's attributes
    // Returns a dx_strdup'd string or NULL
    #define EXTRACT_NAME_ATTR(out_str) do { \
        uint32_t _attr_start = pos + 36; \
        for (uint16_t _a = 0; _a < attr_count; _a++) { \
            uint32_t _aoff = _attr_start + _a * 20; \
            if (_aoff + 20 > size) break; \
            uint32_t _ani = read_u32(data + _aoff + 4); \
            uint32_t _arid = 0; \
            if (axml->res_ids && _ani < axml->res_id_count) \
                _arid = axml->res_ids[_ani]; \
            uint32_t _atype = read_u32(data + _aoff + 12) >> 24; \
            uint32_t _adata = read_u32(data + _aoff + 16); \
            bool _is_name = (_arid == ATTR_NAME) || \
                (_ani < axml->string_count && \
                 strcmp(axml->strings[_ani], "name") == 0); \
            if (_is_name && _atype == ATTR_TYPE_STRING && \
                _adata < axml->string_count) { \
                (out_str) = dx_strdup(axml->strings[_adata]); \
                break; \
            } \
        } \
    } while(0)

    while (pos + 8 <= size) {
        uint16_t chunk_type = read_u16(data + pos);
        uint32_t chunk_size = read_u32(data + pos + 4);

        if (chunk_size < 8 || pos + chunk_size > size) break;

        if (chunk_type == AXML_CHUNK_START_TAG) {
            // Start tag: header(16) + ns_idx(4) + name_idx(4) + ...
            if (pos + 28 > size) break;

            uint32_t name_idx = read_u32(data + pos + 20);
            uint16_t attr_count = read_u16(data + pos + 28);

            const char *tag_name = (name_idx < axml->string_count) ?
                                    axml->strings[name_idx] : "";

            if (strcmp(tag_name, "manifest") == 0) {
                // Look for package attribute
                uint32_t attr_start = pos + 36;
                for (uint16_t a = 0; a < attr_count; a++) {
                    uint32_t aoff = attr_start + a * 20;
                    if (aoff + 20 > size) break;
                    uint32_t attr_name_idx = read_u32(data + aoff + 4);
                    uint32_t attr_type = read_u32(data + aoff + 12) >> 24;
                    uint32_t attr_data = read_u32(data + aoff + 16);

                    if (attr_name_idx < axml->string_count &&
                        strcmp(axml->strings[attr_name_idx], "package") == 0 &&
                        attr_type == ATTR_TYPE_STRING &&
                        attr_data < axml->string_count) {
                        manifest->package_name = dx_strdup(axml->strings[attr_data]);
                        DX_INFO(TAG, "Package: %s", manifest->package_name);
                    }
                }
            } else if (strcmp(tag_name, "uses-sdk") == 0) {
                // Extract minSdkVersion and targetSdkVersion
                uint32_t attr_start = pos + 36;
                for (uint16_t a = 0; a < attr_count; a++) {
                    uint32_t aoff = attr_start + a * 20;
                    if (aoff + 20 > size) break;
                    uint32_t attr_res_id = read_u32(data + aoff + 4);
                    // Check resource ID map if available
                    if (attr_res_id < axml->string_count && axml->res_id_count > attr_res_id) {
                        attr_res_id = axml->res_ids[attr_res_id];
                    }
                    uint32_t attr_data = read_u32(data + aoff + 16);
                    if (attr_res_id == ATTR_MIN_SDK) {
                        manifest->min_sdk = (int32_t)attr_data;
                        DX_INFO(TAG, "minSdkVersion: %d", manifest->min_sdk);
                    } else if (attr_res_id == ATTR_TARGET_SDK) {
                        manifest->target_sdk = (int32_t)attr_data;
                        DX_INFO(TAG, "targetSdkVersion: %d", manifest->target_sdk);
                    }
                }
            } else if (strcmp(tag_name, "uses-permission") == 0) {
                char *perm_name = NULL;
                EXTRACT_NAME_ATTR(perm_name);
                if (perm_name) {
                    append_string(&manifest->permissions, &manifest->permission_count, perm_name);
                    dx_free(perm_name);
                }
            } else if (strcmp(tag_name, "application") == 0) {
                // Extract android:label
                uint32_t attr_start_app = pos + 36;
                for (uint16_t a = 0; a < attr_count; a++) {
                    uint32_t aoff = attr_start_app + a * 20;
                    if (aoff + 20 > size) break;
                    uint32_t attr_res_id = read_u32(data + aoff + 4);
                    if (attr_res_id < axml->string_count && axml->res_id_count > attr_res_id) {
                        attr_res_id = axml->res_ids[attr_res_id];
                    }
                    uint32_t attr_type = read_u32(data + aoff + 12) >> 24;
                    uint32_t attr_data = read_u32(data + aoff + 16);
                    if (attr_res_id == ATTR_LABEL && attr_type == ATTR_TYPE_STRING &&
                        attr_data < axml->string_count && !manifest->app_label) {
                        manifest->app_label = dx_strdup(axml->strings[attr_data]);
                        DX_INFO(TAG, "App label: %s", manifest->app_label);
                    }
                    // Extract android:theme (resource reference)
                    if (attr_res_id == ATTR_THEME && !manifest->app_theme) {
                        if (attr_type == ATTR_TYPE_STRING && attr_data < axml->string_count) {
                            manifest->app_theme = dx_strdup(axml->strings[attr_data]);
                        } else {
                            // Theme is typically a resource reference (0x01 type)
                            char theme_ref[32];
                            snprintf(theme_ref, sizeof(theme_ref), "@0x%08x", attr_data);
                            manifest->app_theme = dx_strdup(theme_ref);
                        }
                        DX_INFO(TAG, "App theme: %s", manifest->app_theme);
                    }
                }
            } else if (strcmp(tag_name, "activity") == 0) {
                in_activity = true;
                found_main = false;
                found_launcher = false;
                dx_free(current_activity_name);
                current_activity_name = NULL;
                EXTRACT_NAME_ATTR(current_activity_name);

                // Add to activities list
                if (current_activity_name) {
                    // Resolve relative name for the list
                    if (current_activity_name[0] == '.' && manifest->package_name) {
                        size_t pkg_len = strlen(manifest->package_name);
                        size_t act_len = strlen(current_activity_name);
                        char *full = (char *)dx_malloc(pkg_len + act_len + 1);
                        if (full) {
                            memcpy(full, manifest->package_name, pkg_len);
                            memcpy(full + pkg_len, current_activity_name, act_len + 1);
                            append_string(&manifest->activities, &manifest->activity_count, full);
                            dx_free(full);
                        }
                    } else {
                        append_string(&manifest->activities, &manifest->activity_count, current_activity_name);
                    }
                }
            } else if (strcmp(tag_name, "service") == 0) {
                char *svc_name = NULL;
                EXTRACT_NAME_ATTR(svc_name);
                if (svc_name) {
                    append_string(&manifest->services, &manifest->service_count, svc_name);
                    dx_free(svc_name);
                }
            } else if (strcmp(tag_name, "receiver") == 0) {
                char *rcv_name = NULL;
                EXTRACT_NAME_ATTR(rcv_name);
                if (rcv_name) {
                    append_string(&manifest->receivers, &manifest->receiver_count, rcv_name);
                    dx_free(rcv_name);
                }
            } else if (strcmp(tag_name, "provider") == 0) {
                char *prv_name = NULL;
                EXTRACT_NAME_ATTR(prv_name);
                if (prv_name) {
                    append_string(&manifest->providers, &manifest->provider_count, prv_name);
                    dx_free(prv_name);
                }
            } else if (strcmp(tag_name, "intent-filter") == 0) {
                in_intent_filter = true;
            } else if (strcmp(tag_name, "action") == 0 && in_intent_filter) {
                uint32_t attr_start = pos + 36;
                for (uint16_t a = 0; a < attr_count; a++) {
                    uint32_t aoff = attr_start + a * 20;
                    if (aoff + 20 > size) break;
                    uint32_t attr_name_idx = read_u32(data + aoff + 4);
                    uint32_t attr_type = read_u32(data + aoff + 12) >> 24;
                    uint32_t attr_data = read_u32(data + aoff + 16);

                    if (attr_name_idx < axml->string_count &&
                        strcmp(axml->strings[attr_name_idx], "name") == 0 &&
                        attr_type == ATTR_TYPE_STRING &&
                        attr_data < axml->string_count &&
                        strcmp(axml->strings[attr_data], "android.intent.action.MAIN") == 0) {
                        found_main = true;
                    }
                }
            } else if (strcmp(tag_name, "category") == 0 && in_intent_filter) {
                uint32_t attr_start = pos + 36;
                for (uint16_t a = 0; a < attr_count; a++) {
                    uint32_t aoff = attr_start + a * 20;
                    if (aoff + 20 > size) break;
                    uint32_t attr_name_idx = read_u32(data + aoff + 4);
                    uint32_t attr_type = read_u32(data + aoff + 12) >> 24;
                    uint32_t attr_data = read_u32(data + aoff + 16);

                    if (attr_name_idx < axml->string_count &&
                        strcmp(axml->strings[attr_name_idx], "name") == 0 &&
                        attr_type == ATTR_TYPE_STRING &&
                        attr_data < axml->string_count &&
                        strcmp(axml->strings[attr_data], "android.intent.category.LAUNCHER") == 0) {
                        found_launcher = true;
                    }
                }
            }
        } else if (chunk_type == AXML_CHUNK_END_TAG) {
            uint32_t name_idx = read_u32(data + pos + 20);
            const char *tag_name = (name_idx < axml->string_count) ?
                                    axml->strings[name_idx] : "";

            if (strcmp(tag_name, "intent-filter") == 0) {
                if (found_main && found_launcher && current_activity_name) {
                    // Resolve relative activity name
                    if (current_activity_name[0] == '.' && manifest->package_name) {
                        size_t pkg_len = strlen(manifest->package_name);
                        size_t act_len = strlen(current_activity_name);
                        char *full = (char *)dx_malloc(pkg_len + act_len + 1);
                        if (full) {
                            memcpy(full, manifest->package_name, pkg_len);
                            memcpy(full + pkg_len, current_activity_name, act_len + 1);
                            manifest->main_activity = full;
                        }
                    } else {
                        manifest->main_activity = dx_strdup(current_activity_name);
                    }
                    DX_INFO(TAG, "Main activity: %s", manifest->main_activity);
                }
                in_intent_filter = false;
            } else if (strcmp(tag_name, "activity") == 0) {
                in_activity = false;
            }
        }

        pos += chunk_size;
    }

    #undef EXTRACT_NAME_ATTR

    dx_free(current_activity_name);
    dx_axml_free(axml);

    if (!manifest->main_activity) {
        DX_WARN(TAG, "No launcher activity found in manifest");
    }

    DX_INFO(TAG, "Manifest: %u permissions, %u activities, %u services, %u receivers, %u providers",
            manifest->permission_count, manifest->activity_count,
            manifest->service_count, manifest->receiver_count, manifest->provider_count);

    *out = manifest;
    return DX_OK;
}

static void free_string_array(char **arr, uint32_t count) {
    if (!arr) return;
    for (uint32_t i = 0; i < count; i++) {
        dx_free(arr[i]);
    }
    dx_free(arr);
}

void dx_manifest_free(DxManifest *manifest) {
    if (!manifest) return;
    dx_free(manifest->package_name);
    dx_free(manifest->main_activity);
    dx_free(manifest->app_label);
    dx_free(manifest->app_theme);
    free_string_array(manifest->permissions, manifest->permission_count);
    free_string_array(manifest->activities, manifest->activity_count);
    free_string_array(manifest->services, manifest->service_count);
    free_string_array(manifest->receivers, manifest->receiver_count);
    free_string_array(manifest->providers, manifest->provider_count);
    dx_free(manifest);
}
