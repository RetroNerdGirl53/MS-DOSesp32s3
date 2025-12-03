#include <Arduino.h>
#include "DOS.h"
#include "KERNEL.h"
#include "USERIO.h"

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
