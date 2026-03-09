#ifndef DX_APK_H
#define DX_APK_H

#include "dx_types.h"

// ZIP local file header
typedef struct {
    char     *filename;
    uint32_t  compressed_size;
    uint32_t  uncompressed_size;
    uint16_t  compression_method;
    uint32_t  data_offset;      // offset to compressed data in file
} DxZipEntry;

typedef struct {
    uint8_t    *data;           // mmap'd or read file data
    uint32_t    data_size;
    DxZipEntry *entries;
    uint32_t    entry_count;
} DxApkFile;

// Parse an APK (ZIP) file from a buffer
DxResult dx_apk_open(const uint8_t *data, uint32_t size, DxApkFile **out);
void     dx_apk_close(DxApkFile *apk);

// Find an entry by path (e.g., "classes.dex", "AndroidManifest.xml")
DxResult dx_apk_find_entry(const DxApkFile *apk, const char *path, const DxZipEntry **out);

// Extract an entry's data (caller must free *out_data)
// Handles STORE (no compression) and DEFLATE
DxResult dx_apk_extract_entry(const DxApkFile *apk, const DxZipEntry *entry,
                               uint8_t **out_data, uint32_t *out_size);

#endif // DX_APK_H
