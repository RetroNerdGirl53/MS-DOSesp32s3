#include "COMMAND.h"
#include "DOS.h"
#include "KERNEL.h"
#include "USERIO.h"

std::vector<ExternalCommand> Command::externalCommands;

// Batch file state
bool Command::batchActive = false;
String Command::batchFile;
int Command::batchLine = 0;
bool Command::echoOn = true;
std::vector<String> Command::batchParams;

// Environment
std::vector<std::pair<String, String>> Command::environment;

void Command::registerCommand(String name, CmdFunc func) {
    name.toUpperCase();
    externalCommands.push_back({name, func});
}

void Command::begin() {
    DosIO::println("\r\nESP-DOS Version 2.00");
    DosIO::println("(C) Copyright Microsoft Corp 1981, 1982, 1983");
    DosIO::println("(C) Ported to ESP32 by Jules 2024");

    // Set default path
    setEnv("PATH", "");
    setEnv("PROMPT", "$n$g");

    printPrompt();
}

void Command::loop() {
    // Check if batch is active
    if (batchActive) {
        String line = readBatchLine();
        if (line == "") {
            batchActive = false;
            printPrompt();
        } else {
            if (echoOn) DosIO::println(line); // Echo command
            processLine(line);
            if (!batchActive) printPrompt(); // If batch finished
        }
    } else {
        // Interactive mode - Block until input
        String line = readLine();
        if (line.length() > 0) {
            processLine(line);
        }
        printPrompt();
    }
}

void Command::printPrompt() {
    String p = getEnv("PROMPT");
    if (p == "") p = "$n$g";

    String out = "";
    for (int i=0; i<p.length(); i++) {
        if (p[i] == '$' && i+1 < p.length()) {
            char c = tolower(p[i+1]);
            if (c == 'n') { // Drive
                 union REGS regs;
                 regs.h.ah = 0x19;
                 int86(0x21, &regs, &regs);
                 out += (char)('A' + regs.h.al);
            } else if (c == 'g') { out += ">"; }
            else if (c == 'l') { out += "<"; }
            else if (c == 'b') { out += "|"; }
            else if (c == 'q') { out += "="; }
            else if (c == 't') { out += "00:00:00"; } // Fake time
            else if (c == 'd') { out += "Fri 01-01-2024"; } // Fake date
            else if (c == 'p') { // Current directory
                char buf[65];
                union REGS regs;
                regs.h.ah = 0x47;
                regs.h.dl = 0;
                regs.x.si = (uintptr_t)buf;
                int86(0x21, &regs, &regs);
                if (strlen(buf) > 0) {
                    out += ":\\";
                    out += buf;
                } else {
                    out += ":\\";
                }
            }
            else if (c == 'v') { out += "ESP-DOS Version 2.00"; }
            else if (c == '_') { out += "\r\n"; }
            else if (c == '$') { out += "$"; }
            i++;
        } else {
            out += p[i];
        }
    }

    DosIO::print(out);
}

String Command::readLine() {
    // Buffered Input AH=0x0A
    uint8_t buffer[130];
    buffer[0] = 128; // Max len

    union REGS regs;
    regs.h.ah = 0x0A;
    regs.x.dx = (uintptr_t)buffer;

    // Blocks until CR
    int86(0x21, &regs, &regs);

    DosIO::println(); // Newline

    int len = buffer[1];
    char str[129];
    memcpy(str, buffer + 2, len);
    str[len] = 0;

    return String(str);
}

