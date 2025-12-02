#ifndef COMMAND_H
#define COMMAND_H

#include <Arduino.h>
#include <vector>

// Type definition for external commands
typedef int (*CmdFunc)(int argc, char* argv[]);

struct ExternalCommand {
    String name;
    CmdFunc func;
};

class Command {
public:
    static void begin();
    static void loop();
    static void registerCommand(String name, CmdFunc func);

private:
    static void printPrompt();
    static void processLine(String line);

    static std::vector<ExternalCommand> externalCommands;

    // Internal Commands
    static void cmdDir(String args);
    static void cmdType(String args);
    static void cmdCls();
    static void cmdVer();
    static void cmdMkdir(String args);
    static void cmdRmdir(String args);
    static void cmdChdir(String args);
    static void cmdCopy(String args);
    static void cmdDel(String args);

    static String readLine();
    static void parseCommand(String line, String &cmd, String &args);
};

#endif
