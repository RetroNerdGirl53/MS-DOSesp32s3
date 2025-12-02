#include <Arduino.h>
#include "kernel.h"
#include "command.h"

// Declaration of external program entry point
extern int hello_main(int argc, char* argv[]);

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

    Command::begin();
}

void loop() {
    Command::loop();
}
