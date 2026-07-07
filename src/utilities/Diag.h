// =============================================================================
//  Diag.h - Ultra-lightweight file logging for load-time diagnostics.
//
//  BricsCAD runs the CAD engine inside a bubblewrap sandbox, which blocks gdb
//  and hides crash minidumps. To locate a load-time crash we instead append a
//  line to a log file (flushing immediately) at each step; after a crash the
//  last line written tells us exactly which step faulted.
//
//  Uses only <cstdio> so it has no dependency on our other layers and cannot
//  itself be the cause of a failure. Enable/disable with EL_DIAG (default on
//  for now; flip to 0 to compile the calls out entirely).
// =============================================================================
#pragma once

#ifndef EL_DIAG
#define EL_DIAG 1
#endif

#if EL_DIAG
#include <cstdio>
#include <cstdlib>

namespace electrical {
// Logging is OPT-IN: it does nothing unless the environment variable EL_DIAG is
// set (to any value). This keeps normal loads silent while allowing on-site
// diagnosis of a load/UI fault by launching BricsCAD with EL_DIAG=1. Output goes
// to $HOME/oe-init.log (one flushed line per step).
inline void diagLog(const char* msg) {
    static const bool enabled = (std::getenv("EL_DIAG") != nullptr);
    if (!enabled) return;
    const char* home = std::getenv("HOME");
    char path[1024];
    std::snprintf(path, sizeof(path), "%s/oe-init.log", home ? home : "/tmp");
    if (FILE* f = std::fopen(path, "a")) {
        std::fprintf(f, "%s\n", msg);
        std::fflush(f);
        std::fclose(f);
    }
}
} // namespace electrical

#define EL_DIAG_LOG(msg) ::electrical::diagLog(msg)
#else
#define EL_DIAG_LOG(msg) ((void)0)
#endif
