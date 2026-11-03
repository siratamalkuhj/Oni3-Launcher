// No precompiled header: this file is shared with OniPatchTests.
#include "XInputPad.h"

#include <cmath>

namespace
{
    const DWORD BUTTON_PRESSED = 0x80;
    const DWORD POV_CENTERED = 0xFFFFFFFF;

    const WORD BUTTON_MASKS[] = {
        XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
        XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
        XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
    };

    int ButtonInstance(PadObject obj)
    {
        return static_cast<int>(obj) - static_cast<int>(PadObject::A);
    }

    LONG PovFromDpad(WORD buttons)
    {
        int dx = ((buttons & XINPUT_GAMEPAD_DPAD_RIGHT) ? 1 : 0) - ((buttons & XINPUT_GAMEPAD_DPAD_LEFT) ? 1 : 0);
        int dy = ((buttons & XINPUT_GAMEPAD_DPAD_UP) ? 1 : 0) - ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) ? 1 : 0);

        // Clockwise from north, in hundredths of a degree.
        static const LONG table[3][3] = {
            // dx = -1, 0, 1
            { 22500, 18000, 13500 }, // dy = -1
            { 27000, -1, 9000 },     // dy = 0
            { 31500, 0, 4500 },      // dy = 1
        };
        return table[dy + 1][dx + 1];
    }
}

DWORD PadObjectToObjId(PadObject obj)
{
    if (PadObjectIsAxis(obj))
        return DIDFT_ABSAXIS | DIDFT_MAKEINSTANCE(static_cast<int>(obj));
    if (obj == PadObject::Pov)
        return DIDFT_POV | DIDFT_MAKEINSTANCE(0);
    if (obj >= PadObject::A && obj < PadObject::Count)
        return DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(ButtonInstance(obj));
    return 0;
}

PadObject PadObjectFromObjId(DWORD objId)
{
    int instance = DIDFT_GETINSTANCE(objId);

    if ((objId & DIDFT_AXIS) && instance < PAD_AXIS_COUNT)
        return static_cast<PadObject>(instance);
    if ((objId & DIDFT_POV) && instance == 0)
        return PadObject::Pov;
    if ((objId & DIDFT_BUTTON) && instance < PAD_BUTTON_COUNT)
        return static_cast<PadObject>(static_cast<int>(PadObject::A) + instance);
    return PadObject::None;
}

bool PadObjectIsAxis(PadObject obj)
{
    return obj >= PadObject::LeftX && obj <= PadObject::RightY;
}

PadObject DefaultPadObjectFor(DWORD semantic, UINT_PTR appData)
{
    switch (semantic) {
    case DIAXIS_ANY_X_1: return PadObject::LeftX;
    case DIAXIS_ANY_Y_1: return PadObject::LeftY;
    case DIAXIS_ANY_3: return PadObject::RightX;
    case DIAXIS_ANY_4: return PadObject::RightY;
    case DIPOV_ANY_1: return PadObject::Pov;
    }

    // Only generic gamepad buttons; keyboard (0x81) and mouse (0x82) semantics belong to other devices.
    if ((semantic & 0xFFFFFF00) != DIBUTTON_ANY(0))
        return PadObject::None;

    switch (appData) {
    case OniAction::ExamineOrDecide: return PadObject::A;
    case OniAction::AbsorbOrCancel: return PadObject::B;
    case OniAction::Attack: return PadObject::X;
    case OniAction::StrategyShell: return PadObject::Y;
    case OniAction::Guard: return PadObject::LB;
    case OniAction::Stance: return PadObject::RB;
    case OniAction::Pause: return PadObject::Back;
    case OniAction::StatusMenu: return PadObject::Start;
    case OniAction::Map: return PadObject::LS;
    case OniAction::Onimusha: return PadObject::LT;
    case OniAction::TurnAround: return PadObject::RT;
    }
    return PadObject::None;
}

XInputPadState::XInputPadState()
    : axisMin(0), axisMax(0xFFFF), deadZone(0), sequence(0)
{
    Reset();
}

