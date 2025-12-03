#ifndef DOS_H
#define DOS_H

#include <stdint.h>
#include "KERNEL.h"

// Define standard DOS types
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

// Define register structures matching typical DOS compiler headers
#pragma pack(push, 1)

// Extended for 32-bit architecture to hold pointers
struct WORDREGS {
    unsigned int ax, bx, cx, dx, si, di, cflag, flags;
};

// Little Endian Layout for 32-bit AX (AL=byte 0, AH=byte 1)
struct BYTEREGS {
    unsigned char al, ah; unsigned short ax_pad;
    unsigned char bl, bh; unsigned short bx_pad;
    unsigned char cl, ch; unsigned short cx_pad;
    unsigned char dl, dh; unsigned short dx_pad;
};

union REGS {
    struct WORDREGS x;
    struct BYTEREGS h;
};

struct SREGS {
    unsigned short es, cs, ss, ds;
};

#pragma pack(pop)

// Emulate int86 and intdos functions
int int86(int intr_num, union REGS *inregs, union REGS *outregs);
int intdos(union REGS *inregs, union REGS *outregs);
int intdosx(union REGS *inregs, union REGS *outregs, struct SREGS *segregs);

// Structure for findfirst/findnext (DOS 2.0 DTA format)
#pragma pack(push, 1)
struct find_t {
    char reserved[21];
    char attrib;
    unsigned short wr_time;
    unsigned short wr_date;
    long size;
    char name[13];
};
#pragma pack(pop)

// File attributes
#define _A_NORMAL 0x00
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _A_SYSTEM 0x04
#define _A_VOLID  0x08
#define _A_SUBDIR 0x10
#define _A_ARCH   0x20

#endif
