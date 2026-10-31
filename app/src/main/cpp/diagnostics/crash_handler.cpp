#include "crash_handler.h"

#include "logger.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>
#include <unwind.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Everything reachable from HandleSignal must avoid malloc, locks and stdio:
// the crash may have happened while the heap or a lock was in a bad state.
// Output is formatted by hand into fixed buffers and written with write(2).
// dladdr and the unwinder are not formally async-signal-safe, but they are
// what Android's own crash tooling relies on and are reliable in practice.

namespace ps5ona::crash {
namespace {

constexpr int kSignals[] = {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT, SIGTRAP};
constexpr int kSignalCount = sizeof(kSignals) / sizeof(kSignals[0]);
constexpr size_t kAltStackSize = 64 * 1024;
constexpr int kMaxFrames = 64;

char g_crash_dir[512];
char g_header[2048];
struct sigaction g_previous[kSignalCount];
std::atomic<bool> g_handling{false};
std::atomic<bool> g_installed{false};

// ---- async-signal-safe output helpers ----

class Out {
public:
    explicit Out(int fd) : fd_(fd) {}

    Out& Str(const char* s) {
        if (s != nullptr) {
            Raw(s, std::strlen(s));
        }
        return *this;
    }

    Out& Dec(int64_t value) {
        char buf[24];
        int pos = sizeof(buf);
        const bool negative = value < 0;
        uint64_t v = negative ? static_cast<uint64_t>(-(value + 1)) + 1 : static_cast<uint64_t>(value);
        do {
            buf[--pos] = static_cast<char>('0' + v % 10);
            v /= 10;
        } while (v != 0);
        if (negative) {
            buf[--pos] = '-';
        }
        Raw(buf + pos, sizeof(buf) - pos);
        return *this;
    }

    Out& Hex(uint64_t value) {
        static constexpr char kDigits[] = "0123456789abcdef";
        char buf[18] = {'0', 'x'};
        for (int i = 0; i < 16; ++i) {
            buf[17 - i] = kDigits[value & 0xf];
            value >>= 4;
        }
        Raw(buf, sizeof(buf));
        return *this;
    }

    Out& Nl() { return Str("\n"); }

private:
    void Raw(const char* data, size_t len) {
        while (len > 0) {
            const ssize_t n = write(fd_, data, len);
            if (n <= 0) {
                return;
            }
            data += n;
            len -= static_cast<size_t>(n);
        }
    }

    int fd_;
};

const char* SignalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV (segmentation fault)";
        case SIGBUS: return "SIGBUS (bus error)";
        case SIGFPE: return "SIGFPE (arithmetic exception)";
        case SIGILL: return "SIGILL (illegal instruction)";
        case SIGABRT: return "SIGABRT (abort)";
        case SIGTRAP: return "SIGTRAP (trap)";
        default: return "unknown signal";
    }
}

const char* CodeName(int sig, int code) {
    if (code == SI_USER) return "SI_USER";
    if (code == SI_TKILL) return "SI_TKILL";
    if (code == SI_QUEUE) return "SI_QUEUE";
    switch (sig) {
        case SIGSEGV:
            if (code == SEGV_MAPERR) return "SEGV_MAPERR (address not mapped)";
            if (code == SEGV_ACCERR) return "SEGV_ACCERR (invalid permissions)";
            break;
        case SIGBUS:
            if (code == BUS_ADRALN) return "BUS_ADRALN (misaligned address)";
            if (code == BUS_ADRERR) return "BUS_ADRERR (nonexistent address)";
            break;
        case SIGFPE:
            if (code == FPE_INTDIV) return "FPE_INTDIV (integer divide by zero)";
            break;
        case SIGILL:
            if (code == ILL_ILLOPC) return "ILL_ILLOPC (illegal opcode)";
            break;
        default:
            break;
    }
    return "";
}

void WriteFrame(Out& out, int index, uintptr_t pc) {
    out.Str("  #");
    if (index < 10) {
        out.Str("0");
    }
    out.Dec(index).Str(" pc ").Hex(pc);

    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(pc), &info) != 0) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(info.dli_fbase);
        out.Str("  ");
        out.Str(info.dli_fname != nullptr ? info.dli_fname : "<unknown>");
        out.Str(" +").Hex(pc - base);
        if (info.dli_sname != nullptr) {
            const uintptr_t sym = reinterpret_cast<uintptr_t>(info.dli_saddr);
            out.Str(" (").Str(info.dli_sname).Str("+").Dec(static_cast<int64_t>(pc - sym)).Str(")");
        }
    }
    out.Nl();
}

struct UnwindState {
    Out* out;
    int index;
    uintptr_t crash_pc;
    bool reached_crash_frame;
};

_Unwind_Reason_Code UnwindCallback(_Unwind_Context* context, void* arg) {
    auto* state = static_cast<UnwindState*>(arg);
    const uintptr_t pc = _Unwind_GetIP(context);
    if (pc == 0) {
        return _URC_END_OF_STACK;
    }
    // Skip the signal handler's own frames; start at the faulting frame.
    if (!state->reached_crash_frame) {
        if (pc != state->crash_pc) {
            return _URC_NO_REASON;
        }
        state->reached_crash_frame = true;
        return _URC_NO_REASON;  // frame #00 was already printed from the register state
    }
    WriteFrame(*state->out, state->index++, pc);
    return state->index >= kMaxFrames ? _URC_END_OF_STACK : _URC_NO_REASON;
}

