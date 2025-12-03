#include "KERNEL.H"
#include "DOS.H"

// Initialize static members
std::vector<DosFileHandle> Kernel::fileHandles(20); // Standard 20 handles
DTA* Kernel::currentDTA = nullptr;
String Kernel::currentDir = "/";
uint8_t Kernel::currentDrive = 2; // Default to C:

File Kernel::dirEnumFile;
String Kernel::dirEnumPath;
String Kernel::dirEnumPattern;

// DTA buffer (simulated memory location)
static DTA defaultDTA;

int int86(int intr_num, union REGS *inregs, union REGS *outregs) {
    Kernel::handleInterrupt(intr_num, inregs, outregs, nullptr);
    return outregs->x.ax;
}

int intdos(union REGS *inregs, union REGS *outregs) {
    Kernel::handleInterrupt(0x21, inregs, outregs, nullptr);
    return outregs->x.ax;
}

int intdosx(union REGS *inregs, union REGS *outregs, struct SREGS *segregs) {
    Kernel::handleInterrupt(0x21, inregs, outregs, segregs);
    return outregs->x.ax;
}

bool Kernel::begin() {
    // Initialize LittleFS
    // Try "spiffs" label first (standard Arduino default)
    if (!LittleFS.begin(true, "/littlefs", 10, "spiffs")) {
        // Try "littlefs" label (some newer schemes)
        if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
             Serial.println("Error: Failed to mount LittleFS. Check Partition Scheme!");
             return false;
        }
    }

    // Set default DTA
    currentDTA = &defaultDTA;

    // Initialize handles
    for (auto &h : fileHandles) {
        h.is_open = false;
    }

    // Pre-open stdin, stdout, stderr, aux, prn (Handles 0-4)
    // For now we just mark them used, but handle I/O specially for them
    for (int i = 0; i < 5; i++) {
        fileHandles[i].is_open = true;
        fileHandles[i].path = "DEV";
    }

    return true;
}

void Kernel::handleInterrupt(int intNum, union REGS *in, union REGS *out, struct SREGS *seg) {
    // Copy in to out initially
    if (in != out) *out = *in;

    switch (intNum) {
        case 0x20: // Terminate
            handleInt20(in, out);
            break;
        case 0x21: // DOS Services
            handleInt21(in, out, seg);
            break;
        default:
            // Unhandled interrupt
            break;
    }
}

void Kernel::handleInt20(union REGS *in, union REGS *out) {
    Serial.println("Program Terminated.");
    // In a real OS this would kill the process. Here we might just return to shell loop?
    // For now, do nothing.
}

void Kernel::handleInt21(union REGS *in, union REGS *out, struct SREGS *seg) {
    uint8_t func = in->h.ah;

    switch (func) {
        case 0x01: // Console Input with Echo
        case 0x07: // Direct Console Input without Echo
        case 0x08: // Console Input without Echo
        case 0x0A: // Buffered Console Input
            consoleInput(in, out);
            break;
        case 0x02: // Console Output
        case 0x06: // Direct Console I/O
        case 0x09: // String Output
            consoleOutput(in, out);
            break;
        case 0x0D: diskReset(in, out); break;
        case 0x0E: selectDisk(in, out); break;
        case 0x19: // Get Current Disk
            out->h.al = currentDrive;
            break;
        case 0x1A: setDTA(in, out); break;
        case 0x30: getVersion(in, out); break;
        case 0x36: getDiskFreeSpace(in, out); break;
        case 0x39: createDirectory(in, out); break;
        case 0x3A: removeDirectory(in, out); break;
        case 0x3B: changeDirectory(in, out); break;
        case 0x3C: createFile(in, out); break;
        case 0x3D: openFile(in, out); break;
        case 0x3E: closeFile(in, out); break;
        case 0x3F: readFile(in, out); break;
        case 0x40: writeFile(in, out); break;
        case 0x41: deleteFile(in, out); break;
        case 0x42: moveFilePointer(in, out); break;
        case 0x43: getFileAttributes(in, out); break;
        case 0x47: // Get Current Directory
            // DS:SI buffer. Returns path without drive/root slash
            // Since we don't have real segments, this assumes buffer is passed via DX usually in simplified model,
            // but for 0x47 it's DS:SI.
            // Wait, this is tricky. Recompiled code will pass a pointer.
            // On ESP32, "Segments" are ignored, so DS:SI -> just SI (pointer).
            {
                char* buf = (char*)(uintptr_t)in->x.si; // Assuming SI holds the pointer
                String path = currentDir;
                if (path.startsWith("/")) path = path.substring(1);
                if (path.endsWith("/")) path = path.substring(0, path.length()-1);
                strcpy(buf, path.c_str());
                clearCarry(out);
            }
            break;
        case 0x4C: // Terminate with return code
            handleInt20(in, out);
            break;
        case 0x4E: findFirst(in, out); break;
        case 0x4F: findNext(in, out); break;
        default:
            // Unimplemented
            // Serial.printf("Unimplemented INT 21h AH=%02X\n", func);
            out->h.al = 0; // Success?
            break;
    }
}

