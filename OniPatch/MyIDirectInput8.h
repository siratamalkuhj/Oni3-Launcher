#pragma once

#include "pch.h"
#include "XInputDevice.h"
#include "Trace.h"

// Wraps the game's IDirectInput8A: adds the XInput virtual pad in front of the
// enumerated devices and hides the Xbox controllers' own DirectInput (HID) devices
// so the same controller is not read twice.
class MyIDirectInput8 : public IDirectInput8A
{
private:
    IDirectInput8A *original;
    LONG refCount;

    static BOOL CALLBACK EnumDevicesBySemanticsFilter(LPCDIDEVICEINSTANCEA lpddi, LPDIRECTINPUTDEVICE8A lpdid, DWORD dwFlags, DWORD dwRemaining, LPVOID pvRef);
    static BOOL CALLBACK EnumDevicesFilter(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef);
    std::vector<DWORD> FindXInputVidPids();
    bool IsXInputDevice(REFGUID guidInstance, const std::vector<DWORD> &xinputVidPids);

public:
    MyIDirectInput8(IDirectInput8A *original);
    virtual ~MyIDirectInput8();

    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID *ppvObj);
    STDMETHOD_(ULONG, AddRef)(THIS);
    STDMETHOD_(ULONG, Release)(THIS);

    /*** IDirectInput8A methods ***/
    STDMETHOD(CreateDevice)(THIS_ REFGUID rguid, LPDIRECTINPUTDEVICE8A *lplpDirectInputDevice, LPUNKNOWN pUnkOuter);
    STDMETHOD(EnumDevices)(THIS_ DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags);
    STDMETHOD(GetDeviceStatus)(THIS_ REFGUID rguidInstance);
    STDMETHOD(RunControlPanel)(THIS_ HWND hwndOwner, DWORD dwFlags);
    STDMETHOD(Initialize)(THIS_ HINSTANCE hinst, DWORD dwVersion);
    STDMETHOD(FindDevice)(THIS_ REFGUID rguidClass, LPCSTR ptszName, LPGUID pguidInstance);
    STDMETHOD(EnumDevicesBySemantics)(THIS_ LPCSTR ptszUserName, LPDIACTIONFORMATA lpdiActionFormat, LPDIENUMDEVICESBYSEMANTICSCBA lpCallback, LPVOID pvRef, DWORD dwFlags);
    STDMETHOD(ConfigureDevices)(THIS_ LPDICONFIGUREDEVICESCALLBACK lpdiCallback, LPDICONFIGUREDEVICESPARAMSA lpdiCDParams, DWORD dwFlags, LPVOID pvRefData);
};
