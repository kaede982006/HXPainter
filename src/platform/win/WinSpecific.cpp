#include <windows.h>
#include <iostream>
#include <cstdio>
#include <QDebug>

void platform_init() {
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        std::cout.clear();
        std::clog.clear();
        std::cerr.clear();
        std::ios::sync_with_stdio();
        qInfo().noquote() << "Windows platform initialized with terminal logging.";
    } else {
        // If not run from a terminal, redirect standard streams to NUL to prevent 
        // potential crashes when calling printf/qDebug in some Windows environments.
        FILE* dummy;
        freopen_s(&dummy, "NUL", "w", stdout);
        freopen_s(&dummy, "NUL", "w", stderr);
    }
}
