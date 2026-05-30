#include <iostream>
#include <QDebug>

void platform_init() {
    // Linux terminal logging works out of the box. Just confirming it's active.
    qInfo().noquote() << "Linux platform initialized with terminal logging.";
}
