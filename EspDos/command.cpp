#include "command.h"
#include "dos.h"
#include "kernel.h" // Needed for some direct checks if int86 isn't enough, but try to use int86

std::vector<ExternalCommand> Command::externalCommands;

void Command::registerCommand(String name, CmdFunc func) {
    name.toUpperCase();
    externalCommands.push_back({name, func});
}

void Command::begin() {
    Serial.println("\nESP-DOS Version 2.00");
    Serial.println("(C) Copyright Microsoft Corp 1981, 1982, 1983");
    Serial.println("(C) Ported to ESP32 by Jules 2024");
    printPrompt();
}

void Command::loop() {
    if (Serial.available()) {
        String line = readLine();
        if (line.length() > 0) {
            processLine(line);
        }
        printPrompt();
    }
}

void Command::printPrompt() {
    // Determine current drive
    union REGS regs;
    regs.h.ah = 0x19; // Get current disk
    int86(0x21, &regs, &regs);
    char drive = 'A' + regs.h.al;

    Serial.printf("\n%c", drive);

    // Determine current directory
    // AH=0x47
    char buf[65];
    regs.h.ah = 0x47;
    regs.h.dl = 0; // Current drive
    regs.x.si = (uintptr_t)buf;
    int86(0x21, &regs, &regs);

    if (strlen(buf) > 0) {
        Serial.print(":\\");
        Serial.print(buf);
        Serial.print(">");
    } else {
        Serial.print(":\\>");
    }
}

String Command::readLine() {
    // Use DOS Buffered Input (AH=0x0A) to demonstrate compatibility
    // Max len 128
    uint8_t buffer[130];
    buffer[0] = 128;

    union REGS regs;
    regs.h.ah = 0x0A;
    regs.x.dx = (uintptr_t)buffer;

    // This call blocks until CR, mimicking DOS behavior implemented in Kernel
    int86(0x21, &regs, &regs);

    Serial.println(); // Newline after input

    // buffer[1] is length, buffer[2...] is chars
    int len = buffer[1];
    char str[129];
    memcpy(str, buffer + 2, len);
    str[len] = 0;

    return String(str);
}

void Command::parseCommand(String line, String &cmd, String &args) {
    line.trim();
    int space = line.indexOf(' ');
    if (space == -1) {
        cmd = line;
        args = "";
    } else {
        cmd = line.substring(0, space);
        args = line.substring(space + 1);
        args.trim();
    }
    cmd.toUpperCase();
}

void Command::processLine(String line) {
    String cmd, args;
    parseCommand(line, cmd, args);

    if (cmd == "") return;

    if (cmd == "DIR") cmdDir(args);
    else if (cmd == "TYPE") cmdType(args);
    else if (cmd == "CLS") cmdCls();
    else if (cmd == "VER") cmdVer();
    else if (cmd == "MD" || cmd == "MKDIR") cmdMkdir(args);
    else if (cmd == "RD" || cmd == "RMDIR") cmdRmdir(args);
    else if (cmd == "CD" || cmd == "CHDIR") cmdChdir(args);
    else if (cmd == "DEL" || cmd == "ERASE") cmdDel(args);
    else {
        // Check external commands
        bool found = false;
        for (const auto& ext : externalCommands) {
            if (ext.name == cmd) {
                // Prepare argv
                std::vector<char*> argv;
                argv.push_back((char*)cmd.c_str()); // argv[0] is program name

                // Split args properly (space delimited)
                char argBuf[128];
                strncpy(argBuf, args.c_str(), sizeof(argBuf) - 1);
                argBuf[sizeof(argBuf) - 1] = 0;

                char* token = strtok(argBuf, " ");
                while (token != NULL) {
                    argv.push_back(token);
                    token = strtok(NULL, " ");
                }

                ext.func(argv.size(), argv.data());
                found = true;
                break;
            }
        }

        if (!found) {
            Serial.println("Bad command or file name");
        }
    }
}

// --- Internal Commands ---

