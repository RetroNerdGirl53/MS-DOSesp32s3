#include <Arduino.h>
#include "DOS.h"
#include "KERNEL.h"
#include "USERIO.h"
#include <LittleFS.h>

extern int chkdsk_main(int argc, char* argv[]);

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
