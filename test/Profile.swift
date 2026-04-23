//
//  Profile.swift
//  test
//
//  Created by Kevin L on 4/16/26.
//


import Foundation

struct Profile: Identifiable, Codable, Hashable {
    var id: String { username }

    let username: String
    var displayName: String
    let createdAt: Date

    /// Per-song completion value reported by ESP32/Arduino.
    /// Key: songID (String), Value: completion value (Int)
    var completionValueBySongID: [String: Int]
}