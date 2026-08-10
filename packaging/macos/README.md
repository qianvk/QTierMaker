# macOS Packaging

Configure and build a Release preset, then create the native drag-install DMG:

```sh
cmake --preset release
cmake --build --preset release
cpack --config build/release/CPackConfig.cmake -G DragNDrop
```

The install step runs Qt's deployment API before CPack creates the DMG. The generated image includes
the application and the standard `/Applications` symlink. After Qt frameworks, plugins, and
resources are installed, `macdeployqt` applies an ad-hoc signature to the final bundle. This requires
no certificate or private signing material and prevents deployment from leaving a stale signature.

The package is intentionally not Developer ID signed or notarized. After copying QTierMaker to
Applications, the first launch is blocked by Gatekeeper. Open **System Settings > Privacy &
Security**, find the QTierMaker message, and choose **Open Anyway**. Confirm **Open** when macOS asks
again. This override is normally required only once for that build.

The `Platform Installers` workflow runs an arm64 build for tags and manual dispatches and publishes
the resulting ad-hoc signed DMG without Apple credentials. A `vX.Y.Z` tag records the DMG SHA-256
hash in `updates.json`.
