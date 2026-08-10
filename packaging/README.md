# Packaging and releases

QTierMaker uses Qt's CMake deployment API to assemble a self-contained application and CPack to
create the platform-native installer:

- macOS: a drag-to-Applications DMG (`DragNDrop`).
- Windows: a machine-wide x64 installer with Start Menu and uninstall integration (`NSIS`).

This keeps dependency discovery in Qt/CMake instead of duplicating `macdeployqt` and `windeployqt`
logic in shell scripts. The commands and signing details are documented in the platform folders.

## GitHub Actions

The `CI` workflow builds and tests on macOS and Windows. The `Platform Installers` workflow can be
run manually to produce workflow artifacts. Pushing a semantic version tag such as `v0.2.1` also
creates or updates the matching GitHub Release with the Windows x64 packages, macOS arm64
DMG, and `updates.json`. Artifact hashes are recorded in the update manifest instead of being
published as separate files.

macOS packages use ad-hoc signing and do not require certificates or private credentials. Gatekeeper
therefore requires users to explicitly allow QTierMaker from System Settings on first launch.
Windows Authenticode signing remains optional and uses these GitHub Actions secrets when present:

| Platform | Secret | Purpose |
| --- | --- | --- |
| Windows | `WINDOWS_CERTIFICATE_BASE64` | Base64-encoded Authenticode `.pfx` |
| Windows | `WINDOWS_CERTIFICATE_PASSWORD` | Password for the `.pfx` |

The workflow stores credentials only in the runner's temporary directory and removes them in an
always-run cleanup step. Certificates, private keys, and passwords must never be committed.
