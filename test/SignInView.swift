import SwiftUI

struct SignInView: View {
    @EnvironmentObject var session: SessionManager
    @EnvironmentObject var profiles: ProfileStore

    @State private var selectedUsername: String? = nil
    @State private var showCreate = false

    var body: some View {
        NavigationView {
            VStack(spacing: 20) {
                Spacer(minLength: 10)

                VStack(spacing: 6) {
                    Text("SLDP")
                        .font(.system(size: 34, weight: .heavy))

                    Text("Choose a profile")
                        .font(.system(size: 16, weight: .semibold))
                }

                GeometryReader { geo in
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 16) {
                            ForEach(profiles.profiles) { p in
                                ProfileCardView(
                                    username: p.username,
                                    isSelected: selectedUsername == p.username
                                )
                                .onTapGesture {
                                    withAnimation(.spring(response: 0.35, dampingFraction: 0.78)) {
                                        selectedUsername = p.username
                                    }
                                }
                            }

                            AddProfileCardView()
                                .onTapGesture { showCreate = true }
                        }
                        .padding(.horizontal, 16)
                        .padding(.vertical, 8)
                        .frame(minWidth: geo.size.width, alignment: .center)
                    }
                }
                .frame(height: 160)

                Button(action: {
                    guard let u = selectedUsername else { return }
                    session.signIn(username: u)
                }) {
                    Text(selectedUsername == nil ? "Select a profile" : "Continue")
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 14)
                        .font(.system(size: 17, weight: .bold))
                }
                .padding(.horizontal, 16)
                .disabled(selectedUsername == nil)

                HStack(spacing: 14) {
                    Button("New Profile") { showCreate = true }
                        .font(.system(size: 14, weight: .semibold))

                    Text("•")

                    Button("Sign in as Guest") {
                        if profiles.profiles.contains(where: { $0.username == "guest" }) {
                            session.signIn(username: "guest")
                        } else {
                            try? profiles.createProfile(username: "guest", displayName: "guest")
                            session.signIn(username: "guest")
                        }
                    }
                    .font(.system(size: 14, weight: .semibold))
                }
                .frame(maxWidth: .infinity, alignment: .center)

                Spacer()
            }
            .padding(.vertical, 10)
            .navigationBarTitle("")
            .navigationBarHidden(true)
            .sheet(isPresented: $showCreate) {
                CreateProfileView { username in
                    selectedUsername = username
                }
            }
        }
        .navigationViewStyle(StackNavigationViewStyle())
    }
}

private struct ProfileCardView: View {
    let username: String
    let isSelected: Bool

    var body: some View {
        VStack(spacing: 10) {
            Image(systemName: "person.fill")
                .font(.system(size: 44, weight: .bold))

            Text(username)
                .font(.system(size: 15, weight: .semibold))

            if isSelected {
                Text("Selected")
                    .font(.system(size: 12, weight: .semibold))
            } else {
                Text(" ")
                    .font(.system(size: 12, weight: .semibold))
            }
        }
        .padding(.vertical, 10)
        .padding(.horizontal, 10)
    }
}

private struct AddProfileCardView: View {
    var body: some View {
        VStack(spacing: 10) {
            Image(systemName: "plus")
                .font(.system(size: 36, weight: .bold))

            Text("Add")
                .font(.system(size: 15, weight: .semibold))
        }
        .padding(.vertical, 10)
        .padding(.horizontal, 10)
    }
}
