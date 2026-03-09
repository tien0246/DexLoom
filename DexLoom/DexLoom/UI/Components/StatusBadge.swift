import SwiftUI

struct StatusBadge: View {
    enum Status {
        case idle, loading, running, error
    }

    let status: Status

    private var color: Color {
        switch status {
        case .idle:    return .dxTextSecondary
        case .loading: return .dxWarning
        case .running: return .dxSecondary
        case .error:   return .dxError
        }
    }

    private var text: String {
        switch status {
        case .idle:    return "Idle"
        case .loading: return "Loading"
        case .running: return "Running"
        case .error:   return "Error"
        }
    }

    var body: some View {
        HStack(spacing: 6) {
            Circle()
                .fill(color)
                .frame(width: 8, height: 8)
            Text(text)
                .font(.dxCaption)
                .foregroundStyle(color)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 4)
        .background(color.opacity(0.15))
        .clipShape(Capsule())
    }
}
