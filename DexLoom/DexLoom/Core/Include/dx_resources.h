#ifndef DX_RESOURCES_H
#define DX_RESOURCES_H

#include "dx_types.h"

// Resource value types (from Android's ResourceTypes.h)
typedef enum {
    DX_RES_TYPE_NULL     = 0,
    DX_RES_TYPE_REF      = 1,   // reference to another resource
    DX_RES_TYPE_STRING   = 3,
    DX_RES_TYPE_FLOAT    = 4,
    DX_RES_TYPE_DIMEN    = 5,   // dimension (dp, sp, px, etc.)
    DX_RES_TYPE_FRACTION = 6,
    DX_RES_TYPE_INT_DEC  = 16,  // 0x10
    DX_RES_TYPE_INT_HEX  = 17,  // 0x11
    DX_RES_TYPE_INT_BOOL = 18,  // 0x12
    DX_RES_TYPE_INT_COLOR_ARGB8 = 28, // 0x1c
    DX_RES_TYPE_INT_COLOR_RGB8  = 29, // 0x1d
    DX_RES_TYPE_INT_COLOR_ARGB4 = 30, // 0x1e
    DX_RES_TYPE_INT_COLOR_RGB4  = 31, // 0x1f
} DxResValueType;

// Dimension unit types (stored in low 4 bits of dimension data)
typedef enum {
    DX_DIMEN_UNIT_PX = 0,
    DX_DIMEN_UNIT_DIP = 1, // dp
    DX_DIMEN_UNIT_SP = 2,
    DX_DIMEN_UNIT_PT = 3,
    DX_DIMEN_UNIT_IN = 4,
    DX_DIMEN_UNIT_MM = 5,
} DxDimenUnit;

// A general resource entry (covers all value types)
typedef struct {
    uint32_t id;             // resource ID (0x7fXXYYYY)
    uint8_t  value_type;     // DxResValueType
    union {
        char    *str_val;    // for DX_RES_TYPE_STRING
        int32_t  int_val;    // for DX_RES_TYPE_INT_DEC, INT_HEX
        float    float_val;  // for DX_RES_TYPE_FLOAT
        uint32_t color_val;  // for DX_RES_TYPE_INT_COLOR_* (ARGB)
        bool     bool_val;   // for DX_RES_TYPE_INT_BOOL
        struct {
            float value;     // dimension value
            uint8_t unit;    // DxDimenUnit
        } dimen;
        uint32_t ref_id;     // for DX_RES_TYPE_REF
    };
    char *entry_name;        // key name, e.g., "app_name", "main_layout"
    char *type_name;         // type name, e.g., "string", "layout", "color"
} DxResourceEntry;

// Parsed resources.arsc data
typedef struct {
    // String pool from resources
    char     **strings;
    uint32_t   string_count;

    // Resource entries: map resource ID -> string value
    // String resource entries (type 0x03 string references)
    struct {
        uint32_t  id;
        char     *value;
    } *string_entries;
    uint32_t string_entry_count;

    // Layout resource IDs (maps ID -> index in layout_buffers)
    struct {
        uint32_t id;
        char    *filename;  // e.g., "res/layout/activity_main.xml"
    } *layout_entries;
    uint32_t layout_entry_count;

    // General resource entry table (all types)
    DxResourceEntry *entries;
    uint32_t entry_count;
    uint32_t entry_capacity;
} DxResources;

DxResult dx_resources_parse(const uint8_t *data, uint32_t size, DxResources **out);
void     dx_resources_free(DxResources *res);

// Look up a string resource by ID
const char *dx_resources_get_string(const DxResources *res, uint32_t id);

// Look up a layout filename by resource ID
const char *dx_resources_get_layout_filename(const DxResources *res, uint32_t id);

// Look up any resource entry by ID
const DxResourceEntry *dx_resources_find_by_id(const DxResources *res, uint32_t id);

// Convenience: look up a string value by resource ID (checks general table too)
const char *dx_resources_get_string_by_id(const DxResources *res, uint32_t id);

// Convenience: look up a resource entry by entry name and type name
const DxResourceEntry *dx_resources_find_by_name(const DxResources *res,
                                                   const char *type_name,
                                                   const char *entry_name);

// Decode a dimension value to a float in the given unit
float dx_resources_decode_dimen(uint32_t raw_data, uint8_t *out_unit);

// Format a dimension value as a string (e.g., "16.0dp"), caller must free
char *dx_resources_format_dimen(float value, uint8_t unit);

// Format a color value as "#AARRGGBB" string, caller must free
char *dx_resources_format_color(uint32_t argb);

#endif // DX_RESOURCES_H
