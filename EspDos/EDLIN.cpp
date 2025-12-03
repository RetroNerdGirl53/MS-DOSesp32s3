#include <Arduino.h>
#include "DOS.h"
#include "USERIO.h"
#include <vector>

// Simple Line Editor (EDLIN style)
// Commands:
// I - Insert lines (before current line, or at end). Ends with ^Z or . on new line.
// L - List lines (range)
// D - Delete lines (range)
// E - End (Save & Exit)
// Q - Quit (No Save)
// Line numbers are 1-based.

static std::vector<String> buffer;
static int currentLine = 1; // 1-based current line pointer

void listLines(int start, int end) {
    if (buffer.empty()) return;
    if (start < 1) start = 1;
    if (end > buffer.size()) end = buffer.size();

    for (int i = start; i <= end; i++) {
        DosIO::printf("%4d: %s\n", i, buffer[i-1].c_str()); // Assuming no newlines stored in buffer strings? Or stripped?
    }
}

void insertLines(int start) {
    if (start < 1) start = 1;
    if (start > buffer.size() + 1) start = buffer.size() + 1;

    int lineNum = start;
    while (true) {
        DosIO::printf("%4d: ", lineNum);
        String line = "";
        char c;
        // Read line
        while (true) {
            union REGS regs;
            regs.h.ah = 0x01; // Echo input
            int86(0x21, &regs, &regs);
            c = regs.h.al;

            if (c == 26) { // ^Z
                DosIO::println();
                return;
            }
            if (c == '\r') {
                DosIO::println();
                break;
            }
            if (c == 8 || c == 127) { // Backspace
                 if (line.length() > 0) {
                     line = line.substring(0, line.length()-1);
                     DosIO::print(" \b"); // Erase char on screen
                 }
                 continue;
            }
            line += c;
        }

        // Check for single dot to end
        if (line == ".") return;

        // Insert
        if (lineNum > buffer.size()) {
            buffer.push_back(line);
        } else {
            buffer.insert(buffer.begin() + (lineNum - 1), line);
        }
        lineNum++;
    }
}

void deleteLines(int start, int end) {
    if (buffer.empty()) return;
    if (start < 1) start = 1;
    if (end > buffer.size()) end = buffer.size();
    if (start > end) return;

    buffer.erase(buffer.begin() + (start - 1), buffer.begin() + end);
}

int edlin_main(int argc, char* argv[]) {
    if (argc < 2) {
        DosIO::println("File name must be specified");
        return 1;
    }

    String filename = argv[1];
    filename.toUpperCase();

    // Load file if exists
    buffer.clear();
    currentLine = 1;

    char path[64]; strcpy(path, filename.c_str());
    union REGS regs;
    regs.h.ah = 0x3D; regs.h.al = 0; regs.x.dx = (uintptr_t)path;
    int86(0x21, &regs, &regs);

    if (!regs.x.cflag) {
        int handle = regs.x.ax;
        DosIO::println("End of input file");
        // Read lines
        String line = "";
        char c;
        while(true) {
            regs.h.ah = 0x3F; regs.x.bx = handle; regs.x.cx = 1; regs.x.dx = (uintptr_t)&c;
            int86(0x21, &regs, &regs);
            if (regs.x.cflag || regs.x.ax == 0) break;

            if (c == '\n') {
                buffer.push_back(line);
                line = "";
            } else if (c != '\r') {
                line += c;
            }
        }
        if (line.length() > 0) buffer.push_back(line);

        regs.h.ah = 0x3E; regs.x.bx = handle; int86(0x21, &regs, &regs);
    } else {
        DosIO::println("New file");
    }

    // Main loop
    while (true) {
        DosIO::print("*");

        // Read command line
        String cmdLine = "";
        char c;
        while(true) {
            regs.h.ah = 0x01; int86(0x21, &regs, &regs);
            c = regs.h.al;
            if (c == '\r') { DosIO::println(); break; }
            cmdLine += c;
        }
        cmdLine.toUpperCase();
        cmdLine.trim();
        if (cmdLine.length() == 0) continue;

        char cmd = cmdLine.charAt(cmdLine.length() - 1);

        // Parse numbers (very basic)
        // Format: [line][,line]cmd
        // Not implementing full range parsing yet, just simplified 1 param usually

        if (cmd == 'Q') {
            DosIO::print("Abort edit (Y/N)?");
            regs.h.ah = 0x01; int86(0x21, &regs, &regs);
            c = regs.h.al; DosIO::println();
            if (c == 'y' || c == 'Y') return 0;
        }
        else if (cmd == 'E') {
            // Save
            regs.h.ah = 0x3C; regs.x.cx = 0; regs.x.dx = (uintptr_t)path;
            int86(0x21, &regs, &regs);
            if (regs.x.cflag) { DosIO::println("Disk write error"); continue; }
            int handle = regs.x.ax;

            for (const auto& l : buffer) {
                String out = l + "\r\n";
                regs.h.ah = 0x40; regs.x.bx = handle; regs.x.cx = out.length(); regs.x.dx = (uintptr_t)out.c_str();
                int86(0x21, &regs, &regs);
            }
            regs.h.ah = 0x3E; regs.x.bx = handle; int86(0x21, &regs, &regs);
            return 0;
        }
        else if (cmd == 'I') {
            // Insert at... default current
            // Parse line number if present
            // Simple: just I -> insert at end if empty, or current?
            // DOS EDLIN I inserts before current line.
            // If empty, inserts at 1.
            insertLines(currentLine);
        }
        else if (cmd == 'L') {
            listLines(1, buffer.size());
        }
        else if (cmd == 'D') {
            // Delete current line
            deleteLines(currentLine, currentLine);
        }
        else {
            DosIO::println("Entry error");
        }
    }

    return 0;
}
