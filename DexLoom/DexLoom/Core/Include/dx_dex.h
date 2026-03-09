#ifndef DX_DEX_H
#define DX_DEX_H

#include "dx_types.h"

// DEX file magic: "dex\n035\0" (or 037, 038, 039)
#define DEX_MAGIC_SIZE 8
#define DEX_MAGIC "dex\n"

// DEX header (standard 0x70 bytes)
typedef struct {
    uint8_t  magic[DEX_MAGIC_SIZE];
    uint32_t checksum;
    uint8_t  signature[20];
    uint32_t file_size;
    uint32_t header_size;
    uint32_t endian_tag;
    uint32_t link_size;
    uint32_t link_off;
    uint32_t map_off;
    uint32_t string_ids_size;
    uint32_t string_ids_off;
    uint32_t type_ids_size;
    uint32_t type_ids_off;
    uint32_t proto_ids_size;
    uint32_t proto_ids_off;
    uint32_t field_ids_size;
    uint32_t field_ids_off;
    uint32_t method_ids_size;
    uint32_t method_ids_off;
    uint32_t class_defs_size;
    uint32_t class_defs_off;
    uint32_t data_size;
    uint32_t data_off;
} DxDexHeader;

// String ID item
typedef struct {
    uint32_t string_data_off;
} DxDexStringId;

// Type ID item
typedef struct {
    uint32_t descriptor_idx;    // index into string_ids
} DxDexTypeId;

// Proto ID item
typedef struct {
    uint32_t shorty_idx;
    uint32_t return_type_idx;
    uint32_t parameters_off;    // offset to type_list or 0
} DxDexProtoId;

// Field ID item
typedef struct {
    uint16_t class_idx;
    uint16_t type_idx;
    uint32_t name_idx;
} DxDexFieldId;

// Method ID item
typedef struct {
    uint16_t class_idx;
    uint16_t proto_idx;
    uint32_t name_idx;
} DxDexMethodId;

// Class def item
typedef struct {
    uint32_t class_idx;
    uint32_t access_flags;
    uint32_t superclass_idx;
    uint32_t interfaces_off;
    uint32_t source_file_idx;
    uint32_t annotations_off;
    uint32_t class_data_off;
    uint32_t static_values_off;
} DxDexClassDef;

// Line number table entry (bytecode address -> source line)
#define DX_MAX_LINE_ENTRIES 100

typedef struct {
    uint32_t address;           // bytecode address (in 16-bit code units)
    int32_t  line;              // source line number
} DxLineEntry;

// Code item
typedef struct {
    uint16_t registers_size;
    uint16_t ins_size;
    uint16_t outs_size;
    uint16_t tries_size;
    uint32_t debug_info_off;
    uint32_t insns_size;        // in 16-bit code units
    uint16_t *insns;            // pointer into DEX data (not owned)

    // Line number table (populated from debug_info_item)
    DxLineEntry *line_table;    // heap-allocated, NULL if no debug info
    uint32_t     line_count;    // number of entries in line_table
} DxDexCodeItem;

// Encoded method from class_data
typedef struct {
    uint32_t method_idx;        // index into method_ids (delta-encoded in file)
    uint32_t access_flags;
    uint32_t code_off;          // offset to code_item or 0
} DxDexEncodedMethod;

// Encoded field from class_data
typedef struct {
    uint32_t field_idx;         // index into field_ids (delta-encoded)
    uint32_t access_flags;
} DxDexEncodedField;

// Parsed class data
typedef struct {
    DxDexEncodedField  *static_fields;
    uint32_t            static_fields_count;
    DxDexEncodedField  *instance_fields;
    uint32_t            instance_fields_count;
    DxDexEncodedMethod *direct_methods;
    uint32_t            direct_methods_count;
    DxDexEncodedMethod *virtual_methods;
    uint32_t            virtual_methods_count;
} DxDexClassData;

// Complete parsed DEX file
struct DxDexFile {
    const uint8_t  *raw_data;
    uint32_t        raw_size;

    DxDexHeader     header;

    // Tables (point into raw_data)
    char          **strings;        // decoded string table
    uint32_t        string_count;

