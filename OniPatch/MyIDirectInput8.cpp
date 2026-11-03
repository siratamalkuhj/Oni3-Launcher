#include "pch.h"
#include "MyIDirectInput8.h"

namespace
{
    struct SemanticsContext
    {
        LPDIENUMDEVICESBYSEMANTICSCBA callback;
        LPVOID pvRef;
        const std::vector<DWORD> *xinputVidPids;
    };

    struct DevicesContext
    {
        MyIDirectInput8 *self;
        LPDIENUMDEVICESCALLBACKA callback;
        LPVOID pvRef;
        const std::vector<DWORD> *xinputVidPids;
    };

    struct CollectContext
    {
        IDirectInput8A *original;
        std::vector<DWORD> *xinputVidPids;
    };

    // Xbox controllers expose "IG_" in the HID path of their gamepad interface.
    bool IsXInputPath(LPDIRECTINPUTDEVICE8A device)
    {
        DIPROPGUIDANDPATH prop = {};
        prop.diph.dwSize = sizeof(DIPROPGUIDANDPATH);
        prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        prop.diph.dwHow = DIPH_DEVICE;

        if (FAILED(device->GetProperty(DIPROP_GUIDANDPATH, &prop.diph)))
            return false;

        _wcsupr_s(prop.wszPath);
        return wcsstr(prop.wszPath, L"IG_") != nullptr;
    }

    DWORD GetVidPid(LPDIRECTINPUTDEVICE8A device)
    {
        DIPROPDWORD prop = {};
        prop.diph.dwSize = sizeof(DIPROPDWORD);
        prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        prop.diph.dwHow = DIPH_DEVICE;

        if (FAILED(device->GetProperty(DIPROP_VIDPID, &prop.diph)))
            return 0;
        return prop.dwData;
    }

    // Some controllers add extra HID interfaces (vendor, media keys) next to the gamepad one;
    // they share its VID/PID, so they are hidden with it.
    bool IsPartOfXInputController(LPDIRECTINPUTDEVICE8A device, const std::vector<DWORD> &xinputVidPids)
    {
        if (IsXInputPath(device))
            return true;

        DWORD vidPid = GetVidPid(device);
        for (DWORD known : xinputVidPids)
            if (vidPid != 0 && vidPid == known)
                return true;
        return false;
    }

    BOOL CALLBACK CollectXInputVidPids(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef)
    {
        CollectContext *context = static_cast<CollectContext *>(pvRef);
        LPDIRECTINPUTDEVICE8A device = nullptr;

        if (SUCCEEDED(context->original->CreateDevice(lpddi->guidInstance, &device, nullptr))) {
            if (IsXInputPath(device)) {
                DWORD vidPid = GetVidPid(device);
                if (vidPid != 0)
                    context->xinputVidPids->push_back(vidPid);
            }
            device->Release();
        }
        return DIENUM_CONTINUE;
    }
}

std::vector<DWORD> MyIDirectInput8::FindXInputVidPids()
{
    std::vector<DWORD> vidPids;
    CollectContext context = { original, &vidPids };

    original->EnumDevices(DI8DEVCLASS_GAMECTRL, CollectXInputVidPids, &context, DIEDFL_ATTACHEDONLY);
    return vidPids;
}

MyIDirectInput8::MyIDirectInput8(IDirectInput8A *original)
    : original(original), refCount(1)
{
}

MyIDirectInput8::~MyIDirectInput8() {}

bool MyIDirectInput8::IsXInputDevice(REFGUID guidInstance, const std::vector<DWORD> &xinputVidPids)
{
    LPDIRECTINPUTDEVICE8A device = nullptr;
    bool result = false;

    if (SUCCEEDED(original->CreateDevice(guidInstance, &device, nullptr))) {
        result = IsPartOfXInputController(device, xinputVidPids);
        device->Release();
    }
    return result;
}

BOOL CALLBACK MyIDirectInput8::EnumDevicesBySemanticsFilter(LPCDIDEVICEINSTANCEA lpddi, LPDIRECTINPUTDEVICE8A lpdid, DWORD dwFlags, DWORD dwRemaining, LPVOID pvRef)
{
    SemanticsContext *context = static_cast<SemanticsContext *>(pvRef);

    if (IsPartOfXInputController(lpdid, *context->xinputVidPids)) {
        std::cout << "Hiding XInput controller interface: " << lpddi->tszInstanceName << std::endl;
        return DIENUM_CONTINUE;
    }
    return context->callback(lpddi, lpdid, dwFlags, dwRemaining, context->pvRef);
}

BOOL CALLBACK MyIDirectInput8::EnumDevicesFilter(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef)
{
    DevicesContext *context = static_cast<DevicesContext *>(pvRef);

    if (context->self->IsXInputDevice(lpddi->guidInstance, *context->xinputVidPids))
        return DIENUM_CONTINUE;
    return context->callback(lpddi, context->pvRef);
}

