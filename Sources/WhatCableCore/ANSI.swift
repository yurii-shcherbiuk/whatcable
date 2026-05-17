import Foundation
#if canImport(Darwin)
import Darwin
#elseif os(Windows)
import ucrt
#elseif canImport(Glibc)
import Glibc
#endif

public enum ANSI {
    public static let isEnabled: Bool = {
        if ProcessInfo.processInfo.environment["NO_COLOR"] != nil { return false }
        #if os(Windows)
        return _isatty(_fileno(stdout)) != 0
        #else
        return isatty(fileno(stdout)) != 0
        #endif
    }()

    public static let reset = "\u{1B}[0m"
    public static let bold = "\u{1B}[1m"
    public static let dim = "\u{1B}[2m"

    public static let red = "\u{1B}[31m"
    public static let green = "\u{1B}[32m"
    public static let yellow = "\u{1B}[33m"
    public static let blue = "\u{1B}[34m"
    public static let magenta = "\u{1B}[35m"
    public static let cyan = "\u{1B}[36m"
    public static let gray = "\u{1B}[90m"

    public static func wrap(_ codes: String, _ text: String) -> String {
        guard isEnabled else { return text }
        return codes + text + reset
    }
}
