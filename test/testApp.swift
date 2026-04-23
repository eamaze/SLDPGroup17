import SwiftUI
import SwiftData

@main
struct testApp: App {
    var sharedModelContainer: ModelContainer = {
        let schema = Schema([
            Item.self,
        ])
        let modelConfiguration = ModelConfiguration(schema: schema, isStoredInMemoryOnly: false)

        do {
            return try ModelContainer(for: schema, configurations: [modelConfiguration])
        } catch {
            fatalError("Could not create ModelContainer: \(error)")
        }
    }()

    @StateObject private var ble = BLEManager()
    @StateObject private var session = SessionManager()
    @StateObject private var profiles = ProfileStore()

    var body: some Scene {
        WindowGroup {
            AppRootView()
                .environmentObject(ble)
                .environmentObject(session)
                .environmentObject(profiles)
        }
        .modelContainer(sharedModelContainer)
    }
}
