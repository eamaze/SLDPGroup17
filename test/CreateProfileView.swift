import SwiftUI

struct CreateProfileView: View {
    @Environment(\.dismiss) private var dismiss
    @EnvironmentObject var profiles: ProfileStore

    var onCreated: (String) -> Void

    @State private var username: String = ""
    @State private var errorText: String?

    var body: some View {
        NavigationView {
            Form {
                Section("New Profile") {
                    TextField("Username", text: $username)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                }

                if let errorText {
                    Text(errorText)
                }
            }
            .navigationBarTitle("Create Profile", displayMode: .inline)
            .navigationBarItems(
                leading: Button("Cancel") { dismiss() },
                trailing: Button("Create") { create() }
            )
        }
    }

    private func create() {
        do {
            let u = username.trimmingCharacters(in: .whitespacesAndNewlines)
            // ProfileStore still expects a displayName parameter.
            // We pass username for displayName so the rest of the app stays consistent.
            try profiles.createProfile(username: u, displayName: u)
            onCreated(u)
            dismiss()
        } catch {
            errorText = error.localizedDescription
        }
    }
}
