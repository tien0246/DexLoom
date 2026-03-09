import SwiftUI

struct RuntimeView: View {
    @ObservedObject var bridge: RuntimeBridge

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                if bridge.isRunning, let root = bridge.renderTree {
                    // Render the Android UI tree
                    ScrollView {
                        AndroidViewRenderer(node: root, bridge: bridge)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding()
                    }
                    .background(Color.white)
                } else if bridge.isRunning {
                    // Running but no render tree yet
                    VStack(spacing: 16) {
                        Image(systemName: "checkmark.circle")
                            .font(.system(size: 64))
                            .foregroundStyle(Color.green)
                        Text("Activity executed successfully")
                            .font(.dxBody)
                            .foregroundStyle(Color.dxText)
                        Text("No visual UI was produced.\nThe app may use Compose or have no setContentView call.")
                            .font(.dxCaption)
                            .foregroundStyle(Color.dxTextSecondary)
                            .multilineTextAlignment(.center)
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .background(Color.dxBackground)
                } else if bridge.isLoaded {
                    VStack(spacing: 16) {
                        Image(systemName: "play.circle")
                            .font(.system(size: 64))
                            .foregroundStyle(Color.dxTextSecondary)
                        Text("APK loaded. Tap Run to start.")
                            .font(.dxBody)
                            .foregroundStyle(Color.dxTextSecondary)
                        Button("Run Activity") {
                            bridge.run()
                        }
                        .buttonStyle(.borderedProminent)
                        .tint(Color.dxPrimary)

                        if let error = bridge.errorMessage {
                            VStack(spacing: 8) {
                                Text(error)
                                    .font(.dxCaption)
                                    .foregroundStyle(Color.dxError)
                                    .multilineTextAlignment(.center)
                                Button("Copy Error + Logs") {
                                    let logText = bridge.copyLogsToClipboard()
                                    let full = "Error: \(error)\n\n--- Logs ---\n\(logText)"
                                    UIPasteboard.general.string = full
                                }
                                .font(.dxCaption)
                                .foregroundStyle(Color.dxPrimary)
                            }
                            .padding()
                            .background(Color.dxError.opacity(0.1))
                            .clipShape(RoundedRectangle(cornerRadius: 8))
                        }
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .background(Color.dxBackground)
                } else {
                    VStack(spacing: 16) {
                        Image(systemName: "cpu")
                            .font(.system(size: 64))
                            .foregroundStyle(Color.dxTextSecondary)
                        Text("No APK loaded")
                            .font(.dxBody)
                            .foregroundStyle(Color.dxTextSecondary)
                        Text("Import an APK from the Home tab")
                            .font(.dxCaption)
                            .foregroundStyle(Color.dxTextSecondary)
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .background(Color.dxBackground)
                }
            }
            .navigationTitle("Runtime")
        }
    }
}

// MARK: - Color helpers

private func argbColor(_ argb: UInt32) -> Color {
    let a = Double((argb >> 24) & 0xFF) / 255.0
    let r = Double((argb >> 16) & 0xFF) / 255.0
    let g = Double((argb >> 8) & 0xFF) / 255.0
    let b = Double(argb & 0xFF) / 255.0
    return Color(.sRGB, red: r, green: g, blue: b, opacity: a)
}

// MARK: - Android View Renderer

struct AndroidViewRenderer: View {
    let node: RenderNode
    let bridge: RuntimeBridge

    var body: some View {
        if (node.type == DX_VIEW_TEXT_VIEW || node.type == DX_VIEW_BUTTON ||
           node.type == DX_VIEW_EDIT_TEXT || node.type == DX_VIEW_IMAGE_VIEW ||
           node.type == DX_VIEW_SWITCH || node.type == DX_VIEW_CHECKBOX ||
           node.type == DX_VIEW_RADIO_BUTTON || node.type == DX_VIEW_PROGRESS_BAR ||
           node.type == DX_VIEW_SEEK_BAR || node.type == DX_VIEW_RATING_BAR ||
           node.type == DX_VIEW_SPINNER || node.type == DX_VIEW_CHIP ||
           node.type == DX_VIEW_WEB_VIEW || node.type == DX_VIEW_VIEW)
           && node.children.isEmpty {
            leafView
                .applyAndroidStyle(node: node)
        } else {
            // Container views
            containerView
                .applyAndroidStyle(node: node)
        }
    }

