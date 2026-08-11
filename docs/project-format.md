# Project Format

QTierMaker projects use UTF-8 JSON with the extension `.qtm`. No legacy project extension is
accepted or migrated.

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

The standard disk layout is:

```text
Project Name/
  Project Name.qtm
  assets/
```

Changing the project title and saving renames both the managed folder and the project file. The new
storage is completed before the previous storage is removed.