// --- Console I/O ---

void Kernel::consoleInput(union REGS *in, union REGS *out) {
    if (in->h.ah == 0x01) {
        // Echo input
        while (!Serial.available()) delay(1);
        char c = Serial.read();
        Serial.print(c);
        out->h.al = c;
    } else if (in->h.ah == 0x08 || in->h.ah == 0x07) {
        // No echo
        while (!Serial.available()) delay(1);
        char c = Serial.read();
        out->h.al = c;
    } else if (in->h.ah == 0x0A) {
        // Buffered input
        // DS:DX points to buffer. Byte 0 = max len. Byte 1 = returned len. Bytes 2+ = string.
        uint8_t* buf = (uint8_t*)(uintptr_t)in->x.dx;
        uint8_t maxLen = buf[0];
        uint8_t count = 0;
        while (count < maxLen) {
            while (!Serial.available()) delay(1);
            char c = Serial.read();
            if (c == '\r') {
                // Serial.print(c); // Echo CR ? DOS usually does echoes CR as CR LF? Or just CR?
                // Actually DOS echoes CR as nothing usually in 0xA, just returns.
                // But shell expects new line.
                // Let's echo CR only.
                // Serial.print('\r');
                // Wait, Command::readLine prints newline after.

                // Let's just store it.
                buf[2 + count] = c; // Store CR as DOS expects
                // count does NOT increment for the CR in the count byte usually?
                // "The byte at offset 1 is set to the number of characters read, excluding the carriage return."
                break;
            }
            if (c == 8 || c == 127) { // Backspace
                if (count > 0) {
                    count--;
                    Serial.print("\b \b");
                }
                continue;
            }
            Serial.print(c);
            buf[2 + count] = c;
            count++;
        }
        buf[1] = count;
    }
}

void Kernel::consoleOutput(union REGS *in, union REGS *out) {
    if (in->h.ah == 0x02) {
        Serial.write(in->h.dl);
    } else if (in->h.ah == 0x06) {
        if (in->h.dl != 0xFF) {
            Serial.write(in->h.dl);
        } else {
            if (Serial.available()) {
                out->h.al = Serial.read();
                clearCarry(out);
                out->x.flags &= ~0x40; // Clear ZF (char available)
            } else {
                out->x.flags |= 0x40; // Set ZF (no char)
            }
        }
    } else if (in->h.ah == 0x09) {
        // Print string ending in $
        char* str = (char*)(uintptr_t)in->x.dx;
        while (*str != '$') {
            Serial.write(*str++);
        }
    }
}

// --- Helper Utilities ---

void Kernel::setCarry(union REGS *out) {
    out->x.cflag = 1;
    out->x.flags |= 1;
}

void Kernel::clearCarry(union REGS *out) {
    out->x.cflag = 0;
    out->x.flags &= ~1;
}

