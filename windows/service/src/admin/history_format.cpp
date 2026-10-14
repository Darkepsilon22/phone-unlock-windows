#include "admin/history_format.h"
#include <string_view>

namespace
{
constexpr std::wstring_view Marker = L" history ";
constexpr std::wstring_view Success = L"0x00000000";
constexpr size_t TimestampLength = 19;

bool TakePrefix(std::wstring_view text, std::wstring_view prefix, std::wstring* rest)
{
    if (!text.starts_with(prefix))
    {
        return false;
    }
    *rest = text.substr(prefix.size());
    return true;
}

bool SplitAtLast(std::wstring_view text, std::wstring_view separator, std::wstring* before, std::wstring* after)
{
    const size_t position = text.rfind(separator);
    if (position == std::wstring_view::npos)
    {
        return false;
    }
    *before = text.substr(0, position);
    *after = text.substr(position + separator.size());
    return true;
}

bool HasTimestamp(std::wstring_view line)
{
    return line.size() >= TimestampLength && line[4] == L'-' && line[7] == L'-' && line[10] == L' ' &&
        line[13] == L':' && line[16] == L':';
}

std::wstring LocalTimestamp(std::wstring_view line)
{
    std::wstring text;
    text.append(line.substr(8, 2)).append(L"/").append(line.substr(5, 2)).append(L"/").append(line.substr(0, 4));
    return text.append(line.substr(10, 9));
}

std::wstring Describe(std::wstring_view event)
{
    std::wstring rest;
    std::wstring name;
    std::wstring detail;
    if (TakePrefix(event, L"unlock requested from tile for phone=", &rest))
    {
        return L"Demande depuis la tuile, téléphone " + rest;
    }
    if (TakePrefix(event, L"unlock requested by phone=", &rest))
    {
        return L"Demande depuis le téléphone " + rest;
    }
    if (TakePrefix(event, L"unlock authorized phone=", &rest))
    {
        return L"Autorisée sur le téléphone " + rest;
    }
    if (TakePrefix(event, L"unlock refused phone=", &rest) && SplitAtLast(rest, L" reason=", &name, &detail))
    {
        return L"Refusée (" + detail + L"), téléphone " + name;
    }
    if (TakePrefix(event, L"unlock result phone=", &rest) && SplitAtLast(rest, L" status=", &name, &detail))
    {
        return detail == Success ? L"Session ouverte, téléphone " + name
                                 : L"Refusée par Windows (" + detail + L"), téléphone " + name;
    }
    if (TakePrefix(event, L"phone paired phone=", &rest))
    {
        return L"Téléphone associé : " + rest;
    }
    if (TakePrefix(event, L"phone revoked hr=", &rest))
    {
        return rest == Success ? std::wstring(L"Téléphone retiré") : L"Retrait impossible (" + rest + L")";
    }
    if (event == L"log cleared")
    {
        return L"Historique effacé";
    }
    return std::wstring(event);
}
}

std::optional<std::wstring> HistoryEntry(const std::wstring& logLine)
{
    std::wstring_view line = logLine;
    while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n'))
    {
        line.remove_suffix(1);
    }
    const size_t marker = line.find(Marker);
    if (!HasTimestamp(line) || marker == std::wstring_view::npos)
    {
        return std::nullopt;
    }
    return LocalTimestamp(line) + L"  " + Describe(line.substr(marker + Marker.size()));
}
