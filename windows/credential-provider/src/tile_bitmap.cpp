#include "tile_bitmap.h"

namespace
{
constexpr int TileSize = 128;
constexpr COLORREF BackgroundColor = RGB(0, 103, 192);
constexpr COLORREF PhoneColor = RGB(255, 255, 255);
constexpr RECT PhoneBody = { 40, 14, 89, 115 };
constexpr RECT PhoneScreen = { 46, 26, 83, 96 };
constexpr int PhoneCornerRadius = 16;
constexpr DWORD OpaqueAlpha = 0xFF000000;

HBITMAP CreateTileSurface(HDC screen, DWORD** pixels)
{
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = TileSize;
    info.bmiHeader.biHeight = -TileSize;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(screen, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(pixels), nullptr, 0);
}

void FillWith(HDC canvas, const RECT& area, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    if (brush)
    {
        FillRect(canvas, &area, brush);
        DeleteObject(brush);
    }
}

void DrawPhone(HDC canvas)
{
    HBRUSH brush = CreateSolidBrush(PhoneColor);
    if (!brush)
    {
        return;
    }
    HGDIOBJ previousBrush = SelectObject(canvas, brush);
    HGDIOBJ previousPen = SelectObject(canvas, GetStockObject(NULL_PEN));
    RoundRect(canvas, PhoneBody.left, PhoneBody.top, PhoneBody.right, PhoneBody.bottom,
        PhoneCornerRadius, PhoneCornerRadius);
    SelectObject(canvas, previousPen);
    SelectObject(canvas, previousBrush);
    DeleteObject(brush);
}

void MakeOpaque(DWORD* pixels)
{
    GdiFlush();
    for (int index = 0; index < TileSize * TileSize; ++index)
    {
        pixels[index] |= OpaqueAlpha;
    }
}

void Paint(HDC screen, HBITMAP bitmap, DWORD* pixels)
{
    HDC canvas = CreateCompatibleDC(screen);
    if (!canvas)
    {
        return;
    }
    HGDIOBJ previousBitmap = SelectObject(canvas, bitmap);
    FillWith(canvas, { 0, 0, TileSize, TileSize }, BackgroundColor);
    DrawPhone(canvas);
    FillWith(canvas, PhoneScreen, BackgroundColor);
    MakeOpaque(pixels);
    SelectObject(canvas, previousBitmap);
    DeleteDC(canvas);
}
}

HBITMAP CreateTileBitmap()
{
    HDC screen = GetDC(nullptr);
    if (!screen)
    {
        return nullptr;
    }
    DWORD* pixels = nullptr;
    HBITMAP bitmap = CreateTileSurface(screen, &pixels);
    if (bitmap)
    {
        Paint(screen, bitmap, pixels);
    }
    ReleaseDC(nullptr, screen);
    return bitmap;
}
