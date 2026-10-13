#include "pairing/pc_identity.h"
#include "storage/file_io.h"
#include "log.h"
#include "encoding/text.h"
#include <wincrypt.h>

namespace
{
constexpr wchar_t CertificateSubject[] = L"PhoneUnlock PC";
constexpr wchar_t PcIdFile[] = L"\\pc_id.bin";
constexpr size_t PcIdSize = 16;
constexpr size_t MaxNameCharacters = 64;

std::optional<cbor::Bytes> LoadOrCreatePcId(const std::wstring& stateDirectory)
{
    const std::wstring path = stateDirectory + PcIdFile;
    std::vector<uint8_t> stored;
    if (SUCCEEDED(ReadSmallFile(path, PcIdSize, &stored)) && stored.size() == PcIdSize)
    {
        return stored;
    }
    std::optional<std::vector<uint8_t>> created = crypto::RandomBytes(PcIdSize);
    if (!created || FAILED(WriteFileAtomically(path, *created)))
    {
        return std::nullopt;
    }
    return created;
}

std::optional<crypto::Digest> CertificateFingerprint()
{
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
        CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG, L"MY");
    if (!store)
    {
        return std::nullopt;
    }
    PCCERT_CONTEXT certificate = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR_W,
        CertificateSubject, nullptr);
    std::optional<crypto::Digest> fingerprint;
    if (certificate)
    {
        const std::optional<std::vector<uint8_t>> spki =
            crypto::CertificateSpki({ certificate->pbCertEncoded, certificate->cbCertEncoded });
        fingerprint = spki ? crypto::SpkiFingerprint(*spki) : std::nullopt;
        CertFreeCertificateContext(certificate);
    }
    CertCloseStore(store, 0);
    return fingerprint;
}

std::string ComputerName()
{
    wchar_t name[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    return GetComputerNameW(name, &size) ? SanitizedDisplayName(Utf8FromWide(name), MaxNameCharacters) : "PC";
}
}

std::optional<PcIdentity> LoadPcIdentity(const std::wstring& stateDirectory)
{
    std::optional<cbor::Bytes> pcId = LoadOrCreatePcId(stateDirectory);
    std::optional<crypto::Digest> fingerprint = CertificateFingerprint();
    if (!pcId || !fingerprint)
    {
        LogLine(L"pc identity unavailable id=%d certificate=%d", pcId.has_value(), fingerprint.has_value());
        return std::nullopt;
    }
    return PcIdentity{ std::move(*pcId), *fingerprint, ComputerName() };
}
