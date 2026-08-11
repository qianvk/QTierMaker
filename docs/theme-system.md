# Theme System

`ThemeManager` exposes semantic tokens rather than raw palette choices. The token groups cover:

- window, sidebar, content, elevated, popover, and control surfaces;
- normal, hovered, pressed, selected, disabled, warning, destructive, and success states;
- primary, secondary, tertiary, disabled, and symbol text/icon roles;
- borders, separators, shadows, tier rows, image borders, and drop targets.

The app supports System, Light, and Dark appearance. `ThemeManager` synchronizes the application
palette and VkUI theme, then emits `themeChanged`; custom-painted widgets invalidate their local
caches and repaint from the new semantic tokens.