String Kernel::getPathFromRegs(union REGS *in, struct SREGS *seg) {
    // DS:DX
    char* str = (char*)(uintptr_t)in->x.dx;
    String path = String(str);

    // Normalize path
    if (path.indexOf(':') != -1) {
         // Strip drive letter for now (assume C:)
         path = path.substring(2);
    }

    if (!path.startsWith("/")) {
        // Relative path
        String base = currentDir;
        if (!base.endsWith("/")) base += "/";
        path = base + path;
    }

    // Resolve ".." (Basic)
    while (path.indexOf("/..") != -1) {
        int ddot = path.indexOf("/..");
        int prevSlash = path.lastIndexOf('/', ddot - 1);
        if (prevSlash != -1) {
            path = path.substring(0, prevSlash) + path.substring(ddot + 3);
            if (path == "") path = "/";
        } else {
            // Root
            path = "/";
        }
    }

    // Resolve "."
    while (path.indexOf("/./") != -1) {
        path.replace("/./", "/");
    }
    if (path.endsWith("/.")) path = path.substring(0, path.length()-2);

    return path;
}

int Kernel::getFreeHandle() {
    for (int i = 5; i < fileHandles.size(); i++) {
        if (!fileHandles[i].is_open) return i;
    }
    return -1;
}

// --- File System Calls ---

void Kernel::createFile(union REGS *in, union REGS *out) {
    String path = getPathFromRegs(in, nullptr);
    int handle = getFreeHandle();

    if (handle == -1) {
        out->x.ax = 4; // Too many open files
        setCarry(out);
        return;
    }

    File f = LittleFS.open(path, "w+");
    if (!f) {
        out->x.ax = 3; // Path not found (or access denied)
        setCarry(out);
        return;
    }

    fileHandles[handle].file = f;
    fileHandles[handle].is_open = true;
    fileHandles[handle].path = path;

    out->x.ax = handle;
    clearCarry(out);
}

void Kernel::openFile(union REGS *in, union REGS *out) {
    String path = getPathFromRegs(in, nullptr);
    int handle = getFreeHandle();

    if (handle == -1) {
        out->x.ax = 4;
        setCarry(out);
        return;
    }

    // Access mode in AL (0=Read, 1=Write, 2=RW)
    const char* mode = "r";
    if ((in->h.al & 0x03) == 1) mode = "r+";
    if ((in->h.al & 0x03) == 2) mode = "r+";

    File f = LittleFS.open(path, mode);
    if (!f) {
        out->x.ax = 2; // File not found
        setCarry(out);
        return;
    }

    fileHandles[handle].file = f;
    fileHandles[handle].is_open = true;
    fileHandles[handle].path = path;

    out->x.ax = handle;
    clearCarry(out);
}

void Kernel::closeFile(union REGS *in, union REGS *out) {
    int handle = in->x.bx;
    if (handle < 0 || handle >= fileHandles.size() || !fileHandles[handle].is_open) {
        out->x.ax = 6; // Invalid handle
        setCarry(out);
        return;
    }

    if (handle >= 5) {
        fileHandles[handle].file.close();
    }
    fileHandles[handle].is_open = false;
    clearCarry(out);
}

void Kernel::readFile(union REGS *in, union REGS *out) {
    int handle = in->x.bx;
    int count = in->x.cx;
    void* buf = (void*)(uintptr_t)in->x.dx;

    if (handle < 0 || handle >= fileHandles.size() || !fileHandles[handle].is_open) {
        out->x.ax = 6;
        setCarry(out);
        return;
    }

    // Handle stdin
    if (handle == 0) {
        // Read from console
        // For simplicity, do basic blocking read
        int read = 0;
        char* cbuf = (char*)buf;
        while (read < count) {
            // Wait for char
             while (!Serial.available()) delay(1);
             cbuf[read++] = Serial.read();
             // In DOS, reading from Stdin raw often returns after 1 char if not buffered?
             // Or waits for count?
             // Usually line buffered if cooked mode, raw if raw.
             // We're simulating "Cooked" basically but without line editing here?
             // Actually handle 0 is typically console input.
             // If we read 1 byte, we return.
        }
        out->x.ax = read;
        clearCarry(out);
        return;
    }

    size_t bytesRead = fileHandles[handle].file.read((uint8_t*)buf, count);
    out->x.ax = bytesRead;
    clearCarry(out);
}

