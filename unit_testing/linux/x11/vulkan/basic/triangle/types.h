/*
 * gibgoCraft Custom Type System
 *
 * ZERO native C types - complete memory sovereignty!
 * Every bit explicitly controlled, platform independent.
 */

#ifndef GIBGOCRAFT_TYPES_H
#define GIBGOCRAFT_TYPES_H

#include <stdint.h>     // Only for uint32_t etc. definitions
#include <stddef.h>     // For size_t in static assertions
#include <stdbool.h>    // For bool in debug builds

// =============================================================================
// CORE INTEGER TYPES - Explicit bit width everywhere
// =============================================================================

typedef uint8_t   u8;
typedef int8_t    i8;
typedef uint16_t  u16;
typedef int16_t   i16;
typedef uint32_t  u32;
typedef int32_t   i32;
typedef uint64_t  u64;
typedef int64_t   i64;

// Compile-time size verification
_Static_assert(sizeof(u8)  == 1, "u8 must be exactly 1 byte");
_Static_assert(sizeof(i8)  == 1, "i8 must be exactly 1 byte");
_Static_assert(sizeof(u16) == 2, "u16 must be exactly 2 bytes");
_Static_assert(sizeof(i16) == 2, "i16 must be exactly 2 bytes");
_Static_assert(sizeof(u32) == 4, "u32 must be exactly 4 bytes");
_Static_assert(sizeof(i32) == 4, "i32 must be exactly 4 bytes");
_Static_assert(sizeof(u64) == 8, "u64 must be exactly 8 bytes");
_Static_assert(sizeof(i64) == 8, "i64 must be exactly 8 bytes");

// =============================================================================
// IEEE 754 FLOATING POINT TYPES - Complete bit-level control
// =============================================================================

// 32-bit IEEE 754 single precision
typedef union {
    u32 bits;               // Raw 32-bit representation

    // IEEE 754 bit fields for direct manipulation
    struct {
        u32 mantissa : 23;  // Fractional part
        u32 exponent : 8;   // Biased exponent (bias = 127)
        u32 sign     : 1;   // Sign bit (0 = positive, 1 = negative)
    } ieee754;

    // Byte access for serialization/endianness
    struct {
        u8 byte0, byte1, byte2, byte3;
    } bytes;
} f32;

// 64-bit IEEE 754 double precision
typedef union {
    u64 bits;               // Raw 64-bit representation

    // IEEE 754 bit fields
    struct {
        u64 mantissa : 52;  // Fractional part
        u64 exponent : 11;  // Biased exponent (bias = 1023)
        u64 sign     : 1;   // Sign bit
    } ieee754;

    // Byte access
    struct {
        u8 byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7;
    } bytes;
} f64;

// Size verification
_Static_assert(sizeof(f32) == 4, "f32 must be exactly 4 bytes");
_Static_assert(sizeof(f64) == 8, "f64 must be exactly 8 bytes");

// =============================================================================
// SPECIAL CONSTANTS
// =============================================================================

// IEEE 754 constants
#define F32_SIGN_MASK        0x80000000U
#define F32_EXPONENT_MASK    0x7F800000U
#define F32_MANTISSA_MASK    0x007FFFFFU
#define F32_EXPONENT_BIAS    127
#define F32_EXPONENT_SHIFT   23

#define F64_SIGN_MASK        0x8000000000000000ULL
#define F64_EXPONENT_MASK    0x7FF0000000000000ULL
#define F64_MANTISSA_MASK    0x000FFFFFFFFFFFFFULL
#define F64_EXPONENT_BIAS    1023
#define F64_EXPONENT_SHIFT   52

// =============================================================================
// F32 CONSTANTS AND UTILITIES
// =============================================================================

// Mathematical constants as f32 - COMPILE-TIME CONSTANTS
#define F32_ZERO        ((f32){.bits = 0x00000000})
#define F32_ONE         ((f32){.bits = 0x3F800000})
#define F32_NEG_ONE     ((f32){.bits = 0xBF800000})
#define F32_PI          ((f32){.bits = 0x40490FDB})  // 3.14159265359
#define F32_E           ((f32){.bits = 0x402DF854})  // 2.71828182846
#define F32_SQRT_2      ((f32){.bits = 0x3FB504F3})  // 1.41421356237

// Static initializer versions for global/static variables
#define F32_ZERO_INIT   {.bits = 0x00000000}
#define F32_ONE_INIT    {.bits = 0x3F800000}
#define F32_NEG_ONE_INIT {.bits = 0xBF800000}

