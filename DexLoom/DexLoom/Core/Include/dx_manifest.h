#ifndef DX_MANIFEST_H
#define DX_MANIFEST_H

#include "dx_types.h"

// Parsed AndroidManifest data
typedef struct {
    char    *package_name;
    char    *main_activity;     // fully qualified class name of launcher activity
    int32_t  min_sdk;
    int32_t  target_sdk;
    char    *app_label;
    char    *app_theme;        // android:theme resource reference

    // Component arrays
    char    **permissions;
    uint32_t  permission_count;
    char    **activities;
    uint32_t  activity_count;
    char    **services;
    uint32_t  service_count;
    char    **receivers;
    uint32_t  receiver_count;
    char    **providers;
    uint32_t  provider_count;
} DxManifest;

// Parse Android Binary XML manifest from raw bytes
DxResult dx_manifest_parse(const uint8_t *data, uint32_t size, DxManifest **out);
void     dx_manifest_free(DxManifest *manifest);

// AXML (Android Binary XML) low-level parser
typedef struct {
    // String pool
    char     **strings;
    uint32_t   string_count;

    // Resource ID map
    uint32_t  *res_ids;
    uint32_t   res_id_count;
} DxAxmlParser;

DxResult dx_axml_parse(const uint8_t *data, uint32_t size, DxAxmlParser **out);
void     dx_axml_free(DxAxmlParser *parser);

#endif // DX_MANIFEST_H
