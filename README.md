# ESP-DOS (v2.0 Port for ESP32)

This project is a functional port and adaptation of MS-DOS v2.0 concepts to run natively on the ESP32-S3 (and likely other ESP32 variants).

It provides a **DOS-compatible API layer** (`dos.h`, `int86`) that allows existing DOS C source code to be recompiled and run on the ESP32 with minimal changes.

## Features

*   **Command Shell (COMMAND.COM)**: Supports internal commands like `DIR`, `TYPE`, `CLS`, `VER`, `MD`, `RD`, `CD`, `DEL`.
*   **Kernel Services (INT 21h)**: Implements core DOS system calls mapped to ESP32 native functions.
    *   File System: Mapped to LittleFS (large partition support).
    *   Console I/O: Mapped to Serial (UART).
*   **Compatibility Layer**: `dos.h` provides `union REGS`, `struct SREGS`, `int86()`, `intdos()`, allowing source-level compatibility for legacy DOS apps.

## How to Build

### Arduino IDE
1.  Open `EspDos/EspDos.ino` in the Arduino IDE.
2.  Select your board: **ESP32S3 Dev Module**.
3.  Configure Board Settings:
    *   **Flash Size**: 16MB (128Mb)
    *   **PSRAM**: OPI PSRAM
    *   **Partition Scheme**: Default 16MB (or Large SPIFFS/LittleFS)
    *   **USB Mode**: Hardware CDC and JTAG (for Serial)
4.  Compile and Upload.

### PlatformIO
1.  Open the project in **PlatformIO** (VSCode).
2.  The `platformio.ini` is configured for the `esp32-s3-devkitc-1`.
3.  Build and Upload:
    ```bash
    pio run --target upload
    ```
4.  Monitor Serial Output (115200 baud):
    ```bash
    pio device monitor
    ```

## Adding Your Own DOS Programs

To run "recompiled" DOS source code:

1.  Copy your C source file (e.g., `MYAPP.C`) into the `EspDos/` folder.
2.  Ensure it includes `#include "dos.h"` (provided by this project).
3.  Rename your `main` function to something unique, e.g., `int myapp_main(int argc, char* argv[])`.
4.  Register your program in `EspDos/EspDos.ino`:
    ```cpp
    extern int myapp_main(int argc, char* argv[]);

    void setup() {
        // ...
        Command::registerCommand("MYAPP", myapp_main);
        // ...
    }
    ```
5.  Recompile and upload. You can now type `MYAPP` at the ESP-DOS prompt.

## Example

An example program `HELLO.EXE` is included (`EspDos/hello.cpp`). It demonstrates:
*   Printing strings using `INT 21h, AH=09h`.
*   Creating and writing to a file using `INT 21h` file services.

Type `HELLO` at the prompt to run it.

## Technical Details

*   **Filesystem**: The ESP32's LittleFS is used as the disk. Directories and files behave like DOS.
*   **Memory**: The DOS "segments" are ignored. Pointers are passed directly in registers (e.g., `DX` holds the pointer value).
*   **API**: Only a subset of INT 21h is implemented, focusing on File I/O and Console I/O.
