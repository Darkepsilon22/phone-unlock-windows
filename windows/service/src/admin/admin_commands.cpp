#include "admin/admin_commands.h"
#include "admin/console.h"
#include "admin/current_user.h"
#include "admin/pair_command.h"
#include "admin/service_client.h"
#include "host/service_host.h"
#include <cstdio>
#include <cwchar>
#include <string>

using pipe_protocol::MessageType;
using pipe_protocol::PayloadWriter;

namespace
{
int Provision()
{
    const std::optional<std::wstring> sid = CurrentUserSid();
    if (!sid)
    {
        return exit_code::Failure;
    }
    fwprintf(stdout, L"Compte %s\nMot de passe du compte (il ne s'affiche pas) : ", sid->c_str());
    const std::optional<SecureString> password = ReadHiddenLine();
    if (!password)
    {
        fwprintf(stderr, L"Mot de passe vide ou trop long.\n");
        return exit_code::Failure;
    }

    PayloadWriter request;
    request.WriteString(*sid);
    request.WriteString(password->c_str(), password->length());
    return SendAndReport(MessageType::Provision, request);
}

#ifdef PHONEUNLOCK_TEST_COMMANDS
constexpr DWORD MillisecondsPerSecond = 1000;
constexpr wchar_t DelayOption[] = L"--delay";

int SimulatePhoneAuthorization(int argc, wchar_t** argv)
{
    const std::optional<std::wstring> sid = CurrentUserSid();
    if (!sid)
    {
        return exit_code::Failure;
    }

    const DWORD delay = argc >= 2 && wcscmp(argv[0], DelayOption) == 0 ? wcstoul(argv[1], nullptr, 10) : 0;
    if (delay > 0)
    {
        fwprintf(stdout, L"Autorisation dans %lu s : verrouille la session maintenant (Windows + L).\n", delay);
        Sleep(delay * MillisecondsPerSecond);
    }

    PayloadWriter request;
    request.WriteString(*sid);
    return SendAndReport(MessageType::Authorize, request);
}
#endif

int Usage()
{
    fwprintf(stdout,
        L"Utilisation :\n"
        L"  PhoneUnlockService console                      exécute le service au premier plan\n"
        L"  PhoneUnlockService provision                    enregistre le mot de passe du compte courant\n"
        L"  PhoneUnlockService pair [--endpoint hôte:port]  associe un téléphone au compte courant\n");
#ifdef PHONEUNLOCK_TEST_COMMANDS
    fwprintf(stdout, L"  PhoneUnlockService authorize [--delay N]        (test) simule l'autorisation du téléphone\n");
#endif
    return exit_code::Failure;
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
    if (command == L"pair")
    {
        return RunPairCommand(argc - 1, argv + 1);
    }
#ifdef PHONEUNLOCK_TEST_COMMANDS
    if (command == L"authorize")
    {
        return SimulatePhoneAuthorization(argc - 1, argv + 1);
    }
#endif
    return Usage();
}
