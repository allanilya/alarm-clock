//
//  SoundView.swift
//  Alarm Clock
//
//  Created by Allan on 1/16/26.
//

import SwiftUI
import UniformTypeIdentifiers

struct SoundView: View {
    @Binding var selectedSound: String
    @EnvironmentObject var bleManager: BLEManager
    @Environment(\.dismiss) private var dismiss
    
    @State private var showingFilePicker = false
    @State private var isUploading = false
    @State private var showingError = false
    @State private var errorMessage = ""

    // Available alarm sounds
    let availableSounds = [
        "tone1": "Tone 1 (Low)",
        "tone2": "Tone 2 (Medium)",
        "tone3": "Tone 3 (High)"
    ]

    var body: some View {
        List {
            Section {
                ForEach(Array(availableSounds.keys.sorted()), id: \.self) { soundKey in
                    Button(action: {
                        selectedSound = soundKey
                    }) {
                        HStack {
                            Image(systemName: "waveform")
                                .foregroundColor(.secondary)
                            Text(availableSounds[soundKey] ?? soundKey)
                                .foregroundColor(.primary)
                            Spacer()
                            if selectedSound == soundKey {
                                Image(systemName: "checkmark")
                                    .foregroundColor(.blue)
                            }
                        }
                    }
                }
            } header: {
                Text("Built-in Tones")
            }

            // Custom sounds section
            Section {
                if bleManager.availableCustomSounds.isEmpty {
                    Text("No custom sounds uploaded yet")
                        .foregroundColor(.secondary)
                        .font(.callout)
                } else {
                    ForEach(bleManager.availableCustomSounds, id: \.self) { soundFile in
                        Button(action: {
                            selectedSound = soundFile
                        }) {
                            HStack {
                                Image(systemName: "music.note")
                                    .foregroundColor(.secondary)
                                Text(displayName(for: soundFile))
                                    .foregroundColor(.primary)
                                Spacer()
                                if selectedSound == soundFile {
                                    Image(systemName: "checkmark")
                                        .foregroundColor(.blue)
                                }
                            }
                        }
                    }
                }
                
                Button(action: {
                    showingFilePicker = true
                }) {
                    HStack {
                        Image(systemName: "plus.circle.fill")
                        Text("Upload Custom Sound")
                        Spacer()
                        if isUploading {
                            ProgressView()
                                .progressViewStyle(CircularProgressViewStyle())
                        }
                    }
                }
                .disabled(!bleManager.isConnected || isUploading)
                
                if !bleManager.isConnected {
                    Text("Connect to ESP32 to upload custom sounds")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            } header: {
                Text("Custom Sounds")
            } footer: {
                Text("Upload audio or video files. Maximum size: 500 KB")
            }
            
            // Upload progress
            if isUploading && bleManager.uploadProgress > 0 {
                Section {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("Uploading...")
                            .font(.headline)
                        ProgressView(value: bleManager.uploadProgress, total: 1.0)
                        Text("\(Int(bleManager.uploadProgress * 100))%")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                }
            }
        }
        .navigationTitle("Sound")
        .sheet(isPresented: $showingFilePicker) {
            DocumentPicker(isUploading: $isUploading, errorMessage: $errorMessage, showingError: $showingError)
                .environmentObject(bleManager)
        }
        .alert("Upload Error", isPresented: $showingError) {
            Button("OK", role: .cancel) { }
        } message: {
            Text(errorMessage)
        }
        .onAppear {
            // Refresh custom sounds list
            Task {
                await bleManager.refreshCustomSoundsList()
            }
        }
    }
    
    /// Generate display name from filename
    private func displayName(for filename: String) -> String {
        var name = filename
        
        // Remove extension
        if let dotIndex = name.lastIndex(of: ".") {
            name = String(name[..<dotIndex])
        }
        
        // Replace underscores with spaces
        name = name.replacingOccurrences(of: "_", with: " ")
        
        // Capitalize first letter
        if let first = name.first {
            name = first.uppercased() + name.dropFirst()
        }
        
        return name
    }
}

// Document picker for file selection
struct DocumentPicker: UIViewControllerRepresentable {
    @EnvironmentObject var bleManager: BLEManager
    @Binding var isUploading: Bool
    @Binding var errorMessage: String
    @Binding var showingError: Bool
    @Environment(\.dismiss) private var dismiss
    
    func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
        let supportedTypes = AudioConverter.getSupportedFileTypes()
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: supportedTypes, asCopy: true)
        picker.delegate = context.coordinator
        picker.allowsMultipleSelection = false
        return picker
    }
    
    func updateUIViewController(_ uiViewController: UIDocumentPickerViewController, context: Context) {}
    
    func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }
    
    class Coordinator: NSObject, UIDocumentPickerDelegate {
        let parent: DocumentPicker
        
        init(_ parent: DocumentPicker) {
            self.parent = parent
        }
        
        func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
            guard let url = urls.first else { return }
            
            parent.isUploading = true
            
            // Convert to MP3
            AudioConverter.convertToMP3(url: url) { result in
                switch result {
                case .success(let (data, filename)):
                    // Upload to ESP32
                    Task {
                        do {
                            try await self.parent.bleManager.uploadSoundFile(filename: filename, data: data)
                            
                            // Refresh list
                            await self.parent.bleManager.refreshCustomSoundsList()
                            
                            DispatchQueue.main.async {
                                self.parent.isUploading = false
                                self.parent.dismiss()
                            }
                        } catch {
                            DispatchQueue.main.async {
                                self.parent.isUploading = false
                                self.parent.errorMessage = error.localizedDescription
                                self.parent.showingError = true
                            }
                        }
                    }
                    
                case .failure(let error):
                    DispatchQueue.main.async {
                        self.parent.isUploading = false
                        self.parent.errorMessage = error.localizedDescription
                        self.parent.showingError = true
                    }
                }
            }
        }
        
        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
            parent.dismiss()
        }
    }
}

#Preview {
    NavigationStack {
        SoundView(selectedSound: .constant("tone1"))
    }
}