void XInputPadState::SetBindings(const std::vector<PadBinding> &bindings)
{
    this->bindings = bindings;
    Reset();
}

void XInputPadState::SetAxisRange(LONG min, LONG max)
{
    axisMin = min;
    axisMax = max;
    Reset();
}

void XInputPadState::SetDeadZone(DWORD deadZone)
{
    this->deadZone = deadZone > 10000 ? 10000 : deadZone;
}

void XInputPadState::Reset()
{
    for (int i = 0; i < PAD_OBJECT_COUNT; i++) {
        PadObject obj = static_cast<PadObject>(i);
        lastValue[i] = ValueOf(nullptr, obj);
    }
}

LONG XInputPadState::ScaleAxis(double normalized) const
{
    // normalized is -1..1; DirectInput axes go from min (left/up) to max (right/down).
    double center = (static_cast<double>(axisMin) + axisMax) / 2.0;
    double half = (static_cast<double>(axisMax) - axisMin) / 2.0;
    return static_cast<LONG>(std::lround(center + normalized * half));
}

void XInputPadState::StickValues(SHORT x, SHORT y, SHORT xinputDeadZone, LONG &outX, LONG &outY) const
{
    double nx = x / 32767.0;
    double ny = -y / 32767.0; // XInput up is positive, DirectInput up is negative
    double magnitude = std::sqrt(nx * nx + ny * ny);
    double dz = (std::max)(deadZone / 10000.0, xinputDeadZone / 32767.0);

    if (magnitude <= dz) {
        outX = ScaleAxis(0.0);
        outY = ScaleAxis(0.0);
        return;
    }

    // Radial dead zone, rescaled so the edge of the dead zone maps to 0.
    double scaled = (std::min)((magnitude - dz) / (1.0 - dz), 1.0);
    nx = (std::max)(-1.0, (std::min)(1.0, nx / magnitude * scaled));
    ny = (std::max)(-1.0, (std::min)(1.0, ny / magnitude * scaled));
    outX = ScaleAxis(nx);
    outY = ScaleAxis(ny);
}

LONG XInputPadState::ValueOf(const XINPUT_GAMEPAD *pad, PadObject obj) const
{
    static const XINPUT_GAMEPAD neutral = {};
    if (pad == nullptr)
        pad = &neutral;

    LONG x, y;
    switch (obj) {
    case PadObject::LeftX:
    case PadObject::LeftY:
        StickValues(pad->sThumbLX, pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, x, y);
        return obj == PadObject::LeftX ? x : y;
    case PadObject::RightX:
    case PadObject::RightY:
        StickValues(pad->sThumbRX, pad->sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, x, y);
        return obj == PadObject::RightX ? x : y;
    case PadObject::Pov:
        return PovFromDpad(pad->wButtons);
    case PadObject::LT:
        return pad->bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ? BUTTON_PRESSED : 0;
    case PadObject::RT:
        return pad->bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ? BUTTON_PRESSED : 0;
    default:
        if (obj >= PadObject::A && obj <= PadObject::RS)
            return (pad->wButtons & BUTTON_MASKS[ButtonInstance(obj)]) ? BUTTON_PRESSED : 0;
        return 0;
    }
}

void XInputPadState::Update(const XINPUT_GAMEPAD *pad, DWORD timeStamp, std::deque<DIDEVICEOBJECTDATA> &out)
{
    LONG current[PAD_OBJECT_COUNT];
    for (int i = 0; i < PAD_OBJECT_COUNT; i++)
        current[i] = ValueOf(pad, static_cast<PadObject>(i));

    for (const PadBinding &binding : bindings) {
        int i = static_cast<int>(binding.object);
        if (current[i] == lastValue[i])
            continue;

        DIDEVICEOBJECTDATA data = {};
        data.dwOfs = binding.actionIndex;
        data.dwData = static_cast<DWORD>(current[i]);
        data.dwTimeStamp = timeStamp;
        data.dwSequence = ++sequence;
        data.uAppData = binding.appData;
        out.push_back(data);
    }

    for (int i = 0; i < PAD_OBJECT_COUNT; i++)
        lastValue[i] = current[i];
}
