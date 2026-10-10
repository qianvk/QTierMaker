# Third-Party Notices

## Qt

QTierMaker uses Qt 6 modules including Core, Gui, Widgets, Svg, Concurrent, and Test. See the Qt project for license terms that apply to your distribution model.

## VkUI

QTierMaker uses VkUI for semantic icons, standard Qt Widgets styling, controls, motion, and
native window management. VkUI is licensed under MIT. Its active window module uses VkUI-owned
AppKit and Win32 backends; the repository retains older Apache-2.0 QWindowKit source only as
uncompiled reference material. See https://github.com/qianvk/vkui for the complete license and
third-party notices.

## AndroidLiquidGlass

The rounded-rectangle SDF, lens refraction, seven-sample chromatic aberration, and directional
highlight formulas in `src/assets/LiquidGlassBackdrop.cpp` are a modified C++/Qt port of
AndroidLiquidGlass (Backdrop) at commit `b18eb0ff12c616546a68c72e7d0097f1ab286c87`.

Copyright 2025 Kyant. Licensed under the Apache License, Version 2.0. A copy is included at
`docs/licenses/Apache-2.0.txt`; the upstream license is available at
https://github.com/Kyant0/AndroidLiquidGlass/blob/kmp/LICENSE.
