# Release Process

QTierMaker follows Semantic Versioning. Stable releases use tags such as `v1.0.0`; internal and
public test builds use prerelease tags such as `v1.1.0-beta.1`.

The localized changelog in each release's `updates.json` is the canonical version history.
QTierMaker intentionally does not maintain a separate `CHANGELOG.md`.

1. Release and tag VkUI first. The dependency release contains source only.
2. Update the QTierMaker VkUI submodule to that exact release commit.
3. Update `project(QTierMaker VERSION x.y.z)` in the root `CMakeLists.txt`. This is the single
   version source for the application UI, native metadata, installers, and update packages.
4. Build and run the unit tests, then generate the platform installers with CPack.
5. Push an annotated tag whose version exactly matches the source version. The `Platform
   Installers` workflow rejects mismatched tags before compiling, then publishes checksums and
   `updates.json` with the GitHub Release.

Prerelease tags append only their semantic suffix through the process-local `QTM_VERSION_SUFFIX`
environment variable. CI also passes `QTM_EXPECTED_VERSION` as an assertion; neither value is stored
in CMake cache or allowed to replace the numeric source version.

Stable releases publish only the supported native architectures:

- `QTierMaker-<version>-Windows-x64-Setup.exe`
- `QTierMaker-<version>-WinUpdate-x64.exe`
- `QTierMaker-<version>-macOS-arm64.dmg`
- `QTierMaker-<version>-macOS-arm64-Update.zip`

The application reads the GitHub Releases API. Stable builds ignore prereleases; beta builds accept
both prerelease and stable releases and select the highest compatible Semantic Version. Downloads
are accepted only when the release asset reports a matching platform, architecture, size, and
SHA-256 digest.

When the selected release includes `updates.json`, the application reads optional release notes
from `localizations.en.changelog` and `localizations.zh_CN.changelog`, using the application
language (or the resolved system language). Missing or invalid localized metadata falls back to the
GitHub release body and never changes the package URL, size, or checksum selected from the release.

Every Windows release contains a full NSIS installer and a `WinUpdate` executable. The update
package carries only the signed application executable, verifies the installed `runtime-version`,
waits for the running application to exit, replaces it with rollback protection, and restarts it.
Clients use the compact package only when its runtime generation and GitHub asset metadata match;
otherwise they use the full installer. Bump `QTM_UPDATE_RUNTIME_VERSION` whenever Qt or the deployed
dynamic plugin/runtime set changes.

Every macOS release contains both the drag-install DMG and a direct-update ZIP. The ZIP contains the
complete ad-hoc signed `QTierMaker.app`, rather than only its main executable, so Qt frameworks,
plugins, resources, metadata, and the bundle signature always move together. The application
verifies the manifest size and SHA-256 digest, then launches a standalone helper that validates the
bundle identifier, version, executable, and code signature. After QTierMaker exits, the helper
replaces the application on the same volume with rollback protection and relaunches it. The DMG is
used when the current application is on a read-only/translocated volume, its parent directory is not
writable, or the helper is unavailable.

The Windows NSIS installer installs machine-wide under `Program Files` by default. It intentionally
omits the software OpenGL renderer, legacy D3D compiler, DXC, Qt translations, and the VC
redistributable installer. CI audits those exclusions and installs only the five MSVC runtime DLLs
imported by the application and deployed Qt libraries.

## Sample Project Asset

The optional sample project is published once under the dedicated `samples-v1` prerelease as
`QTierMaker-Sample-Anime-Girls-v5.tar`. Normal `vX.Y.Z` releases must not duplicate this asset.
The application pins both the immutable download URL and its SHA-256 digest in
`SampleProjectDownloader.cpp`.

To revise the sample, create a new `samples-vN` prerelease instead of replacing the existing asset.
Build the TAR from the repository's `samples` directory, compute its SHA-256 digest, then update
the URL and digest together in the application:

```sh
cd samples
cmake -E tar cf ../QTierMaker-Sample-Anime-Girls-v5.tar --format=gnutar -- "Anime Girls v5"
```
