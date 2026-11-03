#include "pch.h"
#include "Overlay.h"
#include "Cheats.h"

namespace
{
    const UINT TEXTURE_WIDTH = 512;
    const int LINE_HEIGHT = 34;
    const int FONT_HEIGHT = 28;
    const int MARGIN = 4;
    const float SCREEN_X = 16.0f;
    const float SCREEN_Y = 16.0f;
    const COLORREF TEXT_COLOR = RGB(255, 220, 64);

    struct Vertex
    {
        float x, y, z, rhw;
        float u, v;
    };
    const DWORD VERTEX_FVF = D3DFVF_XYZRHW | D3DFVF_TEX1;

    // Draws the lines into a 32-bit top-down DIB; returns its pixels (BGRA).
    std::vector<DWORD> RenderLines(const std::vector<std::wstring> &lines, UINT width, UINT height, bool outlineOnlyWhite)
    {
        std::vector<DWORD> pixels(width * height, 0);
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -static_cast<LONG>(height);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        HDC dc = CreateCompatibleDC(NULL);
        void *bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
        if (bitmap == NULL) {
            DeleteDC(dc);
            return pixels;
        }
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        HFONT font = CreateFontW(FONT_HEIGHT, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Yu Gothic UI");
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);

        for (size_t i = 0; i < lines.size(); i++) {
            int x = MARGIN + 2;
            int y = MARGIN + static_cast<int>(i) * LINE_HEIGHT;
            const std::wstring &text = lines[i];

            // Outline, then the text on top.
            SetTextColor(dc, outlineOnlyWhite ? RGB(255, 255, 255) : RGB(0, 0, 0));
            for (int dx = -2; dx <= 2; dx++)
                for (int dy = -2; dy <= 2; dy++)
                    if (dx != 0 || dy != 0)
                        TextOutW(dc, x + dx, y + dy, text.c_str(), static_cast<int>(text.size()));
            SetTextColor(dc, outlineOnlyWhite ? RGB(255, 255, 255) : TEXT_COLOR);
            TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
        }

        GdiFlush();
        memcpy(pixels.data(), bits, pixels.size() * sizeof(DWORD));

        SelectObject(dc, oldFont);
        DeleteObject(font);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
        return pixels;
    }
}

Overlay &Overlay::Get()
{
    static Overlay instance;
    return instance;
}

Overlay::Overlay()
    : texture(nullptr), stateBlock(nullptr), width(0), height(0), builtVersion(static_cast<DWORD>(-1)), empty(true)
{
}

void Overlay::Release()
{
    if (texture != nullptr) {
        texture->Release();
        texture = nullptr;
    }
    if (stateBlock != nullptr) {
        stateBlock->Release();
        stateBlock = nullptr;
    }
    builtVersion = static_cast<DWORD>(-1);
}

void Overlay::OnLostDevice()
{
    Release();
}

ULONG Overlay::DeviceReferences() const
{
    return (texture != nullptr ? 1 : 0) + (stateBlock != nullptr ? 1 : 0);
}

bool Overlay::Rebuild(IDirect3DDevice9 *device)
{
    std::vector<std::wstring> lines;
    for (int i = 0; i < CHEAT_COUNT; i++)
        if (Cheats::Get().IsEnabled(static_cast<Cheat>(i)))
            lines.push_back(Cheats::Label(static_cast<Cheat>(i)));

    if (texture != nullptr) {
        texture->Release();
        texture = nullptr;
    }
    builtVersion = Cheats::Get().Version();
    empty = lines.empty();
    if (empty)
        return true;

    width = TEXTURE_WIDTH;
    height = 64;
    while (height < static_cast<UINT>(lines.size() * LINE_HEIGHT + MARGIN * 2))
        height *= 2;

    // Mask pass gives the alpha (white shapes on black), color pass gives the colors.
    std::vector<DWORD> mask = RenderLines(lines, width, height, true);
    std::vector<DWORD> color = RenderLines(lines, width, height, false);

    if (FAILED(device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL)))
        return false;

    D3DLOCKED_RECT locked;
    if (FAILED(texture->LockRect(0, &locked, NULL, 0))) {
        texture->Release();
        texture = nullptr;
        return false;
    }
    for (UINT y = 0; y < height; y++) {
        DWORD *row = reinterpret_cast<DWORD *>(static_cast<BYTE *>(locked.pBits) + y * locked.Pitch);
        for (UINT x = 0; x < width; x++) {
            DWORD m = mask[y * width + x];
            DWORD alpha = (std::max)((std::max)(m & 0xFF, (m >> 8) & 0xFF), (m >> 16) & 0xFF);
            row[x] = (alpha << 24) | (color[y * width + x] & 0x00FFFFFF);
        }
    }
    texture->UnlockRect(0);
    return true;
}

void Overlay::Draw(IDirect3DDevice9 *device)
{
    if (builtVersion != Cheats::Get().Version() && !Rebuild(device))
        return;
    if (empty || texture == nullptr)
        return;

    if (stateBlock == nullptr && FAILED(device->CreateStateBlock(D3DSBT_ALL, &stateBlock)))
        return;
    stateBlock->Capture();

    device->SetPixelShader(NULL);
    device->SetVertexShader(NULL);
    device->SetFVF(VERTEX_FVF);
    device->SetTexture(0, texture);
    device->SetTexture(1, NULL);
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

    // Pixel centers sit at -0.5 in Direct3D 9.
    float left = SCREEN_X - 0.5f;
    float top = SCREEN_Y - 0.5f;
    float right = left + width;
    float bottom = top + height;
    Vertex quad[4] = {
        { left, top, 0.0f, 1.0f, 0.0f, 0.0f },
        { right, top, 0.0f, 1.0f, 1.0f, 0.0f },
        { left, bottom, 0.0f, 1.0f, 0.0f, 1.0f },
        { right, bottom, 0.0f, 1.0f, 1.0f, 1.0f },
    };
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex));

    stateBlock->Apply();
}
