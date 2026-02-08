#pragma once

// Portable byte-order conversion utilities.
// Replaces boost/endian/conversion.hpp for use with standalone Asio builds.

#include <cstdint>
#include <cstring>

namespace cme::sim::endian {

// ---------------------------------------------------------------------------
// Compile-time endianness detection
// ---------------------------------------------------------------------------
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define CME_SIM_LITTLE_ENDIAN 1
    #else
        #define CME_SIM_LITTLE_ENDIAN 0
    #endif
#elif defined(_WIN32) || defined(__x86_64__) || defined(__i386__)
    #define CME_SIM_LITTLE_ENDIAN 1
#else
    #define CME_SIM_LITTLE_ENDIAN 1 // assume LE; most modern platforms
#endif

// ---------------------------------------------------------------------------
// Byte-swap helpers
// ---------------------------------------------------------------------------
inline uint16_t bswap16(uint16_t v) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(v);
#else
    return (v >> 8) | (v << 8);
#endif
}

inline uint32_t bswap32(uint32_t v) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(v);
#else
    return ((v & 0xFF000000u) >> 24)
         | ((v & 0x00FF0000u) >> 8)
         | ((v & 0x0000FF00u) << 8)
         | ((v & 0x000000FFu) << 24);
#endif
}

inline uint64_t bswap64(uint64_t v) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(v);
#else
    return ((v & 0xFF00000000000000ull) >> 56)
         | ((v & 0x00FF000000000000ull) >> 40)
         | ((v & 0x0000FF0000000000ull) >> 24)
         | ((v & 0x000000FF00000000ull) >> 8)
         | ((v & 0x00000000FF000000ull) << 8)
         | ((v & 0x0000000000FF0000ull) << 24)
         | ((v & 0x000000000000FF00ull) << 40)
         | ((v & 0x00000000000000FFull) << 56);
#endif
}

// ---------------------------------------------------------------------------
// Native to big-endian (and vice versa) -- in-place
// ---------------------------------------------------------------------------
inline void native_to_big_inplace(uint16_t& v) {
#if CME_SIM_LITTLE_ENDIAN
    v = bswap16(v);
#endif
}

inline void native_to_big_inplace(uint32_t& v) {
#if CME_SIM_LITTLE_ENDIAN
    v = bswap32(v);
#endif
}

inline void native_to_big_inplace(uint64_t& v) {
#if CME_SIM_LITTLE_ENDIAN
    v = bswap64(v);
#endif
}

inline void big_to_native_inplace(uint16_t& v) {
    native_to_big_inplace(v); // symmetric
}

inline void big_to_native_inplace(uint32_t& v) {
    native_to_big_inplace(v);
}

inline void big_to_native_inplace(uint64_t& v) {
    native_to_big_inplace(v);
}

// ---------------------------------------------------------------------------
// Native to little-endian (value-returning)
// ---------------------------------------------------------------------------
inline uint32_t native_to_little(uint32_t v) {
#if CME_SIM_LITTLE_ENDIAN
    return v; // already LE
#else
    return bswap32(v);
#endif
}

inline uint64_t native_to_little(uint64_t v) {
#if CME_SIM_LITTLE_ENDIAN
    return v;
#else
    return bswap64(v);
#endif
}

} // namespace cme::sim::endian
