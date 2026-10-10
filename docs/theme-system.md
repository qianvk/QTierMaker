# Theme System

`ThemeManager` exposes semantic tokens rather than raw palette choices. The token groups cover:

- window, sidebar, content, elevated, popover, and control surfaces;
- normal, hovered, pressed, selected, disabled, warning, destructive, and success states;
- primary, secondary, tertiary, disabled, and symbol text/icon roles;
- borders, separators, shadows, tier rows, image borders, and drop targets.

The app supports System, Light, and Dark appearance. `ThemeManager` maps the persisted appearance
preference onto `VkThemeManager`. VkUI owns the application palette and semantic theme generation;
custom-painted widgets observe its `themeChanged` signal, invalidate affected caches, and repaint
from the new semantic tokens.

Title-bar icon actions, including the sidebar's New action, use ordinary `QToolButton` controls.
VkUI's `VStyle` owns their hover, pressed, disabled, and focus appearance with its default theme
metrics. The New button belongs to the sidebar title bar, so panel collapse clips it with the
sidebar on every supported platform.
