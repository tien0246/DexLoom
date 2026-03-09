#include "../Include/dx_apk.h"
#include "../Include/dx_log.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define TAG "APK"

#include "../Include/dx_memory.h"

// ZIP format constants
#define ZIP_LOCAL_HEADER_SIG    0x04034b50
#define ZIP_CENTRAL_DIR_SIG     0x02014b50
#define ZIP_END_CENTRAL_DIR_SIG 0x06054b50
#define ZIP_METHOD_STORE        0
#define ZIP_METHOD_DEFLATE      8
#define ZIP_MAX_DECOMPRESSED_SIZE (500U * 1024U * 1024U)  // 500 MB

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

// Find End of Central Directory record
static const uint8_t *find_eocd(const uint8_t *data, uint32_t size) {
    // EOCD is at least 22 bytes; search backwards
    if (size < 22) return NULL;

    uint32_t max_search = size < 65557 ? size : 65557; // max comment size + EOCD
    for (uint32_t i = 22; i <= max_search; i++) {
        const uint8_t *p = data + size - i;
        if (read_u32(p) == ZIP_END_CENTRAL_DIR_SIG) {
            // Validate: comment length should match remaining bytes
            uint16_t comment_len = read_u16(p + 20);
            if ((uint32_t)(22 + comment_len) == i) {
                return p;
            }
            // Also accept if it's close (some tools are sloppy)
            // but keep searching for a better match
        }
    }

    // Fallback: accept any EOCD signature (less strict)
    for (uint32_t i = 22; i <= max_search; i++) {
        const uint8_t *p = data + size - i;
        if (read_u32(p) == ZIP_END_CENTRAL_DIR_SIG) {
            return p;
        }
    }
    return NULL;
}

DxResult dx_apk_open(const uint8_t *data, uint32_t size, DxApkFile **out) {
    if (!data || !out) return DX_ERR_NULL_PTR;
    if (size < 22) return DX_ERR_ZIP_INVALID;

    // Find End of Central Directory
    const uint8_t *eocd = find_eocd(data, size);
    if (!eocd) {
        DX_ERROR(TAG, "Cannot find End of Central Directory");
        return DX_ERR_ZIP_INVALID;
    }

    uint16_t entry_count = read_u16(eocd + 10);
    uint32_t cd_offset = read_u32(eocd + 16);

    DX_INFO(TAG, "ZIP: %u entries, central dir at 0x%x", entry_count, cd_offset);

    if (cd_offset >= size) {
        DX_ERROR(TAG, "Central directory offset out of bounds");
        return DX_ERR_ZIP_INVALID;
    }

    DxApkFile *apk = (DxApkFile *)dx_malloc(sizeof(DxApkFile));
    if (!apk) return DX_ERR_OUT_OF_MEMORY;

    apk->data = (uint8_t *)data;
    apk->data_size = size;
    apk->entry_count = entry_count;
    apk->entries = (DxZipEntry *)dx_malloc(sizeof(DxZipEntry) * entry_count);
    if (!apk->entries) {
        dx_free(apk);
        return DX_ERR_OUT_OF_MEMORY;
    }

    // Parse central directory
    const uint8_t *p = data + cd_offset;
    for (uint32_t i = 0; i < entry_count; i++) {
        if (p + 46 > data + size) {
            DX_ERROR(TAG, "Central directory entry %u truncated", i);
            dx_apk_close(apk);
            return DX_ERR_ZIP_INVALID;
        }

        if (read_u32(p) != ZIP_CENTRAL_DIR_SIG) {
            DX_ERROR(TAG, "Invalid central directory signature at entry %u", i);
            dx_apk_close(apk);
            return DX_ERR_ZIP_INVALID;
        }

        uint16_t method = read_u16(p + 10);
        uint32_t compressed = read_u32(p + 20);
        uint32_t uncompressed = read_u32(p + 24);
        uint16_t name_len = read_u16(p + 28);
        uint16_t extra_len = read_u16(p + 30);
        uint16_t comment_len = read_u16(p + 32);
        uint32_t local_offset = read_u32(p + 42);

        // Extract filename
        char *name = (char *)dx_malloc(name_len + 1);
        if (!name) {
            dx_apk_close(apk);
            return DX_ERR_OUT_OF_MEMORY;
        }
        memcpy(name, p + 46, name_len);
        name[name_len] = '\0';

        // Path traversal prevention: reject entries with ".." in filename
        if (strstr(name, "..") != NULL) {
            DX_WARN(TAG, "Rejecting ZIP entry with path traversal: %s", name);
            dx_free(name);
            dx_apk_close(apk);
            return DX_ERR_ZIP_INVALID;
        }

        // Calculate data offset from local file header
        uint32_t data_offset = local_offset;
        if (local_offset + 30 <= size) {
            uint16_t local_name_len = read_u16(data + local_offset + 26);
            uint16_t local_extra_len = read_u16(data + local_offset + 28);
            data_offset = local_offset + 30 + local_name_len + local_extra_len;
        }

        apk->entries[i].filename = name;
        apk->entries[i].compression_method = method;
        apk->entries[i].compressed_size = compressed;
        apk->entries[i].uncompressed_size = uncompressed;
        apk->entries[i].data_offset = data_offset;

        DX_TRACE(TAG, "  [%u] %s (method=%u, size=%u/%u)",
                 i, name, method, compressed, uncompressed);

        p += 46 + name_len + extra_len + comment_len;
    }

    // Validate that we actually parsed all declared entries
    // (the loop above would have exited early on truncation or bad signature)
    // Check that the pointer advanced past the last entry into valid territory
    if (p > data + size) {
        DX_WARN(TAG, "Central directory entry count (%u) exceeds actual entries in file", entry_count);
        dx_apk_close(apk);
        return DX_ERR_ZIP_INVALID;
    }

    *out = apk;
    DX_INFO(TAG, "APK opened successfully with %u entries", entry_count);
    return DX_OK;
}