void Command::cmdDir(String args) {
    // Use FindFirst/FindNext (AH=4E/4F)

    String spec = args;
    if (spec == "") spec = "*.*";
    else if (spec.endsWith("/") || spec.endsWith("\\")) spec += "*.*";
    // If it's a directory, append *.*
    // Need to check? For now assume user types DIR or DIR path/*.*

    // Set DTA
    struct find_t dta;
    union REGS regs;
    regs.h.ah = 0x1A;
    regs.x.dx = (uintptr_t)&dta;
    int86(0x21, &regs, &regs);

    // Find First
    // Fix spec to be absolute or relative correctly handled by Kernel
    char pathBuf[64];
    strcpy(pathBuf, spec.c_str());

    regs.h.ah = 0x4E;
    regs.x.cx = _A_NORMAL | _A_SUBDIR; // Attributes
    regs.x.dx = (uintptr_t)pathBuf;

    int res = int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        Serial.println("File not found");
        return;
    }

    int fileCount = 0;

    while (!regs.x.cflag) {
        // Print entry
        // Name (30 chars align)
        String name = dta.name;
        Serial.print(name);
        for (int i = name.length(); i < 14; i++) Serial.print(" ");

        if (dta.attrib & _A_SUBDIR) {
            Serial.print("<DIR>     ");
        } else {
             Serial.printf(" %9ld", dta.size);
        }

        Serial.println();
        fileCount++;

        // Find Next
        regs.h.ah = 0x4F;
        int86(0x21, &regs, &regs);
    }

    Serial.printf(" %d File(s)\n", fileCount);

    // Free space
    regs.h.ah = 0x36; // Disk free space
    regs.h.dl = 0;    // Default drive
    int86(0x21, &regs, &regs);

    // AX=sec/clus, BX=avail clus, CX=bytes/sec
    unsigned long freeBytes = (unsigned long)regs.x.bx * regs.x.ax * regs.x.cx;
    Serial.printf(" %ld bytes free\n", freeBytes);
}

void Command::cmdType(String args) {
    if (args == "") {
        Serial.println("Required parameter missing");
        return;
    }

    char path[64];
    strcpy(path, args.c_str());

    union REGS regs;
    // Open File (3D)
    regs.h.ah = 0x3D;
    regs.h.al = 0; // Read only
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        Serial.println("File not found");
        return;
    }

    int handle = regs.x.ax;
    char buffer[128];

    while (true) {
        regs.h.ah = 0x3F; // Read
        regs.x.bx = handle;
        regs.x.cx = sizeof(buffer);
        regs.x.dx = (uintptr_t)buffer;
        int86(0x21, &regs, &regs);

        if (regs.x.cflag) break; // Error
        int bytesRead = regs.x.ax;
        if (bytesRead == 0) break; // EOF

        // Print
        Serial.write((uint8_t*)buffer, bytesRead);
    }

    // Close
    regs.h.ah = 0x3E;
    regs.x.bx = handle;
    int86(0x21, &regs, &regs);
    Serial.println();
}

void Command::cmdCls() {
    Serial.print("\033[2J\033[H");
}

void Command::cmdVer() {
    Serial.println("ESP-DOS Version 2.00");
}

void Command::cmdMkdir(String args) {
    if (args == "") {
        Serial.println("Unable to create directory");
        return;
    }
    char path[64];
    strcpy(path, args.c_str());

    union REGS regs;
    regs.h.ah = 0x39;
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        Serial.println("Unable to create directory");
    }
}

void Command::cmdRmdir(String args) {
    if (args == "") {
        Serial.println("Required parameter missing");
        return;
    }
    char path[64];
    strcpy(path, args.c_str());

    union REGS regs;
    regs.h.ah = 0x3A;
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        Serial.println("Invalid path, not directory, or directory not empty");
    }
}

void Command::cmdChdir(String args) {
    if (args == "") {
        // Print current directory
        union REGS regs;
        char buf[65];
        regs.h.ah = 0x47;
        regs.h.dl = 0;
        regs.x.si = (uintptr_t)buf;
        int86(0x21, &regs, &regs);
        Serial.printf("C:\\%s\n", buf);
        return;
    }

    char path[64];
    strcpy(path, args.c_str());

    union REGS regs;
    regs.h.ah = 0x3B;
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        Serial.println("Invalid directory");
    }
}

void Command::cmdDel(String args) {
    if (args == "") {
        Serial.println("Required parameter missing");
        return;
    }

    char path[64];
    strcpy(path, args.c_str());

    union REGS regs;
    regs.h.ah = 0x41;
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        Serial.println("File not found");
    }
}
