#pragma once

#include "pch.h"
#include "XInputPad.h"
#include "Trace.h"
#include "Cheats.h"

// {4F4E4933-5849-4E50-5554-2D5041440001}
extern const GUID GUID_OniXInputDevice;

// A DirectInput 8 gamepad backed by the first connected XInput controller.
// Implements what Onimusha 3 uses: action mapping and buffered data.
class XInputDevice : public IDirectInputDevice8A
{
private:
    LONG refCount;
    bool acquired;
    DWORD bufferSize;
    XInputPadState pad;
    std::deque<DIDEVICEOBJECTDATA> buffer;
    DWORD slot;      // XInput user index in use, XUSER_MAX_COUNT when none
    DWORD lastScan;  // GetTickCount() of the last search for a controller

    static const DWORD RESCAN_INTERVAL_MS = 1000;

    void ReadController();

public:
    XInputDevice();
    virtual ~XInputDevice();

    // True when at least one XInput controller is connected.
    static bool AnyControllerConnected();
    static void FillDeviceInstance(LPDIDEVICEINSTANCEA lpddi);

    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID *ppvObj);
    STDMETHOD_(ULONG, AddRef)(THIS);
    STDMETHOD_(ULONG, Release)(THIS);

    /*** IDirectInputDevice8A methods ***/
    STDMETHOD(GetCapabilities)(THIS_ LPDIDEVCAPS lpDIDevCaps);
    STDMETHOD(EnumObjects)(THIS_ LPDIENUMDEVICEOBJECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags);
    STDMETHOD(GetProperty)(THIS_ REFGUID rguidProp, LPDIPROPHEADER pdiph);
    STDMETHOD(SetProperty)(THIS_ REFGUID rguidProp, LPCDIPROPHEADER pdiph);
    STDMETHOD(Acquire)(THIS);
    STDMETHOD(Unacquire)(THIS);
    STDMETHOD(GetDeviceState)(THIS_ DWORD cbData, LPVOID lpvData);
    STDMETHOD(GetDeviceData)(THIS_ DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags);
    STDMETHOD(SetDataFormat)(THIS_ LPCDIDATAFORMAT lpdf);
    STDMETHOD(SetEventNotification)(THIS_ HANDLE hEvent);
    STDMETHOD(SetCooperativeLevel)(THIS_ HWND hwnd, DWORD dwFlags);
    STDMETHOD(GetObjectInfo)(THIS_ LPDIDEVICEOBJECTINSTANCEA pdidoi, DWORD dwObj, DWORD dwHow);
    STDMETHOD(GetDeviceInfo)(THIS_ LPDIDEVICEINSTANCEA pdidi);
    STDMETHOD(RunControlPanel)(THIS_ HWND hwndOwner, DWORD dwFlags);
    STDMETHOD(Initialize)(THIS_ HINSTANCE hinst, DWORD dwVersion, REFGUID rguid);
    STDMETHOD(CreateEffect)(THIS_ REFGUID rguid, LPCDIEFFECT lpeff, LPDIRECTINPUTEFFECT *ppdeff, LPUNKNOWN punkOuter);
    STDMETHOD(EnumEffects)(THIS_ LPDIENUMEFFECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwEffType);
    STDMETHOD(GetEffectInfo)(THIS_ LPDIEFFECTINFOA pdei, REFGUID rguid);
    STDMETHOD(GetForceFeedbackState)(THIS_ LPDWORD pdwOut);
    STDMETHOD(SendForceFeedbackCommand)(THIS_ DWORD dwFlags);
    STDMETHOD(EnumCreatedEffectObjects)(THIS_ LPDIENUMCREATEDEFFECTOBJECTSCALLBACK lpCallback, LPVOID pvRef, DWORD fl);
    STDMETHOD(Escape)(THIS_ LPDIEFFESCAPE pesc);
    STDMETHOD(Poll)(THIS);
    STDMETHOD(SendDeviceData)(THIS_ DWORD cbObjectData, LPCDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD fl);
    STDMETHOD(EnumEffectsInFile)(THIS_ LPCSTR lpszFileName, LPDIENUMEFFECTSINFILECALLBACK pec, LPVOID pvRef, DWORD dwFlags);
    STDMETHOD(WriteEffectToFile)(THIS_ LPCSTR lpszFileName, DWORD dwEntries, LPDIFILEEFFECT rgDiFileEft, DWORD dwFlags);
    STDMETHOD(BuildActionMap)(THIS_ LPDIACTIONFORMATA lpdiaf, LPCSTR lpszUserName, DWORD dwFlags);
    STDMETHOD(SetActionMap)(THIS_ LPDIACTIONFORMATA lpdiaf, LPCSTR lpszUserName, DWORD dwFlags);
    STDMETHOD(GetImageInfo)(THIS_ LPDIDEVICEIMAGEINFOHEADERA lpdiDevImageInfoHeader);
};
