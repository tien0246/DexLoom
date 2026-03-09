import SwiftUI

struct AppRouter: View {
    @State private var appState = AppState()

    var body: some View {
        TabView(selection: $appState.selectedTab) {
            Tab(AppTab.home.rawValue, systemImage: AppTab.home.icon, value: .home) {
                HomeView(bridge: appState.bridge)
            }

            Tab(AppTab.runtime.rawValue, systemImage: AppTab.runtime.icon, value: .runtime) {
                RuntimeView(bridge: appState.bridge)
            }

            Tab(AppTab.logs.rawValue, systemImage: AppTab.logs.icon, value: .logs) {
                LogsView(bridge: appState.bridge)
            }
        }
        .tint(Color.dxPrimary)
    }
}
