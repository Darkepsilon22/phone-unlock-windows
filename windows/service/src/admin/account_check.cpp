#include "admin/account_check.h"
#include "admin/account_password.h"
#include "admin/console.h"
#include "admin/current_user.h"
#include <cstdio>

std::optional<ConfirmedAccount> ConfirmAccountPassword()
{
    std::optional<std::wstring> sid = CurrentUserSid();
    if (!sid)
    {
        return std::nullopt;
    }
    const std::optional<LogonName> name = LogonNameFor(*sid);
    if (!name)
    {
        fwprintf(stderr, L"Nom de connexion du compte introuvable.\n");
        return std::nullopt;
    }
    fwprintf(stdout, L"Compte %s\\%s\nMot de passe du compte (il ne s'affiche pas) : ", name->domain.c_str(),
        name->user.c_str());
    std::optional<SecureString> password = ReadHiddenLine();
    if (!password)
    {
        fwprintf(stderr, L"Mot de passe vide ou trop long.\n");
        return std::nullopt;
    }

    const DWORD verification = VerifyAccountPassword(*name, *password);
    if (verification == ERROR_LOGON_FAILURE)
    {
        fwprintf(stderr, L"Mot de passe refusé par Windows : rien n'a été modifié.\n");
        return std::nullopt;
    }
    if (verification != NO_ERROR)
    {
        fwprintf(stderr, L"Vérification impossible (erreur %lu) : rien n'a été modifié.\n", verification);
        return std::nullopt;
    }
    return ConfirmedAccount{ std::move(*sid), std::move(*password) };
}
