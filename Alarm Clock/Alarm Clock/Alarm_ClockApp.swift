//
//  Alarm_ClockApp.swift
//  Alarm Clock
//
//  Created by Allan on 1/15/26.
//

import SwiftUI

@main
struct Alarm_ClockApp: App {
    // Initialize CoreData persistence controller
    let persistenceController = PersistenceController.shared

    // Initialize BLE Manager with persistence
    @StateObject private var bleManager = BLEManager(persistenceController: .shared)

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environment(\.managedObjectContext, persistenceController.viewContext)
                .environmentObject(bleManager)
        }
    }
}