String Command::readBatchLine() {
    // Read next line from batch file
    // Simplistic implementation: Re-open file, seek to line?
    // Inefficient. Better to hold handle open?
    // Since we can't easily persist the handle across loop calls without state,
    // let's just re-read or cache.
    // For authenticity, we should use a handle.
    // Let's implement open handle state in Command if possible or just use a helper.

    // Quick hack: Read file completely? No, bad for memory.
    // Let's use DOS file I/O to read line by line.

    union REGS regs;
    char path[64];
    strcpy(path, batchFile.c_str());

    // Open
    regs.h.ah = 0x3D;
    regs.h.al = 0;
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) return "";
    int handle = regs.x.ax;

    // Seek to current line start
    // We need to store file offset, not line number!
    // Re-purpose batchLine as offset.
    long offset = batchLine;
    regs.h.ah = 0x42;
    regs.h.al = 0; // From start
    regs.x.bx = handle;
    regs.x.cx = (offset >> 16);
    regs.x.dx = (offset & 0xFFFF);
    int86(0x21, &regs, &regs);

    String line = "";
    char c;
    while(true) {
        regs.h.ah = 0x3F;
        regs.x.bx = handle;
        regs.x.cx = 1;
        regs.x.dx = (uintptr_t)&c;
        int86(0x21, &regs, &regs);

        if (regs.x.cflag || regs.x.ax == 0) break;

        offset++;
        if (c == '\n') {
            break;
        } else if (c != '\r') {
            line += c;
        }
    }

    batchLine = offset; // Update offset

    regs.h.ah = 0x3E;
    regs.x.bx = handle;
    int86(0x21, &regs, &regs);

    if (line == "" && regs.x.ax == 0) return ""; // EOF empty
    return line;
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
    if (line.startsWith(":")) return; // Label
    if (line.length() == 0) return;

    // Expand %1 %2 etc if batch
    if (batchActive) {
        for (int i=0; i<batchParams.size(); i++) {
            String ph = "%" + String(i);
            line.replace(ph, batchParams[i]);
        }
    }

    String cmd, args;
    parseCommand(line, cmd, args);

    if (cmd == "") return;

    // Internal Commands
    if (cmd == "DIR") cmdDir(args);
    else if (cmd == "TYPE") cmdType(args);
    else if (cmd == "CLS") cmdCls();
    else if (cmd == "VER") cmdVer();
    else if (cmd == "MD" || cmd == "MKDIR") cmdMkdir(args);
    else if (cmd == "RD" || cmd == "RMDIR") cmdRmdir(args);
    else if (cmd == "CD" || cmd == "CHDIR") cmdChdir(args);
    else if (cmd == "DEL" || cmd == "ERASE") cmdDel(args);
    else if (cmd == "COPY") cmdCopy(args);
    else if (cmd == "REN" || cmd == "RENAME") cmdRen(args);
    else if (cmd == "VOL") cmdVol(args);
    else if (cmd == "DATE") cmdDate(args);
    else if (cmd == "TIME") cmdTime(args);
    else if (cmd == "ECHO") cmdEcho(args);
    else if (cmd == "PATH") cmdPath(args);
    else if (cmd == "PROMPT") cmdPrompt(args);
    else if (cmd == "SET") cmdSet(args);
    else if (cmd == "VERIFY") cmdVerify(args);
    else if (cmd == "PAUSE") cmdPause(args);
    else if (cmd == "REM") { /* Do nothing */ }
    else if (cmd == "EXIT") { batchActive = false; }
    else if (cmd == "SHIFT") cmdShift(args);
    else if (cmd == "GOTO") cmdGoto(args);
    else if (cmd == "IF") cmdIf(args);
    else if (cmd == "FOR") cmdFor(args);
    else {
        // External
        bool found = false;

        // 1. Check registered built-in externals
        for (const auto& ext : externalCommands) {
            if (ext.name == cmd || ext.name == (cmd + ".COM") || ext.name == (cmd + ".EXE")) {
                std::vector<char*> argv;
                argv.push_back((char*)cmd.c_str());

                char argBuf[128];
                strncpy(argBuf, args.c_str(), sizeof(argBuf)-1);
                argBuf[sizeof(argBuf)-1]=0;
                char* token = strtok(argBuf, " ");
                while (token) { argv.push_back(token); token = strtok(NULL, " "); }

                ext.func(argv.size(), argv.data());
                found = true;
                break;
            }
        }

        // 2. TODO: Check disk for .COM / .EXE / .BAT
        if (!found) {
             // Check if BAT
             String batName = cmd;
             if (!batName.endsWith(".BAT")) batName += ".BAT";

             union REGS regs;
             regs.h.ah = 0x3D;
             regs.h.al = 0;
             regs.x.dx = (uintptr_t)batName.c_str();
             int86(0x21, &regs, &regs);
             if (!regs.x.cflag) {
                 // Close handle
                 int h = regs.x.ax;
                 regs.h.ah = 0x3E; regs.x.bx = h; int86(0x21, &regs, &regs);

                 // Run batch
                 batchActive = true;
                 batchFile = batName;
                 batchLine = 0;
                 batchParams.clear();
                 // Parse args
                 batchParams.push_back(cmd);
                 String tmpArgs = args;
                 while(tmpArgs.length() > 0) {
                     int sp = tmpArgs.indexOf(' ');
                     if (sp == -1) { batchParams.push_back(tmpArgs); break; }
                     batchParams.push_back(tmpArgs.substring(0, sp));
                     tmpArgs = tmpArgs.substring(sp+1);
                     tmpArgs.trim();
                 }
                 found = true;
             }
        }

        if (!found) {
            DosIO::println("Bad command or file name");
        }
    }
}

