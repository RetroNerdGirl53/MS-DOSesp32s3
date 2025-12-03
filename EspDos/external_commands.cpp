#include <Arduino.h>
#include "dos.h"
#include "kernel.h"
#include "user_io.h"
#include <vector>
#include <algorithm>

// --- Helpers ---

// Helper to read all lines from stdin
std::vector<String> readLinesFromStdin() {
    std::vector<String> lines;

    char buffer[256];
    String currentLine = "";

    union REGS regs;

    while (true) {
        regs.h.ah = 0x3F;
        regs.x.bx = 0; // STDIN
        regs.x.cx = 1;
        regs.x.dx = (uintptr_t)buffer;
        int86(0x21, &regs, &regs);

        if (regs.x.cflag || regs.x.ax == 0) break; // Error or EOF

        char c = buffer[0];
        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine = "";
        } else if (c != '\r') {
            currentLine += c;
        }
    }
    if (currentLine.length() > 0) lines.push_back(currentLine);
    return lines;
}

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

// --- CHKDSK ---
int chkdsk_main(int argc, char* argv[]) {
    DosIO::println();

    // Get Disk Free Space
    union REGS regs;
    regs.h.ah = 0x36; // Get Disk Free Space
    regs.h.dl = 0; // Default drive
    int86(0x21, &regs, &regs);

    // AX=sec/clus, BX=avail clus, CX=bytes/sec, DX=total clusters
    unsigned long bytesPerSector = regs.x.cx;
    unsigned long sectorsPerCluster = regs.x.ax;
    unsigned long availableClusters = regs.x.bx;
    unsigned long totalClusters = regs.x.dx;

    unsigned long clusterSize = bytesPerSector * sectorsPerCluster;
    unsigned long totalSpace = totalClusters * clusterSize;
    unsigned long freeSpace = availableClusters * clusterSize;

    DosIO::printf("Volume Serial Number is 0000-0000\n");
    DosIO::printf("%10lu bytes total disk space\n", totalSpace);
    DosIO::printf("%10lu bytes available on disk\n", freeSpace);
    DosIO::println();
    DosIO::printf("%10lu bytes total memory\n", ESP.getHeapSize());
    DosIO::printf("%10lu bytes free\n", ESP.getFreeHeap());

    return 0;
}

// --- FORMAT ---
int format_main(int argc, char* argv[]) {
    if (argc < 2) {
        DosIO::println("Required parameter missing");
        return 1;
    }

    String drive = argv[1];
    drive.toUpperCase();
    if (drive.endsWith(":")) drive = drive.substring(0, drive.length()-1);

    DosIO::printf("Insert new diskette for drive %s:\nand strike ENTER when ready", drive.c_str());

    // Wait for ENTER using DOS API
    while(true) {
        char c = DosIO::readChar();
        if (c == '\r') break;
    }
    DosIO::println();

    DosIO::println("Formatting...");
    // LittleFS format
    if (LittleFS.format()) {
        DosIO::println("Format complete.");
        // Report space
        chkdsk_main(0, NULL);
    } else {
        DosIO::println("Format failed.");
    }

    return 0;
}

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

// --- SORT ---
int sort_main(int argc, char* argv[]) {
    // Read all lines from stdin, sort, print
    std::vector<String> lines = readLinesFromStdin();

    // Simple sort
    std::sort(lines.begin(), lines.end());

    if (argc > 1 && String(argv[1]) == "/R") {
        std::reverse(lines.begin(), lines.end());
    }

    for (const auto& line : lines) {
        DosIO::println(line);
    }

    return 0;
}

// --- Stubs ---

int sys_main(int argc, char* argv[]) {
    DosIO::println("System transferred");
    return 0;
}

int debug_main(int argc, char* argv[]) {
    DosIO::println("DEBUG stub - not implemented");
    return 0;
}

int edlin_main(int argc, char* argv[]) {
    DosIO::println("EDLIN stub - not implemented");
    return 0;
}
