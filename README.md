# QTierMaker

[![CI](https://github.com/qianvk/QTierMaker/actions/workflows/ci.yml/badge.svg)](https://github.com/qianvk/QTierMaker/actions/workflows/ci.yml)
[![Latest Release](https://img.shields.io/github/v/release/qianvk/QTierMaker)](https://github.com/qianvk/QTierMaker/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-2f80ed.svg)](LICENSE)

QTierMaker is a native, local-first tier-list editor for Windows and macOS, built with Qt 6 and
C++20. It combines direct drag-and-drop ranking with a project gallery, animated visual overviews,
image editing, and self-contained `.qtm` projects.

## About

- **Direct tier editing:** drag images between rows, reorder images and tiers, and edit row names
  and colors in place.
- **Gallery and visual overviews:** keep unranked images in the project gallery, then spread tier or
  gallery images across the board for fast scanning and selection.
- **Flexible image presentation:** use square thumbnails with per-image crop control or preserve
  original aspect ratios in an equal-height layout.
- **Focused preview:** inspect images in a window-level animated preview with optional projected
  image backgrounds.
- **Custom boards:** choose a background image, tune its visibility, use light or dark appearance,
  and export the finished tier list as PNG, JPEG, or PDF.
- **Reliable local projects:** imported assets, tier order, image assignments, crops, background,
  and presentation settings live together in a portable project folder.
- **Desktop workflow:** undo and redo, autosave, keyboard shortcuts, English and Simplified Chinese
  UI, and in-app updates for supported Windows x64 and macOS arm64 releases.

[Download the latest release](https://github.com/qianvk/QTierMaker/releases/latest) or see the
[user guide](docs/user-guide.md) to get started.

## Project Status

QTierMaker is a personal vibe-coding project published for use and reference. It does not accept
external code contributions or pull requests. Release changes are maintained in the localized
`updates.json` attached to each GitHub Release instead of a repository changelog.

## macOS Installation

The macOS build is distributed without Apple notarization. After copying QTierMaker to
Applications, try to open it once, then open **System Settings > Privacy & Security** and choose
**Open Anyway**. Confirm **Open** when macOS asks again.

## Edit and Arrange

![Anime Girls v5 open in the tier editor](docs/screenshots/editor.webp)

Arrange images directly on the tier board, customize tiers and backgrounds, and keep unranked
images ready in the project gallery.

## Gallery Overview

![Anime Girls v5 shown in Gallery Overview](docs/screenshots/gallery-overview.webp)

Gallery Overview brings the full image collection into one balanced canvas for quick scanning,
selection, and preview.

## Image Preview

![An Anime Girls v5 image shown in Preview](docs/screenshots/image-preview.webp)

Preview presents the selected image against the project background without leaving the current
workspace.

## Documentation

- [User guide](docs/user-guide.md)
- [Keyboard shortcuts](docs/shortcuts.md)
- [Project format](docs/project-format.md)
- [Build guide](docs/build.md)
- [Privacy](docs/privacy.md)
