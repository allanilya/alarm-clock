//
//  PersistenceController.swift
//  Alarm Clock
//
//  Created by Allan on 1/16/26.
//

import CoreData
import Foundation

/// CoreData persistence controller for managing alarm storage
class PersistenceController {

    // MARK: - Singleton

    static let shared = PersistenceController()

    // MARK: - Preview Instance

    /// Preview instance with in-memory store for SwiftUI previews
    static var preview: PersistenceController = {
        let controller = PersistenceController(inMemory: true)
        let viewContext = controller.container.viewContext

        // Create sample alarms for preview
        for i in 0..<3 {
            let entity = AlarmEntity(context: viewContext)
            entity.id = Int16(i)
            entity.hour = Int16([7, 22, 8][i])
            entity.minute = Int16([0, 30, 30][i])
            entity.daysOfWeek = Int16([0x3E, 0x7F, 0x41][i])
            entity.sound = ["tone1", "tone2", "tone3"][i]
            entity.enabled = i < 2
            entity.lastSyncedWithESP32 = Date()
        }

        do {
            try viewContext.save()
        } catch {
            let nsError = error as NSError
            fatalError("Unresolved preview error \(nsError), \(nsError.userInfo)")
        }

        return controller
    }()

    // MARK: - Properties

    let container: NSPersistentContainer

    var viewContext: NSManagedObjectContext {
        return container.viewContext
    }

    // MARK: - Initialization

    init(inMemory: Bool = false) {
        container = NSPersistentContainer(name: "AlarmClock")

        if inMemory {
            container.persistentStoreDescriptions.first?.url = URL(fileURLWithPath: "/dev/null")
        }

        container.loadPersistentStores { description, error in
            if let error = error {
                // In production, handle this error appropriately
                fatalError("Core Data failed to load: \(error.localizedDescription)")
            }
        }

        // Configure context
        container.viewContext.automaticallyMergesChangesFromParent = true
        container.viewContext.mergePolicy = NSMergeByPropertyObjectTrumpMergePolicy
    }

    // MARK: - Save Context

    /// Save changes to persistent store
    func save() {
        let context = container.viewContext

        if context.hasChanges {
            do {
                try context.save()
                print("PersistenceController: Saved context successfully")
            } catch {
                let nsError = error as NSError
                print("PersistenceController: Failed to save context - \(nsError), \(nsError.userInfo)")
            }
        }
    }

    // MARK: - Alarm CRUD Operations

    /// Fetch all alarms from CoreData
    func fetchAlarms() -> [Alarm] {
        let request = NSFetchRequest<AlarmEntity>(entityName: "AlarmEntity")
        request.sortDescriptors = [NSSortDescriptor(keyPath: \AlarmEntity.id, ascending: true)]

        do {
            let entities = try viewContext.fetch(request)
            return entities.map { $0.toAlarm() }
        } catch {
            print("PersistenceController: Failed to fetch alarms - \(error.localizedDescription)")
            return []
        }
    }

    /// Save or update an alarm in CoreData
    func saveAlarm(_ alarm: Alarm) {
        // Check if alarm already exists
        let request = NSFetchRequest<AlarmEntity>(entityName: "AlarmEntity")
        request.predicate = NSPredicate(format: "id == %d", alarm.id)

        do {
            let results = try viewContext.fetch(request)
            let entity: AlarmEntity

            if let existing = results.first {
                // Update existing alarm
                entity = existing
                print("PersistenceController: Updating alarm \(alarm.id)")
            } else {
                // Create new alarm
                entity = AlarmEntity(context: viewContext)
                print("PersistenceController: Creating new alarm \(alarm.id)")
            }

            // Update properties
            entity.id = Int16(alarm.id)
            entity.hour = Int16(alarm.hour)
            entity.minute = Int16(alarm.minute)
            entity.daysOfWeek = Int16(alarm.daysOfWeek)
            entity.sound = alarm.sound
            entity.enabled = alarm.enabled
            entity.label = alarm.label
            entity.snoozeEnabled = alarm.snoozeEnabled
            entity.permantlyDisabled = alarm.permanentlyDisabled  // Note: typo in CoreData
            entity.scheduledDate = alarm.scheduledDate
            entity.lastSyncedWithESP32 = Date()

            save()
        } catch {
            print("PersistenceController: Failed to save alarm - \(error.localizedDescription)")
        }
    }

    /// Save multiple alarms (batch operation)
    func saveAlarms(_ alarms: [Alarm]) {
        for alarm in alarms {
            saveAlarm(alarm)
        }
    }

    /// Delete an alarm by ID
    func deleteAlarm(id: Int) {
        let request = NSFetchRequest<AlarmEntity>(entityName: "AlarmEntity")
        request.predicate = NSPredicate(format: "id == %d", id)

        do {
            let results = try viewContext.fetch(request)
            for entity in results {
                viewContext.delete(entity)
            }
            save()
            print("PersistenceController: Deleted alarm \(id)")
        } catch {
            print("PersistenceController: Failed to delete alarm - \(error.localizedDescription)")
        }
    }

    /// Delete all alarms
    func deleteAllAlarms() {
        let request = NSFetchRequest<NSFetchRequestResult>(entityName: "AlarmEntity")
        let batchDelete = NSBatchDeleteRequest(fetchRequest: request)

        do {
            try viewContext.execute(batchDelete)
            save()
            print("PersistenceController: Deleted all alarms")
        } catch {
            print("PersistenceController: Failed to delete all alarms - \(error.localizedDescription)")
        }
    }

    /// Get alarm by ID
    func getAlarm(id: Int) -> Alarm? {
        let request = NSFetchRequest<AlarmEntity>(entityName: "AlarmEntity")
        request.predicate = NSPredicate(format: "id == %d", id)

        do {
            let results = try viewContext.fetch(request)
            return results.first?.toAlarm()
        } catch {
            print("PersistenceController: Failed to fetch alarm \(id) - \(error.localizedDescription)")
            return nil
        }
    }
}

// MARK: - AlarmEntity Extension

extension AlarmEntity {
    /// Convert AlarmEntity to Alarm model
    func toAlarm() -> Alarm {
        return Alarm(
            id: Int(id),
            hour: Int(hour),
            minute: Int(minute),
            daysOfWeek: Int(daysOfWeek),
            sound: sound ?? "tone1",
            enabled: enabled,
            label: label ?? "Alarm",
            snoozeEnabled: snoozeEnabled,
            permanentlyDisabled: permantlyDisabled,  // Note: typo in CoreData
            scheduledDate: scheduledDate
        )
    }
}
