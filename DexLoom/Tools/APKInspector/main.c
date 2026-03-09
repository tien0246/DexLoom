// APK Inspector - standalone CLI tool for inspecting APK files
// Build: cc -o apkinspector main.c ../../DexLoom/Core/APK/dx_apk.c
//        ../../DexLoom/Core/APK/dx_manifest.c ../../DexLoom/Core/APK/dx_resources.c
//        ../../DexLoom/Core/DEX/dx_dex.c ../../DexLoom/Core/DEX/dx_opcode.c
//        ../../DexLoom/Core/Base/dx_log.c ../../DexLoom/Core/Base/dx_memory.c
//        -lz -I../../DexLoom/Core/Include

#include "dx_types.h"
#include "dx_log.h"
#include "dx_apk.h"
#include "dx_manifest.h"
#include "dx_dex.h"
#include "dx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <apk_file> [--entries] [--manifest] [--dex] [--disasm]\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --entries   List all ZIP entries\n");
    fprintf(stderr, "  --manifest  Parse and print AndroidManifest.xml\n");
    fprintf(stderr, "  --dex       Parse and print DEX tables\n");
    fprintf(stderr, "  --disasm    Disassemble bytecode\n");
    fprintf(stderr, "  (no flags)  Print summary\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *apk_path = argv[1];
    bool show_entries = false;
    bool show_manifest = false;
    bool show_dex = false;
    bool show_disasm = false;
    bool any_flag = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--entries") == 0)  { show_entries = true; any_flag = true; }
        if (strcmp(argv[i], "--manifest") == 0) { show_manifest = true; any_flag = true; }
        if (strcmp(argv[i], "--dex") == 0)      { show_dex = true; any_flag = true; }
        if (strcmp(argv[i], "--disasm") == 0)    { show_disasm = true; any_flag = true; }
    }

    if (!any_flag) {
        show_entries = show_manifest = show_dex = true;
    }

    dx_log_init();
    dx_log_set_level(DX_LOG_INFO);

    // Read file
    FILE *f = fopen(apk_path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open: %s\n", apk_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)size);
    fread(data, 1, (size_t)size, f);
    fclose(f);

    printf("=== APK Inspector: %s (%ld bytes) ===\n\n", apk_path, size);

    // Parse ZIP
    DxApkFile *apk = NULL;
    DxResult res = dx_apk_open(data, (uint32_t)size, &apk);
    if (res != DX_OK) {
        fprintf(stderr, "ZIP parse failed: %s\n", dx_result_string(res));
        free(data);
        return 1;
    }

    if (show_entries) {
        printf("--- ZIP Entries (%u) ---\n", apk->entry_count);
        for (uint32_t i = 0; i < apk->entry_count; i++) {
            printf("  [%3u] %-50s  %8u -> %8u  method=%u\n",
                   i, apk->entries[i].filename,
                   apk->entries[i].compressed_size,
                   apk->entries[i].uncompressed_size,
                   apk->entries[i].compression_method);
        }
        printf("\n");
    }

    if (show_manifest) {
        const DxZipEntry *manifest_entry = NULL;
        if (dx_apk_find_entry(apk, "AndroidManifest.xml", &manifest_entry) == DX_OK) {
            uint8_t *mdata = NULL;
            uint32_t msize = 0;
            if (dx_apk_extract_entry(apk, manifest_entry, &mdata, &msize) == DX_OK) {
                DxManifest *manifest = NULL;
                if (dx_manifest_parse(mdata, msize, &manifest) == DX_OK) {
                    printf("--- Manifest ---\n");
                    printf("  Package:       %s\n", manifest->package_name ? manifest->package_name : "(null)");
                    printf("  Main Activity: %s\n", manifest->main_activity ? manifest->main_activity : "(null)");
                    printf("\n");
                    dx_manifest_free(manifest);
                }
                free(mdata);
            }
        }
    }

    if (show_dex) {
        const DxZipEntry *dex_entry = NULL;
        if (dx_apk_find_entry(apk, "classes.dex", &dex_entry) == DX_OK) {
            uint8_t *ddata = NULL;
            uint32_t dsize = 0;
            if (dx_apk_extract_entry(apk, dex_entry, &ddata, &dsize) == DX_OK) {
                DxDexFile *dex = NULL;
                if (dx_dex_parse(ddata, dsize, &dex) == DX_OK) {
                    printf("--- DEX Summary ---\n");
                    printf("  Strings: %u\n", dex->string_count);
                    printf("  Types:   %u\n", dex->type_count);
                    printf("  Protos:  %u\n", dex->proto_count);
                    printf("  Fields:  %u\n", dex->field_count);
                    printf("  Methods: %u\n", dex->method_count);
                    printf("  Classes: %u\n", dex->class_count);
                    printf("\n");

                    printf("--- Classes ---\n");
                    for (uint32_t i = 0; i < dex->class_count; i++) {
                        const char *cls = dx_dex_get_type(dex, dex->class_defs[i].class_idx);
                        const char *super = dx_dex_get_type(dex, dex->class_defs[i].superclass_idx);
                        printf("  [%u] %s extends %s\n", i, cls ? cls : "?", super ? super : "?");

                        dx_dex_parse_class_data(dex, i);
                        DxDexClassData *cd = dex->class_data[i];
                        if (!cd) continue;

                        for (uint32_t m = 0; m < cd->direct_methods_count; m++) {
                            const char *name = dx_dex_get_method_name(dex, cd->direct_methods[m].method_idx);
                            printf("    direct: %s (code_off=0x%x)\n",
                                   name ? name : "?", cd->direct_methods[m].code_off);

                            if (show_disasm && cd->direct_methods[m].code_off) {
                                DxDexCodeItem code;
                                if (dx_dex_parse_code_item(dex, cd->direct_methods[m].code_off, &code) == DX_OK) {
                                    printf("      regs=%u ins=%u outs=%u insns=%u\n",
                                           code.registers_size, code.ins_size, code.outs_size, code.insns_size);
                                    for (uint32_t pc = 0; pc < code.insns_size; ) {
                                        uint8_t op = code.insns[pc] & 0xFF;
                                        printf("      %04u: %s (0x%02x)\n", pc, dx_opcode_name(op), op);
                                        // Simple width estimation
                                        if (op == 0x00 || op == 0x01 || op == 0x07 || op == 0x0A ||
                                            op == 0x0C || op == 0x0E || op == 0x0F || op == 0x11 ||
                                            op == 0x12 || op == 0x28)
                                            pc += 1;
                                        else if (op == 0x6E || op == 0x70 || op == 0x71 || op == 0x72 || op == 0x14)
                                            pc += 3;
                                        else
                                            pc += 2;
                                    }
                                }
                            }
                        }
                        for (uint32_t m = 0; m < cd->virtual_methods_count; m++) {
                            const char *name = dx_dex_get_method_name(dex, cd->virtual_methods[m].method_idx);
                            printf("    virtual: %s (code_off=0x%x)\n",
                                   name ? name : "?", cd->virtual_methods[m].code_off);
                        }
                    }

                    dx_dex_free(dex);
                }
            }
        }
    }

    dx_apk_close(apk);
    free(data);

    printf("\n=== Done ===\n");
    return 0;
}
