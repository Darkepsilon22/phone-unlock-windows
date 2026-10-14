#include <windows.h>
#include "admin/history_command.h"
#include "admin/account_check.h"
#include "admin/console.h"
#include "admin/history_format.h"
#include "admin/service_client.h"
#include "encoding/text.h"
#include "product_paths.h"
#include <cstdio>
#include <cwchar>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

using pipe_protocol::MessageType;
using pipe_protocol::PayloadWriter;

namespace
{
constexpr wchar_t ClearOption[] = L"--clear";
constexpr size_t ShownEntries = 50;
constexpr int DeleteAttempts = 20;
constexpr DWORD DeleteRetryDelay = 50;

std::wstring LogPath(const wchar_t* directory, const wchar_t* file)
{
    wchar_t expanded[MAX_PATH] = {};
    const DWORD length = ExpandEnvironmentStringsW(directory, expanded, MAX_PATH);
    return length > 0 && length <= MAX_PATH ? std::wstring(expanded) + L"\\" + file : std::wstring();
}

std::optional<std::vector<std::wstring>> ReadEntries(const std::wstring& path)
{
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        {
            return std::vector<std::wstring>();
        }
        if (error == ERROR_ACCESS_DENIED)
        {
            fwprintf(stderr, L"Accès refusé : lance la commande dans un terminal administrateur.\n");
        }
        else
        {
            fwprintf(stderr, L"Journal illisible (erreur %lu).\n", error);
        }
        return std::nullopt;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        fwprintf(stderr, L"Journal illisible.\n");
        return std::nullopt;
    }
    std::vector<std::wstring> entries;
    std::string line;
    while (std::getline(file, line))
    {
        if (std::optional<std::wstring> entry = HistoryEntry(WideFromUtf8(line)))
        {
            entries.push_back(std::move(*entry));
        }
    }
    return entries;
}

int ShowHistory()
{
    const std::optional<std::vector<std::wstring>> entries =
        ReadEntries(LogPath(product_paths::LogDirectory, product_paths::ServiceLogFile));
    if (!entries)
    {
        return exit_code::Failure;
    }
    if (entries->empty())
    {
        fwprintf(stdout, L"Aucun historique.\n");
        return exit_code::Success;
    }
    const size_t first = entries->size() > ShownEntries ? entries->size() - ShownEntries : 0;
    if (first > 0)
    {
        fwprintf(stdout, L"%zu événements, voici les %zu plus récents :\n", entries->size(), ShownEntries);
    }
    for (size_t index = first; index < entries->size(); ++index)
    {
        fwprintf(stdout, L"%s\n", (*entries)[index].c_str());
    }
    return exit_code::Success;
}

bool DeleteLog(const std::wstring& path)
{
    DWORD error = ERROR_SUCCESS;
    for (int attempt = 0; attempt < DeleteAttempts; ++attempt)
    {
        if (DeleteFileW(path.c_str()))
        {
            return true;
        }
        error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        {
            return true;
        }
        if (error != ERROR_SHARING_VIOLATION)
        {
            break;
        }
        Sleep(DeleteRetryDelay);
    }
    fwprintf(stderr, L"Impossible d'effacer %s (erreur %lu).\n", path.c_str(), error);
    return false;
}

int ClearHistory()
{
    if (!ConfirmAccountPassword())
    {
        return exit_code::Failure;
    }
    if (!AskYesNo(L"Effacer l'historique et les journaux du service et de la tuile ?"))
    {
        fwprintf(stdout, L"Annulé.\n");
        return exit_code::Success;
    }
    const bool serviceLog = DeleteLog(LogPath(product_paths::LogDirectory, product_paths::ServiceLogFile));
    const bool consoleLog = DeleteLog(LogPath(product_paths::LogDirectory, product_paths::ConsoleLogFile));
    const bool tileLog = DeleteLog(LogPath(product_paths::TileLogDirectory, product_paths::TileLogFile));
    if (!serviceLog || !consoleLog || !tileLog)
    {
        return exit_code::Failure;
    }
    return SendAndReport(MessageType::ClearHistory, PayloadWriter());
}
}

int RunHistoryCommand(int argc, wchar_t** argv)
{
    if (argc == 0)
    {
        return ShowHistory();
    }
    if (argc == 1 && wcscmp(argv[0], ClearOption) == 0)
    {
        return ClearHistory();
    }
    fwprintf(stderr, L"Utilisation : PhoneUnlockService history [--clear]\n");
    return exit_code::Failure;
}
