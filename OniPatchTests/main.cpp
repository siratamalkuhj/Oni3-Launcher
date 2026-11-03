// OniPatchTests
//   OniPatchTests.exe        unit tests for the XInput -> DirectInput translation (no controller needed)
//   OniPatchTests.exe live   loads OniPatch.dll and drives DirectInput like Onimusha 3 does,
//                            printing the devices and the buffered data for 15 seconds

#include "XInputPad.h"

#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "dxguid.lib")

namespace
{
    int failures = 0;

    void Check(bool condition, const char *what, int line)
    {
        if (!condition) {
            failures++;
            std::printf("FAIL (line %d): %s\n", line, what);
        }
    }

#define CHECK(c) Check((c), #c, __LINE__)

    // Gamepad part of the Onimusha 3 action table (semantics and uAppData as in oni3.exe).
    DIACTIONA GameActions[] = {
        { 0x07, DIAXIS_ANY_Y_1, 0, { "Forward movement" } },
        { 0x08, DIAXIS_ANY_X_1, 0, { "Step left/right" } },
        { 0x09, DIAXIS_ANY_3, 0, { "Stance X" } },
        { 0x0A, DIAXIS_ANY_4, 0, { "Stance Y" } },
        { 0x0D, DIBUTTON_ANY(0), 0, { "Examine/Decide" } },
        { 0x0B, DIBUTTON_ANY(1), 0, { "Absorb/Cancel" } },
        { 0x0C, DIBUTTON_ANY(2), 0, { "Attack" } },
        { 0x01, DIBUTTON_ANY(3), 0, { "Strategy shell" } },
        { 0x0E, DIBUTTON_ANY(4), 0, { "Guard" } },
        { 0x10, DIBUTTON_ANY(5), 0, { "Stance" } },
        { 0x0F, DIBUTTON_ANY(6), 0, { "Map" } },
        { 0x11, DIBUTTON_ANY(7), 0, { "Turn around" } },
        { 0x14, DIBUTTON_ANY(8), 0, { "Pause" } },
        { 0x13, DIBUTTON_ANY(9), 0, { "Status menu" } },
        { 0x05, DIBUTTON_ANY(10), 0, { "Run forward" } },
        { 0x06, DIBUTTON_ANY(11), 0, { "Back step" } },
        { 0x12, DIBUTTON_ANY(12), 0, { "Onimusha" } },
        { 0x03, DIBUTTON_ANY(13), 0, { "Rotate left" } },
        { 0x04, DIBUTTON_ANY(14), 0, { "Rotate right" } },
        { 0x0C, 0x81000424, 0, { "Attack (keyboard)" } },
        { 0x16, DIPOV_ANY_1, 0, { "Direction key" } },
    };
    const DWORD GAME_ACTION_COUNT = sizeof(GameActions) / sizeof(GameActions[0]);

    std::vector<PadBinding> DefaultBindings()
    {
        std::vector<PadBinding> bindings;
        for (DWORD i = 0; i < GAME_ACTION_COUNT; i++) {
            PadObject obj = DefaultPadObjectFor(GameActions[i].dwSemantic, GameActions[i].uAppData);
            if (obj != PadObject::None)
                bindings.push_back({ obj, i, GameActions[i].uAppData });
        }
        return bindings;
    }

    const DIDEVICEOBJECTDATA *Find(const std::deque<DIDEVICEOBJECTDATA> &events, UINT_PTR appData)
    {
        for (const DIDEVICEOBJECTDATA &e : events)
            if (e.uAppData == appData)
                return &e;
        return nullptr;
    }

    void TestObjectIds()
    {
        for (int i = 0; i < PAD_OBJECT_COUNT; i++) {
            PadObject obj = static_cast<PadObject>(i);
            CHECK(PadObjectFromObjId(PadObjectToObjId(obj)) == obj);
        }
        CHECK(PadObjectFromObjId(DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(12)) == PadObject::None);
    }

