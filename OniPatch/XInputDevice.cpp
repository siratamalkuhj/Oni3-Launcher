#include "pch.h"
#include "XInputDevice.h"

const GUID GUID_OniXInputDevice = { 0x4F4E4933, 0x5849, 0x4E50, { 0x55, 0x54, 0x2D, 0x50, 0x41, 0x44, 0x00, 0x01 } };

namespace
{
    const char DEVICE_NAME[] = "Xbox Controller (XInput)";

    struct ObjectInfo
    {
        PadObject object;
        const GUID *type;
        DWORD offset; // offset in DIJOYSTATE
        const char *name;
    };

    const ObjectInfo OBJECTS[] = {
        { PadObject::LeftX, &GUID_XAxis, DIJOFS_X, "Left Stick X" },
        { PadObject::LeftY, &GUID_YAxis, DIJOFS_Y, "Left Stick Y" },
        { PadObject::RightX, &GUID_RxAxis, DIJOFS_RX, "Right Stick X" },
        { PadObject::RightY, &GUID_RyAxis, DIJOFS_RY, "Right Stick Y" },
        { PadObject::Pov, &GUID_POV, DIJOFS_POV(0), "D-Pad" },
        { PadObject::A, &GUID_Button, DIJOFS_BUTTON(0), "A" },
        { PadObject::B, &GUID_Button, DIJOFS_BUTTON(1), "B" },
        { PadObject::X, &GUID_Button, DIJOFS_BUTTON(2), "X" },
        { PadObject::Y, &GUID_Button, DIJOFS_BUTTON(3), "Y" },
        { PadObject::LB, &GUID_Button, DIJOFS_BUTTON(4), "LB" },
        { PadObject::RB, &GUID_Button, DIJOFS_BUTTON(5), "RB" },
        { PadObject::Back, &GUID_Button, DIJOFS_BUTTON(6), "Back" },
        { PadObject::Start, &GUID_Button, DIJOFS_BUTTON(7), "Start" },
        { PadObject::LS, &GUID_Button, DIJOFS_BUTTON(8), "Left Stick Button" },
        { PadObject::RS, &GUID_Button, DIJOFS_BUTTON(9), "Right Stick Button" },
        { PadObject::LT, &GUID_Button, DIJOFS_BUTTON(10), "LT" },
        { PadObject::RT, &GUID_Button, DIJOFS_BUTTON(11), "RT" },
    };

    void FillObjectInstance(const ObjectInfo &info, LPDIDEVICEOBJECTINSTANCEA pdidoi)
    {
        DWORD size = pdidoi->dwSize;

        ZeroMemory(pdidoi, size);
        pdidoi->dwSize = size;
        pdidoi->guidType = *info.type;
        pdidoi->dwOfs = info.offset;
        pdidoi->dwType = PadObjectToObjId(info.object);
        pdidoi->dwFlags = PadObjectIsAxis(info.object) ? DIDOI_ASPECTPOSITION : 0;
        strcpy_s(pdidoi->tszName, info.name);
    }

    bool IsGameAxisRangeFlag(DWORD actionFlags)
    {
        return (actionFlags & DIA_NORANGE) == 0;
    }

    // Like DirectInput, only accept the exact structure sizes: Onimusha 3 calls GetDeviceInfo
    // with an uninitialized dwSize, and writing that many bytes would smash its stack.
    bool IsValidSize(DWORD size, DWORD current, DWORD dx3)
    {
        return size == current || size == dx3;
    }
}

XInputDevice::XInputDevice()
    : refCount(1), acquired(false), bufferSize(0), slot(XUSER_MAX_COUNT), lastScan(GetTickCount() - RESCAN_INTERVAL_MS)
{
}

XInputDevice::~XInputDevice() {}

bool XInputDevice::AnyControllerConnected()
{
    XINPUT_STATE state;

    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
        if (XInputGetState(i, &state) == ERROR_SUCCESS)
            return true;
    return false;
}

void XInputDevice::FillDeviceInstance(LPDIDEVICEINSTANCEA lpddi)
{
    DWORD size = lpddi->dwSize;

    ZeroMemory(lpddi, size);
    lpddi->dwSize = size;
    lpddi->guidInstance = GUID_OniXInputDevice;
    lpddi->guidProduct = GUID_OniXInputDevice;
    lpddi->dwDevType = DI8DEVTYPE_GAMEPAD | (DI8DEVTYPEGAMEPAD_STANDARD << 8);
    strcpy_s(lpddi->tszInstanceName, DEVICE_NAME);
    strcpy_s(lpddi->tszProductName, DEVICE_NAME);
    if (size >= sizeof(DIDEVICEINSTANCEA)) {
        lpddi->wUsagePage = 0x01; // generic desktop
        lpddi->wUsage = 0x05;     // game pad
    }
}

