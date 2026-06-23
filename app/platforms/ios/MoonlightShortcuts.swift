#if canImport(AppIntents) && canImport(UIKit)
import AppIntents
import Foundation
import UIKit

@available(iOS 16.0, *)
private enum MoonlightShortcutURL {
    static func launchURL(host: String, appID: String, appName: String) -> URL? {
        let trimmedHost = host.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedAppID = appID.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedAppName = appName.trimmingCharacters(in: .whitespacesAndNewlines)

        guard !trimmedHost.isEmpty, !trimmedAppID.isEmpty else {
            return nil
        }

        var components = URLComponents()
        components.scheme = "moonlightswitch"
        components.host = "launch"
        components.queryItems = [
            URLQueryItem(name: isMACAddress(trimmedHost) ? "host" : "ip", value: trimmedHost),
            URLQueryItem(name: "appid", value: trimmedAppID),
            URLQueryItem(name: "appname", value: trimmedAppName),
        ]
        return components.url
    }

    private static func isMACAddress(_ value: String) -> Bool {
        let parts = value.split(separator: ":", omittingEmptySubsequences: false)
        guard parts.count == 6 else {
            return false
        }

        return parts.allSatisfy { part in
            part.count == 2 && part.unicodeScalars.allSatisfy { scalar in
                CharacterSet(charactersIn: "0123456789abcdefABCDEF").contains(scalar)
            }
        }
    }
}

@available(iOS 16.0, *)
private func moonlightLocalizedResource(_ value: String) -> LocalizedStringResource {
    LocalizedStringResource(String.LocalizationValue(value))
}

@available(iOS 16.0, *)
private enum MoonlightShortcutError: Error, CustomLocalizedStringResourceConvertible {
    case invalidLaunchURL

    var localizedStringResource: LocalizedStringResource {
        switch self {
        case .invalidLaunchURL:
            return "Host and App ID are required."
        }
    }
}

@available(iOS 16.0, *)
private struct MoonlightSettingsFile: Decodable {
    let hosts: [MoonlightSavedHost]?
}

@available(iOS 16.0, *)
private struct MoonlightSavedHost: Decodable {
    let address: String
    let remoteAddress: String
    let hostname: String
    let mac: String
    let favorites: [MoonlightSavedApp]

    private enum CodingKeys: String, CodingKey {
        case address
        case remoteAddress = "remote_address"
        case legacyRemoteAddress = "remoteAddress"
        case hostname
        case mac
        case favorites
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        address = try container.decodeIfPresent(String.self, forKey: .address) ?? ""
        remoteAddress =
            try container.decodeIfPresent(String.self, forKey: .remoteAddress) ??
            container.decodeIfPresent(String.self, forKey: .legacyRemoteAddress) ??
            ""
        hostname = try container.decodeIfPresent(String.self, forKey: .hostname) ?? ""
        mac = try container.decodeIfPresent(String.self, forKey: .mac) ?? ""
        favorites = try container.decodeIfPresent([MoonlightSavedApp].self, forKey: .favorites) ?? []
    }

    var launchHost: String {
        if !mac.isEmpty {
            return mac
        }
        if !address.isEmpty {
            return address
        }
        return remoteAddress
    }

    var displayHost: String {
        if !hostname.isEmpty {
            return hostname
        }
        return launchHost
    }
}

@available(iOS 16.0, *)
private struct MoonlightSavedApp: Decodable {
    let name: String
    let id: Int

    private enum CodingKeys: String, CodingKey {
        case name
        case id
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        name = try container.decodeIfPresent(String.self, forKey: .name) ?? ""
        id = try container.decodeIfPresent(Int.self, forKey: .id) ?? 0
    }
}

@available(iOS 16.0, *)
private enum MoonlightGameStore {
    static var workingDirectoryURL: URL? {
        FileManager.default.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask
        ).first?.appendingPathComponent("Moonlight-Switch", isDirectory: true)
    }

    static func boxArtURL(appID: String) -> URL? {
        guard let boxArtURL = workingDirectoryURL?
            .appendingPathComponent("boxart", isDirectory: true)
            .appendingPathComponent("\(appID).png") else {
            return nil
        }

        guard FileManager.default.fileExists(atPath: boxArtURL.path) else {
            return nil
        }

        return boxArtURL
    }

    static func game(host: String, appID: String, appName: String, displayHost: String? = nil) -> MoonlightGameEntity? {
        let trimmedHost = host.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedAppID = appID.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedAppName = appName.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmedHost.isEmpty, !trimmedAppID.isEmpty else {
            return nil
        }

        return MoonlightGameEntity(
            host: trimmedHost,
            appID: trimmedAppID,
            appName: trimmedAppName.isEmpty ? trimmedAppID : trimmedAppName,
            displayHost: displayHost?.trimmingCharacters(in: .whitespacesAndNewlines).nonEmpty ?? trimmedHost,
            boxArtPath: boxArtURL(appID: trimmedAppID)?.path
        )
    }

    static func allFavorites() throws -> [MoonlightGameEntity] {
        guard let settingsURL = workingDirectoryURL?.appendingPathComponent("settings.json") else {
            return []
        }

        guard FileManager.default.fileExists(atPath: settingsURL.path) else {
            return []
        }

        let data = try Data(contentsOf: settingsURL)
        let settings = try JSONDecoder().decode(MoonlightSettingsFile.self, from: data)

        return settings.hosts?.flatMap { host -> [MoonlightGameEntity] in
            let launchHost = host.launchHost
            guard !launchHost.isEmpty else {
                return []
            }

            return host.favorites.compactMap { app in
                guard app.id != 0 else {
                    return nil
                }

                let appID = String(app.id)
                return game(
                    host: launchHost,
                    appID: appID,
                    appName: app.name,
                    displayHost: host.displayHost
                )
            }
        } ?? []
    }
}