    void TestDefaultMapping()
    {
        CHECK(DefaultPadObjectFor(DIAXIS_ANY_Y_1, 0x07) == PadObject::LeftY);
        CHECK(DefaultPadObjectFor(DIAXIS_ANY_X_1, 0x08) == PadObject::LeftX);
        CHECK(DefaultPadObjectFor(DIAXIS_ANY_3, 0x09) == PadObject::RightX);
        CHECK(DefaultPadObjectFor(DIAXIS_ANY_4, 0x0A) == PadObject::RightY);
        CHECK(DefaultPadObjectFor(DIPOV_ANY_1, 0x16) == PadObject::Pov);
        CHECK(DefaultPadObjectFor(DIBUTTON_ANY(2), OniAction::Attack) == PadObject::X);
        CHECK(DefaultPadObjectFor(DIBUTTON_ANY(12), OniAction::Onimusha) == PadObject::LT);
        CHECK(DefaultPadObjectFor(DIBUTTON_ANY(4), OniAction::Guard) == PadObject::LB);
        CHECK(DefaultPadObjectFor(DIBUTTON_ANY(5), OniAction::Stance) == PadObject::RB);
        CHECK(DefaultPadObjectFor(DIBUTTON_ANY(8), OniAction::Pause) == PadObject::Back);
        CHECK(DefaultPadObjectFor(DIBUTTON_ANY(9), OniAction::StatusMenu) == PadObject::Start);
        CHECK(DefaultPadObjectFor(DIBUTTON_ANY(7), OniAction::TurnAround) == PadObject::RT);
        // Keyboard semantics are never taken by the pad.
        CHECK(DefaultPadObjectFor(0x81000424, OniAction::Attack) == PadObject::None);
        // Actions without a default control.
        CHECK(DefaultPadObjectFor(DIBUTTON_ANY(10), OniAction::RunForward) == PadObject::None);

        // No control is bound twice.
        std::vector<PadBinding> bindings = DefaultBindings();
        for (size_t i = 0; i < bindings.size(); i++)
            for (size_t j = i + 1; j < bindings.size(); j++)
                CHECK(bindings[i].object != bindings[j].object);
        CHECK(bindings.size() == 16);
    }

    void TestNeutralProducesNothing()
    {
        XInputPadState pad;
        std::deque<DIDEVICEOBJECTDATA> events;
        XINPUT_GAMEPAD neutral = {};

        pad.SetBindings(DefaultBindings());
        pad.SetAxisRange(-100, 100);
        pad.Update(&neutral, 1, events);
        CHECK(events.empty());
        pad.Update(nullptr, 2, events);
        CHECK(events.empty());
    }

    void TestButtonsAndTriggers()
    {
        XInputPadState pad;
        std::deque<DIDEVICEOBJECTDATA> events;
        XINPUT_GAMEPAD g = {};

        pad.SetBindings(DefaultBindings());
        pad.SetAxisRange(-100, 100);

        g.wButtons = XINPUT_GAMEPAD_X;
        g.bLeftTrigger = 255;
        g.bRightTrigger = XINPUT_GAMEPAD_TRIGGER_THRESHOLD; // not past the threshold
        pad.Update(&g, 10, events);
        CHECK(events.size() == 2);
        const DIDEVICEOBJECTDATA *attack = Find(events, OniAction::Attack);
        CHECK(attack != nullptr && attack->dwData == 0x80 && attack->dwOfs == 6 && attack->dwTimeStamp == 10);
        const DIDEVICEOBJECTDATA *oni = Find(events, OniAction::Onimusha);
        CHECK(oni != nullptr && oni->dwData == 0x80);
        CHECK(events[0].dwSequence < events[1].dwSequence);

        // Holding produces nothing new.
        events.clear();
        pad.Update(&g, 11, events);
        CHECK(events.empty());

        // Release X, press RT.
        g.wButtons = 0;
        g.bRightTrigger = 200;
        pad.Update(&g, 12, events);
        CHECK(events.size() == 2);
        attack = Find(events, OniAction::Attack);
        CHECK(attack != nullptr && attack->dwData == 0);
        const DIDEVICEOBJECTDATA *turn = Find(events, OniAction::TurnAround);
        CHECK(turn != nullptr && turn->dwData == 0x80);

        // Disconnect releases everything held.
        events.clear();
        pad.Update(nullptr, 13, events);
        CHECK(events.size() == 2);
        for (const DIDEVICEOBJECTDATA &e : events)
            CHECK(e.dwData == 0);
    }

