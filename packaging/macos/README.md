# macOS Packaging

Configure and build a Release preset, then create the native drag-install DMG:

```sh
cmake --preset release
cmake --build --preset release
cpack --config build/release/CPackConfig.cmake -G DragNDrop
cmake --build build/release --target QTierMakerMacUpdatePackage
```

The install step runs Qt's deployment API before CPack creates the DMG. A deterministic finalizer
then keeps only the runtime used by QTierMaker: Qt Core, GUI, Widgets, Network, DBus, SVG, and
Concurrent; the Cocoa platform and native style; SecureTransport TLS; and GIF, JPEG, and WebP image
plugins. QML, Qt Quick, Virtual Keyboard, OpenGL, unused codecs, and optional network plugins are
removed. Every remaining Mach-O is thinned to arm64, the complete app is ad-hoc signed once, and its
architecture, dependencies, plugin allowlist, and signature are audited. The generated image
includes the application and the standard `/Applications` symlink.

The package is intentionally not Developer ID signed or notarized. After copying QTierMaker to
Applications, the first launch is blocked by Gatekeeper. Open **System Settings > Privacy &
Security**, find the QTierMaker message, and choose **Open Anyway**. Confirm **Open** when macOS asks
again. This override is normally required only once for that build.

The `Platform Installers` workflow runs an arm64 build for tags and manual dispatches and publishes
the resulting ad-hoc signed DMG without Apple credentials. A `vX.Y.Z` tag records the package
SHA-256 hashes in `updates.json`.

The update target installs through the same finalizer as the DMG and creates
`QTierMaker-<version>-macOS-arm64-Update.zip` with `ditto`. This preserves
the framework symlinks and executable modes of the complete signed app bundle without generating a
redundant `__MACOSX` AppleDouble tree.
The ZIP is the normal in-app update payload; the DMG remains the first-install and recovery package.
The updater never replaces only `Contents/MacOS/QTierMaker`, because an executable built against a
new Qt deployment can be incompatible with the installed frameworks, plugins, or resources and any
bundle modification invalidates the existing code-signature seal.

For a writable installed application, QTierMaker verifies the ZIP size and SHA-256 digest before
copying its bundled update helper to the user cache. The helper waits for QTierMaker to exit,
extracts and validates the new app in a sibling staging directory, atomically swaps the bundle with
a backup, relaunches the new version, and rolls back if replacement or relaunch fails. Applications
running from a disk image, App Translocation, or a non-writable directory fall back to opening the
DMG.

CI mounts the completed DMG, audits both packaged app bundles independently, and runs the production
binary's `--deployment-smoke-test`. The smoke test loads Cocoa, the native style, all advertised
image formats, SecureTransport, and the bundled update helper. Packaging fails if minimization ever
removes a required runtime component or a new SDK silently adds an unused one.
