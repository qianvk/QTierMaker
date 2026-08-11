# Architecture

QTierMaker is split into small modules:

- `app` owns startup and service construction.
- `window` owns the frameless shell, app title bar, routing, and shortcuts.
- `navigation` implements the sidebar model/view/delegate.
- `pages` contains Edit, Projects, and Preferences pages.
- `tier` contains domain data and tier board widgets.
- `persistence` handles JSON serialization, atomic saves, project repository operations, and recent-project metadata.
- `assets` handles import paths, local asset migration, image loading, and thumbnails.
- `export` renders projects to PNG, JPEG, or PDF.
- `settings`, `theme`, and `i18n` centralize app-wide state.
- `platform` isolates Finder/Explorer/file-manager behavior.

Domain data uses stable UUIDs rather than indexes. `ProjectHistory` records complete persistent
states in a `QUndoStack`, skips no-op edits, and treats the model as authoritative when restoring a
state. Persistence serializes the project into a readable schema-versioned JSON file and writes it
atomically.

`ProjectFileLayout` owns the `.qtm` naming contract. A managed project is stored as
`<project-name>/<project-name>.qtm` with assets beside the project file. A title rename first builds
the new storage, saves the new project, updates recent-project metadata, and only then removes the
old storage.
