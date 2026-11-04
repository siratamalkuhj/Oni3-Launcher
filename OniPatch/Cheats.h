#pragma once

#include "pch.h"
#include "Trace.h"
#include <string>

// In-game cheats for Onimusha 3 PC (oni3.exe 1.1.0.0, image base 0x400000).
// Addresses come from a Cheat Engine table and were checked against the executable.
enum class Cheat : int
{
    Invincible,     // F1, LT + D-pad up
    InfiniteOni,    // F2, LT + D-pad right: oni power and souls never decrease
    InfiniteItems,  // F3, LT + D-pad down
    OneHitKill,     // F4, LT + D-pad left
    AlwaysIssen,    // F5, LT + Y: attacks near an enemy become a real (homing) issen,
                    //     gun shots become gun criticals
    AutoChain,      // F6, LT + B: issen chains continue without pressing attack
    MoveSpeed,      // F7, LT + X: cycles off / x2 / x3 / x4 horizontal player movement
    AutoGuard,      // F8, LT + A: hits are deflected (just guard) without pressing guard
    Count
};

constexpr int CHEAT_COUNT = static_cast<int>(Cheat::Count);

class Cheats
{
public:
    static Cheats &Get();

    // Redirects the game's damage and oni power code to the cheat stubs. Call once at load.
    void InstallCodePatches();
    // Once per frame: hotkeys and memory cheats.
    void Tick();
    // Handles the LT combos and hides them from the game.
    XINPUT_GAMEPAD FilterPad(const XINPUT_GAMEPAD &pad);

    bool IsEnabled(Cheat cheat) const;
    // Label of an enabled cheat for the overlay, e.g. "[F1] Invincible" in Japanese.
    std::wstring Label(Cheat cheat) const;
    // Changes each time a cheat is toggled, so the overlay knows when to redraw.
    DWORD Version() const { return version; }

private:
    struct Watch
    {
        DWORD address;
        int size;      // 1, 2 or 4 bytes
        DWORD last;
        bool valid;
    };

    Cheats();
    void Toggle(Cheat cheat);
    void PollKeyboard();
    void KeepFromDecreasing(std::vector<Watch> &watches, size_t loadThreshold);

    bool enabled[CHEAT_COUNT];
    bool keyDown[CHEAT_COUNT];
    WORD lastPadButtons;
    int moveMultiplier;  // 1 when the move speed cheat is off
    DWORD version;
    std::vector<Watch> soulWatches;
    std::vector<Watch> itemWatches;
};
