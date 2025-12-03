#include <Arduino.h>
#include "DOS.h"

// --- HELLO ---
int hello_main(int argc, char* argv[]) {
    // Print using DOS API (int 21h, AH=09h)
    char msg[] = "Hello from a 'recompiled' DOS application running on ESP32!$";

    union REGS regs;
    regs.h.ah = 0x09;
    regs.x.dx = (uintptr_t)msg;
    int86(0x21, &regs, &regs);

    // Demonstrate file I/O
    // Create a file "HELLO.TXT"
    char filename[] = "HELLO.TXT";
    regs.h.ah = 0x3C; // Create
    regs.x.cx = 0;    // Attributes
    regs.x.dx = (uintptr_t)filename;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        char err[] = "\r\nError creating file.$";
        regs.h.ah = 0x09;
        regs.x.dx = (uintptr_t)err;
        int86(0x21, &regs, &regs);
    } else {
        int handle = regs.x.ax;
        char content[] = "This file was written by the hello application via DOS syscalls.\r\n";

        regs.h.ah = 0x40; // Write
        regs.x.bx = handle;
        regs.x.cx = strlen(content);
        regs.x.dx = (uintptr_t)content;
        int86(0x21, &regs, &regs);

        regs.h.ah = 0x3E; // Close
        regs.x.bx = handle;
        int86(0x21, &regs, &regs);

        char success[] = "\r\nCreated HELLO.TXT successfully.$";
        regs.h.ah = 0x09;
        regs.x.dx = (uintptr_t)success;
        int86(0x21, &regs, &regs);
    }

    char done[] = "\r\nExiting application.\r\n$";
    regs.h.ah = 0x09;
    regs.x.dx = (uintptr_t)done;
    int86(0x21, &regs, &regs);

    return 0;
}