void Kernel::writeFile(union REGS *in, union REGS *out) {
    int handle = in->x.bx;
    int count = in->x.cx;
    void* buf = (void*)(uintptr_t)in->x.dx;

    if (handle < 0 || handle >= fileHandles.size() || !fileHandles[handle].is_open) {
        out->x.ax = 6;
        setCarry(out);
        return;
    }

    // Handle stdout/stderr
    if (handle == 1 || handle == 2) {
        Serial.write((uint8_t*)buf, count);
        out->x.ax = count;
        clearCarry(out);
        return;
    }

    size_t bytesWritten = fileHandles[handle].file.write((uint8_t*)buf, count);
    out->x.ax = bytesWritten;
    clearCarry(out);
}

void Kernel::deleteFile(union REGS *in, union REGS *out) {
    String path = getPathFromRegs(in, nullptr);
    if (LittleFS.remove(path)) {
        clearCarry(out);
    } else {
        out->x.ax = 2; // File not found
        setCarry(out);
    }
}

void Kernel::moveFilePointer(union REGS *in, union REGS *out) {
    int handle = in->x.bx;
    // CX:DX offset
    long offset = (long)((in->x.cx << 16) | in->x.dx);
    uint8_t origin = in->h.al; // 0=start, 1=cur, 2=end

    if (handle < 0 || handle >= fileHandles.size() || !fileHandles[handle].is_open) {
        out->x.ax = 6;
        setCarry(out);
        return;
    }

    SeekMode mode = SeekSet;
    if (origin == 1) mode = SeekCur;
    if (origin == 2) mode = SeekEnd;

    if (fileHandles[handle].file.seek(offset, mode)) {
        size_t pos = fileHandles[handle].file.position();
        out->x.dx = (pos >> 16);
        out->x.ax = (pos & 0xFFFF);
        clearCarry(out);
    } else {
        out->x.ax = 1; // Seek failed
        setCarry(out);
    }
}

void Kernel::getFileAttributes(union REGS *in, union REGS *out) {
    // 0=Get, 1=Set
    // Simplification: We only support Get
    if (in->h.al == 0) {
        // LittleFS doesn't store attributes really. Fake it.
        // Check if directory
        String path = getPathFromRegs(in, nullptr);
        if (path == "/") {
            out->x.cx = _A_SUBDIR;
            clearCarry(out);
            return;
        }

        File f = LittleFS.open(path);
        if (!f) {
            out->x.ax = 2;
            setCarry(out);
            return;
        }

        if (f.isDirectory()) {
            out->x.cx = _A_SUBDIR;
        } else {
            out->x.cx = _A_NORMAL;
        }
        f.close();
        clearCarry(out);
    } else {
        // Set not supported yet
        clearCarry(out);
    }
}

void Kernel::getVersion(union REGS *in, union REGS *out) {
    out->h.al = 2;  // Major
    out->h.ah = 0;  // Minor
}

void Kernel::diskReset(union REGS *in, union REGS *out) {
    // Flush all files
    for (auto &h : fileHandles) {
        if (h.is_open && h.file) {
            h.file.flush();
        }
    }
    clearCarry(out);
}

void Kernel::selectDisk(union REGS *in, union REGS *out) {
    currentDrive = in->h.dl;
    out->h.al = 5; // Report 5 drives (A-E)
}

