//
//  LabelView.swift
//  Alarm Clock
//
//  Created by Allan on 1/16/26.
//

import SwiftUI

struct LabelView: View {
    @Binding var label: String
    @Environment(\.dismiss) private var dismiss
    @FocusState private var isFocused: Bool

    var body: some View {
        List {
            Section {
                TextField("Alarm", text: $label)
                    .textInputAutocapitalization(.sentences)
                    .focused($isFocused)
            }
        }
        .navigationTitle("Label")
        .onAppear {
            // Auto-focus the text field when view appears
            isFocused = true
        }
    }
}

#Preview {
    NavigationStack {
        LabelView(label: .constant("Morning Alarm"))
    }
}
