import Foundation

/// Decides how to present USB devices that reached the Mac over a Thunderbolt
/// tunnel (issue #274). These devices sit behind a Thunderbolt dock or display
/// and match no physical port, so the normal per-port device list never shows
/// them. This helper groups them and, when it is safe to do so, points them at
/// the single port they belong behind.
///
/// Safety rule: nest the devices under a port only when **exactly one**
/// Thunderbolt device is connected. With one connection there is no ambiguity
/// about what the tunnelled devices are behind, so the attribution is certain
/// without the per-port tunnel join (the `apciec`/`acio` correlation that is
/// not yet confirmed on multi-port hardware). With two or more Thunderbolt
/// devices the helper returns no host port and the caller renders a flat
/// "Other USB devices" section instead of guessing.
///
/// Pure logic, no IOKit. Shared by the menu bar app, the CLI text output, and
/// the JSON output so all three group identically.
public enum TunnelledDeviceGrouping {
    public struct Result: Equatable {
        /// The Thunderbolt-tunnelled devices, in input order. Empty when there
        /// are none, in which case the caller shows no extra section.
        public let devices: [USBDevice]
        /// The `serviceName` of the one connected Thunderbolt port these devices
        /// nest under (e.g. "Port-USB-C@2"), or `nil` to render them flat. Only
        /// set when exactly one Thunderbolt device is connected.
        public let hostPortServiceName: String?

        public init(devices: [USBDevice], hostPortServiceName: String?) {
            self.devices = devices
            self.hostPortServiceName = hostPortServiceName
        }
    }

    /// USB Hub device class (`bDeviceClass`). The internal hubs inside a
    /// Thunderbolt dock or display are plumbing, not devices the user plugged
    /// in, so they are filtered out. Spec-mandated, so this is robust (no
    /// name matching). The dock/display's own functions (e.g. a display's
    /// camera/audio) are left in: they are real USB devices.
    private static let usbHubClass: UInt8 = 0x09

    public static func group(
        devices: [USBDevice],
        ports: [AppleHPMInterface],
        thunderboltSwitches: [IOThunderboltSwitch]
    ) -> Result {
        let tunnelled = devices.filter {
            $0.isThunderboltTunnelled && $0.deviceClass != usbHubClass
        }
        guard !tunnelled.isEmpty else {
            return Result(devices: [], hostPortServiceName: nil)
        }

        // Ports that currently have a Thunderbolt device downstream (a dock or
        // display). A single dock fanning out to two displays is still one
        // connection (one port), so this counts physical Thunderbolt links.
        let portsWithDevice = ports.filter { port in
            guard let socketID = ThunderboltTopology.socketID(for: port),
                  let root = ThunderboltTopology.hostRoot(forSocketID: socketID, in: thunderboltSwitches)
            else { return false }
            return !ThunderboltTopology.tree(from: root, in: thunderboltSwitches).isEmpty
        }

        let hostPortServiceName = portsWithDevice.count == 1
            ? portsWithDevice.first?.serviceName
            : nil
        return Result(devices: tunnelled, hostPortServiceName: hostPortServiceName)
    }
}
