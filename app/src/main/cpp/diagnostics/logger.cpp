#include "logger.h"

#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <ctime>
#include <mutex>

namespace ps5ona::log {
namespace {

constexpr long kMaxLogBytes = 2 * 1024 * 1024;

std::mutex g_mutex;
FILE* g_file = nullptr;
std::string g_path;
std::string g_rotated_path;

char LevelChar(Level level) {
    switch (level) {
        case Level::Verbose: return 'V';
        case Level::Debug: return 'D';
        case Level::Info: return 'I';
        case Level::Warn: return 'W';
        case Level::Error: return 'E';
        case Level::Fatal: return 'F';
    }
    return '?';
}

void OpenLocked() {
    g_file = std::fopen(g_path.c_str(), "a");
}

void RotateIfNeededLocked() {
    if (g_file == nullptr || std::ftell(g_file) < kMaxLogBytes) {
        return;
    }
    std::fclose(g_file);
    g_file = nullptr;
    std::rename(g_path.c_str(), g_rotated_path.c_str());
    OpenLocked();
}

}  // namespace

void Init(const std::string& log_dir) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file != nullptr) {
        return;
    }
    mkdir(log_dir.c_str(), 0700);
    g_path = log_dir + "/native.log";
    g_rotated_path = log_dir + "/native.log.1";
    OpenLocked();
    if (g_file == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, PS5_LOG_TAG, "Could not open %s; logging to logcat only",
                            g_path.c_str());
        return;
    }
    std::fseek(g_file, 0, SEEK_END);
    RotateIfNeededLocked();
}

void WriteV(Level level, const char* tag, const char* fmt, va_list args) {
    char message[2048];
    va_list copy;
    va_copy(copy, args);
    std::vsnprintf(message, sizeof(message), fmt, copy);
    va_end(copy);

    __android_log_write(static_cast<int>(level), tag, message);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file == nullptr) {
        return;
    }

    timespec now{};
    clock_gettime(CLOCK_REALTIME, &now);
    tm local{};
    localtime_r(&now.tv_sec, &local);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);

    std::fprintf(g_file, "%s.%03ld %5d %c %s: %s\n", stamp, now.tv_nsec / 1000000L, gettid(), LevelChar(level),
                 tag, message);
    std::fflush(g_file);
    RotateIfNeededLocked();
}

void Write(Level level, const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    WriteV(level, tag, fmt, args);
    va_end(args);
}

void Flush() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file != nullptr) {
        std::fflush(g_file);
    }
}

}  // namespace ps5ona::log
