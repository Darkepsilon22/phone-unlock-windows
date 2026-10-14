#include "admin/phones_command.h"
#include "admin/account_check.h"
#include "admin/console.h"
#include "admin/service_client.h"
#include <cstdio>
#include <cwchar>
#include <optional>
#include <string>
#include <vector>

using pipe_protocol::Message;
using pipe_protocol::MessageType;
using pipe_protocol::PayloadReader;
using pipe_protocol::PayloadWriter;

namespace
{
constexpr size_t AnswerLength = 16;

struct Phone
{
    std::wstring pairingId;
    std::wstring accountSid;
    std::wstring name;
    std::wstring createdAt;
};

std::optional<std::vector<Phone>> ListPhones()
{
    const std::optional<Message> reply = ExchangeWithService(MessageType::ListPhones, PayloadWriter());
    if (!reply)
    {
        return std::nullopt;
    }
    PayloadReader reader(reply->payload);
    int32_t count = 0;
    if (!ReadSuccess(reader) || !reader.ReadInt32(&count) || count < 0)
    {
        return std::nullopt;
    }
    std::vector<Phone> phones(static_cast<size_t>(count));
    for (Phone& phone : phones)
    {
        if (!reader.ReadString(&phone.pairingId) || !reader.ReadString(&phone.accountSid) ||
            !reader.ReadString(&phone.name) || !reader.ReadString(&phone.createdAt))
        {
            return std::nullopt;
        }
    }
    return phones;
}

void PrintPhones(const std::vector<Phone>& phones)
{
    if (phones.empty())
    {
        fwprintf(stdout, L"Aucun téléphone associé.\n");
        return;
    }
    for (size_t index = 0; index < phones.size(); ++index)
    {
        const Phone& phone = phones[index];
        fwprintf(stdout, L"%zu. %s, associé le %s, compte %s\n", index + 1, phone.name.c_str(),
            LocalDateTime(phone.createdAt).c_str(), phone.accountSid.c_str());
    }
}

std::optional<size_t> AskNumber(size_t count)
{
    fwprintf(stdout, L"Numéro du téléphone à retirer (Entrée pour annuler) : ");
    wchar_t answer[AnswerLength] = {};
    if (!fgetws(answer, AnswerLength, stdin))
    {
        return std::nullopt;
    }
    wchar_t* end = nullptr;
    const unsigned long number = wcstoul(answer, &end, 10);
    if (end == answer || number == 0 || number > count)
    {
        return std::nullopt;
    }
    return static_cast<size_t>(number - 1);
}
}

int RunPhonesCommand()
{
    const std::optional<std::vector<Phone>> phones = ListPhones();
    if (!phones)
    {
        return exit_code::Failure;
    }
    PrintPhones(*phones);
    return exit_code::Success;
}

int RunRevokeCommand()
{
    if (!ConfirmAccountPassword())
    {
        return exit_code::Failure;
    }
    const std::optional<std::vector<Phone>> phones = ListPhones();
    if (!phones)
    {
        return exit_code::Failure;
    }
    PrintPhones(*phones);
    if (phones->empty())
    {
        return exit_code::Success;
    }

    const std::optional<size_t> choice = AskNumber(phones->size());
    if (!choice)
    {
        fwprintf(stdout, L"Annulé.\n");
        return exit_code::Success;
    }
    const Phone& phone = (*phones)[*choice];
    fwprintf(stdout, L"Ce téléphone ne pourra plus rien demander à ce PC : %s\n", phone.name.c_str());
    if (!AskYesNo(L"Retirer ce téléphone ?"))
    {
        fwprintf(stdout, L"Annulé.\n");
        return exit_code::Success;
    }

    PayloadWriter request;
    request.WriteString(phone.pairingId);
    return SendAndReport(MessageType::RevokePhone, request);
}
