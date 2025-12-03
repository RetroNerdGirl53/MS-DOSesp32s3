#include <Arduino.h>
#include "DOS.h"
#include "USERIO.h"

// --- FIND ---
int find_main(int argc, char* argv[]) {
    if (argc < 2) {
        DosIO::println("FIND: Parameter format not correct");
        return 1;
    }

    String search = argv[1];
    // Remove quotes if present
    if (search.startsWith("\"") && search.endsWith("\"")) {
        search = search.substring(1, search.length()-1);
    }

    if (argc > 2) {
        // Read file
        String path = argv[2];
        char pBuf[64]; strcpy(pBuf, path.c_str());

        union REGS regs;
        regs.h.ah = 0x3D; regs.h.al = 0; regs.x.dx = (uintptr_t)pBuf;
        int86(0x21, &regs, &regs);

        if (regs.x.cflag) {
            DosIO::println("File not found");
            return 1;
        }
        int handle = regs.x.ax;

        DosIO::printf("---------- %s\n", argv[2]);

        String line = "";
        char c;
        while (true) {
             regs.h.ah = 0x3F; regs.x.bx = handle; regs.x.cx = 1; regs.x.dx = (uintptr_t)&c;
             int86(0x21, &regs, &regs);
             if (regs.x.cflag || regs.x.ax == 0) break;

             if (c == '\n') {
                 if (line.indexOf(search) != -1) DosIO::println(line);
                 line = "";
             } else if (c != '\r') {
                 line += c;
             }
        }
        if (line.indexOf(search) != -1) DosIO::println(line);

        regs.h.ah = 0x3E; regs.x.bx = handle; int86(0x21, &regs, &regs);

    } else {
        // Stdin
        String line = "";
        char buffer[1];
        union REGS regs;
        while(true) {
            regs.h.ah = 0x3F;
            regs.x.bx = 0;
            regs.x.cx = 1;
            regs.x.dx = (uintptr_t)buffer;
            int86(0x21, &regs, &regs);
            if (regs.x.cflag || regs.x.ax == 0) break;

            if (buffer[0] == '\n') {
                if (line.indexOf(search) != -1) {
                    DosIO::println(line);
                }
                line = "";
            } else if (buffer[0] != '\r') {
                line += buffer[0];
            }
        }
        if (line.length() > 0 && line.indexOf(search) != -1) {
            DosIO::println(line);
        }
    }

    return 0;
}
