# Project Format

QTierMaker projects use UTF-8 JSON with the extension `.qtmproject`. Existing `.tlmproject`
projects remain readable and are written with the new extension the next time Save As is used.

The root object contains:

- `schemaVersion`: integer, currently `1`.
- `app`: writer metadata.
- `project`: id, name, path-derived metadata, and timestamps.
- `tiers`: ordered tier rows with stable ids.
- `images`: image entries with source/asset paths and assignment metadata.
- `canvas.imagePresentationMode`: `square` or `noCrop`; omitted values default to `square`.
- `settings`: project-level settings.

Unknown future fields are ignored when safe. Required malformed fields cause a clear validation error instead of a crash.

Asset paths are stored relative to the project directory where possible.