    DxDexTypeId    *type_ids;
    uint32_t        type_count;

    DxDexProtoId   *proto_ids;
    uint32_t        proto_count;

    DxDexFieldId   *field_ids;
    uint32_t        field_count;

    DxDexMethodId  *method_ids;
    uint32_t        method_count;

    DxDexClassDef  *class_defs;
    uint32_t        class_count;

    // Parsed class data (lazy, indexed by class_def index)
    DxDexClassData **class_data;    // array of pointers, NULL until parsed
};

// Parse a DEX file from a buffer (buffer must remain valid)
DxResult dx_dex_parse(const uint8_t *data, uint32_t size, DxDexFile **out);
void     dx_dex_free(DxDexFile *dex);

// Get string by index
const char *dx_dex_get_string(const DxDexFile *dex, uint32_t idx);

// Get type descriptor string by type index
const char *dx_dex_get_type(const DxDexFile *dex, uint32_t type_idx);

// Parse class data for a class_def (lazy)
DxResult dx_dex_parse_class_data(DxDexFile *dex, uint32_t class_def_idx);

// Parse a code item at the given offset
DxResult dx_dex_parse_code_item(const DxDexFile *dex, uint32_t offset, DxDexCodeItem *out);

// Get method name
const char *dx_dex_get_method_name(const DxDexFile *dex, uint32_t method_idx);

// Get method's class descriptor
const char *dx_dex_get_method_class(const DxDexFile *dex, uint32_t method_idx);

// Get method's prototype shorty
const char *dx_dex_get_method_shorty(const DxDexFile *dex, uint32_t method_idx);

// Get field name
const char *dx_dex_get_field_name(const DxDexFile *dex, uint32_t field_idx);

// Get field's class descriptor
const char *dx_dex_get_field_class(const DxDexFile *dex, uint32_t field_idx);

// Get method parameter count
uint32_t dx_dex_get_method_param_count(const DxDexFile *dex, uint32_t method_idx);

// Get method parameter type descriptor by index
const char *dx_dex_get_method_param_type(const DxDexFile *dex, uint32_t method_idx, uint32_t param_idx);

// Get method return type descriptor
const char *dx_dex_get_method_return_type(const DxDexFile *dex, uint32_t method_idx);

// Parse encoded static field default values from class_def.static_values_off
DxResult dx_dex_parse_static_values(const DxDexFile *dex, uint32_t offset,
                                     DxValue *out_values, uint32_t max_count);

// Annotation entry (type descriptor + visibility)
typedef struct {
    const char *type;       // annotation type descriptor e.g. "Lretrofit2/http/GET;"
    uint8_t     visibility; // 0=BUILD, 1=RUNTIME, 2=SYSTEM
} DxAnnotationEntry;

// Parsed annotations directory for a class_def
typedef struct {
    DxAnnotationEntry *class_annotations;
    uint32_t           class_annotation_count;

    // Per-method annotations (parallel arrays)
    uint32_t          *method_idxs;           // DEX method_idx for each entry
    DxAnnotationEntry **method_annotations;   // array of annotation arrays
    uint32_t          *method_annotation_counts;
    uint32_t           annotated_method_count;
} DxAnnotationsDirectory;

// Parse debug info for a code item, populating its line_table.
// Call after dx_dex_parse_code_item. The DxDexFile must remain valid.
DxResult dx_dex_parse_debug_info(const DxDexFile *dex, DxDexCodeItem *code);

// Look up source line number for a bytecode address. Returns -1 if unknown.
int dx_method_get_line(const DxMethod *method, uint32_t pc);

// Free a code item's line table (call before discarding a DxDexCodeItem)
void dx_dex_free_code_item(DxDexCodeItem *code);

// Parse annotations directory for a class_def. Caller must free with dx_dex_free_annotations.
DxResult dx_dex_parse_annotations(const DxDexFile *dex, uint32_t annotations_off,
                                   DxAnnotationsDirectory *out);

// Free a parsed annotations directory
void dx_dex_free_annotations(DxAnnotationsDirectory *dir);

#endif // DX_DEX_H
