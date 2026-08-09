# macOS Packaging

Configure and build a Release preset, then create the native drag-install DMG:

```sh
cmake --preset release
cmake --build --preset release
cpack --config build/release/CPackConfig.cmake -G DragNDrop
```

The install step runs Qt's deployment API before CPack creates the DMG. The generated image includes
the application and the standard `/Applications` symlink. Signing and notarization identities are
local/private and must never be committed.

For distribution outside local testing, configure
`-DQTM_MACOS_CODESIGN_IDENTITY="Developer ID Application: ..."`, then sign, submit, and staple the
DMG with credentials stored in the login keychain:

```sh
codesign --force --timestamp --sign "Developer ID Application: ..." QTierMaker-*.dmg
xcrun notarytool submit QTierMaker-*.dmg --keychain-profile QTierMaker --wait
xcrun stapler staple QTierMaker-*.dmg
```

The `Platform Installers` workflow runs an arm64 build for tags and manual dispatches. With the
signing and App Store Connect secrets listed in the parent packaging README, it imports the
certificate into an ephemeral keychain, signs the deployed app and DMG, submits the DMG to Apple's
notary service, and staples the ticket. A `vX.Y.Z` tag publishes the notarized arm64 DMG; its
SHA-256 hash is recorded in `updates.json`.