// --- Environment ---
String Command::getEnv(String key) {
    for (auto &p : environment) {
        if (p.first == key) return p.second;
    }
    return "";
}

void Command::setEnv(String key, String val) {
    for (auto &p : environment) {
        if (p.first == key) {
            p.second = val;
            return;
        }
    }
    environment.push_back({key, val});
}

// --- Internal Commands ---

void Command::cmdDir(String args) {
    String spec = args;
    if (spec == "") spec = "*.*";
    else if (spec.endsWith("/") || spec.endsWith("\\")) spec += "*.*";

    struct find_t dta;
    union REGS regs;
    regs.h.ah = 0x1A;
    regs.x.dx = (uintptr_t)&dta;
    int86(0x21, &regs, &regs);

    char pathBuf[64];
    strcpy(pathBuf, spec.c_str());

    regs.h.ah = 0x4E;
    regs.x.cx = _A_NORMAL | _A_SUBDIR;
    regs.x.dx = (uintptr_t)pathBuf;

    int res = int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        DosIO::println("File not found");
        return;
    }

    int fileCount = 0;

    while (!regs.x.cflag) {
        String name = dta.name;
        DosIO::print(name);
        for (int i = name.length(); i < 14; i++) DosIO::print(" ");

        if (dta.attrib & _A_SUBDIR) {
            DosIO::print("<DIR>     ");
        } else {
             char sz[16];
             sprintf(sz, " %9ld", dta.size);
             DosIO::print(sz);
        }

        DosIO::println();
        fileCount++;

        regs.h.ah = 0x4F;
        int86(0x21, &regs, &regs);
    }

    DosIO::printf(" %d File(s)\n", fileCount);

    regs.h.ah = 0x36;
    regs.h.dl = 0;
    int86(0x21, &regs, &regs);
    unsigned long freeBytes = (unsigned long)regs.x.bx * regs.x.ax * regs.x.cx;
    DosIO::printf(" %ld bytes free\n", freeBytes);
}

void Command::cmdType(String args) {
    if (args == "") {
        DosIO::println("Required parameter missing");
        return;
    }

    char path[64];
    strcpy(path, args.c_str());

    union REGS regs;
    regs.h.ah = 0x3D;
    regs.h.al = 0;
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        DosIO::println("File not found");
        return;
    }

    int handle = regs.x.ax;
    char buffer[128];

    while (true) {
        regs.h.ah = 0x3F;
        regs.x.bx = handle;
        regs.x.cx = sizeof(buffer);
        regs.x.dx = (uintptr_t)buffer;
        int86(0x21, &regs, &regs);

        if (regs.x.cflag) break;
        int bytesRead = regs.x.ax;
        if (bytesRead == 0) break;

        // Write to stdout (1)
        regs.h.ah = 0x40;
        regs.x.bx = 1;
        regs.x.cx = bytesRead;
        regs.x.dx = (uintptr_t)buffer;
        int86(0x21, &regs, &regs);
    }

    regs.h.ah = 0x3E;
    regs.x.bx = handle;
    int86(0x21, &regs, &regs);
    DosIO::println();
}

void Command::cmdCls() {
    DosIO::print("\033[2J\033[H");
}