void dx_apk_close(DxApkFile *apk) {
    if (!apk) return;
    for (uint32_t i = 0; i < apk->entry_count; i++) {
        dx_free(apk->entries[i].filename);
    }
    dx_free(apk->entries);
    dx_free(apk);
}

DxResult dx_apk_find_entry(const DxApkFile *apk, const char *path, const DxZipEntry **out) {
    if (!apk || !path || !out) return DX_ERR_NULL_PTR;

    for (uint32_t i = 0; i < apk->entry_count; i++) {
        if (strcmp(apk->entries[i].filename, path) == 0) {
            *out = &apk->entries[i];
            return DX_OK;
        }
    }

    DX_DEBUG(TAG, "Entry not found: %s", path);
    return DX_ERR_NOT_FOUND;
}

DxResult dx_apk_extract_entry(const DxApkFile *apk, const DxZipEntry *entry,
                               uint8_t **out_data, uint32_t *out_size) {
    if (!apk || !entry || !out_data || !out_size) return DX_ERR_NULL_PTR;

    if (entry->data_offset + entry->compressed_size > apk->data_size) {
        DX_ERROR(TAG, "Entry data out of bounds: %s", entry->filename);
        return DX_ERR_ZIP_INVALID;
    }

    // Maximum decompressed size check (reject zip bombs)
    if (entry->uncompressed_size > ZIP_MAX_DECOMPRESSED_SIZE) {
        DX_WARN(TAG, "Entry %s decompressed size (%u) exceeds 500MB limit",
                entry->filename, entry->uncompressed_size);
        return DX_ERR_ZIP_INVALID;
    }

    const uint8_t *compressed_data = apk->data + entry->data_offset;

    if (entry->compression_method == ZIP_METHOD_STORE) {
        // No compression - copy directly
        uint8_t *buf = (uint8_t *)dx_malloc(entry->uncompressed_size);
        if (!buf) return DX_ERR_OUT_OF_MEMORY;
        memcpy(buf, compressed_data, entry->uncompressed_size);
        *out_data = buf;
        *out_size = entry->uncompressed_size;
        return DX_OK;
    }

    if (entry->compression_method == ZIP_METHOD_DEFLATE) {
        // Inflate using zlib
        uint8_t *buf = (uint8_t *)dx_malloc(entry->uncompressed_size);
        if (!buf) return DX_ERR_OUT_OF_MEMORY;

        z_stream strm;
        memset(&strm, 0, sizeof(strm));
        strm.next_in = (Bytef *)compressed_data;
        strm.avail_in = entry->compressed_size;
        strm.next_out = (Bytef *)buf;
        strm.avail_out = entry->uncompressed_size;

        // -MAX_WBITS for raw deflate (no zlib/gzip header)
        if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
            dx_free(buf);
            DX_ERROR(TAG, "inflateInit2 failed for %s", entry->filename);
            return DX_ERR_ZIP_INVALID;
        }

        int ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);

        if (ret != Z_STREAM_END) {
            dx_free(buf);
            DX_ERROR(TAG, "inflate failed for %s: %d", entry->filename, ret);
            return DX_ERR_ZIP_INVALID;
        }

        *out_data = buf;
        *out_size = entry->uncompressed_size;
        return DX_OK;
    }

    DX_ERROR(TAG, "Unsupported compression method %u for %s",
             entry->compression_method, entry->filename);
    return DX_ERR_ZIP_INVALID;
}
