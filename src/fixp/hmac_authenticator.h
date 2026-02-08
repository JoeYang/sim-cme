#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <string>

// Try to use OpenSSL if available
#if __has_include(<openssl/hmac.h>)
#define CME_SIM_HAS_OPENSSL 1
#include <openssl/hmac.h>
#include <openssl/evp.h>
#else
#define CME_SIM_HAS_OPENSSL 0
#endif

namespace cme::sim::fixp {

class HmacAuthenticator {
public:
    // Compute HMAC-SHA256 of (data, len) using the given key.
    static std::array<char, 32> computeHmac(
        const std::string& key,
        const char* data, size_t len)
    {
        std::array<char, 32> result{};
#if CME_SIM_HAS_OPENSSL
        unsigned int out_len = 0;
        unsigned char* digest = HMAC(
            EVP_sha256(),
            key.data(), static_cast<int>(key.size()),
            reinterpret_cast<const unsigned char*>(data), len,
            reinterpret_cast<unsigned char*>(result.data()), &out_len);
        if (!digest) {
            // HMAC failed -- leave result zeroed
            result.fill(0);
        }
#else
        // Stub: fill with zeros when OpenSSL is not available
        (void)key; (void)data; (void)len;
        result.fill(0);
#endif
        return result;
    }

    // Verify that expected_signature matches the HMAC of (data, len).
    // If OpenSSL is not available, always returns true (bypass for testing).
    static bool verify(
        const std::string& key,
        const char* data, size_t len,
        const char* expected_signature)
    {
#if CME_SIM_HAS_OPENSSL
        auto computed = computeHmac(key, data, len);
        // Constant-time comparison to avoid timing attacks
        unsigned char diff = 0;
        for (size_t i = 0; i < 32; ++i) {
            diff |= static_cast<unsigned char>(computed[i]) ^
                     static_cast<unsigned char>(expected_signature[i]);
        }
        return diff == 0;
#else
        (void)key; (void)data; (void)len; (void)expected_signature;
        return true; // always pass when OpenSSL unavailable
#endif
    }
};

} // namespace cme::sim::fixp