/*** IUnknown methods ***/
HRESULT MyIDirectInput8::QueryInterface(THIS_ REFIID riid, LPVOID *ppvObj)
{
    if (ppvObj == nullptr)
        return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirectInput8A) {
        *ppvObj = this;
        AddRef();
        return S_OK;
    }
    return original->QueryInterface(riid, ppvObj);
}

ULONG MyIDirectInput8::AddRef(THIS)
{
    return InterlockedIncrement(&refCount);
}

ULONG MyIDirectInput8::Release(THIS)
{
    ULONG count = InterlockedDecrement(&refCount);

    if (count == 0) {
        original->Release();
        delete this;
    }
    return count;
}

/*** IDirectInput8A methods ***/
HRESULT MyIDirectInput8::CreateDevice(THIS_ REFGUID rguid, LPDIRECTINPUTDEVICE8A *lplpDirectInputDevice, LPUNKNOWN pUnkOuter)
{
    if (rguid == GUID_OniXInputDevice) {
        if (lplpDirectInputDevice == nullptr)
            return DIERR_INVALIDPARAM;
        *lplpDirectInputDevice = new XInputDevice();
        return DI_OK;
    }
    return original->CreateDevice(rguid, lplpDirectInputDevice, pUnkOuter);
}

HRESULT MyIDirectInput8::EnumDevices(THIS_ DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags)
{
    if (lpCallback == nullptr)
        return DIERR_INVALIDPARAM;

    bool wantsGamepad = dwDevType == DI8DEVCLASS_ALL || dwDevType == DI8DEVCLASS_GAMECTRL || dwDevType == DI8DEVTYPE_GAMEPAD;
    if (wantsGamepad && (dwFlags & DIEDFL_FORCEFEEDBACK) == 0 && XInputDevice::AnyControllerConnected()) {
        DIDEVICEINSTANCEA instance;
        instance.dwSize = sizeof(instance);
        XInputDevice::FillDeviceInstance(&instance);
        if (lpCallback(&instance, pvRef) == DIENUM_STOP)
            return DI_OK;
    }

    std::vector<DWORD> xinputVidPids = FindXInputVidPids();
    DevicesContext context = { this, lpCallback, pvRef, &xinputVidPids };
    return original->EnumDevices(dwDevType, EnumDevicesFilter, &context, dwFlags);
}

HRESULT MyIDirectInput8::GetDeviceStatus(THIS_ REFGUID rguidInstance)
{
    if (rguidInstance == GUID_OniXInputDevice)
        return XInputDevice::AnyControllerConnected() ? DI_OK : DI_NOTATTACHED;
    return original->GetDeviceStatus(rguidInstance);
}

HRESULT MyIDirectInput8::RunControlPanel(THIS_ HWND hwndOwner, DWORD dwFlags)
{
    return original->RunControlPanel(hwndOwner, dwFlags);
}

HRESULT MyIDirectInput8::Initialize(THIS_ HINSTANCE hinst, DWORD dwVersion)
{
    return original->Initialize(hinst, dwVersion);
}

HRESULT MyIDirectInput8::FindDevice(THIS_ REFGUID rguidClass, LPCSTR ptszName, LPGUID pguidInstance)
{
    return original->FindDevice(rguidClass, ptszName, pguidInstance);
}

HRESULT MyIDirectInput8::EnumDevicesBySemantics(THIS_ LPCSTR ptszUserName, LPDIACTIONFORMATA lpdiActionFormat, LPDIENUMDEVICESBYSEMANTICSCBA lpCallback, LPVOID pvRef, DWORD dwFlags)
{
    TraceFunc();

    if (lpCallback == nullptr)
        return DIERR_INVALIDPARAM;

    // The game only reads the first gamepad in single player, so the virtual pad goes first.
    // It is offered only when a controller is plugged in, to leave other gamepads alone otherwise.
    if ((dwFlags & DIEDBSFL_FORCEFEEDBACK) == 0 && XInputDevice::AnyControllerConnected()) {
        DIDEVICEINSTANCEA instance;
        instance.dwSize = sizeof(instance);
        XInputDevice::FillDeviceInstance(&instance);

        XInputDevice *device = new XInputDevice();
        BOOL result = lpCallback(&instance, device, DIEDBS_MAPPEDPRI1, 1, pvRef);
        // Like DirectInput, drop our reference: the game AddRefs the devices it keeps.
        device->Release();
        TraceMsg("XInput virtual device offered");
        if (result == DIENUM_STOP)
            return DI_OK;
    }

    std::vector<DWORD> xinputVidPids = FindXInputVidPids();
    SemanticsContext context = { lpCallback, pvRef, &xinputVidPids };
    return original->EnumDevicesBySemantics(ptszUserName, lpdiActionFormat, EnumDevicesBySemanticsFilter, &context, dwFlags);
}

HRESULT MyIDirectInput8::ConfigureDevices(THIS_ LPDICONFIGUREDEVICESCALLBACK lpdiCallback, LPDICONFIGUREDEVICESPARAMSA lpdiCDParams, DWORD dwFlags, LPVOID pvRefData)
{
    return original->ConfigureDevices(lpdiCallback, lpdiCDParams, dwFlags, pvRefData);
}
