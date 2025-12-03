#include <Arduino.h>
#include "USERIO.h"

// --- SYS ---
int sys_main(int argc, char* argv[]) {
    DosIO::println("System transferred");
    return 0;
}