    // MARK: - Leaf views

    @ViewBuilder
    private var leafView: some View {
        switch node.type {
        case DX_VIEW_TEXT_VIEW:
            textView

        case DX_VIEW_BUTTON:
            buttonView

        case DX_VIEW_EDIT_TEXT:
            editTextView

        case DX_VIEW_IMAGE_VIEW:
            imageView

        case DX_VIEW_SWITCH:
            switchView

        case DX_VIEW_CHECKBOX:
            checkboxView

        case DX_VIEW_RADIO_BUTTON:
            radioButtonView

        case DX_VIEW_PROGRESS_BAR:
            ProgressView()
                .progressViewStyle(.linear)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)

        case DX_VIEW_SEEK_BAR:
            seekBarView

        case DX_VIEW_RATING_BAR:
            ratingBarView

        case DX_VIEW_SPINNER:
            spinnerView

        case DX_VIEW_CHIP:
            chipView

        case DX_VIEW_WEB_VIEW:
            webViewPlaceholder

        case DX_VIEW_VIEW:
            // Generic <View/> — spacer or divider
            Rectangle()
                .fill(node.bgColor != 0 ? argbColor(node.bgColor) : Color.clear)
                .frame(height: 1)

        default:
            EmptyView()
        }
    }

    // MARK: - Container views

    @ViewBuilder
    private var containerView: some View {
        switch node.type {
        case DX_VIEW_LINEAR_LAYOUT:
            if node.orientation == DX_ORIENTATION_VERTICAL {
                VStack(alignment: .leading, spacing: 0) {
                    childViews
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            } else {
                HStack(alignment: .center, spacing: 0) {
                    childViews
                }
            }

        case DX_VIEW_CONSTRAINT_LAYOUT, DX_VIEW_FRAME_LAYOUT:
            // Approximate: overlay children (constraint solving is impractical)
            ZStack(alignment: .topLeading) {
                childViews
            }
            .frame(maxWidth: .infinity, alignment: .leading)

        case DX_VIEW_RELATIVE_LAYOUT:
            // RelativeLayout: use ZStack with per-child alignment based on relative_flags
            ZStack {
                ForEach(node.children) { child in
                    AndroidViewRenderer(node: child, bridge: bridge)
                        .frame(
                            maxWidth: relativeNeedsFullWidth(child) ? .infinity : nil,
                            maxHeight: relativeNeedsFullHeight(child) ? .infinity : nil,
                            alignment: relativeChildAlignment(child)
                        )
                }
            }
            .frame(maxWidth: .infinity, alignment: .topLeading)

        case DX_VIEW_SCROLL_VIEW:
            ScrollView {
                VStack(alignment: .leading, spacing: 0) {
                    childViews
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }

        case DX_VIEW_CARD_VIEW:
            VStack(alignment: .leading, spacing: 0) {
                childViews
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Color.white)
            .clipShape(RoundedRectangle(cornerRadius: 8))
            .shadow(color: .black.opacity(0.12), radius: 4, x: 0, y: 2)

        case DX_VIEW_TOOLBAR:
            HStack(spacing: 8) {
                if let text = node.text {
                    Text(text)
                        .font(.headline)
                        .foregroundStyle(.primary)
                }
                Spacer()
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 12)
            .background(Color(.systemBackground))

        case DX_VIEW_LIST_VIEW, DX_VIEW_GRID_VIEW:
            // ListView / GridView — vertical list of adapter-provided children
            ScrollView {
                VStack(alignment: .leading, spacing: 1) {
                    childViews
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)

        case DX_VIEW_TAB_LAYOUT:
            // TabLayout — render tabs as horizontal buttons
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 0) {
                    childViews
                }
            }
            .frame(maxWidth: .infinity)

        case DX_VIEW_VIEW_PAGER:
            // ViewPager — show first page only (no swipe)
            ZStack(alignment: .topLeading) {
                if let firstChild = node.children.first {
                    AndroidViewRenderer(node: firstChild, bridge: bridge)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)

        case DX_VIEW_RADIO_GROUP:
            // RadioGroup — vertical list of radio buttons
            VStack(alignment: .leading, spacing: 4) {
                childViews
            }
            .frame(maxWidth: .infinity, alignment: .leading)

        case DX_VIEW_BOTTOM_NAV:
            // BottomNavigationView — horizontal bar at bottom
            HStack {
                childViews
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 8)
            .background(Color(.systemBackground))
            .overlay(alignment: .top) {
                Divider()
            }

        case DX_VIEW_FAB:
            // FloatingActionButton — circular button
            Button {
                bridge.dispatchClick(viewId: node.viewId)
            } label: {
                Image(systemName: "plus")
                    .font(.title2)
                    .foregroundStyle(.white)
                    .frame(width: 56, height: 56)
                    .background(node.bgColor != 0 ? argbColor(node.bgColor) : Color.blue)
                    .clipShape(Circle())
                    .shadow(color: .black.opacity(0.2), radius: 6, x: 0, y: 3)
            }

        default:
            // DX_VIEW_VIEW_GROUP, DX_VIEW_RECYCLER_VIEW, etc.
            VStack(alignment: .leading, spacing: 0) {
                childViews
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    // MARK: - Child rendering

    @ViewBuilder
    private var childViews: some View {
        ForEach(node.children) { child in
            if child.type.rawValue >= 0 {  // always true, just keeps ForEach happy
                AndroidViewRenderer(node: child, bridge: bridge)
            }
        }
    }

    // MARK: - Specific view builders

    private var textView: some View {
        Text(node.text ?? "")
            .font(.system(size: max(sp(node.textSize), 1)))
            .foregroundStyle(node.textColor != 0 ? argbColor(node.textColor) : .black)
            .multilineTextAlignment(gravityAlignment)
            .frame(maxWidth: .infinity, alignment: gravityFrameAlignment)
    }

    private var buttonView: some View {
        Button {
            bridge.dispatchClick(viewId: node.viewId)
        } label: {
            Text(node.text ?? "Button")
                .font(.system(size: max(sp(node.textSize), 1)))
                .padding(.horizontal, 16)
                .padding(.vertical, 10)
                .frame(maxWidth: .infinity)
                .background(Color.blue.opacity(0.1))
                .foregroundStyle(.blue)
                .clipShape(RoundedRectangle(cornerRadius: 6))
                .overlay(
                    RoundedRectangle(cornerRadius: 6)
                        .stroke(Color.blue.opacity(0.3), lineWidth: 1)
                )
        }
    }

    private var editTextView: some View {
        EditTextFieldView(
            initialText: node.text ?? "",
            hint: node.hint ?? "",
            textSize: sp(node.textSize),
            viewId: node.viewId,
            bridge: bridge
        )
    }

    private var imageView: some View {
        Group {
            if let data = node.imageData, let uiImage = UIImage(data: data) {
                Image(uiImage: uiImage)
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .frame(maxWidth: .infinity, minHeight: 40)
            } else {
                Image(systemName: "photo")
                    .font(.system(size: 40))
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, minHeight: 60)
            }
        }
    }

    private var switchView: some View {
        HStack {
            if let text = node.text {
                Text(text)
                    .font(.system(size: max(sp(node.textSize), 1)))
            }
            Spacer()
            Image(systemName: node.isChecked ? "switch.2" : "switch.2")
                .foregroundStyle(node.isChecked ? .blue : .gray)
        }
    }

    private var checkboxView: some View {
        HStack(spacing: 8) {
            Image(systemName: node.isChecked ? "checkmark.square.fill" : "square")
                .foregroundStyle(node.isChecked ? .blue : .gray)
            if let text = node.text {
                Text(text)
                    .font(.system(size: max(sp(node.textSize), 1)))
            }
        }
    }

    private var radioButtonView: some View {
        HStack(spacing: 8) {
            Image(systemName: node.isChecked ? "circle.inset.filled" : "circle")
                .foregroundStyle(node.isChecked ? .blue : .gray)
            if let text = node.text {
                Text(text)
                    .font(.system(size: max(sp(node.textSize), 1)))
            }
        }
    }

    private var seekBarView: some View {
        HStack {
            Rectangle()
                .fill(Color.blue)
                .frame(width: 80, height: 4)
            Circle()
                .fill(Color.blue)
                .frame(width: 20, height: 20)
            Rectangle()
                .fill(Color.gray.opacity(0.3))
                .frame(height: 4)
        }
        .frame(maxWidth: .infinity, minHeight: 32)
    }

    private var ratingBarView: some View {
        HStack(spacing: 4) {
            ForEach(0..<5, id: \.self) { i in
                Image(systemName: i < 3 ? "star.fill" : "star")
                    .foregroundStyle(i < 3 ? .yellow : .gray.opacity(0.4))
            }
        }
    }

    private var spinnerView: some View {
        HStack {
            Text(node.text ?? "Select...")
                .foregroundStyle(node.text != nil ? .primary : .secondary)
            Spacer()
            Image(systemName: "chevron.down")
                .foregroundStyle(.secondary)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
        .background(
            RoundedRectangle(cornerRadius: 4)
                .stroke(Color.gray.opacity(0.4), lineWidth: 1)
        )
    }

    private var chipView: some View {
        Text(node.text ?? "Chip")
            .font(.system(size: max(sp(node.textSize), 12)))
            .padding(.horizontal, 12)
            .padding(.vertical, 6)
            .background(
                Capsule()
                    .fill(node.bgColor != 0 ? argbColor(node.bgColor) : Color.gray.opacity(0.15))
            )
            .overlay(
                Capsule()
                    .stroke(Color.gray.opacity(0.3), lineWidth: 1)
            )
    }

    private var webViewPlaceholder: some View {
        VStack(spacing: 8) {
            Image(systemName: "globe")
                .font(.system(size: 32))
                .foregroundStyle(.secondary)
            Text("WebView")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, minHeight: 100)
        .background(Color.gray.opacity(0.05))
    }

    // MARK: - Gravity helpers

    private var gravityAlignment: TextAlignment {
        let h = node.gravity & 0x07  // horizontal gravity bits
        if h == 1 { return .center }       // CENTER_HORIZONTAL
        if h == 5 { return .trailing }     // RIGHT / END
        return .leading
    }

    private var gravityFrameAlignment: Alignment {
        let h = node.gravity & 0x07
        if h == 1 { return .center }
        if h == 5 { return .trailing }
        return .leading
    }

    // MARK: - RelativeLayout helpers

    /// Compute SwiftUI alignment for a child inside a RelativeLayout ZStack
    private func relativeChildAlignment(_ child: RenderNode) -> Alignment {
        let f = child.relativeFlags

        // centerInParent takes priority
        if f & UInt16(DX_REL_CENTER_IN_PARENT) != 0 {
            return .center
        }

        // Determine horizontal alignment
        let left   = f & UInt16(DX_REL_ALIGN_PARENT_LEFT) != 0
        let right  = f & UInt16(DX_REL_ALIGN_PARENT_RIGHT) != 0
        let centerH = f & UInt16(DX_REL_CENTER_HORIZONTAL) != 0

        // Determine vertical alignment
        let top    = f & UInt16(DX_REL_ALIGN_PARENT_TOP) != 0
        let bottom = f & UInt16(DX_REL_ALIGN_PARENT_BOTTOM) != 0
        let centerV = f & UInt16(DX_REL_CENTER_VERTICAL) != 0

        let h: HorizontalAlignment
        if centerH || (left && right) {
            h = .center
        } else if right {
            h = .trailing
        } else {
            h = .leading   // default or explicit left
        }

        let v: VerticalAlignment
        if centerV || (top && bottom) {
            v = .center
        } else if bottom {
            v = .bottom
        } else {
            v = .top       // default or explicit top
        }

        return Alignment(horizontal: h, vertical: v)
    }

    /// Whether this child should stretch horizontally (centerHorizontal or both left+right)
    private func relativeNeedsFullWidth(_ child: RenderNode) -> Bool {
        let f = child.relativeFlags
        let left  = f & UInt16(DX_REL_ALIGN_PARENT_LEFT) != 0
        let right = f & UInt16(DX_REL_ALIGN_PARENT_RIGHT) != 0
        return (left && right) || child.width == -1 // match_parent
    }

    /// Whether this child should stretch vertically (centerVertical or both top+bottom)
    private func relativeNeedsFullHeight(_ child: RenderNode) -> Bool {
        let f = child.relativeFlags
        let top    = f & UInt16(DX_REL_ALIGN_PARENT_TOP) != 0
        let bottom = f & UInt16(DX_REL_ALIGN_PARENT_BOTTOM) != 0
        return (top && bottom) || child.height == -1 // match_parent
    }
}

// MARK: - dp/sp/px to iOS points conversion
//
// Android dp (density-independent pixels) and iOS points are both defined
// relative to a 160dpi baseline. On iOS, 1pt = 1/163" (effectively 160dpi),
// so 1dp = 1pt regardless of screen scale (@2x, @3x).
//
// The C layer (dx_ui_decode_dimension) already converts all dimension units
// (px, dp, sp, pt, in, mm) to iOS-point-equivalent values before they reach
// Swift, so these functions apply no additional scaling. They exist as named
// conversion points so the mapping is explicit and can be adjusted if needed
// (e.g., for user-configurable text scaling).

/// Scale factor applied to all dp values. 1.0 is correct for standard iOS displays.
private let dpScale: CGFloat = 1.0

/// Scale factor for sp (text) values. Increase for accessibility large-text support.
private let spScale: CGFloat = 1.0

/// Convert Android dp to iOS points.
private func dp(_ value: Int32) -> CGFloat {
    CGFloat(value) * dpScale
}

/// Convert Android sp (text size) to iOS points.
private func sp(_ value: Float) -> CGFloat {
    CGFloat(value) * spScale
}

// MARK: - Android style modifier

private struct AndroidStyleModifier: ViewModifier {
    let node: RenderNode

    func body(content: Content) -> some View {
        content
            .applyLayoutSize(width: node.width, height: node.height, weight: node.weight)
            .padding(.leading, dp(node.padding.0))
            .padding(.top, dp(node.padding.1))
            .padding(.trailing, dp(node.padding.2))
            .padding(.bottom, dp(node.padding.3))
            .background(node.bgColor != 0 ? argbColor(node.bgColor) : Color.clear)
            .padding(.leading, dp(node.margin.0))
            .padding(.top, dp(node.margin.1))
            .padding(.trailing, dp(node.margin.2))
            .padding(.bottom, dp(node.margin.3))
    }
}

private struct LayoutSizeModifier: ViewModifier {
    let width: Int32
    let height: Int32
    let weight: Float

    func body(content: Content) -> some View {
        content
            .frame(
                minWidth: widthMin, idealWidth: nil, maxWidth: widthMax,
                minHeight: heightMin, idealHeight: nil, maxHeight: heightMax
            )
    }

    // width: -1 match_parent -> maxWidth: .infinity
    //        -2 wrap_content -> natural size (nil)
    //        >0 specific dp  -> fixed width (converted via dp())
    private var widthMin: CGFloat? {
        if width > 0 { return dp(width) }
        return nil
    }
    private var widthMax: CGFloat? {
        if width == -1 || weight > 0 { return .infinity }   // match_parent or weighted
        if width > 0 { return dp(width) }
        return nil  // wrap_content
    }
    private var heightMin: CGFloat? {
        if height > 0 { return dp(height) }
        return nil
    }
    private var heightMax: CGFloat? {
        if height == -1 { return .infinity }
        if height > 0 { return dp(height) }
        return nil  // wrap_content
    }
}

extension View {
    fileprivate func applyLayoutSize(width: Int32, height: Int32, weight: Float) -> some View {
        modifier(LayoutSizeModifier(width: width, height: height, weight: weight))
    }
}

extension View {
    fileprivate func applyAndroidStyle(node: RenderNode) -> some View {
        modifier(AndroidStyleModifier(node: node))
    }
}

// MARK: - EditText interactive view

private struct EditTextFieldView: View {
    let initialText: String
    let hint: String
    let textSize: CGFloat
    let viewId: UInt32
    let bridge: RuntimeBridge

    @State private var text: String = ""
    @FocusState private var isFocused: Bool

    var body: some View {
        TextField(hint, text: $text)
            .font(.system(size: max(textSize, 1)))
            .padding(.horizontal, 8)
            .frame(maxWidth: .infinity)
            .frame(height: 44)
            .background(
                RoundedRectangle(cornerRadius: 4)
                    .stroke(isFocused ? Color.blue : Color.gray.opacity(0.4), lineWidth: isFocused ? 2 : 1)
            )
            .focused($isFocused)
            .onAppear { text = initialText }
            .onChange(of: text) { _, newValue in
                bridge.updateEditText(viewId: viewId, text: newValue)
            }
    }
}
