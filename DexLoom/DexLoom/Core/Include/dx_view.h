#ifndef DX_VIEW_H
#define DX_VIEW_H

#include "dx_types.h"

// RelativeLayout positioning bit flags (stored in DxUINode.relative_flags)
#define DX_REL_ALIGN_PARENT_TOP      0x0001
#define DX_REL_ALIGN_PARENT_BOTTOM   0x0002
#define DX_REL_ALIGN_PARENT_LEFT     0x0004
#define DX_REL_ALIGN_PARENT_RIGHT    0x0008
#define DX_REL_CENTER_IN_PARENT      0x0010
#define DX_REL_CENTER_HORIZONTAL     0x0020
#define DX_REL_CENTER_VERTICAL       0x0040
#define DX_REL_ABOVE                 0x0080
#define DX_REL_BELOW                 0x0100
#define DX_REL_LEFT_OF               0x0200
#define DX_REL_RIGHT_OF              0x0400

// UI tree node - represents an Android View in the internal UI tree
struct DxUINode {
    DxViewType   type;
    uint32_t     view_id;       // android:id resource ID
    DxVisibility visibility;

    // Text content (for TextView/Button/EditText)
    char        *text;
    char        *hint;          // hint text for EditText

    // Layout attributes
    DxOrientation orientation;  // for LinearLayout
    float         text_size;    // for TextView (default 16.0)
    float         weight;       // layout_weight (0 = none)
    int32_t       width;        // -1 = match_parent, -2 = wrap_content
    int32_t       height;
    int32_t       gravity;      // text/content gravity
    int32_t       padding[4];   // left, top, right, bottom
    int32_t       margin[4];    // left, top, right, bottom
    uint32_t      bg_color;     // background color (ARGB, 0 = none)
    uint32_t      text_color;   // text color (ARGB, 0 = default)
    bool          is_checked;   // for CheckBox/Switch

    // RelativeLayout positioning flags (bit field)
    uint16_t      relative_flags;
    // RelativeLayout sibling references (view IDs)
    uint32_t      rel_above;       // layout_above: view ID
    uint32_t      rel_below;       // layout_below: view ID
    uint32_t      rel_left_of;     // layout_toLeftOf: view ID
    uint32_t      rel_right_of;    // layout_toRightOf: view ID

    // Image data (for ImageView - extracted from APK drawable resources)
    uint8_t     *image_data;       // PNG/JPEG bytes (owned, freed on destroy)
    uint32_t     image_data_len;   // length of image_data

    // Click listener (reference to DxObject implementing OnClickListener)
    DxObject    *click_listener;

    // Back-reference to runtime object
    DxObject    *runtime_obj;

    // Tree structure
    DxUINode    *parent;
    DxUINode   **children;
    uint32_t     child_count;
    uint32_t     child_capacity;
};

// Render model node - serialized for Swift bridge consumption
typedef struct DxRenderNode {
    DxViewType   type;
    uint32_t     view_id;
    DxVisibility visibility;
    char        *text;
    char        *hint;
    DxOrientation orientation;
    float         text_size;
    int32_t       width;        // -1 = match_parent, -2 = wrap_content
    int32_t       height;       // -1 = match_parent, -2 = wrap_content
    float         weight;       // layout_weight (0 = none)
    int32_t       gravity;
    int32_t       padding[4];
    int32_t       margin[4];
    uint32_t      bg_color;
    uint32_t      text_color;
    bool          is_checked;
    bool          has_click_listener;
    uint16_t      relative_flags;
    uint32_t      rel_above;
    uint32_t      rel_below;
    uint32_t      rel_left_of;
    uint32_t      rel_right_of;

    // Image data (for ImageView)
    const uint8_t *image_data;     // PNG/JPEG bytes (NOT owned - points into DxUINode data)
    uint32_t       image_data_len;

    struct DxRenderNode *children;
    uint32_t     child_count;
} DxRenderNode;

// Render model - complete UI snapshot for Swift
struct DxRenderModel {
    DxRenderNode *root;
    uint32_t      version;      // incremented on each update
};

// UI tree operations
DxUINode *dx_ui_node_create(DxViewType type, uint32_t view_id);
void      dx_ui_node_destroy(DxUINode *node);
void      dx_ui_node_add_child(DxUINode *parent, DxUINode *child);
DxUINode *dx_ui_node_find_by_id(DxUINode *root, uint32_t view_id);
void      dx_ui_node_set_text(DxUINode *node, const char *text);
uint32_t  dx_ui_node_count(const DxUINode *node);
uint32_t  dx_ui_node_score_layout(const DxUINode *root);

// Render model
DxRenderModel *dx_render_model_create(DxUINode *root);
void           dx_render_model_destroy(DxRenderModel *model);

// Layout XML parsing -> UI tree
DxResult dx_layout_parse(DxContext *ctx, const uint8_t *xml_data, uint32_t xml_size, DxUINode **out);

// Dimension unit conversion (dp/sp/px -> iOS points)
float dx_ui_dp_to_points(float dp);
float dx_ui_sp_to_points(float sp);

#endif // DX_VIEW_H