void XInputDevice::ReadController()
{
    XINPUT_STATE state;
    const XINPUT_GAMEPAD *gamepad = nullptr;
    DWORD now = GetTickCount();

    if (slot < XUSER_MAX_COUNT) {
        if (XInputGetState(slot, &state) == ERROR_SUCCESS)
            gamepad = &state.Gamepad;
        else
            slot = XUSER_MAX_COUNT;
    }

    if (gamepad == nullptr && now - lastScan >= RESCAN_INTERVAL_MS) {
        // Querying empty slots is slow, so look for another controller at most once per interval.
        lastScan = now;
        slot = XUSER_MAX_COUNT;
        for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
            if (XInputGetState(i, &state) == ERROR_SUCCESS) {
                slot = i;
                gamepad = &state.Gamepad;
                break;
            }
        }
    }

    // A disconnected controller reports neutral values, releasing anything held.
    if (gamepad != nullptr) {
        // Cheat combos (right stick button + D-pad / Y) are not passed to the game.
        XINPUT_GAMEPAD filtered = Cheats::Get().FilterPad(*gamepad);
        pad.Update(&filtered, now, buffer);
    } else {
        pad.Update(nullptr, now, buffer);
    }
}

/*** IUnknown methods ***/
HRESULT XInputDevice::QueryInterface(THIS_ REFIID riid, LPVOID *ppvObj)
{
    if (ppvObj == nullptr)
        return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirectInputDevice8A) {
        *ppvObj = this;
        AddRef();
        return S_OK;
    }
    *ppvObj = nullptr;
    return E_NOINTERFACE;
}

ULONG XInputDevice::AddRef(THIS)
{
    return InterlockedIncrement(&refCount);
}

ULONG XInputDevice::Release(THIS)
{
    ULONG count = InterlockedDecrement(&refCount);

    if (count == 0)
        delete this;
    return count;
}

/*** IDirectInputDevice8A methods ***/
HRESULT XInputDevice::GetCapabilities(THIS_ LPDIDEVCAPS lpDIDevCaps)
{
    if (lpDIDevCaps == nullptr || !IsValidSize(lpDIDevCaps->dwSize, sizeof(DIDEVCAPS), sizeof(DIDEVCAPS_DX3)))
        return DIERR_INVALIDPARAM;

    DWORD size = lpDIDevCaps->dwSize;
    ZeroMemory(lpDIDevCaps, size);
    lpDIDevCaps->dwSize = size;
    lpDIDevCaps->dwFlags = DIDC_ATTACHED | DIDC_POLLEDDEVICE;
    lpDIDevCaps->dwDevType = DI8DEVTYPE_GAMEPAD | (DI8DEVTYPEGAMEPAD_STANDARD << 8);
    lpDIDevCaps->dwAxes = PAD_AXIS_COUNT;
    lpDIDevCaps->dwButtons = PAD_BUTTON_COUNT;
    lpDIDevCaps->dwPOVs = 1;
    return DI_OK;
}

HRESULT XInputDevice::EnumObjects(THIS_ LPDIENUMDEVICEOBJECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags)
{
    if (lpCallback == nullptr)
        return DIERR_INVALIDPARAM;

    for (const ObjectInfo &info : OBJECTS) {
        DWORD objId = PadObjectToObjId(info.object);
        if (dwFlags != DIDFT_ALL && (DIDFT_GETTYPE(objId) & dwFlags) == 0)
            continue;

        DIDEVICEOBJECTINSTANCEA instance;
        instance.dwSize = sizeof(instance);
        FillObjectInstance(info, &instance);
        if (lpCallback(&instance, pvRef) == DIENUM_STOP)
            break;
    }
    return DI_OK;
}

