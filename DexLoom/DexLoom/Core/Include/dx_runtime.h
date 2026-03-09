#ifndef DX_RUNTIME_H
#define DX_RUNTIME_H

#include "dx_types.h"
#include "dx_context.h"

// High-level runtime API for the Swift bridge

// Initialize the entire runtime
DxResult dx_runtime_init(DxContext *ctx);

// Load an APK file and prepare for execution
DxResult dx_runtime_load(DxContext *ctx, const char *apk_path);

// Execute the main activity
DxResult dx_runtime_run(DxContext *ctx);

// Dispatch a UI event (button click)
DxResult dx_runtime_dispatch_click(DxContext *ctx, uint32_t view_id);

// Update EditText content from Swift UI input
DxResult dx_runtime_update_edit_text(DxContext *ctx, uint32_t view_id, const char *text);

// Dispatch back button press (calls Activity.onBackPressed)
DxResult dx_runtime_dispatch_back(DxContext *ctx);

// Get current render model (for SwiftUI to consume)
DxRenderModel *dx_runtime_get_render_model(DxContext *ctx);

// Shutdown
void dx_runtime_shutdown(DxContext *ctx);

// Opcode names and widths for tracing/verification
const char *dx_opcode_name(uint8_t opcode);
uint32_t dx_opcode_width(uint8_t opcode);

#endif // DX_RUNTIME_H
