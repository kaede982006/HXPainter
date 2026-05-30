#include <windows.h>
#include <iostream>
#include <cstdio>
#include <QDebug>

void platform_init() {
    // Attach to parent console if it exists (e.g., when run from cmd or PowerShell).
    // We intentionally DO NOT use AllocConsole() here to prevent a new empty 
    // console window from flashing/popping up when the user double-clicks the executable.
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        std::cout.clear();
        std::clog.clear();
        std::cerr.clear();
        std::ios::sync_with_stdio();
        qInfo().noquote() << "Windows platform initialized with terminal logging.";
    }
}
