//
//  SessionManager.swift
//  test
//
//  Created by Kevin L on 4/16/26.
//


import Foundation
import Combine

@MainActor
final class SessionManager: ObservableObject {
    @Published var activeUsername: String? = nil

    private let storageKey = "sldpgroup17.activeUsername.v1"

    init() {
        activeUsername = UserDefaults.standard.string(forKey: storageKey)
    }

    func signIn(username: String) {
        activeUsername = username
        UserDefaults.standard.set(username, forKey: storageKey)
    }

    func signOut() {
        activeUsername = nil
        UserDefaults.standard.removeObject(forKey: storageKey)
    }
}