    void TestSticks()
    {
        XInputPadState pad;
        std::deque<DIDEVICEOBJECTDATA> events;
        XINPUT_GAMEPAD g = {};

        pad.SetBindings(DefaultBindings());
        pad.SetAxisRange(-100, 100);
        pad.SetDeadZone(500);

        // Inside the XInput dead zone: nothing.
        g.sThumbLX = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE - 1;
        pad.Update(&g, 1, events);
        CHECK(events.empty());

        // Full up on the left stick is the minimum on the forward axis.
        g.sThumbLX = 0;
        g.sThumbLY = 32767;
        pad.Update(&g, 2, events);
        CHECK(events.size() == 1);
        const DIDEVICEOBJECTDATA *forward = Find(events, 0x07);
        CHECK(forward != nullptr && static_cast<LONG>(forward->dwData) == -100);

        // Full right on the right stick.
        events.clear();
        g.sThumbLY = 0;
        g.sThumbRX = 32767;
        pad.Update(&g, 3, events);
        CHECK(events.size() == 2);
        forward = Find(events, 0x07);
        CHECK(forward != nullptr && static_cast<LONG>(forward->dwData) == 0);
        const DIDEVICEOBJECTDATA *stanceX = Find(events, 0x09);
        CHECK(stanceX != nullptr && static_cast<LONG>(stanceX->dwData) == 100);

        // Full down-left on the right stick stays in range.
        events.clear();
        g.sThumbRX = -32768;
        g.sThumbRY = -32768;
        pad.Update(&g, 4, events);
        stanceX = Find(events, 0x09);
        const DIDEVICEOBJECTDATA *stanceY = Find(events, 0x0A);
        CHECK(stanceX != nullptr && static_cast<LONG>(stanceX->dwData) < -60 && static_cast<LONG>(stanceX->dwData) >= -100);
        CHECK(stanceY != nullptr && static_cast<LONG>(stanceY->dwData) > 60 && static_cast<LONG>(stanceY->dwData) <= 100);

        // Default DirectInput range when the game sets none.
        XInputPadState raw;
        XINPUT_GAMEPAD r = {};
        r.sThumbLX = 32767;
        CHECK(raw.ValueOf(&r, PadObject::LeftX) == 0xFFFF);
        CHECK(raw.ValueOf(nullptr, PadObject::LeftX) == 0x8000);
    }

    void TestPov()
    {
        XInputPadState pad;
        XINPUT_GAMEPAD g = {};

        CHECK(pad.ValueOf(&g, PadObject::Pov) == -1);
        g.wButtons = XINPUT_GAMEPAD_DPAD_UP;
        CHECK(pad.ValueOf(&g, PadObject::Pov) == 0);
        g.wButtons = XINPUT_GAMEPAD_DPAD_RIGHT;
        CHECK(pad.ValueOf(&g, PadObject::Pov) == 9000);
        g.wButtons = XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT;
        CHECK(pad.ValueOf(&g, PadObject::Pov) == 22500);
        g.wButtons = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_LEFT;
        CHECK(pad.ValueOf(&g, PadObject::Pov) == 31500);
        g.wButtons = XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
        CHECK(pad.ValueOf(&g, PadObject::Pov) == -1);
    }

    int RunUnitTests()
    {
        TestObjectIds();
        TestDefaultMapping();
        TestNeutralProducesNothing();
        TestButtonsAndTriggers();
        TestSticks();
        TestPov();

        if (failures == 0)
            std::printf("All unit tests passed.\n");
        else
            std::printf("%d check(s) failed.\n", failures);
        return failures == 0 ? 0 : 1;
    }

    // ---- live mode ----

    struct LiveDevice
    {
        LPDIRECTINPUTDEVICE8A device;
        DIDEVICEINSTANCEA info;
    };

    std::vector<LiveDevice> liveDevices;

    BOOL CALLBACK OnDevice(LPCDIDEVICEINSTANCEA lpddi, LPDIRECTINPUTDEVICE8A lpdid, DWORD dwFlags, DWORD dwRemaining, LPVOID pvRef)
    {
        std::printf("  device: %s (type 0x%08lX)\n", lpddi->tszInstanceName, lpddi->dwDevType);
        if ((lpddi->dwDevType & 0xFF) == DI8DEVTYPE_KEYBOARD || (lpddi->dwDevType & 0xFF) == DI8DEVTYPE_MOUSE)
            return DIENUM_CONTINUE;
        lpdid->AddRef();
        liveDevices.push_back({ lpdid, *lpddi });
        return DIENUM_CONTINUE;
    }

