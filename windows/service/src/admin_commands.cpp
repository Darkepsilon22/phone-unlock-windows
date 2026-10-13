#include "admin_commands.h"
#include "pipe_io.h"
#include "secret_store.h"
#include "service_host.h"
#include <sddl.h>
#include <cstdio>
#include <cwchar>
#include <optional>
#include <string>

using pipe_protocol::MessageType;
using pipe_protocol::PayloadReader;
using pipe_protocol::PayloadWriter;

namespace
{
constexpr DWORD ConnectTimeout = 3000;
constexpr DWORD MaxPasswordLength = 256;
constexpr DWORD MillisecondsPerSecond = 1000;
constexpr int Success = 0;
constexpr int Failure = 1;

std::optional<std::wstring> CurrentUserSid()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return std::nullopt;
    }
    BYTE buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    DWORD size = 0;
    const BOOL read = GetTokenInformation(token, TokenUser, buffer, sizeof(buffer), &size);
    CloseHandle(token);

    LPWSTR text = nullptr;
    if (!read || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer)->User.Sid, &text))
    {
        return std::nullopt;
    }
    std::wstring sid(text);
    LocalFree(text);
    return sid;
}

std::optional<SecureString> ReadHiddenLine()
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (!GetConsoleMode(input, &mode) || !SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT))
    {
        return std::nullopt;
    }

    wchar_t buffer[MaxPasswordLength + 2] = {};
    DWORD read = 0;
    const BOOL ok = ReadConsoleW(input, buffer, MaxPasswordLength + 2, &read, nullptr);
    SetConsoleMode(input, mode);
    fwprintf(stdout, L"\n");

    while (read > 0 && (buffer[read - 1] == L'\r' || buffer[read - 1] == L'\n'))
    {
        --read;
    }
    std::optional<SecureString> line;
    if (ok && read > 0 && read <= MaxPasswordLength)
    {
        line = SecureString(buffer, read);
    }
    SecureZeroMemory(buffer, sizeof(buffer));
    return line;
}

int Send(MessageType type, const PayloadWriter& writer)
{
    HANDLE pipe = pipe_protocol::ConnectToService(pipe_protocol::AdminPipeName, nullptr, ConnectTimeout);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        const DWORD error = GetLastError();
        fwprintf(stderr, error == ERROR_ACCESS_DENIED
            ? L"Accès refusé : lance la commande dans un terminal administrateur.\n"
            : L"Service injoignable (erreur %lu). Est-il démarré ?\n", error);
        return Failure;
    }

    std::optional<pipe_protocol::Message> reply;
    if (pipe_protocol::WriteMessage(pipe, type, writer.Bytes(), nullptr))
    {
        reply = pipe_protocol::ReadMessage(pipe, nullptr);
    }
    CloseHandle(pipe);

    int32_t hr = E_FAIL;
    if (!reply || reply->type != MessageType::Ack || !PayloadReader(reply->payload).ReadInt32(&hr))
    {
        fwprintf(stderr, L"Réponse du service invalide.\n");
        return Failure;
    }
    fwprintf(stdout, SUCCEEDED(hr) ? L"OK\n" : L"Refusé par le service : 0x%08X\n", static_cast<unsigned>(hr));
    return SUCCEEDED(hr) ? Success : Failure;
}

int Provision()
{
    const std::optional<std::wstring> sid = CurrentUserSid();
    if (!sid)
    {
        fwprintf(stderr, L"SID du compte introuvable.\n");
        return Failure;
    }
    fwprintf(stdout, L"Compte %s\nMot de passe du compte (il ne s'affiche pas) : ", sid->c_str());
    const std::optional<SecureString> password = ReadHiddenLine();
    if (!password)
    {
        fwprintf(stderr, L"Mot de passe vide ou trop long.\n");
        return Failure;
    }

    PayloadWriter writer;
    writer.WriteString(*sid);
    writer.WriteString(password->c_str(), password->length());
    return Send(MessageType::Provision, writer);
}

int Authorize(int argc, wchar_t** argv)
{
    const std::optional<std::wstring> sid = CurrentUserSid();
    if (!sid)
    {
        fwprintf(stderr, L"SID du compte introuvable.\n");
        return Failure;
    }

    const DWORD delay = argc >= 2 && wcscmp(argv[0], L"--delay") == 0 ? wcstoul(argv[1], nullptr, 10) : 0;
    if (delay > 0)
    {
        fwprintf(stdout, L"Autorisation dans %lu s : verrouille la session maintenant (Windows + L).\n", delay);
        Sleep(delay * MillisecondsPerSecond);
    }

    PayloadWriter writer;
    writer.WriteString(*sid);
    return Send(MessageType::Authorize, writer);
}

int Usage()
{
    fwprintf(stdout,
        L"Utilisation :\n"
        L"  PhoneUnlockService console               exécute le service au premier plan\n"
        L"  PhoneUnlockService provision             enregistre le mot de passe du compte courant\n"
        L"  PhoneUnlockService authorize [--delay N] simule l'autorisation du téléphone\n");
    return Failure;
}
}

int RunAdminCommand(int argc, wchar_t** argv)
{
    const std::wstring command = argv[0];
    if (command == L"console")
    {
        return RunInConsole();
    }
    if (command == L"provision")
    {
        return Provision();
    }
    if (command == L"authorize")
    {
        return Authorize(argc - 1, argv + 1);
    }
    return Usage();
}
