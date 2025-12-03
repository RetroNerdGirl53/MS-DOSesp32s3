#include <Arduino.h>
#include "KERNEL.H"
#include "COMMAND.H"

// Declaration of external program entry point
extern int hello_main(int argc, char* argv[]);

// Forward declarations for new external commands (to be implemented)
extern int chkdsk_main(int argc, char* argv[]);
extern int format_main(int argc, char* argv[]);
extern int more_main(int argc, char* argv[]);
extern int find_main(int argc, char* argv[]);
extern int sort_main(int argc, char* argv[]);
extern int sys_main(int argc, char* argv[]);
extern int debug_main(int argc, char* argv[]);
extern int edlin_main(int argc, char* argv[]);

TaskHandle_t dosTask;

void dosLoop(void * parameter) {
    // Main execution loop for the DOS shell on Core 0
    while (true) {
        Command::loop();
        delay(10); // Small yield to keep watchdog happy if needed, though DOS usually blocks
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial); // Wait for serial connection

    Serial.println("\nStarting ESP-DOS...");

    if (!Kernel::begin()) {
        Serial.println("Kernel initialization failed!");
        while (1);
    }

    // Register user applications
    Command::registerCommand("HELLO", hello_main);
    Command::registerCommand("HELLO.EXE", hello_main);

    // Register new commands
    Command::registerCommand("CHKDSK", chkdsk_main);
    Command::registerCommand("CHKDSK.COM", chkdsk_main);
    Command::registerCommand("FORMAT", format_main);
    Command::registerCommand("FORMAT.COM", format_main);
    Command::registerCommand("MORE", more_main);
    Command::registerCommand("MORE.COM", more_main);
    Command::registerCommand("FIND", find_main);
    Command::registerCommand("FIND.EXE", find_main);
    Command::registerCommand("SORT", sort_main);
    Command::registerCommand("SORT.EXE", sort_main);
    Command::registerCommand("SYS", sys_main);
    Command::registerCommand("SYS.COM", sys_main);
    Command::registerCommand("DEBUG", debug_main);
    Command::registerCommand("DEBUG.COM", debug_main);
    Command::registerCommand("EDLIN", edlin_main);
    Command::registerCommand("EDLIN.COM", edlin_main);

    Command::begin();

    // Create task on Core 0
    xTaskCreatePinnedToCore(
        dosLoop,    // Function
        "DOS",      // Name
        16384,      // Stack size (increased for safety)
        NULL,       // Parameter
        1,          // Priority
        &dosTask,   // Handle
        0           // Core ID (0)
    );
}

void loop() {
    // Main loop does nothing now, everything is in dosLoop on Core 0
    delay(1000);
}
