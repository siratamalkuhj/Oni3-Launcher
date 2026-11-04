#pragma once

#include "pch.h"

// Writes access violations and other fatal exceptions to %LOCALAPPDATA%\OniLauncher\crash.log.
// oni3.exe installs its own unhandled exception filter and exits silently, so Windows keeps no
// record of its crashes. The handler only logs: the exception goes on to the game as before.
class CrashLog
{
public:
    static void Install();
};
