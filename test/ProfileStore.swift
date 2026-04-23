//
//  ProfileStore.swift
//  test
//
//  Created by Kevin L on 4/16/26.
//


import Foundation
import Combine

@MainActor
final class ProfileStore: ObservableObject {
    @Published private(set) var profiles: [Profile] = []

    private let storageKey = "sldpgroup17.profiles.v1"

    init() {
        load()
        if profiles.isEmpty {
            // Seed a default profile to make first run usable
            profiles = [
                Profile(username: "guest",
                        displayName: "Guest",
                        createdAt: Date(),
                        completionValueBySongID: [:])
            ]
            save()
        }
    }

    func createProfile(username: String, displayName: String) throws {
        let u = username.trimmingCharacters(in: .whitespacesAndNewlines)
        let d = displayName.trimmingCharacters(in: .whitespacesAndNewlines)

        guard !u.isEmpty else { throw StoreError("Username required.") }
        guard profiles.first(where: { $0.username.lowercased() == u.lowercased() }) == nil else {
            throw StoreError("That username already exists.")
        }

        profiles.append(Profile(username: u,
                                displayName: d.isEmpty ? u : d,
                                createdAt: Date(),
                                completionValueBySongID: [:]))
        profiles.sort { $0.username.lowercased() < $1.username.lowercased() }
        save()
    }

    func completionValue(username: String, songID: String) -> Int? {
        profiles.first(where: { $0.username == username })?
            .completionValueBySongID[songID]
    }

    /// Store/overwrite completion value received from Arduino (DO NOT increment).
    func setCompletionValue(username: String, songID: String, value: Int) {
        guard let idx = profiles.firstIndex(where: { $0.username == username }) else { return }
        profiles[idx].completionValueBySongID[songID] = value
        save()
    }

    // MARK: - Persistence
    private func load() {
        guard let data = UserDefaults.standard.data(forKey: storageKey) else { return }
        do {
            profiles = try JSONDecoder().decode([Profile].self, from: data)
        } catch {
            profiles = []
        }
    }

    private func save() {
        do {
            let data = try JSONEncoder().encode(profiles)
            UserDefaults.standard.set(data, forKey: storageKey)
        } catch {
            // ignore (preview)
        }
    }

    struct StoreError: LocalizedError {
        let message: String
        init(_ message: String) { self.message = message }
        var errorDescription: String? { message }
    }
}