HRESULT XInputDevice::GetProperty(THIS_ REFGUID rguidProp, LPDIPROPHEADER pdiph)
{
    if (pdiph == nullptr || pdiph->dwHeaderSize != sizeof(DIPROPHEADER))
        return DIERR_INVALIDPARAM;

    if (&rguidProp == &DIPROP_BUFFERSIZE) {
        if (pdiph->dwSize != sizeof(DIPROPDWORD))
            return DIERR_INVALIDPARAM;
        reinterpret_cast<LPDIPROPDWORD>(pdiph)->dwData = bufferSize;
        return DI_OK;
    }
    if (&rguidProp == &DIPROP_PRODUCTNAME || &rguidProp == &DIPROP_INSTANCENAME) {
        if (pdiph->dwSize != sizeof(DIPROPSTRING))
            return DIERR_INVALIDPARAM;
        MultiByteToWideChar(CP_ACP, 0, DEVICE_NAME, -1, reinterpret_cast<LPDIPROPSTRING>(pdiph)->wsz, MAX_PATH);
        return DI_OK;
    }
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::SetProperty(THIS_ REFGUID rguidProp, LPCDIPROPHEADER pdiph)
{
    if (pdiph == nullptr || pdiph->dwHeaderSize != sizeof(DIPROPHEADER))
        return DIERR_INVALIDPARAM;
    if ((&rguidProp == &DIPROP_BUFFERSIZE || &rguidProp == &DIPROP_DEADZONE) && pdiph->dwSize != sizeof(DIPROPDWORD))
        return DIERR_INVALIDPARAM;
    if (&rguidProp == &DIPROP_RANGE && pdiph->dwSize != sizeof(DIPROPRANGE))
        return DIERR_INVALIDPARAM;

    if (&rguidProp == &DIPROP_BUFFERSIZE) {
        bufferSize = reinterpret_cast<LPCDIPROPDWORD>(pdiph)->dwData;
        return DI_OK;
    }
    if (&rguidProp == &DIPROP_DEADZONE) {
        // Onimusha 3 sets a device-wide 5% dead zone; the XInput stick dead zone still applies.
        pad.SetDeadZone(reinterpret_cast<LPCDIPROPDWORD>(pdiph)->dwData);
        return DI_OK;
    }
    if (&rguidProp == &DIPROP_RANGE) {
        LPCDIPROPRANGE range = reinterpret_cast<LPCDIPROPRANGE>(pdiph);
        pad.SetAxisRange(range->lMin, range->lMax);
        return DI_OK;
    }
    if (&rguidProp == &DIPROP_SATURATION || &rguidProp == &DIPROP_AXISMODE)
        return DI_OK;
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::Acquire(THIS)
{
    if (acquired)
        return DI_NOEFFECT;
    acquired = true;
    return DI_OK;
}

HRESULT XInputDevice::Unacquire(THIS)
{
    if (!acquired)
        return DI_NOEFFECT;
    acquired = false;
    buffer.clear();
    pad.Reset();
    return DI_OK;
}

HRESULT XInputDevice::GetDeviceState(THIS_ DWORD cbData, LPVOID lpvData)
{
    // Onimusha 3 only reads buffered action data.
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::GetDeviceData(THIS_ DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags)
{
    if (pdwInOut == nullptr || cbObjectData != sizeof(DIDEVICEOBJECTDATA))
        return DIERR_INVALIDPARAM;
    if (!acquired)
        return DIERR_NOTACQUIRED;

    HRESULT result = DI_OK;

    ReadController();
    if (bufferSize != 0 && buffer.size() > bufferSize) {
        buffer.erase(buffer.begin(), buffer.begin() + (buffer.size() - bufferSize));
        result = DI_BUFFEROVERFLOW;
    }

    DWORD count = (std::min)(*pdwInOut, static_cast<DWORD>(buffer.size()));
    if (rgdod != nullptr)
        for (DWORD i = 0; i < count; i++)
            rgdod[i] = buffer[i];
    if ((dwFlags & DIGDD_PEEK) == 0)
        buffer.erase(buffer.begin(), buffer.begin() + count);

    *pdwInOut = count;
    return result;
}

HRESULT XInputDevice::SetDataFormat(THIS_ LPCDIDATAFORMAT lpdf)
{
    return DI_OK;
}

HRESULT XInputDevice::SetEventNotification(THIS_ HANDLE hEvent)
{
    return DI_OK;
}

HRESULT XInputDevice::SetCooperativeLevel(THIS_ HWND hwnd, DWORD dwFlags)
{
    return DI_OK;
}

HRESULT XInputDevice::GetObjectInfo(THIS_ LPDIDEVICEOBJECTINSTANCEA pdidoi, DWORD dwObj, DWORD dwHow)
{
    if (pdidoi == nullptr || !IsValidSize(pdidoi->dwSize, sizeof(DIDEVICEOBJECTINSTANCEA), sizeof(DIDEVICEOBJECTINSTANCE_DX3A)))
        return DIERR_INVALIDPARAM;

    for (const ObjectInfo &info : OBJECTS) {
        if ((dwHow == DIPH_BYID && PadObjectToObjId(info.object) == dwObj) ||
            (dwHow == DIPH_BYOFFSET && info.offset == dwObj)) {
            FillObjectInstance(info, pdidoi);
            return DI_OK;
        }
    }
    return DIERR_OBJECTNOTFOUND;
}

HRESULT XInputDevice::GetDeviceInfo(THIS_ LPDIDEVICEINSTANCEA pdidi)
{
    if (pdidi == nullptr || !IsValidSize(pdidi->dwSize, sizeof(DIDEVICEINSTANCEA), sizeof(DIDEVICEINSTANCE_DX3A)))
        return DIERR_INVALIDPARAM;

    FillDeviceInstance(pdidi);
    return DI_OK;
}

HRESULT XInputDevice::RunControlPanel(THIS_ HWND hwndOwner, DWORD dwFlags)
{
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::Initialize(THIS_ HINSTANCE hinst, DWORD dwVersion, REFGUID rguid)
{
    return DI_OK;
}

HRESULT XInputDevice::CreateEffect(THIS_ REFGUID rguid, LPCDIEFFECT lpeff, LPDIRECTINPUTEFFECT *ppdeff, LPUNKNOWN punkOuter)
{
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::EnumEffects(THIS_ LPDIENUMEFFECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwEffType)
{
    return DI_OK;
}

HRESULT XInputDevice::GetEffectInfo(THIS_ LPDIEFFECTINFOA pdei, REFGUID rguid)
{
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::GetForceFeedbackState(THIS_ LPDWORD pdwOut)
{
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::SendForceFeedbackCommand(THIS_ DWORD dwFlags)
{
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::EnumCreatedEffectObjects(THIS_ LPDIENUMCREATEDEFFECTOBJECTSCALLBACK lpCallback, LPVOID pvRef, DWORD fl)
{
    return DI_OK;
}

HRESULT XInputDevice::Escape(THIS_ LPDIEFFESCAPE pesc)
{
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::Poll(THIS)
{
    if (!acquired)
        return DIERR_NOTACQUIRED;

    ReadController();
    return DI_OK;
}

HRESULT XInputDevice::SendDeviceData(THIS_ DWORD cbObjectData, LPCDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD fl)
{
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::EnumEffectsInFile(THIS_ LPCSTR lpszFileName, LPDIENUMEFFECTSINFILECALLBACK pec, LPVOID pvRef, DWORD dwFlags)
{
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::WriteEffectToFile(THIS_ LPCSTR lpszFileName, DWORD dwEntries, LPDIFILEEFFECT rgDiFileEft, DWORD dwFlags)
{
    return DIERR_UNSUPPORTED;
}

HRESULT XInputDevice::BuildActionMap(THIS_ LPDIACTIONFORMATA lpdiaf, LPCSTR lpszUserName, DWORD dwFlags)
{
    if (lpdiaf == nullptr || lpdiaf->rgoAction == nullptr)
        return DIERR_INVALIDPARAM;

    bool used[PAD_OBJECT_COUNT] = {};
    bool mapped = false;

    TraceFunc();

    for (DWORD i = 0; i < lpdiaf->dwNumActions; i++) {
        DIACTIONA &action = lpdiaf->rgoAction[i];

        if (action.dwFlags & DIA_APPNOMAP)
            continue;

        PadObject obj = DefaultPadObjectFor(action.dwSemantic, action.uAppData);
        if (obj != PadObject::None && !used[static_cast<int>(obj)]) {
            used[static_cast<int>(obj)] = true;
            action.guidInstance = GUID_OniXInputDevice;
            action.dwObjID = PadObjectToObjId(obj);
            action.dwHow = DIAH_DEFAULT;
            mapped = true;
            TraceParam("Mapped action", i);
        } else if (action.guidInstance == GUID_OniXInputDevice) {
            action.guidInstance = GUID_NULL;
            action.dwObjID = 0;
            action.dwHow = DIAH_UNMAPPED;
        }
    }
    return mapped ? DI_OK : DI_NOEFFECT;
}

HRESULT XInputDevice::SetActionMap(THIS_ LPDIACTIONFORMATA lpdiaf, LPCSTR lpszUserName, DWORD dwFlags)
{
    if (lpdiaf == nullptr || lpdiaf->rgoAction == nullptr)
        return DIERR_INVALIDPARAM;
    if (acquired)
        return DIERR_ACQUIRED;

    std::vector<PadBinding> bindings;
    bool hasRange = false;

    for (DWORD i = 0; i < lpdiaf->dwNumActions; i++) {
        const DIACTIONA &action = lpdiaf->rgoAction[i];

        if (action.guidInstance != GUID_OniXInputDevice || action.dwHow == DIAH_UNMAPPED)
            continue;

        PadObject obj = PadObjectFromObjId(action.dwObjID);
        if (obj == PadObject::None)
            continue;
        bindings.push_back({ obj, i, action.uAppData });
        if (PadObjectIsAxis(obj) && IsGameAxisRangeFlag(action.dwFlags))
            hasRange = true;
    }

    if (hasRange)
        pad.SetAxisRange(lpdiaf->lAxisMin, lpdiaf->lAxisMax);
    bufferSize = lpdiaf->dwBufferSize;
    pad.SetBindings(bindings);
    buffer.clear();
    return DI_OK;
}

HRESULT XInputDevice::GetImageInfo(THIS_ LPDIDEVICEIMAGEINFOHEADERA lpdiDevImageInfoHeader)
{
    return DIERR_UNSUPPORTED;
}
