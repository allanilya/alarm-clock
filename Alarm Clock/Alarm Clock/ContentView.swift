//
//  ContentView.swift
//  Alarm Clock
//
//  Created by Allan on 1/15/26.
//

import SwiftUI

struct ContentView: View {
    @StateObject private var bleManager = BLEManager()

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
        }
    }
}

#Preview {
    ContentView()
}
