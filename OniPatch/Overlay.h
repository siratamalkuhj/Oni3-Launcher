#pragma once

#include "pch.h"

// Draws the enabled cheats in the top-left corner of the game.
// The text is rendered with GDI into a texture (no D3DX needed) and redrawn only when it changes.
class Overlay
{
public:
    static Overlay &Get();

    // Call between BeginScene and EndScene.
    void Draw(IDirect3DDevice9 *device);
    // Call before IDirect3DDevice9::Reset.
    void OnLostDevice();
    // Device references held by the overlay's texture and state block.
    ULONG DeviceReferences() const;

private:
    Overlay();
    bool Rebuild(IDirect3DDevice9 *device);
    void Release();

    IDirect3DTexture9 *texture;
    IDirect3DStateBlock9 *stateBlock;
    UINT width;
    UINT height;
    DWORD builtVersion;
    bool empty;
};
