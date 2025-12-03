#ifndef USER_IO_H
#define USER_IO_H

#include <Arduino.h>
#include "dos.h"

// Helper class for Userland I/O using ONLY DOS Interrupts
// No direct Serial access allowed here.

class DosIO {
public:
    static void print(String str) {
        // AH=0x40 Write to Handle 1 (Stdout)
        // or AH=0x09 String Output (ends in $)

        // Use AH=0x40 for safety with binary data or strings without $
        const char* buf = str.c_str();
        int len = str.length();

        union REGS regs;
        regs.h.ah = 0x40;
        regs.x.bx = 1; // STDOUT
        regs.x.cx = len;
        regs.x.dx = (uintptr_t)buf;
        int86(0x21, &regs, &regs);
    }

    static void println(String str) {
        print(str + "\r\n");
    }

    static void println() {
        print("\r\n");
    }

    static void printf(const char* format, ...) {
        char buf[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        print(String(buf));
    }

    static char readChar() {
        // AH=0x01 Console Input with Echo
        // or AH=0x3F Read Handle 0
        // Use AH=0x01 for interactive shell char
        union REGS regs;
        regs.h.ah = 0x01;
        int86(0x21, &regs, &regs);
        return (char)regs.h.al;
    }
};

#endif
