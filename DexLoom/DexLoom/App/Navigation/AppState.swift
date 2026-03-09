import SwiftUI

enum AppTab: String, CaseIterable {
    case home = "Home"
    case runtime = "Runtime"
    case logs = "Logs"

    var icon: String {
        switch self {
        case .home: return "house.fill"
        case .runtime: return "play.rectangle.fill"
        case .logs: return "doc.text.fill"
        }
    }
}

@MainActor
@Observable
final class AppState {
    var selectedTab: AppTab = .home
    var bridge = RuntimeBridge()
}