@available(iOS 16.0, *)
private extension String {
    var nonEmpty: String? {
        isEmpty ? nil : self
    }
}

@available(iOS 16.0, *)
struct MoonlightGameEntity: AppEntity, Sendable {
    static var typeDisplayRepresentation: TypeDisplayRepresentation = "Game"
    static var defaultQuery = MoonlightGameQuery()

    let id: String

    @Property(title: "Host")
    var host: String

    @Property(title: "App ID")
    var appID: String

    @Property(title: "App Name")
    var appName: String

    @Property(title: "Display Host")
    var displayHost: String

    private let boxArtPath: String?

    init(host: String, appID: String, appName: String, displayHost: String, boxArtPath: String?) {
        id = "\(host)|\(appID)"
        self.boxArtPath = boxArtPath
        self.host = host
        self.appID = appID
        self.appName = appName
        self.displayHost = displayHost
    }

    var displayRepresentation: DisplayRepresentation {
        let subtitle = displayHost == host ? host : "\(displayHost) - \(host)"
        return DisplayRepresentation(
            title: moonlightLocalizedResource(appName),
            subtitle: moonlightLocalizedResource(subtitle),
            image: displayImage
        )
    }

    var launchURL: URL? {
        MoonlightShortcutURL.launchURL(host: host, appID: appID, appName: appName)
    }

    private var displayImage: DisplayRepresentation.Image {
        if let boxArtPath,
           FileManager.default.fileExists(atPath: boxArtPath) {
            return DisplayRepresentation.Image(
                url: URL(fileURLWithPath: boxArtPath),
                width: 300,
                height: 400,
                isTemplate: false
            )
        }

        return DisplayRepresentation.Image(systemName: "play.rectangle")
    }
}

@available(iOS 16.0, *)
struct MoonlightGameQuery: EntityStringQuery {
    init() {}

    func entities(for identifiers: [MoonlightGameEntity.ID]) async throws -> [MoonlightGameEntity] {
        let identifiers = Set(identifiers)
        return try MoonlightGameStore.allFavorites().filter { identifiers.contains($0.id) }
    }

    func suggestedEntities() async throws -> [MoonlightGameEntity] {
        try MoonlightGameStore.allFavorites()
    }

    func entities(matching string: String) async throws -> [MoonlightGameEntity] {
        let query = string.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard !query.isEmpty else {
            return try MoonlightGameStore.allFavorites()
        }

        return try MoonlightGameStore.allFavorites().filter { favorite in
            favorite.appName.lowercased().contains(query) ||
            favorite.displayHost.lowercased().contains(query) ||
            favorite.host.lowercased().contains(query) ||
            favorite.appID.contains(query)
        }
    }
}

@available(iOS 16.0, *)
struct MoonlightGetFavoriteGamesIntent: AppIntent {
    static var title: LocalizedStringResource = "Get Favorite Games"
    static var description = IntentDescription("Get games saved in Moonlight's Favorites list.")
    static var openAppWhenRun = false
    static var authenticationPolicy: IntentAuthenticationPolicy = .alwaysAllowed

    func perform() async throws -> some IntentResult & ReturnsValue<[MoonlightGameEntity]> {
        .result(value: try MoonlightGameStore.allFavorites())
    }
}

@available(iOS 16.0, *)
enum MoonlightGameDetail: String, AppEnum, Sendable {
    case appID
    case appName
    case host
    case displayHost

    static var typeDisplayRepresentation: TypeDisplayRepresentation = "Game Detail"
    static var caseDisplayRepresentations: [MoonlightGameDetail: DisplayRepresentation] = [
        .appID: "App ID",
        .appName: "App Name",
        .host: "Host",
        .displayHost: "Display Host",
    ]
}

@available(iOS 16.0, *)
struct MoonlightGetGameDetailIntent: AppIntent {
    static var title: LocalizedStringResource = "Get Game Detail"
    static var description = IntentDescription("Get a field from a Moonlight game.")
    static var openAppWhenRun = false
    static var authenticationPolicy: IntentAuthenticationPolicy = .alwaysAllowed

