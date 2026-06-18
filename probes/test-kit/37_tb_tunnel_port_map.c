// Map a Thunderbolt-tunnelled USB device back to its physical USB-C port.
//
// Why this exists: when a Thunderbolt display or dock is connected (e.g. an
// Apple Studio Display), the USB devices behind it (mouse, keyboard, hub) do
// NOT sit under the native Apple Silicon USB bus for that port. They arrive over
// the Thunderbolt PCIe tunnel and surface under a tunnelled host controller,
// `AppleUSBXHCITR`, in a separate registry subtree with no `UsbIOPort` ancestor.
// So WhatCable's normal device->port correlation (probe 36's locationID match,
// plus the `UsbIOPort` walk) misses them entirely, and the device is orphaned
// (public issue #274, AORUS / Studio Display reports).
//
// The join that DOES tie them to a port runs through the Thunderbolt fabric, not
// the USB tree. Apple Silicon exposes each Thunderbolt port as two sibling
// `AppleARMIODevice` roots that share an index N:
//
//     apciecN@...   the PCIe-C tunnel   (hosts AppleUSBXHCITR -> tunnelled USB)
//     acioN@...     the Thunderbolt HAL (hosts the host IOThunderboltSwitch)
//
// The host switch under acioN carries a `UID`, which is the same value WhatCable
// already computes per port as `thunderboltSwitchUID`. So the attribution chain
// is: tunnelled device -> AppleUSBXHCITR -> apciecN  ==(by index)==  acioN ->
// host switch UID -> the Port-USB-C@N whose thunderboltSwitchUID matches.
//
// This was confirmed end to end on ONE 2-port laptop (issue #274 dump). It is
// NOT yet proven on 3-4 port Macs (Studio / Pro / larger MBP), and the existing
// test-kit probes don't capture the apciec/acio/XHCITR side at all. This probe
// captures exactly that subtree as raw data:
//
//   1. apciecN roots          (via ApplePCIECHostBridge paths)   - idle-safe
//   2. acioN roots + host UID  (via IOThunderboltSwitch)          - idle-safe
//   3. tunnelled controllers   (AppleUSBXHCITR path + locationID) - when attached
//   4. tunnelled USB devices   (IOUSBHostDevice under a TR)       - when attached
//
// Sections 1+2 confirm the apciecN<->acioN index pairing even with nothing
// plugged in; 3+4 confirm the device->port chain end to end when a TB dock or
// display is attached. Pairing is by index N, parsed offline from the paths; do
// not assume it here. No serial numbers or EDID are read (model name only).
//
// Compile: clang -framework IOKit -framework CoreFoundation -o 37_tb_tunnel_port_map 37_tb_tunnel_port_map.c

#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <string.h>

// Read a CFNumber property as long long. Returns -1 if absent.
static long long readNumber(io_service_t s, CFStringRef key) {
    CFTypeRef v = IORegistryEntryCreateCFProperty(s, key, kCFAllocatorDefault, 0);
    long long out = -1;
    if (v && CFGetTypeID(v) == CFNumberGetTypeID()) {
        CFNumberGetValue(v, kCFNumberLongLongType, &out);
    }
    if (v) CFRelease(v);
    return out;
}

// Copy a CFString property into buf. Returns 1 on success.
static int readString(io_service_t s, CFStringRef key, char *buf, size_t n) {
    buf[0] = '\0';
    CFTypeRef v = IORegistryEntryCreateCFProperty(s, key, kCFAllocatorDefault, 0);
    int ok = 0;
    if (v && CFGetTypeID(v) == CFStringGetTypeID()) {
        ok = CFStringGetCString(v, buf, n, kCFStringEncodingUTF8) ? 1 : 0;
    }
    if (v) CFRelease(v);
    return ok;
}

// The IOService-plane path. Starts at the plane root, so the apciecN / acioN
// token sits near the front and survives truncation of a long path.
static void readPath(io_service_t s, char *buf, size_t n) {
    io_string_t path = {0};
    if (IORegistryEntryGetPath(s, kIOServicePlane, path) == KERN_SUCCESS) {
        snprintf(buf, n, "%s", path);
    } else {
        snprintf(buf, n, "(path unavailable)");
    }
}

// True if any ancestor in the IOService plane is an AppleUSBXHCITR controller,
// i.e. this device arrived over a Thunderbolt PCIe tunnel rather than the native
// USB bus. Walks up to the plane root, releasing each node on the way (no leak on
// any exit path, including loop exhaustion).
static int hasTunnelAncestor(io_service_t s) {
    io_service_t cur = s;
    IOObjectRetain(cur);
    int tunnelled = 0;
    for (int depth = 0; depth < 64; depth++) {
        io_service_t parent = 0;
        kern_return_t kr = IORegistryEntryGetParentEntry(cur, kIOServicePlane, &parent);
        IOObjectRelease(cur);
        if (kr != KERN_SUCCESS || !parent) { cur = 0; break; }
        cur = parent;
        io_name_t name = {0};
        IORegistryEntryGetName(cur, name);
        if (strstr(name, "XHCITR")) { tunnelled = 1; break; }
    }
    if (cur) IOObjectRelease(cur);
    return tunnelled;
}

