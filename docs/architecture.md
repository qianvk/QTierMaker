# Architecture

QTierMaker is split into small modules:

- `app` owns startup and service construction.
- `window` owns the full-content native shell, app title bar, routing, and shortcuts.
- `navigation` implements the sidebar model/view/delegate.
- `pages` contains Edit, Projects, and Preferences pages.
- `tier` contains domain data and tier board widgets.
- `persistence` handles JSON serialization, atomic saves, project repository operations, and recent-project metadata.
- `assets` handles import paths, local asset migration, image loading, and thumbnails.
- `export` renders the displayed tier board to PNG or JPEG.
- `settings`, `theme`, and `i18n` centralize app-wide state.
- `platform` isolates Finder/Explorer/file-manager behavior.

Domain data uses stable UUIDs rather than indexes. `ProjectHistory` records complete persistent
states in a `QUndoStack`, skips no-op edits, and treats the model as authoritative when restoring a
state. Persistence serializes the project into a readable schema-versioned JSON file and writes it
atomically.

VkUI's `VWindowAgent` creates its host's native window during attachment. Each application dialog
owns a separate agent and native handle, including before it is shown; it never attaches to its
parent's native window. Agents are destroyed before their QWidget base so native callbacks detach
while the host remains valid.

On Windows, VkUI preserves live HWND visibility, modal disablement, and minimize/maximize
state when updating chrome capabilities. Full-client size calculations enter Qt's native-event
pipeline through a filter restricted to the agent's own HWND, allowing Qt to record the actual
frame margins. VkUI extends the DWM frame and forwards native caption messages to DWM,
which owns caption buttons, hover states, and Windows Snap Layouts. Qt's expanded-client
caption renderer is disabled on Windows. An alpha-only QWidget exposes the DWM button surface
without drawing buttons; the native caption color follows the app title bar's background surface.
Resizable Windows hosts retain native side/bottom resize borders, keeping the DWM close button
flush with the visible client edge. These margins stay consistent when caption buttons are hidden.
When DWM declines a maximized caption hit test, VkUI queries USER32's native accessible button
rectangles; drawing, hover states, and button actions still pass through the Windows frame APIs.
Native safe-area padding is disabled because the application's layouts already reserve its
own title bars. Explicit Windows light/dark preferences also update Qt's native caption color
scheme, while System appearance restores the OS selection.

Focus Mode hides the navigation animation slot after collapsing navigation. QSplitter then
removes that slot and its adjacent handle from the visible layout on all platforms. Changing
handle width to zero is insufficient: Qt retains an overlapping grab area and can show it again
after layout/style updates. Leaving Focus Mode restores the slot's explicit visibility and panel
state without changing handle width. Regression checks follow the real tier viewport and handle
visibility through resize, theme/font changes, maximized windows, and repeated toggles.
Tier row fill clips use the complete row rectangle, matching the viewport background clip;
half-pixel stroke alignment must not inset filled shapes. Pixel regressions check both horizontal
edges at 100%, 125%, 150%, and 200% rendering scales, as well as the actual Focus Mode viewport.
Tier board geometry reads VkUI's platform `windowCornerRadius` metric without rounding it to an
integer. Rows, canvas backgrounds, drag placeholders, and shadows share the same radius and clip
geometry. Image tiles intersect the outer row clip instead of replacing it. In Focus Mode, the
board fills the complete client rectangle and leaves corner clipping to the native window, avoiding
a second inset curve and preserving native maximized/fullscreen behavior on every platform.

Project list delegates use the same font metrics for text placement and row size hints. The title,
metadata, and path are allocated complete lines with padding on every platform, including larger
fonts and high-DPI screens; no platform-specific row heights are used.

`ProjectFileLayout` owns the `.qtm` naming contract. A managed project is stored as
`<project-name>/<project-name>.qtm` with assets beside the project file. A title rename first builds
the new storage, saves the new project, updates recent-project metadata, and only then removes the
old storage.