void Command::cmdVer() {
    DosIO::println("ESP-DOS Version 2.00");
}

void Command::cmdMkdir(String args) {
    if (args == "") {
        DosIO::println("Unable to create directory");
        return;
    }
    char path[64];
    strcpy(path, args.c_str());

    union REGS regs;
    regs.h.ah = 0x39;
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        DosIO::println("Unable to create directory");
    }
}

void Command::cmdRmdir(String args) {
    if (args == "") {
        DosIO::println("Required parameter missing");
        return;
    }
    char path[64];
    strcpy(path, args.c_str());

    union REGS regs;
    regs.h.ah = 0x3A;
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        DosIO::println("Invalid path, not directory, or directory not empty");
    }
}

void Command::cmdChdir(String args) {
    if (args == "") {
        char buf[65];
        union REGS regs;
        regs.h.ah = 0x47;
        regs.h.dl = 0;
        regs.x.si = (uintptr_t)buf;
        int86(0x21, &regs, &regs);
        DosIO::printf("C:\\%s\n", buf);
        return;
    }

    char path[64];
    strcpy(path, args.c_str());

    union REGS regs;
    regs.h.ah = 0x3B;
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        DosIO::println("Invalid directory");
    }
}

void Command::cmdDel(String args) {
    if (args == "") {
        DosIO::println("Required parameter missing");
        return;
    }

    char path[64];
    strcpy(path, args.c_str());

    union REGS regs;
    regs.h.ah = 0x41;
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        DosIO::println("File not found");
    }
}

void Command::cmdCopy(String args) {
    // Basic COPY source dest
    int sp = args.indexOf(' ');
    if (sp == -1) {
        DosIO::println("Invalid parameters");
        return;
    }
    String src = args.substring(0, sp);
    String dst = args.substring(sp+1);
    dst.trim();

    // Open Src
    char srcPath[64]; strcpy(srcPath, src.c_str());
    union REGS regs;
    regs.h.ah = 0x3D; regs.h.al = 0; regs.x.dx = (uintptr_t)srcPath;
    int86(0x21, &regs, &regs);
    if (regs.x.cflag) { DosIO::println("File not found"); return; }
    int hSrc = regs.x.ax;

    // Create Dst
    char dstPath[64]; strcpy(dstPath, dst.c_str());
    regs.h.ah = 0x3C; regs.x.cx = 0; regs.x.dx = (uintptr_t)dstPath;
    int86(0x21, &regs, &regs);
    if (regs.x.cflag) {
        DosIO::println("Unable to create file");
        regs.h.ah=0x3E; regs.x.bx=hSrc; int86(0x21, &regs, &regs);
        return;
    }
    int hDst = regs.x.ax;

    // Copy loop
    char buf[128];
    while(true) {
        // Read
        regs.h.ah = 0x3F; regs.x.bx = hSrc; regs.x.cx = sizeof(buf); regs.x.dx = (uintptr_t)buf;
        int86(0x21, &regs, &regs);
        if (regs.x.cflag || regs.x.ax == 0) break;
        int count = regs.x.ax;

        // Write
        regs.h.ah = 0x40; regs.x.bx = hDst; regs.x.cx = count; regs.x.dx = (uintptr_t)buf;
        int86(0x21, &regs, &regs);
        if (regs.x.cflag || regs.x.ax != count) {
            DosIO::println("Write error");
            break;
        }
    }

    // Close
    regs.h.ah = 0x3E; regs.x.bx = hSrc; int86(0x21, &regs, &regs);
    regs.h.ah = 0x3E; regs.x.bx = hDst; int86(0x21, &regs, &regs);
    DosIO::println("    1 File(s) copied");
}

void Command::cmdRen(String args) {
    int sp = args.indexOf(' ');
    if (sp == -1) { DosIO::println("Invalid parameters"); return; }
    String src = args.substring(0, sp);
    String dst = args.substring(sp+1);
    dst.trim();

    // Not implemented in Kernel yet! Need AH=56h
    // Stub
    DosIO::println("Rename not implemented in Kernel yet");
}

