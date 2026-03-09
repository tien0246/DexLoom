import SwiftUI
import UniformTypeIdentifiers

struct LogsView: View {
    @ObservedObject var bridge: RuntimeBridge
    @State private var filterLevel: String = "ALL"
    @State private var autoScroll = true
    @State private var showCopiedToast = false

    private let levels = ["ALL", "TRACE", "DEBUG", "INFO", "WARN", "ERROR"]

    var filteredLogs: [LogEntry] {
        if filterLevel == "ALL" {
            return bridge.logs
        }
        return bridge.logs.filter { $0.level == filterLevel }
    }

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                // Filter bar
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 8) {
                        ForEach(levels, id: \.self) { level in
                            Button(level) {
                                filterLevel = level
                            }
                            .font(.dxCaption)
                            .padding(.horizontal, 10)
                            .padding(.vertical, 6)
                            .background(filterLevel == level ? Color.dxPrimary : Color.dxSurface)
                            .foregroundStyle(filterLevel == level ? Color.white : Color.dxTextSecondary)
                            .clipShape(Capsule())
                        }

                        Spacer()

                        // Copy logs button
                        Button {
                            let text = bridge.copyLogsToClipboard()
                            UIPasteboard.general.string = text
                            showCopiedToast = true
                            DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
                                showCopiedToast = false
                            }
                        } label: {
                            Label("Copy", systemImage: "doc.on.doc")
                                .font(.dxCaption)
                                .foregroundStyle(Color.dxPrimary)
                        }

                        Button("Clear") {
                            bridge.logs.removeAll()
                        }
                        .font(.dxCaption)
                        .foregroundStyle(Color.dxError)
                    }
                    .padding(.horizontal)
                    .padding(.vertical, 8)
                }
                .background(Color.dxSurface)

                // Log list
                ScrollViewReader { proxy in
                    ScrollView {
                        LazyVStack(alignment: .leading, spacing: 2) {
                            ForEach(filteredLogs) { entry in
                                LogRowView(entry: entry)
                                    .id(entry.id)
                                    .contextMenu {
                                        Button {
                                            UIPasteboard.general.string = "[\(entry.level)] \(entry.tag): \(entry.message)"
                                        } label: {
                                            Label("Copy Line", systemImage: "doc.on.doc")
                                        }
                                    }
                            }
                        }
                        .padding(.horizontal, 8)
                        .padding(.vertical, 4)
                    }
                    .onChange(of: bridge.logs.count) {
                        if autoScroll, let last = filteredLogs.last {
                            proxy.scrollTo(last.id, anchor: .bottom)
                        }
                    }
                }
            }
            .background(Color.dxBackground)
            .navigationTitle("Logs (\(filteredLogs.count))")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Menu {
                        Button {
                            autoScroll.toggle()
                        } label: {
                            Label(autoScroll ? "Auto-scroll On" : "Auto-scroll Off",
                                  systemImage: autoScroll ? "checkmark.circle.fill" : "circle")
                        }

                        Divider()

                        Button {
                            let text = bridge.copyLogsToClipboard()
                            UIPasteboard.general.string = text
                            showCopiedToast = true
                            DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
                                showCopiedToast = false
                            }
                        } label: {
                            Label("Copy All Logs", systemImage: "doc.on.doc")
                        }

                        // Copy errors only
                        Button {
                            let errorLogs = bridge.logs.filter { $0.level == "ERROR" || $0.level == "WARN" }
                            let text = errorLogs.map { entry in
                                "[\(entry.level)] \(entry.tag): \(entry.message)"
                            }.joined(separator: "\n")
                            UIPasteboard.general.string = text.isEmpty ? "(no errors)" : text
                            showCopiedToast = true
                            DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
                                showCopiedToast = false
                            }
                        } label: {
                            Label("Copy Errors Only", systemImage: "exclamationmark.triangle")
                        }
                    } label: {
                        Image(systemName: "ellipsis.circle")
                    }
                }
            }
            .overlay {
                if showCopiedToast {
                    VStack {
                        Spacer()
                        Text("Copied to clipboard")
                            .font(.dxCaption)
                            .padding(.horizontal, 16)
                            .padding(.vertical, 10)
                            .background(.ultraThinMaterial)
                            .clipShape(Capsule())
                            .padding(.bottom, 20)
                    }
                    .transition(.move(edge: .bottom).combined(with: .opacity))
                    .animation(.easeInOut(duration: 0.3), value: showCopiedToast)
                }
            }
        }
    }
}

struct LogRowView: View {
    let entry: LogEntry

    private var levelColor: Color {
        switch entry.level {
        case "ERROR": return .dxError
        case "WARN":  return .dxWarning
        case "INFO":  return .dxSecondary
        case "DEBUG": return .dxPrimary
        default:      return .dxTextSecondary
        }
    }

    private static let timeFormatter: DateFormatter = {
        let f = DateFormatter()
        f.dateFormat = "HH:mm:ss.SSS"
        return f
    }()

    var body: some View {
        HStack(alignment: .top, spacing: 6) {
            Text(Self.timeFormatter.string(from: entry.timestamp))
                .font(.system(size: 10, design: .monospaced))
                .foregroundStyle(Color.dxTextSecondary)

            Text(entry.level.prefix(1))
                .font(.system(size: 10, weight: .bold, design: .monospaced))
                .foregroundStyle(levelColor)
                .frame(width: 12)

            Text(entry.tag)
                .font(.system(size: 10, weight: .medium, design: .monospaced))
                .foregroundStyle(Color.dxPrimary)
                .frame(width: 60, alignment: .leading)
                .lineLimit(1)

            Text(entry.message)
                .font(.system(size: 11, design: .monospaced))
                .foregroundStyle(Color.dxText)
                .lineLimit(3)
        }
        .padding(.vertical, 2)
    }
}
