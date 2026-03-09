import SwiftUI
import UniformTypeIdentifiers

struct HomeView: View {
    @ObservedObject var bridge: RuntimeBridge
    @State private var showFilePicker = false
    @State private var showBundledDemo = false

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 20) {
                    // Header
                    VStack(spacing: 8) {
                        Image(systemName: "cpu")
                            .font(.system(size: 48))
                            .foregroundStyle(Color.dxPrimary)

                        Text("DexLoom")
                            .font(.dxTitle)
                            .foregroundStyle(Color.dxText)

                        Text("Android Runtime Compatibility Layer")
                            .font(.dxCaption)
                            .foregroundStyle(Color.dxTextSecondary)
                    }
                    .padding(.top, 20)

                    // Status card
                    if bridge.isLoaded {
                        VStack(alignment: .leading, spacing: 12) {
                            Label("APK Loaded", systemImage: "checkmark.circle.fill")
                                .foregroundStyle(Color.dxSecondary)
                                .font(.dxHeadline)

                            if let pkg = bridge.packageName {
                                InfoRow(label: "Package", value: pkg)
                            }
                            if let activity = bridge.activityName {
                                InfoRow(label: "Activity", value: activity)
                            }
                            InfoRow(label: "Classes", value: "\(bridge.dexClassCount)")
                            InfoRow(label: "Methods", value: "\(bridge.dexMethodCount)")
                            InfoRow(label: "Strings", value: "\(bridge.dexStringCount)")
                        }
                        .padding()
                        .background(Color.dxSurface)
                        .clipShape(RoundedRectangle(cornerRadius: 12))
                    }

                    // Error
                    if let error = bridge.errorMessage {
                        VStack(alignment: .leading, spacing: 8) {
                            HStack {
                                Image(systemName: "exclamationmark.triangle.fill")
                                    .foregroundStyle(Color.dxError)
                                Text(error)
                                    .font(.dxCaption)
                                    .foregroundStyle(Color.dxError)
                                Spacer()
                                Button {
                                    let logText = bridge.copyLogsToClipboard()
                                    let full = "Error: \(error)\n\n--- Logs ---\n\(logText)"
                                    UIPasteboard.general.string = full
                                } label: {
                                    Image(systemName: "doc.on.doc")
                                        .font(.caption)
                                        .foregroundStyle(Color.dxError)
                                }
                            }
                        }
                        .padding()
                        .background(Color.dxError.opacity(0.1))
                        .clipShape(RoundedRectangle(cornerRadius: 8))
                    }

                    // Actions
                    VStack(spacing: 12) {
                        Button {
                            showFilePicker = true
                        } label: {
                            Label("Import APK File", systemImage: "doc.badge.plus")
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(Color.dxPrimary)
                                .foregroundStyle(.white)
                                .clipShape(RoundedRectangle(cornerRadius: 10))
                                .font(.dxBody)
                        }

                        Button {
                            loadBundledDemo()
                        } label: {
                            Label("Load Demo APK", systemImage: "play.circle")
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(Color.dxSurface)
                                .foregroundStyle(Color.dxPrimary)
                                .clipShape(RoundedRectangle(cornerRadius: 10))
                                .font(.dxBody)
                        }

                        if bridge.isLoaded && !bridge.isRunning {
                            Button {
                                bridge.run()
                            } label: {
                                Label("Run Activity", systemImage: "play.fill")
                                    .frame(maxWidth: .infinity)
                                    .padding()
                                    .background(Color.dxSecondary)
                                    .foregroundStyle(.white)
                                    .clipShape(RoundedRectangle(cornerRadius: 10))
                                    .font(.dxBody)
                            }
                        }
                    }

                    // Info
                    VStack(alignment: .leading, spacing: 8) {
                        Text("About")
                            .font(.dxHeadline)
                            .foregroundStyle(Color.dxText)

                        Text("DexLoom is a research-grade DEX bytecode interpreter that runs inside a standard iOS app sandbox. It interprets Dalvik bytecode and maps Android UI concepts to SwiftUI.")
                            .font(.dxCaption)
                            .foregroundStyle(Color.dxTextSecondary)

                        Text("Supported: Activity/AppCompatActivity, TextView, Button, LinearLayout, ConstraintLayout, full Dalvik opcode set, AndroidX stubs.")
                            .font(.dxCaption)
                            .foregroundStyle(Color.dxTextSecondary)
                    }
                    .padding()
                    .background(Color.dxSurface)
                    .clipShape(RoundedRectangle(cornerRadius: 12))

                    Spacer()
                }
                .padding()
            }
            .background(Color.dxBackground)
            .navigationTitle("DexLoom")
            .fileImporter(
                isPresented: $showFilePicker,
                allowedContentTypes: [.init(filenameExtension: "apk") ?? .data],
                allowsMultipleSelection: false
            ) { result in
                handleFileImport(result)
            }
        }
    }

    private func handleFileImport(_ result: Result<[URL], Error>) {
        switch result {
        case .success(let urls):
            guard let url = urls.first else { return }

            // Copy to app sandbox
            let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            let dest = docs.appendingPathComponent(url.lastPathComponent)

            do {
                guard url.startAccessingSecurityScopedResource() else { return }
                defer { url.stopAccessingSecurityScopedResource() }

                if FileManager.default.fileExists(atPath: dest.path) {
                    try FileManager.default.removeItem(at: dest)
                }
                try FileManager.default.copyItem(at: url, to: dest)
                bridge.loadAPK(at: dest)
            } catch {
                bridge.errorMessage = "Import failed: \(error.localizedDescription)"
            }

        case .failure(let error):
            bridge.errorMessage = "File picker error: \(error.localizedDescription)"
        }
    }

    private func loadBundledDemo() {
        if let url = Bundle.main.url(forResource: "HelloAndroid", withExtension: "apk") {
            bridge.loadAPK(at: url)
        } else {
            bridge.errorMessage = "Demo APK not found in bundle. Place HelloAndroid.apk in Samples/."
        }
    }
}

struct InfoRow: View {
    let label: String
    let value: String

    var body: some View {
        HStack {
            Text(label)
                .font(.dxCaption)
                .foregroundStyle(Color.dxTextSecondary)
            Spacer()
            Text(value)
                .font(.dxCode)
                .foregroundStyle(Color.dxText)
                .lineLimit(1)
                .truncationMode(.middle)
        }
    }
}