// Special IEEE 754 values
#define F32_INFINITY    ((f32){.bits = 0x7F800000})
#define F32_NEG_INFINITY ((f32){.bits = 0xFF800000})
#define F32_NAN         ((f32){.bits = 0x7FC00000})
#define F32_EPSILON     ((f32){.bits = 0x34000000})  // Smallest representable difference

// =============================================================================
// ENDIANNESS HANDLING
// =============================================================================

// Detect endianness at compile time
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define GIBGO_LITTLE_ENDIAN 1
    #define GIBGO_BIG_ENDIAN 0
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define GIBGO_LITTLE_ENDIAN 0
    #define GIBGO_BIG_ENDIAN 1
#else
    // Runtime detection fallback
    static inline int gibgo_is_little_endian(void) {
        u16 test = 0x0001;
        return *(u8*)&test == 1;
    }
#endif

// Byte swap utilities for cross-platform binary compatibility
static inline u16 u16_swap_bytes(u16 value) {
    return (u16)((value << 8) | (value >> 8));
}

static inline u32 u32_swap_bytes(u32 value) {
    return ((value << 24) & 0xFF000000) |
           ((value << 8)  & 0x00FF0000) |
           ((value >> 8)  & 0x0000FF00) |
           ((value >> 24) & 0x000000FF);
}

static inline u64 u64_swap_bytes(u64 value) {
    return ((value << 56) & 0xFF00000000000000ULL) |
           ((value << 40) & 0x00FF000000000000ULL) |
           ((value << 24) & 0x0000FF0000000000ULL) |
           ((value << 8)  & 0x000000FF00000000ULL) |
           ((value >> 8)  & 0x00000000FF000000ULL) |
           ((value >> 24) & 0x0000000000FF0000ULL) |
           ((value >> 40) & 0x000000000000FF00ULL) |
           ((value >> 56) & 0x00000000000000FFULL);
}

// Force little-endian for network/file formats
static inline u32 u32_to_le(u32 value) {
#if GIBGO_LITTLE_ENDIAN
    return value;
#else
    return u32_swap_bytes(value);
#endif
}

static inline u32 u32_from_le(u32 value) {
#if GIBGO_LITTLE_ENDIAN
    return value;
#else
    return u32_swap_bytes(value);
#endif
}

// =============================================================================
// TYPE CONVERSION UTILITIES
// =============================================================================

// Convert between f32 and native float (when absolutely necessary)
// Use sparingly - defeats the purpose of custom types!
static inline f32 f32_from_native(float native) {
    f32 result;
    // Bit-cast to avoid undefined behavior
    result.bits = *(u32*)&native;
    return result;
}

static inline float f32_to_native(f32 custom) {
    // Bit-cast to avoid undefined behavior
    return *(float*)&custom.bits;
}

// =============================================================================
// MEMORY ALIGNMENT UTILITIES
// =============================================================================

// Platform-specific alignment macros
#if defined(__GNUC__) || defined(__clang__)
    #define GIBGO_ALIGN(n) __attribute__((aligned(n)))
    #define GIBGO_PACKED   __attribute__((packed))
#elif defined(_MSC_VER)
    #define GIBGO_ALIGN(n) __declspec(align(n))
    #define GIBGO_PACKED
#else
    #define GIBGO_ALIGN(n)
    #define GIBGO_PACKED
#endif

// Common alignment requirements for GPU data
#define GIBGO_ALIGN_4   GIBGO_ALIGN(4)   // 32-bit alignment
#define GIBGO_ALIGN_8   GIBGO_ALIGN(8)   // 64-bit alignment
#define GIBGO_ALIGN_16  GIBGO_ALIGN(16)  // SIMD/cache line alignment
#define GIBGO_ALIGN_32  GIBGO_ALIGN(32)  // AVX alignment
#define GIBGO_ALIGN_64  GIBGO_ALIGN(64)  // Cache line alignment

// =============================================================================
// DEBUG UTILITIES
// =============================================================================

#ifdef DEBUG
// Debug printing for custom types
void u32_debug_print(u32 value);
void f32_debug_print(f32 value);
void f32_debug_print_detailed(f32 value);
#else
#define u32_debug_print(x) ((void)0)
#define f32_debug_print(x) ((void)0)
#define f32_debug_print_detailed(x) ((void)0)
#endif

#endif // GIBGOCRAFT_TYPES_H