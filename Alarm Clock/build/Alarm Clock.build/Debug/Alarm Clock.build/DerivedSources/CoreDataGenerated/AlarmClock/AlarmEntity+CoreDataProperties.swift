//
//  AlarmEntity+CoreDataProperties.swift
//  
//
//  Created by Allan on 1/16/26.
//
//  This file was automatically generated and should not be edited.
//

import Foundation
import CoreData


extension AlarmEntity {

    @nonobjc public class func fetchRequest() -> NSFetchRequest<AlarmEntity> {
        return NSFetchRequest<AlarmEntity>(entityName: "AlarmEntity")
    }

    @NSManaged public var daysOfWeek: Int16
    @NSManaged public var enabled: Bool
    @NSManaged public var hour: Int16
    @NSManaged public var id: Int16
    @NSManaged public var lastSyncedWithESP32: Date?
    @NSManaged public var minute: Int16
    @NSManaged public var sound: String?

}

extension AlarmEntity : Identifiable {

}
