#include "admin/history_format.h"
#include "test_runner.h"

namespace
{
std::wstring Line(const std::wstring& event)
{
    return L"2026-10-14 16:05:30.092 pid=624 tid=8304 history " + event + L"\r";
}

TEST(OrdinaryLogLinesAreNotHistory)
{
    CHECK(!HistoryEntry(L"2026-10-14 16:05:30.092 pid=624 tid=8304 https GET /v1/state certificate=1"));
    CHECK(!HistoryEntry(L"history unlock authorized phone=Pixel 5"));
    CHECK(!HistoryEntry(L""));
}

TEST(SuccessfulUnlockReadsAsOpenedSession)
{
    CHECK(*HistoryEntry(Line(L"unlock result phone=Pixel 5 status=0x00000000")) ==
        L"14/10/2026 16:05:30  Session ouverte, téléphone Pixel 5");
}

TEST(WindowsRefusalKeepsTheStatus)
{
    CHECK(*HistoryEntry(Line(L"unlock result phone=Pixel 5 status=0xC000006D")) ==
        L"14/10/2026 16:05:30  Refusée par Windows (0xC000006D), téléphone Pixel 5");
}

TEST(RequestsAndRefusalsAreDescribed)
{
    CHECK(*HistoryEntry(Line(L"unlock requested by phone=Pixel 5")) ==
        L"14/10/2026 16:05:30  Demande depuis le téléphone Pixel 5");
    CHECK(*HistoryEntry(Line(L"unlock requested from tile for phone=Pixel 5")) ==
        L"14/10/2026 16:05:30  Demande depuis la tuile, téléphone Pixel 5");
    CHECK(*HistoryEntry(Line(L"unlock authorized phone=Pixel 5")) ==
        L"14/10/2026 16:05:30  Autorisée sur le téléphone Pixel 5");
    CHECK(*HistoryEntry(Line(L"unlock refused phone=Pixel 5 reason=EXPIRED")) ==
        L"14/10/2026 16:05:30  Refusée (EXPIRED), téléphone Pixel 5");
}

TEST(PairingRevocationAndClearingAreDescribed)
{
    CHECK(*HistoryEntry(Line(L"phone paired phone=Pixel 5")) == L"14/10/2026 16:05:30  Téléphone associé : Pixel 5");
    CHECK(*HistoryEntry(Line(L"phone revoked hr=0x00000000")) == L"14/10/2026 16:05:30  Téléphone retiré");
    CHECK(*HistoryEntry(Line(L"phone revoked hr=0x80070490")) ==
        L"14/10/2026 16:05:30  Retrait impossible (0x80070490)");
    CHECK(*HistoryEntry(Line(L"log cleared")) == L"14/10/2026 16:05:30  Historique effacé");
}

TEST(UnknownEventsAreShownAsWritten)
{
    CHECK(*HistoryEntry(Line(L"something new")) == L"14/10/2026 16:05:30  something new");
}
}
