#include <windows.h>
#include <iostream>
#include <cstdio>
#include <QDebug>

void platform_init() {
    // Attach to parent console or allocate a new one for debug logs in Windows GUI mode
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        std::cout.clear();
        std::clog.clear();
        std::cerr.clear();
        std::ios::sync_with_stdio();
    }
    qInfo().noquote() << "Windows platform initialized with terminal logging.";
}
