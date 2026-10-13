#include "admin/qr_console.h"
#include <qrcodegen.hpp>
#include <cstdio>

namespace
{
constexpr int QuietZone = 2;
constexpr wchar_t BothLight = L'\x2588';
constexpr wchar_t TopLight = L'\x2580';
constexpr wchar_t BottomLight = L'\x2584';
constexpr wchar_t BothDark = L' ';

wchar_t CellFor(const qrcodegen::QrCode& qr, int x, int y)
{
    const bool topLight = !qr.getModule(x, y);
    const bool bottomLight = !qr.getModule(x, y + 1);
    if (topLight && bottomLight)
    {
        return BothLight;
    }
    if (topLight)
    {
        return TopLight;
    }
    return bottomLight ? BottomLight : BothDark;
}
}

void PrintQrCode(const std::string& text)
{
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int size = qr.getSize();
    for (int y = -QuietZone; y < size + QuietZone; y += 2)
    {
        std::wstring line;
        for (int x = -QuietZone; x < size + QuietZone; ++x)
        {
            line.push_back(CellFor(qr, x, y));
        }
        fwprintf(stdout, L"%s\n", line.c_str());
    }
}
