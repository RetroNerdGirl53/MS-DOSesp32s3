#include <Arduino.h>
#include "DOS.h"
#include "USERIO.h"

// --- MORE ---
int more_main(int argc, char* argv[]) {
    int lines = 0;
    char buffer[1];
    union REGS regs;

    while (true) {
        regs.h.ah = 0x3F;
        regs.x.bx = 0; // STDIN
        regs.x.cx = 1;
        regs.x.dx = (uintptr_t)buffer;
        int86(0x21, &regs, &regs);

        if (regs.x.cflag || regs.x.ax == 0) break; // EOF

        // Write char using DOS
        regs.h.ah = 0x40;
        regs.x.bx = 1; // STDOUT
        regs.x.cx = 1;
        regs.x.dx = (uintptr_t)buffer;
        int86(0x21, &regs, &regs);

        if (buffer[0] == '\n') {
            lines++;
            if (lines >= 23) {
                DosIO::print("-- More --");

                // Wait for key
                DosIO::readChar();

                DosIO::print("\r          \r"); // Clear "More"
                lines = 0;
            }
        }
    }
    return 0;
}
