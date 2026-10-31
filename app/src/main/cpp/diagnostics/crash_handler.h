#pragma once

#include <string>

namespace ps5ona::crash {

// Installs handlers for fatal signals (SIGSEGV, SIGBUS, SIGFPE, SIGILL,
// SIGABRT, SIGTRAP). On a crash a plain-text report is written to
// <crash_dir>/native-crash-<epoch>.txt, then the previous handler (normally
// Android's debuggerd) runs so the system tombstone is still produced.
//
// `header` is copied into every report (device / build information).
void Install(const std::string& crash_dir, const std::string& header);

// Deliberately crashes the process. Used by the debug-only "test crash"
// button to verify the whole reporting pipeline end to end.
[[noreturn]] void TriggerTestCrash();

}  // namespace ps5ona::crash