void Kernel::getDiskFreeSpace(union REGS *in, union REGS *out) {
    // DL = drive
    // Returns: AX=sectors/cluster, BX=avail clusters, CX=bytes/sector, DX=total clusters
    // Fake values for compatibility

    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    size_t free = total - used;

    out->x.cx = 512; // Bytes per sector
    out->x.ax = 8;   // Sectors per cluster (4KB cluster)

    uint32_t clusterSize = 512 * 8;
    out->x.dx = total / clusterSize;
    out->x.bx = free / clusterSize;

    if (out->x.dx > 0xFFFF) out->x.dx = 0xFFFF; // Cap at 16-bit limits for old apps
    if (out->x.bx > 0xFFFF) out->x.bx = 0xFFFF;
}

void Kernel::createDirectory(union REGS *in, union REGS *out) {
    String path = getPathFromRegs(in, nullptr);
    if (LittleFS.mkdir(path)) {
        clearCarry(out);
    } else {
        out->x.ax = 3; // Path not found / access denied
        setCarry(out);
    }
}

void Kernel::removeDirectory(union REGS *in, union REGS *out) {
    String path = getPathFromRegs(in, nullptr);
    if (LittleFS.rmdir(path)) {
        clearCarry(out);
    } else {
        out->x.ax = 16; // Current directory / in use / not found
        setCarry(out);
    }
}

void Kernel::changeDirectory(union REGS *in, union REGS *out) {
    String path = getPathFromRegs(in, nullptr);
    File f = LittleFS.open(path);
    if (f && f.isDirectory()) {
        currentDir = path;
        f.close();
        clearCarry(out);
    } else {
        if (f) f.close();
        out->x.ax = 3;
        setCarry(out);
    }
}

void Kernel::setDTA(union REGS *in, union REGS *out) {
    currentDTA = (DTA*)(uintptr_t)in->x.dx;
}

void Kernel::findFirst(union REGS *in, union REGS *out) {
    // DS:DX = ASCIZ spec
    // CX = Attributes
    String spec = getPathFromRegs(in, nullptr);

    // Split into dir and pattern
    int lastSlash = spec.lastIndexOf('/');
    if (lastSlash == -1) {
        dirEnumPath = currentDir;
        dirEnumPattern = spec;
    } else {
        dirEnumPath = spec.substring(0, lastSlash + 1);
        dirEnumPattern = spec.substring(lastSlash + 1);
        if (dirEnumPath.length() == 0) dirEnumPath = "/";
    }

    // Open directory
    if (dirEnumFile) dirEnumFile.close();
    dirEnumFile = LittleFS.open(dirEnumPath);

    if (!dirEnumFile || !dirEnumFile.isDirectory()) {
        out->x.ax = 3; // Path not found
        setCarry(out);
        return;
    }

    // Start search (FindNext logic)
    findNext(in, out);
}

void Kernel::findNext(union REGS *in, union REGS *out) {
    // Uses state in dirEnumFile
    if (!dirEnumFile) {
        out->x.ax = 18; // No more files
        setCarry(out);
        return;
    }

    File entry = dirEnumFile.openNextFile();
    while (entry) {
        String name = entry.name();
        // Check match (Very simple globbing: *.*, ?, or exact)
        // For now, assume *.* or exact match for simplicity
        // TODO: Implement real wildcard matching

        bool match = false;
        if (dirEnumPattern == "*.*" || dirEnumPattern == "*") {
            match = true;
        } else if (name.equalsIgnoreCase(dirEnumPattern)) {
            match = true;
        }

        if (match) {
            // Fill DTA
            if (currentDTA) {
                // Populate DTA
                memset(currentDTA, 0, sizeof(DTA));
                currentDTA->attrib = entry.isDirectory() ? _A_SUBDIR : _A_NORMAL;

                // Time/Date (Fake for now, or get from FS if available)
                currentDTA->time = 0;
                currentDTA->date = 0;
                currentDTA->size = entry.size();

                // Name (8.3 format typically required, but we might truncate)
                strncpy(currentDTA->name, name.c_str(), 12);
                currentDTA->name[12] = 0;
            }
            entry.close();
            clearCarry(out);
            return;
        }

        entry.close();
        entry = dirEnumFile.openNextFile();
    }

    // End of list
    out->x.ax = 18;
    setCarry(out);
}
