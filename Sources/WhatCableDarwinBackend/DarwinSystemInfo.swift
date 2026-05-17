import Darwin
import Foundation
import WhatCableCore

public enum DarwinSystemInfo {
    public static func current() -> CableReport.SystemInfo {
        CableReport.SystemInfo(
            hardwareModel: fetchMacModel(),
            osVersion: fetchOSVersion()
        )
    }

    private static func fetchMacModel() -> String {
        var size = 0
        sysctlbyname("hw.model", nil, &size, nil, 0)
        guard size > 0 else { return "unknown" }
        var buf = [CChar](repeating: 0, count: size)
        sysctlbyname("hw.model", &buf, &size, nil, 0)
        return String(cString: buf)
    }

    private static func fetchOSVersion() -> String {
        let v = ProcessInfo.processInfo.operatingSystemVersion
        return "\(v.majorVersion).\(v.minorVersion).\(v.patchVersion)"
    }
}
