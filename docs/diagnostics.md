# Diagnostics: logging and crash reports

Roadmap item 3. Everything stays in the app's private storage until the user chooses to export it.

## What gets recorded

| Location (app private storage) | Written by | Contents |
| --- | --- | --- |
| `files/logs/native.log` | Native logger | Timestamped log lines from C++ (`LOGI`/`LOGW`/…) and Kotlin (`Diagnostics.i/w/e`). Rotates to `native.log.1` at 2 MB. Every line is also mirrored to logcat under the `PS5onA` tag. |
| `files/crashes/native-crash-<epoch>.txt` | Native signal handler | Signal, code, fault address, device header, all ARM64 registers, symbolized backtrace. |
| `files/crashes/java-crash-<epoch>.txt` | Kotlin uncaught-exception handler | Thread, device header, full stack trace. |

The newest 20 crash reports are kept.

## Native crash handler

`cpp/diagnostics/crash_handler.cpp` installs `sigaction` handlers for `SIGSEGV`, `SIGBUS`, `SIGFPE`, `SIGILL`, `SIGABRT` and `SIGTRAP`.

- The handler avoids malloc, locks and stdio. The report is formatted by hand into fixed buffers and written with `write(2)`.
- It runs on an alternate signal stack on the thread that installed it, so the main thread can still report a stack overflow.
- After writing the report it restores the previous handler and re-raises the signal. Android's `debuggerd` therefore still produces the normal tombstone, which is available via `adb bugreport`.
- Frames are symbolized with `dladdr`. Offsets in the form `libps5ona.so +0x…` can be resolved against the unstripped library with `llvm-addr2line` or `ndk-stack`.

This matters once guest code runs. Any SIGSEGV that the future x86-64 → ARM64 JIT handles on purpose, such as fastmem faults, has to be caught *before* this handler. The handler should then forward only the faults it doesn't recognise.

## Using it

- **Settings → Diagnostics → Export logs & crash reports** builds a zip and opens the Android share sheet. The zip contains `device.txt`, `settings.txt`, `logs/`, `crashes/` and the last 5000 `logcat.txt` lines for this process.
- If a crash report was written during the previous run, PS5onA offers to export it at the next launch.
- **Debug builds only:** Settings has *Trigger native test crash* and *Trigger Kotlin test crash* buttons for checking the reporting pipeline end to end.

## Logging from code

```cpp
#include "diagnostics/logger.h"
LOGI("Vulkan device: %s", props.deviceName);
```

```kotlin
Diagnostics.i("Games folder selected: $name")
Diagnostics.e("Failed to parse param.json", error)
```
