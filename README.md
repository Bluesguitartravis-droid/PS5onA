# PS5onA

Android ARM64 research port of the open-source KytyPS5 translation layer, initially informed by the KytyPS5-Native fork.

## Goal

Bring as much of the KytyPS5 translation layer as practical to modern Android handhelds, with the Odin 3 Ultra as the primary development target.

## Current status

**Milestone 0: Android bootstrap**

The first target is intentionally small: build an ARM64 APK, start the native library, initialize Android logging, and prove that the Android/NDK/Vulkan toolchain works before attempting guest execution.

PS5 game executables are x86-64 while the target Android device is ARM64. A real game-running port will therefore require an x86-64 -> ARM64 guest CPU translation strategy in addition to Android platform work. The BC-250-specific native GPU backend from KytyPS5-Native is not applicable to Adreno hardware; PS5onA will target Vulkan.

## Planned architecture

- Android ARM64 (arm64-v8a)
- Kotlin/Java Android shell
- C++20 native core through JNI
- Vulkan presentation/rendering path
- SDL3 where it remains useful
- Android Storage Access Framework for user-selected game files
- Android controller support, including built-in handheld controls
- Exportable crash/diagnostic logs
- CI-built debug APK artifacts
- No game files, firmware, keys, or copyrighted system software included

## Upstream projects

- KytyPS5: https://github.com/KytyPS5/KytyPS5
- KytyPS5-Native: https://github.com/superkeka/KytyPS5-Native

PS5onA is independent and is not affiliated with Sony Interactive Entertainment or PlayStation.

## Legal use

Use only software and game data you are legally entitled to use. This repository does not distribute PlayStation firmware, games, keys, or proprietary Sony components.

## Roadmap

1. Android app + NDK bootstrap
2. Vulkan device/surface capability probe
3. Native logging and crash-report export
4. Port architecture-independent Kyty subsystems
5. Android filesystem/input/audio adaptation
6. Guest x86-64 execution backend for ARM64
7. Bring up simple PS5 homebrew/test binaries
8. Incremental commercial-game compatibility work

## Contributors

- [Bluesguitartravis-droid](https://github.com/Bluesguitartravis-droid) — project owner
- Claude (Anthropic) — AI development assistant
