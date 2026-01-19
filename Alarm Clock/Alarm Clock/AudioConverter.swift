//
//  AudioConverter.swift
//  Alarm Clock
//
//  Audio/Video to M4A conversion utility
//  Extracts audio from video files and converts to M4A format
//

import Foundation
import AVFoundation
import UniformTypeIdentifiers

enum AudioConverterError: Error {
    case unsupportedFormat
    case fileTooLarge
    case conversionFailed
    case noAudioTrack
    case exportFailed
    
    var localizedDescription: String {
        switch self {
        case .unsupportedFormat:
            return "Unsupported file format"
        case .fileTooLarge:
            return "File is too large (max 1 MB after conversion)"
        case .conversionFailed:
            return "Failed to convert audio"
        case .noAudioTrack:
            return "Video file has no audio track"
        case .exportFailed:
            return "Failed to export audio"
        }
    }
}

class AudioConverter {
    
    static let maxFileSize: Int = 1048576  // 1 MB
    
    /// Get supported file types for picker
    static func getSupportedFileTypes() -> [UTType] {
        return [
            // Audio formats
            .mp3,
            .mpeg4Audio,  // m4a
            .wav,
            // Video formats
            .movie,       // .mov
            .mpeg4Movie,  // .mp4
            .video
        ]
    }
    
    /// Convert media file (audio or video) to M4A
    /// - Parameters:
    ///   - url: URL to source file
    ///   - completion: Completion handler with Result containing M4A data and suggested filename
    static func convertToMP3(url: URL, completion: @escaping (Result<(data: Data, filename: String), AudioConverterError>) -> Void) {
        
        // Load asset using modern API
        let asset = AVURLAsset(url: url)
        
        // Check if file has audio track (async check)
        Task {
            do {
                let tracks = try await asset.loadTracks(withMediaType: .audio)
                guard !tracks.isEmpty else {
                    completion(.failure(.noAudioTrack))
                    return
                }
                
                // Generate output filename
                let originalFilename = url.deletingPathExtension().lastPathComponent
                let outputFilename = sanitizeFilename(originalFilename) + ".m4a"
                
                // Create temporary output URL
                let tempDir = FileManager.default.temporaryDirectory
                let outputURL = tempDir.appendingPathComponent(UUID().uuidString + ".m4a")
                
                // Remove existing file if present
                try? FileManager.default.removeItem(at: outputURL)
                
                // Create export session
                guard let exportSession = AVAssetExportSession(asset: asset, presetName: AVAssetExportPresetAppleM4A) else {
                    completion(.failure(.conversionFailed))
                    return
                }
                
                exportSession.outputURL = outputURL
                exportSession.outputFileType = .m4a
                
                // Export audio using modern async API
                await performExport(session: exportSession, outputURL: outputURL, asset: asset, outputFilename: outputFilename, completion: completion)
                
            } catch {
                completion(.failure(.noAudioTrack))
            }
        }
    }
    
    /// Perform export using modern async API
    private static func performExport(session: AVAssetExportSession, outputURL: URL, asset: AVAsset, outputFilename: String, completion: @escaping (Result<(data: Data, filename: String), AudioConverterError>) -> Void) async {
        do {
            try await session.export(to: outputURL, as: .m4a)
            
            // Read exported file
            guard let data = try? Data(contentsOf: outputURL) else {
                completion(.failure(.exportFailed))
                return
            }
            
            // Clean up temporary file
            try? FileManager.default.removeItem(at: outputURL)
            
            // Check file size
            if data.count > maxFileSize {
                // Try to compress by converting to lower quality
                compressAudio(asset: asset, targetSize: maxFileSize) { result in
                    switch result {
                    case .success(let compressedData):
                        completion(.success((data: compressedData, filename: outputFilename)))
                    case .failure(_):
                        completion(.failure(.fileTooLarge))
                    }
                }
            } else {
                // File size is acceptable
                completion(.success((data: data, filename: outputFilename)))
            }
        } catch {
            print("Export failed: \(error.localizedDescription)")
            // Clean up
            try? FileManager.default.removeItem(at: outputURL)
            completion(.failure(.exportFailed))
        }
    }
    
    /// Compress audio to target size
    private static func compressAudio(asset: AVAsset, targetSize: Int, completion: @escaping (Result<Data, AudioConverterError>) -> Void) {
        
        let tempDir = FileManager.default.temporaryDirectory
        let outputURL = tempDir.appendingPathComponent(UUID().uuidString + ".m4a")
        
        // Remove existing file if present
        try? FileManager.default.removeItem(at: outputURL)
        
        // Try lower quality preset
        guard let exportSession = AVAssetExportSession(asset: asset, presetName: AVAssetExportPresetLowQuality) else {
            completion(.failure(.conversionFailed))
            return
        }
        
        exportSession.outputURL = outputURL
        exportSession.outputFileType = .m4a
        
        // Use async export
        Task {
            do {
                try await exportSession.export(to: outputURL, as: .m4a)
                
                guard let data = try? Data(contentsOf: outputURL) else {
                    completion(.failure(.exportFailed))
                    return
                }
                
                // Clean up
                try? FileManager.default.removeItem(at: outputURL)
                
                if data.count > targetSize {
                    // Still too large after compression
                    completion(.failure(.fileTooLarge))
                } else {
                    completion(.success(data))
                }
            } catch {
                // Clean up
                try? FileManager.default.removeItem(at: outputURL)
                completion(.failure(.conversionFailed))
            }
        }
    }
    
    /// Sanitize filename for ESP32
    private static func sanitizeFilename(_ filename: String) -> String {
        var sanitized = filename
        
        // Replace spaces with underscores
        sanitized = sanitized.replacingOccurrences(of: " ", with: "_")
        
        // Remove special characters
        let allowedChars = CharacterSet.alphanumerics.union(CharacterSet(charactersIn: "_-"))
        sanitized = sanitized.components(separatedBy: allowedChars.inverted).joined()
        
        // Limit length to 19 characters (SPIFFS path limit: 31 chars - 8 for "/alarms/" - 4 for ".m4a" = 19)
        if sanitized.count > 19 {
            sanitized = String(sanitized.prefix(19))
        }
        
        // Ensure not empty
        if sanitized.isEmpty {
            sanitized = "alarm_sound"
        }
        
        return sanitized
    }
    
    /// Extract audio from video file
    static func extractAudioFromVideo(url: URL, completion: @escaping (Result<(data: Data, filename: String), AudioConverterError>) -> Void) {
        // Same implementation as convertToMP3 since we handle both
        convertToMP3(url: url, completion: completion)
    }
}
