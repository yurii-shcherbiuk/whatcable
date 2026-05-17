import Foundation

public enum AppInfo {
    public static let name = "WhatCable"

    // nonisolated(unsafe): benign race -- resolveVersion() is idempotent.
    // Mutable so Windows entry points can call setVersion() before first use.
    nonisolated(unsafe) private static var _version: String?

    public static var version: String {
        if let v = _version { return v }
        let resolved = resolveVersion()
        _version = resolved
        return resolved
    }

    public static func setVersion(_ v: String) {
        _version = v
    }

    private static func resolveVersion() -> String {
        #if canImport(AppKit)
        if let v = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String {
            return v
        }
        let exe = Bundle.main.executablePath ?? CommandLine.arguments.first ?? ""
        var dir = URL(fileURLWithPath: exe)
            .resolvingSymlinksInPath()
            .deletingLastPathComponent()
        for _ in 0..<4 {
            let plist = dir.appendingPathComponent("Info.plist")
            if let data = try? Data(contentsOf: plist),
               let parsed = try? PropertyListSerialization.propertyList(from: data, format: nil) as? [String: Any],
               let v = parsed["CFBundleShortVersionString"] as? String {
                return v
            }
            dir = dir.deletingLastPathComponent()
        }
        #endif
        return "dev"
    }
    public static let credit = "WhatCable"
    public static var tagline: String { coreLocalized("What can this USB-C cable actually do?") }
    public static let copyright = "© \(Calendar.current.component(.year, from: Date())) \(credit)"
    public static let helpURL = URL(string: "https://github.com/darrylmorley/whatcable")!

    /// Compare dot-separated numeric versions. Non-numeric segments compare as 0.
    public static func isNewer(remote: String, current: String) -> Bool {
        let r = parts(remote)
        let c = parts(current)
        for i in 0..<max(r.count, c.count) {
            let rv = i < r.count ? r[i] : 0
            let cv = i < c.count ? c[i] : 0
            if rv != cv { return rv > cv }
        }
        return false
    }

    private static func parts(_ version: String) -> [Int] {
        version.split(separator: ".").map { Int($0) ?? 0 }
    }
}