    int RunLive()
    {
        HMODULE patch = LoadLibraryA("OniPatch.dll");
        if (patch == NULL) {
            std::printf("OniPatch.dll not found next to the test executable.\n");
            return 1;
        }

        HMODULE dinput = LoadLibraryA("dinput8.dll");
        typedef HRESULT(WINAPI *Create)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
        Create create = reinterpret_cast<Create>(GetProcAddress(dinput, "DirectInput8Create"));
        IDirectInput8A *di = nullptr;
        if (create == nullptr || FAILED(create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8A, reinterpret_cast<LPVOID *>(&di), NULL))) {
            std::printf("DirectInput8Create failed.\n");
            return 1;
        }

        // Same header values as oni3.exe (0x406C60).
        DIACTIONFORMATA format = {};
        format.dwSize = sizeof(format);
        format.dwActionSize = sizeof(DIACTIONA);
        format.dwDataSize = GAME_ACTION_COUNT * sizeof(DWORD);
        format.dwNumActions = GAME_ACTION_COUNT;
        format.rgoAction = GameActions;
        format.guidActionMap = { 0x4F4E4933, 0x0001, 0x0002, { 0, 0, 0, 0, 0, 0, 0, 1 } };
        format.dwGenre = 0x0A000000;
        format.dwBufferSize = 16;
        format.lAxisMin = -100;
        format.lAxisMax = 100;
        strcpy_s(format.tszActionMap, "OniPatchTests");

        std::printf("Enumerating devices by semantics:\n");
        di->EnumDevicesBySemantics(NULL, &format, OnDevice, NULL, 0);
        if (liveDevices.empty()) {
            std::printf("No gamepad found.\n");
            return 1;
        }

        for (LiveDevice &d : liveDevices) {
            DIPROPDWORD dz = { { sizeof(DIPROPDWORD), sizeof(DIPROPHEADER), 0, DIPH_DEVICE }, 500 };
            d.device->SetCooperativeLevel(GetConsoleWindow(), DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
            HRESULT build = d.device->BuildActionMap(&format, NULL, 0);
            HRESULT set = d.device->SetActionMap(&format, NULL, 0);
            d.device->SetProperty(DIPROP_DEADZONE, &dz.diph);
            std::printf("%s: BuildActionMap 0x%08lX, SetActionMap 0x%08lX\n", d.info.tszInstanceName, build, set);
        }

        LiveDevice &first = liveDevices.front();

        // oni3.exe calls GetDeviceInfo with an uninitialized dwSize: it must fail without writing.
        DIDEVICEINSTANCEA info;
        std::memset(&info, 0xAB, sizeof(info));
        info.dwSize = 0xCCCCCCCC;
        HRESULT badSize = first.device->GetDeviceInfo(&info);
        bool untouched = info.dwDevType == 0xABABABAB;
        std::printf("GetDeviceInfo with a bad dwSize: 0x%08lX (%s)\n", badSize,
            badSize == DIERR_INVALIDPARAM && untouched ? "ok" : "FAIL");
        if (badSize != DIERR_INVALIDPARAM || !untouched)
            failures++;

        std::printf("Reading the first gamepad for 15 seconds, press buttons and move the sticks...\n");
        DWORD end = GetTickCount() + 15000;
        while (GetTickCount() < end) {
            DIDEVICEOBJECTDATA data[16];
            DWORD count = 16;

            first.device->Acquire();
            first.device->Poll();
            if (SUCCEEDED(first.device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), data, &count, 0))) {
                for (DWORD i = 0; i < count; i++) {
                    const char *name = data[i].dwOfs < GAME_ACTION_COUNT ? GameActions[data[i].dwOfs].lptszActionName : "?";
                    std::printf("  [%lu] %-18s app=0x%02X value=%ld\n", data[i].dwSequence, name,
                        static_cast<unsigned>(data[i].uAppData), static_cast<LONG>(data[i].dwData));
                }
            }
            Sleep(16);
        }

        for (LiveDevice &d : liveDevices) {
            d.device->Unacquire();
            d.device->Release();
        }
        di->Release();
        return failures == 0 ? 0 : 1;
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && std::string(argv[1]) == "live")
        return RunLive();
    return RunUnitTests();
}