int OpenReportFile(time_t now) {
    char path[600];
    size_t pos = 0;
    auto append = [&](const char* s) {
        while (*s != '\0' && pos + 1 < sizeof(path)) {
            path[pos++] = *s++;
        }
    };
    append(g_crash_dir);
    append("/native-crash-");
    char digits[24];
    int d = sizeof(digits);
    digits[--d] = '\0';
    uint64_t v = static_cast<uint64_t>(now);
    do {
        digits[--d] = static_cast<char>('0' + v % 10);
        v /= 10;
    } while (v != 0);
    append(digits + d);
    append(".txt");
    path[pos] = '\0';
    return open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
}

void WriteReport(int fd, int sig, siginfo_t* info, void* raw_context) {
    Out out(fd);
    const time_t now = time(nullptr);

    out.Str("PS5onA native crash report\n");
    out.Str("==========================\n");
    out.Str("Time (unix): ").Dec(now).Nl();
    out.Str("Process: ").Dec(getpid()).Str("  Thread: ").Dec(gettid()).Nl();
    out.Str("Signal: ").Dec(sig).Str(" ").Str(SignalName(sig)).Nl();
    out.Str("Code: ").Dec(info->si_code).Str(" ").Str(CodeName(sig, info->si_code)).Nl();
    out.Str("Fault address: ").Hex(reinterpret_cast<uintptr_t>(info->si_addr)).Nl();
    out.Nl().Str(g_header).Nl();

    uintptr_t crash_pc = 0;
#if defined(__aarch64__)
    const auto* uc = static_cast<ucontext_t*>(raw_context);
    const auto& mc = uc->uc_mcontext;
    crash_pc = mc.pc;
    out.Nl().Str("Registers:\n");
    for (int i = 0; i < 31; ++i) {
        out.Str(i < 10 ? "  x0" : "  x").Dec(i).Str(" ").Hex(mc.regs[i]);
        if (i % 4 == 3) {
            out.Nl();
        }
    }
    out.Nl();
    out.Str("  sp  ").Hex(mc.sp).Str("  pc  ").Hex(mc.pc).Str("  pstate ").Hex(mc.pstate).Nl();
#else
    (void)raw_context;
#endif

    out.Nl().Str("Backtrace:\n");
    if (crash_pc != 0) {
        WriteFrame(out, 0, crash_pc);
    }
    UnwindState state{&out, crash_pc != 0 ? 1 : 0, crash_pc, crash_pc == 0};
    _Unwind_Backtrace(UnwindCallback, &state);
    if (!state.reached_crash_frame) {
        // The unwinder never reported the faulting pc, so frames could not be
        // filtered. Print the raw walk instead (it includes handler frames).
        out.Str("  -- unfiltered unwind (includes crash handler frames) --\n");
        UnwindState raw{&out, 1, 0, true};
        _Unwind_Backtrace(UnwindCallback, &raw);
    }
    out.Nl().Str("End of report\n");
}

int SignalIndex(int sig) {
    for (int i = 0; i < kSignalCount; ++i) {
        if (kSignals[i] == sig) {
            return i;
        }
    }
    return -1;
}

void HandleSignal(int sig, siginfo_t* info, void* context) {
    const int index = SignalIndex(sig);

    // Only the first crashing thread writes a report; a second fault while
    // reporting goes straight to the previous handler.
    bool expected = false;
    if (g_handling.compare_exchange_strong(expected, true)) {
        const int fd = OpenReportFile(time(nullptr));
        if (fd >= 0) {
            WriteReport(fd, sig, info, context);
            fsync(fd);
            close(fd);
        }
    }

    // Restore the previous handler (debuggerd) and re-deliver the signal so
    // Android still records a tombstone and the process terminates normally.
    if (index >= 0) {
        sigaction(sig, &g_previous[index], nullptr);
    } else {
        signal(sig, SIG_DFL);
    }
    syscall(SYS_tgkill, getpid(), gettid(), sig);
}

void CopyBounded(char* dest, size_t size, const std::string& src) {
    const size_t n = src.size() < size - 1 ? src.size() : size - 1;
    std::memcpy(dest, src.data(), n);
    dest[n] = '\0';
}

}  // namespace

void Install(const std::string& crash_dir, const std::string& header) {
    bool expected = false;
    if (!g_installed.compare_exchange_strong(expected, true)) {
        return;
    }

    CopyBounded(g_crash_dir, sizeof(g_crash_dir), crash_dir);
    CopyBounded(g_header, sizeof(g_header), header);

    // A stack overflow leaves no room on the faulting thread's stack, so the
    // handler runs on an alternate stack. This covers the installing thread;
    // other threads fall back to the normal stack.
    stack_t alt{};
    alt.ss_sp = std::malloc(kAltStackSize);
    alt.ss_size = kAltStackSize;
    alt.ss_flags = 0;
    if (alt.ss_sp != nullptr) {
        sigaltstack(&alt, nullptr);
    }

    struct sigaction action{};
    action.sa_sigaction = HandleSignal;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);

    for (int i = 0; i < kSignalCount; ++i) {
        if (sigaction(kSignals[i], &action, &g_previous[i]) != 0) {
            LOGW("Failed to install crash handler for signal %d", kSignals[i]);
        }
    }
    LOGI("Native crash handler installed; reports go to %s", g_crash_dir);
}

void TriggerTestCrash() {
    LOGW("Triggering deliberate native test crash");
    log::Flush();
    volatile int* null_pointer = nullptr;
    *null_pointer = 0x50533521;  // NOLINT: intentional
    std::abort();
}

}  // namespace ps5ona::crash
