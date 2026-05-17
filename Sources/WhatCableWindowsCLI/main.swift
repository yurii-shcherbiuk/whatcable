import WhatCableCore
import WhatCableWindowsBackend

let backend = WindowsSnapshotProvider()

do {
    let snapshot = try await backend.snapshot()
    if snapshot.ports.isEmpty {
        print("No USB-C cables detected.")
    } else {
        let text = TextFormatter.render(
            ports: snapshot.ports,
            sources: snapshot.powerSources,
            identities: snapshot.identities,
            showRaw: false,
            adapter: snapshot.adapter,
            thunderboltSwitches: snapshot.thunderboltSwitches,
            isDesktopMac: false,
            federatedIdentities: snapshot.federatedIdentities,
            usb3Transports: snapshot.usb3Transports
        )
        print(text)
    }
} catch {
    print("Error: \(error)")
}
