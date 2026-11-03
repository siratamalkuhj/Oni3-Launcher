#pragma once

// Pure translation logic between an XInput gamepad state and DirectInput
// action-mapped buffered data. No COM and no XInput calls here so it can be
// unit tested without a controller (see OniPatchTests).

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <Xinput.h>
#include <deque>
#include <vector>

// Physical controls exposed by the virtual pad.
enum class PadObject : int
{
    LeftX, LeftY, RightX, RightY,   // axes, DIDFT instances 0..3
    Pov,                            // POV, DIDFT instance 0
    A, B, X, Y,                     // buttons, DIDFT instances 0..11
    LB, RB, Back, Start,
    LS, RS, LT, RT,
    Count,
    None = -1
};

constexpr int PAD_AXIS_COUNT = 4;
constexpr int PAD_BUTTON_COUNT = 12;
constexpr int PAD_OBJECT_COUNT = static_cast<int>(PadObject::Count);

// DirectInput object id (DIDFT_* | instance) <-> PadObject
DWORD PadObjectToObjId(PadObject obj);
PadObject PadObjectFromObjId(DWORD objId);
bool PadObjectIsAxis(PadObject obj);

// Onimusha 3 action ids (DIACTION::uAppData), identical in every language table.
namespace OniAction
{
    constexpr UINT_PTR StrategyShell = 0x01;  // senjutsu-kaku
    constexpr UINT_PTR RunForward = 0x05;
    constexpr UINT_PTR BackStep = 0x06;
    constexpr UINT_PTR AbsorbOrCancel = 0x0B; // PS2 circle position (label depends on language)
    constexpr UINT_PTR Attack = 0x0C;
    constexpr UINT_PTR ExamineOrDecide = 0x0D; // PS2 cross position (label depends on language)
    constexpr UINT_PTR Guard = 0x0E;
    constexpr UINT_PTR Map = 0x0F;
    constexpr UINT_PTR Stance = 0x10;
    constexpr UINT_PTR TurnAround = 0x11;
    constexpr UINT_PTR Onimusha = 0x12;
    constexpr UINT_PTR StatusMenu = 0x13;
    constexpr UINT_PTR Pause = 0x14;
}

// Picks the pad control for one action, or PadObject::None.
// Axes and the POV are chosen by semantic, buttons by the game's action id.
PadObject DefaultPadObjectFor(DWORD semantic, UINT_PTR appData);

struct PadBinding
{
    PadObject object;
    DWORD actionIndex;   // index in DIACTIONFORMAT::rgoAction, reported as dwOfs
    UINT_PTR appData;
};

class XInputPadState
{
public:
    XInputPadState();

    void SetBindings(const std::vector<PadBinding> &bindings);
    const std::vector<PadBinding> &GetBindings() const { return bindings; }
    void SetAxisRange(LONG min, LONG max);
    // DirectInput dead zone, 0..10000. The XInput recommended dead zone is used when larger.
    void SetDeadZone(DWORD deadZone);
    // Forget the last reported values so the next Update reports everything that is not neutral.
    void Reset();

    // Appends one DIDEVICEOBJECTDATA per binding whose value changed.
    void Update(const XINPUT_GAMEPAD *pad, DWORD timeStamp, std::deque<DIDEVICEOBJECTDATA> &out);

    // Value DirectInput would report for a control (axes in range, buttons 0/0x80, POV centidegrees or -1).
    LONG ValueOf(const XINPUT_GAMEPAD *pad, PadObject obj) const;

private:
    LONG ScaleAxis(double normalized) const;
    void StickValues(SHORT x, SHORT y, SHORT xinputDeadZone, LONG &outX, LONG &outY) const;

    std::vector<PadBinding> bindings;
    LONG lastValue[PAD_OBJECT_COUNT];
    LONG axisMin;
    LONG axisMax;
    DWORD deadZone;
    DWORD sequence;
};
