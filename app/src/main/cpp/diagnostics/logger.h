#pragma once

#include <cstdarg>
#include <string>

namespace ps5ona::log {

enum class Level : int {
    Verbose = 2,
    Debug = 3,
    Info = 4,
    Warn = 5,
    Error = 6,
    Fatal = 7,
};

// Opens <log_dir>/native.log for appending. Lines are mirrored to logcat.
// When the file grows past the size limit it is rotated to native.log.1.
// Safe to call more than once; later calls are ignored.
void Init(const std::string& log_dir);

void Write(Level level, const char* tag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
void WriteV(Level level, const char* tag, const char* fmt, va_list args);

// Flushes buffered output. The logger flushes after every line, so this is
// only needed before handing files to another component.
void Flush();

}  // namespace ps5ona::log

#define PS5_LOG_TAG "PS5onA"
#define LOGV(...) ::ps5ona::log::Write(::ps5ona::log::Level::Verbose, PS5_LOG_TAG, __VA_ARGS__)
#define LOGD(...) ::ps5ona::log::Write(::ps5ona::log::Level::Debug, PS5_LOG_TAG, __VA_ARGS__)
#define LOGI(...) ::ps5ona::log::Write(::ps5ona::log::Level::Info, PS5_LOG_TAG, __VA_ARGS__)
#define LOGW(...) ::ps5ona::log::Write(::ps5ona::log::Level::Warn, PS5_LOG_TAG, __VA_ARGS__)
#define LOGE(...) ::ps5ona::log::Write(::ps5ona::log::Level::Error, PS5_LOG_TAG, __VA_ARGS__)
