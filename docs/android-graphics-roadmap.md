# Android graphics and frame-generation plan

## Home screen

The launcher should remain deliberately quiet:

- Large box-art tiles are the primary content.
- A small **+ Game** action adds library folders.
- A small **Settings** action contains technical controls.
- Emulator diagnostics and tuning controls do not live on the library screen.
- Once Kyty metadata parsing is connected, the placeholder tiles should be replaced by the title's own icon / cover artwork when available.

## Internal resolution

PS5onA stores an internal rendering scale from 25% through 200%.

The UI is already present. When the Kyty Vulkan renderer is integrated, this setting should scale the guest render dimensions before presentation. It should not merely scale the final Android surface.

Suggested presets:

- 50% - performance
- 75% - balanced
- 100% - native target
- 125% / 150% / 200% - quality modes where hardware permits

## Turnip / custom Adreno drivers

PS5onA should support per-application custom Vulkan drivers rather than modifying Android's system driver.

Planned implementation:

1. User chooses a compatible Turnip driver ZIP/package through Android's Storage Access Framework.
2. PS5onA validates and copies the package into app-private storage.
3. A driver manager records installed driver versions.
4. The Vulkan bootstrap uses libadrenotools to load the selected custom Adreno driver for PS5onA.
5. **System driver** always remains available as the fallback.
6. If the device is not compatible with custom Adreno drivers, hide or disable this option.

The current UI stores the selected package URI. Native driver loading is a later integration milestone.

## Frame generation

The user-facing workflow should support Lossless Scaling frame generation without redistributing proprietary Lossless Scaling assets.

Planned workflow:

1. User legally obtains Lossless Scaling.
2. User selects their own `Lossless.dll` in PS5onA.
3. PS5onA imports/extracts only the assets required by the Android frame-generation pipeline into app-private storage.
4. The proprietary DLL is not included in the repository, APK, release assets, or automatic downloads.
5. Users can enable/disable frame generation and choose a multiplier.
6. Frame-generation initialization must fail gracefully and return to normal presentation if requirements are not met.

Initial UI options:

- Off / On
- 2x
- 3x
- 4x

Potential later controls:

- Performance mode
- Flow scale
- Anti-artifact mode
- Frame pacing
- FPS cap
- Generated-vs-real FPS diagnostics

## Performance order

Frame generation should not be treated as a replacement for achieving stable real frames. Recommended tuning order:

1. Select compatible Vulkan driver
2. Lower internal resolution if necessary
3. Reach stable base frame pacing
4. Enable frame generation
5. Raise frame-generation multiplier only when base performance remains stable
