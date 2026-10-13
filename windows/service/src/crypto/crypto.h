#pragma once

#include <windows.h>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace crypto
{
constexpr size_t Sha256Size = 32;
constexpr size_t EcdsaP256SignatureSize = 64;

using Digest = std::array<uint8_t, Sha256Size>;

std::optional<Digest> Sha256(std::span<const uint8_t> data);

std::optional<std::vector<uint8_t>> RandomBytes(size_t count);

std::optional<std::vector<uint8_t>> CertificateSpki(std::span<const uint8_t> certificateDer);

std::optional<Digest> SpkiFingerprint(std::span<const uint8_t> spkiDer);

bool IsEcdsaP256Spki(std::span<const uint8_t> spkiDer);

bool ConstantTimeEquals(std::span<const uint8_t> a, std::span<const uint8_t> b);

bool VerifyEcdsaP256(std::span<const uint8_t> spkiDer, std::span<const uint8_t> message,
    std::span<const uint8_t> rawSignature);
}
