//
//  DexLoomApp.swift
//  DexLoom
//
//  Created by 이지안 on 3/7/26.
//

import SwiftUI

@main
struct DexLoomApp: App {
    var body: some Scene {
        WindowGroup {
            AppRouter()
                .preferredColorScheme(.dark)
        }
    }
}
