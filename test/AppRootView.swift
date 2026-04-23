//
//  AppRootView.swift
//  test
//
//  Created by Kevin L on 4/16/26.
//


import SwiftUI

struct AppRootView: View {
    @EnvironmentObject var session: SessionManager

    var body: some View {
        if let _ = session.activeUsername {
            NavigationStack {
                SongListView()
            }
        } else {
            SignInView()
        }
    }
}