    @Parameter(title: "Detail", description: "Game field to read.")
    var detail: MoonlightGameDetail

    @Parameter(title: "Game", description: "Game to inspect.")
    var game: MoonlightGameEntity

    static var parameterSummary: some ParameterSummary {
        Summary("Get \(\.$detail) from \(\.$game)")
    }

    func perform() async throws -> some IntentResult & ReturnsValue<String> {
        let value: String
        switch detail {
        case .appID:
            value = game.appID
        case .appName:
            value = game.appName
        case .host:
            value = game.host
        case .displayHost:
            value = game.displayHost
        }

        return .result(value: value)
    }
}

@available(iOS 16.0, *)
struct MoonlightGetGameDeepLinkIntent: AppIntent {
    static var title: LocalizedStringResource = "Get Game Deep Link"
    static var description = IntentDescription("Get a moonlightswitch:// launch URL for a Moonlight game.")
    static var openAppWhenRun = false
    static var authenticationPolicy: IntentAuthenticationPolicy = .alwaysAllowed

    @Parameter(title: "Game", description: "Game to create a deep link for.")
    var game: MoonlightGameEntity

    static var parameterSummary: some ParameterSummary {
        Summary("Get deep link for \(\.$game)")
    }

    func perform() async throws -> some IntentResult & ReturnsValue<URL> {
        guard let launchURL = game.launchURL else {
            throw MoonlightShortcutError.invalidLaunchURL
        }

        return .result(value: launchURL)
    }
}

@available(iOS 16.0, *)
struct MoonlightCreateGameIntent: AppIntent {
    static var title: LocalizedStringResource = "Create Game"
    static var description = IntentDescription("Create a game value from host, app ID, and app name.")
    static var openAppWhenRun = false
    static var authenticationPolicy: IntentAuthenticationPolicy = .alwaysAllowed

    @Parameter(title: "Host", description: "Paired PC IP address or MAC address.")
    var host: String

    @Parameter(title: "App ID", description: "GameStream application ID.")
    var appID: String

    @Parameter(title: "App Name", description: "Game name shown while launching.")
    var appName: String

    static var parameterSummary: some ParameterSummary {
        Summary("Create \(\.$appName) with app ID \(\.$appID) from \(\.$host)")
    }

    func perform() async throws -> some IntentResult & ReturnsValue<MoonlightGameEntity> {
        guard let game = MoonlightGameStore.game(host: host, appID: appID, appName: appName) else {
            throw MoonlightShortcutError.invalidLaunchURL
        }

        return .result(value: game)
    }
}

@available(iOS 16.0, *)
struct MoonlightLaunchGameIntent: AppIntent {
    static var title: LocalizedStringResource = "Launch Game"
    static var description = IntentDescription("Open Moonlight and start streaming a game.")
    static var openAppWhenRun = true
    static var authenticationPolicy: IntentAuthenticationPolicy = .alwaysAllowed

    @Parameter(title: "Game", description: "Game to launch.")
    var game: MoonlightGameEntity

    static var parameterSummary: some ParameterSummary {
        Summary("Launch \(\.$game)")
    }

    @MainActor
    func perform() async throws -> some IntentResult {
        guard let launchURL = game.launchURL else {
            throw MoonlightShortcutError.invalidLaunchURL
        }

        await UIApplication.shared.open(launchURL)
        return .result()
    }
}

@available(iOS 16.0, *)
struct MoonlightShortcutsProvider: AppShortcutsProvider {
    static var appShortcuts: [AppShortcut] {
        AppShortcut(
            intent: MoonlightLaunchGameIntent(),
            phrases: [
                "Launch a game in \(.applicationName)",
                "Start streaming with \(.applicationName)",
            ],
            shortTitle: "Launch Game",
            systemImageName: "play.rectangle"
        )
        AppShortcut(
            intent: MoonlightCreateGameIntent(),
            phrases: [
                "Create a game in \(.applicationName)",
                "Make a game in \(.applicationName)",
            ],
            shortTitle: "Create Game",
            systemImageName: "plus"
        )
        AppShortcut(
            intent: MoonlightGetFavoriteGamesIntent(),
            phrases: [
                "Get favorite games from \(.applicationName)",
                "Show favorite games from \(.applicationName)",
            ],
            shortTitle: "Get Favorites",
            systemImageName: "star"
        )
        AppShortcut(
            intent: MoonlightGetGameDetailIntent(),
            phrases: [
                "Get game detail from \(.applicationName)",
                "Read game detail from \(.applicationName)",
            ],
            shortTitle: "Get Game Detail",
            systemImageName: "info.circle"
        )
        AppShortcut(
            intent: MoonlightGetGameDeepLinkIntent(),
            phrases: [
                "Get game deep link from \(.applicationName)",
                "Create game deep link with \(.applicationName)",
            ],
            shortTitle: "Get Deep Link",
            systemImageName: "link"
        )
    }
}
#endif
