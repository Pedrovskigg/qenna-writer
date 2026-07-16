#pragma once
#include <QString>

class CrashLogger {
public:
    static void install();
    static void log(const char* action);
    static void log(const QString& action);
    static QString logFilePath();
};