void Command::cmdVol(String args) {
    DosIO::println(" Volume in drive C has no label");
}

void Command::cmdDate(String args) {
    if (args == "") {
        DosIO::println("Current date is Fri 01-01-2024"); // Fake
        DosIO::print("Enter new date: ");
        readLine(); // Ignore input
    }
}

void Command::cmdTime(String args) {
    if (args == "") {
        DosIO::println("Current time is 00:00:00.00"); // Fake
        DosIO::print("Enter new time: ");
        readLine(); // Ignore
    }
}

void Command::cmdEcho(String args) {
    if (args.equalsIgnoreCase("ON")) echoOn = true;
    else if (args.equalsIgnoreCase("OFF")) echoOn = false;
    else DosIO::println(args);
}

void Command::cmdPath(String args) {
    if (args == "") DosIO::println("PATH=" + getEnv("PATH"));
    else setEnv("PATH", args);
}

void Command::cmdPrompt(String args) {
    setEnv("PROMPT", args);
}

void Command::cmdSet(String args) {
    if (args == "") {
        for (auto &p : environment) {
            DosIO::println(p.first + "=" + p.second);
        }
    } else {
        int eq = args.indexOf('=');
        if (eq == -1) {
            setEnv(args, "");
        } else {
            setEnv(args.substring(0, eq), args.substring(eq+1));
        }
    }
}

void Command::cmdVerify(String args) {
    // Stub
    if (args == "") DosIO::println("VERIFY is off");
}

void Command::cmdPause(String args) {
    DosIO::println("Strike a key when ready . . .");
    DosIO::readChar();
    DosIO::println();
}

void Command::cmdShift(String args) {
    if (batchParams.size() > 0) {
        batchParams.erase(batchParams.begin());
    }
}

void Command::cmdGoto(String args) {
    if (!batchActive) return;
    String label = ":" + args;
    label.toUpperCase();

    // Rewind file and search for label
    // Use DOS file I/O to read line by line.

    union REGS regs;
    char path[64];
    strcpy(path, batchFile.c_str());

    // Open
    regs.h.ah = 0x3D;
    regs.h.al = 0;
    regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        DosIO::println("Batch file missing");
        batchActive = false;
        return;
    }
    int handle = regs.x.ax;

    long currentOffset = 0;
    bool found = false;

    while(true) {
        // Read char by char to form line
        String line = "";
        char c;
        long lineStart = currentOffset;

        while(true) {
            regs.h.ah = 0x3F;
            regs.x.bx = handle;
            regs.x.cx = 1;
            regs.x.dx = (uintptr_t)&c;
            int86(0x21, &regs, &regs);

            if (regs.x.cflag || regs.x.ax == 0) break; // EOF/Error

            currentOffset++;
            if (c == '\n') break;
            if (c != '\r') line += c;
        }

        if (line.length() == 0 && regs.x.ax == 0) break; // EOF

        line.trim();
        line.toUpperCase();

        if (line == label) {
            found = true;
            batchLine = currentOffset; // Set next line to read
            break;
        }
    }

    regs.h.ah = 0x3E;
    regs.x.bx = handle;
    int86(0x21, &regs, &regs);

    if (!found) {
        DosIO::println("Label not found");
        batchActive = false;
    }
}

void Command::cmdIf(String args) {
    // Very basic IF EXIST implementation
    if (args.startsWith("EXIST ")) {
        String file = args.substring(6);
        int sp = file.indexOf(' ');
        if (sp != -1) {
            String cmd = file.substring(sp+1);
            file = file.substring(0, sp);

            char path[64]; strcpy(path, file.c_str());
            union REGS regs;
            regs.h.ah = 0x3D; regs.h.al = 0; regs.x.dx = (uintptr_t)path;
            int86(0x21, &regs, &regs);
            if (!regs.x.cflag) {
                // Found
                int h = regs.x.ax;
                regs.h.ah = 0x3E; regs.x.bx = h; int86(0x21, &regs, &regs);
                processLine(cmd);
            }
        }
    }
}

void Command::cmdFor(String args) {
    // Stub
    DosIO::println("FOR loop not implemented");
}
