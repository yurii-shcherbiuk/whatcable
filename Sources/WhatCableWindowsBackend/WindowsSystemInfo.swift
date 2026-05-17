import Foundation
import WhatCableCore

public enum WindowsSystemInfo {
    public static func current() -> CableReport.SystemInfo {
        CableReport.SystemInfo(
            hardwareModel: fetchHardwareModel(),
            osVersion: fetchOSVersion()
        )
    }

    private static func fetchHardwareModel() -> String {
        #if os(Windows)
        // TODO: Query WMI Win32_ComputerSystem for Model, or read from registry
        return "unknown"
        #else
        return "unknown (not Windows)"
        #endif
    }

    private static func fetchOSVersion() -> String {
        let v = ProcessInfo.processInfo.operatingSystemVersion
        return "\(v.majorVersion).\(v.minorVersion).\(v.patchVersion)"
    }
}
