//
//  ContentView.swift
//  Alarm Clock
//
//  Created by Allan on 1/15/26.
//

import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var bleManager: BLEManager

    var body: some View {
        TabView {
            AlarmListView(bleManager: bleManager)
                .tabItem {
                    Label("Alarms", systemImage: "alarm.fill")
                }

            TimeView(bleManager: bleManager)
                .tabItem {
                    Label("Time Sync", systemImage: "clock.arrow.circlepath")
                }

            SettingsView()
                .tabItem {
                    Label("Settings", systemImage: "gear")
                }
                .environmentObject(bleManager)
        }
    }
}

#Preview {
    ContentView()
        .environmentObject(BLEManager(persistenceController: .preview))
}