// Print name + path for every instance of a class. Used for the idle-safe
// structural roots (PCIe-C bridges -> apciecN, TB switches -> acioN). Returns the
// number of instances printed so callers can report "(none)" across several
// candidate class names. Does not print "(none)" itself.
static int dumpPaths(const char *cls, int withUID) {
    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                     IOServiceMatching(cls), &iter) != KERN_SUCCESS) {
        return 0;
    }
    io_service_t s;
    int n = 0;
    while ((s = IOIteratorNext(iter))) {
        io_name_t name = {0};
        IORegistryEntryGetName(s, name);
        char path[1024];
        readPath(s, path, sizeof(path));
        if (withUID) {
            long long uid = readNumber(s, CFSTR("UID"));
            printf("  %-28s UID=%llu (0x%llx)\n      %s\n",
                   name, (unsigned long long)uid, (unsigned long long)uid, path);
        } else {
            printf("  %-28s %s\n", name, path);
        }
        n++;
        IOObjectRelease(s);
    }
    IOObjectRelease(iter);
    return n;
}

int main(void) {
    printf("=== Thunderbolt-tunnelled USB -> physical port map (issue #274) ===\n");
    printf("Pair apciecN with acioN by index N (offline). Host-switch UID matches WhatCable's per-port thunderboltSwitchUID. Tunnelled devices live under AppleUSBXHCITR, not the native bus.\n\n");

    // 1. PCIe-C tunnel roots. Paths contain apciecN@... (idle-safe).
    printf("--- PCIe-C host bridges (-> apciecN tunnel root) ---\n");
    if (dumpPaths("ApplePCIECHostBridge", 0) == 0) printf("  (none)\n");

    // 2. Thunderbolt switches. Host switches carry the UID and sit under acioN
    //    (idle-safe); a nested switch is an attached dock/display. Paths show
    //    which is which by depth. Apple uses the class prefix `IOIOThunderboltSwitch*`
    //    on some Macs/macOS (older, e.g. Type5) and `IOThunderboltSwitch*` on others
    //    (M5 / macOS 26, Type7), so match BOTH or the host UID goes missing on half
    //    the fleet (mirrors IOIOThunderboltSwitchWatcher.matchClasses).
    printf("\n--- IOThunderboltSwitch (host switch UID -> acioN root; nested = attached device) ---\n");
    {
        int n = dumpPaths("IOIOThunderboltSwitch", 1);
        n += dumpPaths("IOThunderboltSwitch", 1);
        if (n == 0) printf("  (none)\n");
    }

    // 3. Tunnelled USB host controllers. Present only with a TB device attached.
    printf("\n--- AppleUSBXHCITR (tunnelled USB host controllers) ---\n");
    {
        io_iterator_t iter;
        if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                         IOServiceMatching("AppleUSBXHCITR"), &iter) == KERN_SUCCESS) {
            io_service_t s;
            int n = 0;
            while ((s = IOIteratorNext(iter))) {
                long long loc = readNumber(s, CFSTR("locationID"));
                char path[1024];
                readPath(s, path, sizeof(path));
                printf("  locationID=%lld (0x%llx)\n      %s\n",
                       loc, (unsigned long long)loc, path);
                n++;
                IOObjectRelease(s);
            }
            if (n == 0) printf("  (none - no Thunderbolt-tunnelled USB controller active)\n");
            IOObjectRelease(iter);
        }
    }

    // 4. Tunnelled USB devices: those with an AppleUSBXHCITR ancestor. Match a
    //    device's path/locationID back to a controller above, and the controller's
    //    apciecN to an acioN host-switch UID, to close device -> port.
    printf("\n--- Tunnelled USB devices (IOUSBHostDevice under a TR controller) ---\n");
    {
        io_iterator_t iter;
        int tunnelled = 0, total = 0;
        if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                         IOServiceMatching("IOUSBHostDevice"), &iter) == KERN_SUCCESS) {
            io_service_t s;
            while ((s = IOIteratorNext(iter))) {
                total++;
                if (hasTunnelAncestor(s)) {
                    tunnelled++;
                    long long loc = readNumber(s, CFSTR("locationID"));
                    char product[256];
                    if (!readString(s, CFSTR("USB Product Name"), product, sizeof(product)) || !product[0]) {
                        io_name_t nm = {0};
                        IORegistryEntryGetName(s, nm);
                        snprintf(product, sizeof(product), "%s", nm);
                    }
                    char path[1024];
                    readPath(s, path, sizeof(path));
                    printf("  locationID=%lld (0x%llx)  %s\n      %s\n",
                           loc, (unsigned long long)loc, product, path);
                }
                IOObjectRelease(s);
            }
            IOObjectRelease(iter);
        }
        if (tunnelled == 0) printf("  (none - no devices behind a Thunderbolt tunnel)\n");
        printf("\n  (%d of %d connected USB devices are tunnelled)\n", tunnelled, total);
    }

    return 0;
}
