//
//  CustomSoundsView.swift
//  Alarm Clock
//
//  Manage custom uploaded alarm sounds
//

import SwiftUI
import AVFoundation

struct CustomSoundsView: View {
    @EnvironmentObject var bleManager: BLEManager
    @Environment(\.dismiss) private var dismiss
    
    @State private var showingFilePicker = false
    @State private var isUploading = false
    @State private var showingError = false
    @State private var errorMessage = ""
    @State private var showingDeleteConfirmation = false
    @State private var soundToDelete: String?
    
    var body: some View {
        NavigationView {
            List {
                // Storage info section
                Section {
                    HStack {
                        Image(systemName: "internaldrive")
                            .foregroundColor(.blue)
                        Text("Custom Sounds")
                        Spacer()
                        Text("\(bleManager.availableCustomSounds.count) files")
                            .foregroundColor(.secondary)
                    }
                } header: {
                    Text("Storage")
                } footer: {
                    Text("Maximum 5-6 custom sounds (500 KB each)")
                }
                
                // Custom sounds list
                Section {
                    if bleManager.availableCustomSounds.isEmpty {
                        VStack(spacing: 12) {
                            Image(systemName: "music.note.list")
                                .font(.system(size: 48))
                                .foregroundColor(.gray)
                            Text("No custom sounds")
                                .font(.headline)
                                .foregroundColor(.secondary)
                            Text("Upload audio or video files to use as alarm sounds")
                                .font(.caption)
                                .foregroundColor(.secondary)
                                .multilineTextAlignment(.center)
                        }
                        .frame(maxWidth: .infinity)
                        .padding()
                    } else {
                        ForEach(bleManager.availableCustomSounds, id: \.self) { soundFile in
                            HStack {
                                Image(systemName: "music.note")
                                    .foregroundColor(.blue)
                                
                                VStack(alignment: .leading) {
                                    Text(displayName(for: soundFile))
                                        .font(.body)
                                    Text(soundFile)
                                        .font(.caption)
                                        .foregroundColor(.secondary)
                                }
                                
                                Spacer()
                                
                                Button(action: {
                                    soundToDelete = soundFile
                                    showingDeleteConfirmation = true
                                }) {
                                    Image(systemName: "trash")
                                        .foregroundColor(.red)
                                }
                                .buttonStyle(BorderlessButtonStyle())
                            }
                        }
                    }
                } header: {
                    Text("Uploaded Sounds")
                }
                
                // Upload section
                Section {
                    Button(action: {
                        showingFilePicker = true
                    }) {
                        HStack {
                            Image(systemName: "plus.circle.fill")
                                .foregroundColor(.blue)
                            Text("Upload New Sound")
                            Spacer()
                            if isUploading {
                                ProgressView()
                                    .progressViewStyle(CircularProgressViewStyle())
                            }
                        }
                    }
                    .disabled(!bleManager.isConnected || isUploading)
                    
                    if !bleManager.isConnected {
                        Text("Connect to ESP32 to upload sounds")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                } footer: {
                    Text("Supported formats: MP3, M4A, WAV, MP4, MOV\nMaximum file size: 500 KB")
                }
                
                // Upload progress
                if isUploading && bleManager.uploadProgress > 0 {
                    Section {
                        VStack(alignment: .leading, spacing: 8) {
                            HStack {
                                Text("Uploading...")
                                    .font(.headline)
                                Spacer()
                                Text("\(Int(bleManager.uploadProgress * 100))%")
                                    .font(.caption)
                                    .foregroundColor(.secondary)
                            }
                            ProgressView(value: bleManager.uploadProgress, total: 1.0)
                        }
                    }
                }
            }
            .navigationTitle("Custom Sounds")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Done") {
                        dismiss()
                    }
                }
            }
            .sheet(isPresented: $showingFilePicker) {
                DocumentPicker(isUploading: $isUploading, errorMessage: $errorMessage, showingError: $showingError)
                    .environmentObject(bleManager)
            }
            .alert("Upload Error", isPresented: $showingError) {
                Button("OK", role: .cancel) { }
            } message: {
                Text(errorMessage)
            }
            .alert("Delete Sound", isPresented: $showingDeleteConfirmation) {
                Button("Cancel", role: .cancel) { }
                Button("Delete", role: .destructive) {
                    if let filename = soundToDelete {
                        deleteSound(filename)
                    }
                }
            } message: {
                if let filename = soundToDelete {
                    Text("Delete \"\(displayName(for: filename))\"? This cannot be undone.")
                }
            }
            .onAppear {
                Task {
                    await bleManager.refreshCustomSoundsList()
                }
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
    
    /// Delete a sound file
    private func deleteSound(_ filename: String) {
        Task {
            do {
                try await bleManager.deleteCustomSound(filename: filename)
            } catch {
                errorMessage = error.localizedDescription
                showingError = true
            }
        }
    }
}

#Preview {
    CustomSoundsView()
        .environmentObject(BLEManager())
}
