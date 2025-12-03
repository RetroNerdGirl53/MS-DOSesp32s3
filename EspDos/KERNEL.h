#ifndef KERNEL_H
#define KERNEL_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <FFat.h>
#include <vector>

// Forward declaration of REGS from dos.h
// We can't include dos.h here if it includes kernel.h, circular dependency.
// But dos.h includes kernel.h. So we should forward declare here.
union REGS;
struct SREGS;

struct DosFileHandle {
    File file;
    bool is_open;
    String path;
};

struct DTA {
    uint8_t reserved[21];
    uint8_t attrib;
    uint16_t time;
    uint16_t date;
    uint32_t size;
    char name[13];
};

class Kernel {
public:
    static bool begin();

    // The core dispatcher
    static void handleInterrupt(int intNum, union REGS *in, union REGS *out, struct SREGS *seg);

private:
    static void handleInt21(union REGS *in, union REGS *out, struct SREGS *seg);
    static void handleInt20(union REGS *in, union REGS *out); // Terminate

    // INT 21h handlers
    static void consoleInput(union REGS *in, union REGS *out);          // AH=01, 07, 08, 0A
    static void consoleOutput(union REGS *in, union REGS *out);         // AH=02, 06, 09
    static void diskReset(union REGS *in, union REGS *out);             // AH=0D
    static void selectDisk(union REGS *in, union REGS *out);            // AH=0E
    static void getDiskFreeSpace(union REGS *in, union REGS *out);      // AH=36

    // File I/O
    static void openFile(union REGS *in, union REGS *out);              // AH=3D
    static void closeFile(union REGS *in, union REGS *out);             // AH=3E
    static void readFile(union REGS *in, union REGS *out);              // AH=3F
    static void writeFile(union REGS *in, union REGS *out);             // AH=40
    static void deleteFile(union REGS *in, union REGS *out);            // AH=41
    static void moveFilePointer(union REGS *in, union REGS *out);       // AH=42
    static void getFileAttributes(union REGS *in, union REGS *out);     // AH=43
    static void createDirectory(union REGS *in, union REGS *out);       // AH=39
    static void removeDirectory(union REGS *in, union REGS *out);       // AH=3A
    static void changeDirectory(union REGS *in, union REGS *out);       // AH=3B
    static void createFile(union REGS *in, union REGS *out);            // AH=3C

    // Search
    static void findFirst(union REGS *in, union REGS *out);             // AH=4E
    static void findNext(union REGS *in, union REGS *out);              // AH=4F
    static void setDTA(union REGS *in, union REGS *out);                // AH=1A

    static void getVersion(union REGS *in, union REGS *out);            // AH=30

public:
    static fs::FS* vol; // Active filesystem

private:
    // Helper methods
    static int getFreeHandle();
    static String getPathFromRegs(union REGS *in, struct SREGS *seg);
    static void setCarry(union REGS *out);
    static void clearCarry(union REGS *out);

    // State
    static std::vector<DosFileHandle> fileHandles;
    static DTA* currentDTA; // Pointer to current DTA (in ESP memory)
    static String currentDir; // Simplified CWD
    static uint8_t currentDrive; // 0=A, 2=C (Mapped to LittleFS)

    // Find state (simplification for single tasking)
    static File dirEnumFile;
    static String dirEnumPath;
    static String dirEnumPattern;
};

#endif
