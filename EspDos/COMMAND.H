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
    static void cmdDel(String args);

    // New Commands
    static void cmdCopy(String args);
    static void cmdRen(String args);
    static void cmdVol(String args);
    static void cmdDate(String args);
    static void cmdTime(String args);
    static void cmdEcho(String args);
    static void cmdPath(String args);
    static void cmdPrompt(String args);
    static void cmdSet(String args);
    static void cmdVerify(String args);
    static void cmdPause(String args);
    static void cmdShift(String args);
    static void cmdGoto(String args);
    static void cmdIf(String args);
    static void cmdFor(String args);

    static String readLine();
    static String readBatchLine();
    static void parseCommand(String line, String &cmd, String &args);

    // Batch State
    static bool batchActive;
    static String batchFile;
    static int batchLine;
    static bool echoOn;
    static std::vector<String> batchParams;

    // Environment
    static std::vector<std::pair<String, String>> environment;
    static String getEnv(String key);
    static void setEnv(String key, String val);
};

#endif
