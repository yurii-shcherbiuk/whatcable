import Foundation

// The bundle used for all localized strings in WhatCableCore.
// Defaults to the module bundle (system language). Call setCoreLocale(_:)
// to switch to a specific lproj bundle for live language switching.
public var _coreLocalizedBundle: Bundle = .module

public func setCoreLocale(_ identifier: String) {
    if identifier.isEmpty {
        _coreLocalizedBundle = .module
    } else if let url = Bundle.module.url(forResource: identifier, withExtension: "lproj"),
              let b = Bundle(url: url) {
        _coreLocalizedBundle = b
    } else {
        _coreLocalizedBundle = .module
    }
}

/// Single entry point for every localized string in WhatCableCore.
///
/// Apple platforms honor `_coreLocalizedBundle`, so `setCoreLocale(_:)`
/// live language switching keeps working exactly as before. Swift's
/// Windows Foundation has neither `String.LocalizationValue` nor any
/// `String(localized:)` initializer, so there the overload takes a
/// plain `String` and returns the default (English) text. Call sites
/// pass string literals (some interpolated), which satisfy
/// `String.LocalizationValue` on Apple and `String` on Windows.
#if canImport(Darwin)
public func coreLocalized(_ value: String.LocalizationValue) -> String {
    String(localized: value, bundle: _coreLocalizedBundle)
}
#else
public func coreLocalized(_ value: String) -> String {
    value
}
#endif
