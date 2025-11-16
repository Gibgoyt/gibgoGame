/*
 * gibgoCraft Custom Math Library
 *
 * Vector/Matrix math using ONLY custom types.
 * Complete control over memory layout and operations.
 */

#ifndef GIBGOCRAFT_MATH_H
#define GIBGOCRAFT_MATH_H

#include "types.h"
#include <math.h>  // For sqrtf function

// =============================================================================
// VECTOR TYPES - Explicit memory layout for GPU compatibility
// =============================================================================

// 2D Vector - 8 bytes, natural alignment
typedef struct GIBGO_ALIGN_8 {
    f32 x, y;
} Vec2f;
_Static_assert(sizeof(Vec2f) == 8, "Vec2f must be exactly 8 bytes");

// 3D Vector - 16 bytes with explicit padding for GPU alignment
typedef struct GIBGO_ALIGN_16 {
    f32 x, y, z;
    f32 _padding;    // Explicit padding to 16 bytes for GPU efficiency
} Vec3f;
_Static_assert(sizeof(Vec3f) == 16, "Vec3f must be exactly 16 bytes");

// 4D Vector - 16 bytes, perfect for SIMD operations
typedef struct GIBGO_ALIGN_16 {
    f32 x, y, z, w;
} Vec4f;
_Static_assert(sizeof(Vec4f) == 16, "Vec4f must be exactly 16 bytes");

// =============================================================================
// MATRIX TYPES - GPU-optimized layout
// =============================================================================

// 2x2 Matrix - column major for GPU compatibility
typedef struct GIBGO_ALIGN_16 {
    Vec2f cols[2];   // 2 columns, each a Vec2f
} Mat2f;
_Static_assert(sizeof(Mat2f) == 16, "Mat2f must be exactly 16 bytes");

// 3x3 Matrix - padded columns for efficient GPU access
typedef struct GIBGO_ALIGN_16 {
    Vec3f cols[3];   // 3 columns, each a Vec3f (with padding)
} Mat3f;
_Static_assert(sizeof(Mat3f) == 48, "Mat3f must be exactly 48 bytes");

// 4x4 Matrix - optimal for 3D graphics transformations
typedef struct GIBGO_ALIGN_16 {
    Vec4f cols[4];   // 4 columns, each a Vec4f
} Mat4f;
_Static_assert(sizeof(Mat4f) == 64, "Mat4f must be exactly 64 bytes");

// =============================================================================
// F32 ARITHMETIC - Core floating point operations
// =============================================================================

// Basic arithmetic (using native float for now, can be replaced with manual IEEE 754)
static inline f32 f32_add(f32 a, f32 b) {
    float native_a = f32_to_native(a);
    float native_b = f32_to_native(b);
    return f32_from_native(native_a + native_b);
}

static inline f32 f32_sub(f32 a, f32 b) {
    float native_a = f32_to_native(a);
    float native_b = f32_to_native(b);
    return f32_from_native(native_a - native_b);
}

static inline f32 f32_mul(f32 a, f32 b) {
    float native_a = f32_to_native(a);
    float native_b = f32_to_native(b);
    return f32_from_native(native_a * native_b);
}

static inline f32 f32_div(f32 a, f32 b) {
    float native_a = f32_to_native(a);
    float native_b = f32_to_native(b);
    return f32_from_native(native_a / native_b);
}

static inline f32 f32_neg(f32 a) {
    f32 result = a;
    result.ieee754.sign = !result.ieee754.sign;  // Flip sign bit
    return result;
}

static inline f32 f32_abs(f32 a) {
    f32 result = a;
    result.ieee754.sign = 0;  // Clear sign bit
    return result;
}

// Comparison operations
static inline bool f32_eq(f32 a, f32 b) {
    return a.bits == b.bits;
}

static inline bool f32_lt(f32 a, f32 b) {
    float native_a = f32_to_native(a);
    float native_b = f32_to_native(b);
    return native_a < native_b;
}

static inline bool f32_gt(f32 a, f32 b) {
    float native_a = f32_to_native(a);
    float native_b = f32_to_native(b);
    return native_a > native_b;
}

// =============================================================================
// VEC2F OPERATIONS
// =============================================================================

static inline Vec2f vec2f_create(f32 x, f32 y) {
    Vec2f result = {x, y};
    return result;
}

static inline Vec2f vec2f_zero(void) {
    return vec2f_create(F32_ZERO, F32_ZERO);
}

static inline Vec2f vec2f_one(void) {
    return vec2f_create(F32_ONE, F32_ONE);
}

static inline Vec2f vec2f_add(Vec2f a, Vec2f b) {
    return vec2f_create(
        f32_add(a.x, b.x),
        f32_add(a.y, b.y)
    );
}

static inline Vec2f vec2f_sub(Vec2f a, Vec2f b) {
    return vec2f_create(
        f32_sub(a.x, b.x),
        f32_sub(a.y, b.y)
    );
}

static inline Vec2f vec2f_mul_scalar(Vec2f v, f32 scalar) {
    return vec2f_create(
        f32_mul(v.x, scalar),
        f32_mul(v.y, scalar)
    );
}

static inline f32 vec2f_dot(Vec2f a, Vec2f b) {
    return f32_add(
        f32_mul(a.x, b.x),
        f32_mul(a.y, b.y)
    );
}

static inline f32 vec2f_length_squared(Vec2f v) {
    return vec2f_dot(v, v);
}

// =============================================================================
// VEC3F OPERATIONS
// =============================================================================

static inline Vec3f vec3f_create(f32 x, f32 y, f32 z) {
    Vec3f result = {x, y, z, F32_ZERO};  // Zero padding
    return result;
}

static inline Vec3f vec3f_zero(void) {
    return vec3f_create(F32_ZERO, F32_ZERO, F32_ZERO);
}

static inline Vec3f vec3f_one(void) {
    return vec3f_create(F32_ONE, F32_ONE, F32_ONE);
}

static inline Vec3f vec3f_add(Vec3f a, Vec3f b) {
    return vec3f_create(
        f32_add(a.x, b.x),
        f32_add(a.y, b.y),
        f32_add(a.z, b.z)
    );
}

static inline Vec3f vec3f_sub(Vec3f a, Vec3f b) {
    return vec3f_create(
        f32_sub(a.x, b.x),
        f32_sub(a.y, b.y),
        f32_sub(a.z, b.z)
    );
}

static inline Vec3f vec3f_mul_scalar(Vec3f v, f32 scalar) {
    return vec3f_create(
        f32_mul(v.x, scalar),
        f32_mul(v.y, scalar),
        f32_mul(v.z, scalar)
    );
}

static inline f32 vec3f_dot(Vec3f a, Vec3f b) {
    return f32_add(
        f32_add(f32_mul(a.x, b.x), f32_mul(a.y, b.y)),
        f32_mul(a.z, b.z)
    );
}

static inline Vec3f vec3f_cross(Vec3f a, Vec3f b) {
    return vec3f_create(
        f32_sub(f32_mul(a.y, b.z), f32_mul(a.z, b.y)),
        f32_sub(f32_mul(a.z, b.x), f32_mul(a.x, b.z)),
        f32_sub(f32_mul(a.x, b.y), f32_mul(a.y, b.x))
    );
}

static inline f32 vec3f_length_squared(Vec3f v) {
    return vec3f_dot(v, v);
}

// =============================================================================
// VEC4F OPERATIONS
// =============================================================================

static inline Vec4f vec4f_create(f32 x, f32 y, f32 z, f32 w) {
    Vec4f result = {x, y, z, w};
    return result;
}

static inline Vec4f vec4f_zero(void) {
    return vec4f_create(F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO);
}

static inline Vec4f vec4f_add(Vec4f a, Vec4f b) {
    return vec4f_create(
        f32_add(a.x, b.x),
        f32_add(a.y, b.y),
        f32_add(a.z, b.z),
        f32_add(a.w, b.w)
    );
}

static inline Vec4f vec4f_sub(Vec4f a, Vec4f b) {
    return vec4f_create(
        f32_sub(a.x, b.x),
        f32_sub(a.y, b.y),
        f32_sub(a.z, b.z),
        f32_sub(a.w, b.w)
    );
}

static inline Vec4f vec4f_mul_scalar(Vec4f v, f32 scalar) {
    return vec4f_create(
        f32_mul(v.x, scalar),
        f32_mul(v.y, scalar),
        f32_mul(v.z, scalar),
        f32_mul(v.w, scalar)
    );
}

static inline f32 vec4f_dot(Vec4f a, Vec4f b) {
    return f32_add(
        f32_add(f32_mul(a.x, b.x), f32_mul(a.y, b.y)),
        f32_add(f32_mul(a.z, b.z), f32_mul(a.w, b.w))
    );
}

// =============================================================================
// CONVERSION UTILITIES
// =============================================================================

// Convert Vec3f to Vec4f (common for homogeneous coordinates)
static inline Vec4f vec3f_to_vec4f(Vec3f v, f32 w) {
    return vec4f_create(v.x, v.y, v.z, w);
}

// Convert Vec2f to Vec3f
static inline Vec3f vec2f_to_vec3f(Vec2f v, f32 z) {
    return vec3f_create(v.x, v.y, z);
}

// Extract Vec3f from Vec4f
static inline Vec3f vec4f_to_vec3f(Vec4f v) {
    return vec3f_create(v.x, v.y, v.z);
}

// Extract Vec2f from Vec3f
static inline Vec2f vec3f_to_vec2f(Vec3f v) {
    return vec2f_create(v.x, v.y);
}

// =============================================================================
// SPECIALIZED GRAPHICS OPERATIONS
// =============================================================================

// Color operations (treating Vec3f as RGB, Vec4f as RGBA)
static inline Vec3f rgb_create(f32 r, f32 g, f32 b) {
    return vec3f_create(r, g, b);
}

static inline Vec4f rgba_create(f32 r, f32 g, f32 b, f32 a) {
    return vec4f_create(r, g, b, a);
}

// Common RGB colors
#define RGB_RED    rgb_create(F32_ONE, F32_ZERO, F32_ZERO)
#define RGB_GREEN  rgb_create(F32_ZERO, F32_ONE, F32_ZERO)
#define RGB_BLUE   rgb_create(F32_ZERO, F32_ZERO, F32_ONE)
#define RGB_WHITE  rgb_create(F32_ONE, F32_ONE, F32_ONE)
#define RGB_BLACK  rgb_create(F32_ZERO, F32_ZERO, F32_ZERO)

// Common RGBA colors
#define RGBA_RED      rgba_create(F32_ONE, F32_ZERO, F32_ZERO, F32_ONE)
#define RGBA_GREEN    rgba_create(F32_ZERO, F32_ONE, F32_ZERO, F32_ONE)
#define RGBA_BLUE     rgba_create(F32_ZERO, F32_ZERO, F32_ONE, F32_ONE)
#define RGBA_WHITE    rgba_create(F32_ONE, F32_ONE, F32_ONE, F32_ONE)
#define RGBA_BLACK    rgba_create(F32_ZERO, F32_ZERO, F32_ZERO, F32_ONE)
#define RGBA_CLEAR    rgba_create(F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO)

// =============================================================================
// SIMD OPTIMIZATIONS (when available)
// =============================================================================

#if defined(__SSE__) && defined(__x86_64__)
#include <xmmintrin.h>

// SIMD-optimized Vec4f operations
static inline Vec4f vec4f_add_simd(Vec4f a, Vec4f b) {
    __m128 va = _mm_load_ps((float*)&a);
    __m128 vb = _mm_load_ps((float*)&b);
    __m128 result = _mm_add_ps(va, vb);

    Vec4f output;
    _mm_store_ps((float*)&output, result);
    return output;
}

static inline Vec4f vec4f_mul_scalar_simd(Vec4f v, f32 scalar) {
    __m128 vv = _mm_load_ps((float*)&v);
    __m128 vs = _mm_set1_ps(f32_to_native(scalar));
    __m128 result = _mm_mul_ps(vv, vs);

    Vec4f output;
    _mm_store_ps((float*)&output, result);
    return output;
}

// Use SIMD versions when available
#define vec4f_add_fast      vec4f_add_simd
#define vec4f_mul_scalar_fast vec4f_mul_scalar_simd
#else
// Fallback to scalar versions
#define vec4f_add_fast      vec4f_add
#define vec4f_mul_scalar_fast vec4f_mul_scalar
#endif

// =============================================================================
// DEBUG UTILITIES
// =============================================================================

#ifdef DEBUG
void vec2f_debug_print(Vec2f v);
void vec3f_debug_print(Vec3f v);
void vec4f_debug_print(Vec4f v);
#else
#define vec2f_debug_print(v) ((void)0)
#define vec3f_debug_print(v) ((void)0)
#define vec4f_debug_print(v) ((void)0)
#endif

// =============================================================================
// MAT4F OPERATIONS - Essential for 3D graphics transformations
// =============================================================================

// Create identity matrix
static inline Mat4f mat4f_identity(void) {
    Mat4f result = {
        .cols = {
            {F32_ONE,  F32_ZERO, F32_ZERO, F32_ZERO},  // Column 0: (1,0,0,0)
            {F32_ZERO, F32_ONE,  F32_ZERO, F32_ZERO},  // Column 1: (0,1,0,0)
            {F32_ZERO, F32_ZERO, F32_ONE,  F32_ZERO},  // Column 2: (0,0,1,0)
            {F32_ZERO, F32_ZERO, F32_ZERO, F32_ONE}    // Column 3: (0,0,0,1)
        }
    };
    return result;
}

// Matrix multiplication: C = A * B (column-major order)
static inline Mat4f mat4f_multiply(const Mat4f* a, const Mat4f* b) {
    Mat4f result;

    // For each column of the result
    for (int col = 0; col < 4; col++) {
        // For each row of the result
        for (int row = 0; row < 4; row++) {
            f32 sum = F32_ZERO;

            // Dot product of row 'row' of A with column 'col' of B
            for (int k = 0; k < 4; k++) {
                f32 a_elem = ((f32*)&a->cols[k])[row];  // A[row][k]
                f32 b_elem = ((f32*)&b->cols[col])[k];  // B[k][col]
                sum = f32_add(sum, f32_mul(a_elem, b_elem));
            }

            ((f32*)&result.cols[col])[row] = sum;  // result[row][col] = sum
        }
    }

    return result;
}

// Transform a Vec4f by a Mat4f (matrix-vector multiplication)
static inline Vec4f mat4f_mul_vec4f(const Mat4f* m, Vec4f v) {
    return vec4f_create(
        f32_add(f32_add(f32_mul(m->cols[0].x, v.x), f32_mul(m->cols[1].x, v.y)),
                f32_add(f32_mul(m->cols[2].x, v.z), f32_mul(m->cols[3].x, v.w))),
        f32_add(f32_add(f32_mul(m->cols[0].y, v.x), f32_mul(m->cols[1].y, v.y)),
                f32_add(f32_mul(m->cols[2].y, v.z), f32_mul(m->cols[3].y, v.w))),
        f32_add(f32_add(f32_mul(m->cols[0].z, v.x), f32_mul(m->cols[1].z, v.y)),
                f32_add(f32_mul(m->cols[2].z, v.z), f32_mul(m->cols[3].z, v.w))),
        f32_add(f32_add(f32_mul(m->cols[0].w, v.x), f32_mul(m->cols[1].w, v.y)),
                f32_add(f32_mul(m->cols[2].w, v.z), f32_mul(m->cols[3].w, v.w)))
    );
}

// Translation matrix
static inline Mat4f mat4f_translate(f32 x, f32 y, f32 z) {
    Mat4f result = mat4f_identity();
    result.cols[3].x = x;  // Translation X
    result.cols[3].y = y;  // Translation Y
    result.cols[3].z = z;  // Translation Z
    return result;
}

// Scale matrix
static inline Mat4f mat4f_scale(f32 x, f32 y, f32 z) {
    Mat4f result = mat4f_identity();
    result.cols[0].x = x;  // Scale X
    result.cols[1].y = y;  // Scale Y
    result.cols[2].z = z;  // Scale Z
    return result;
}

// Trigonometric functions (using accurate standard library functions)
static inline f32 f32_sin(f32 radians) {
    // Use accurate standard library function instead of Taylor series approximation
    // The previous Taylor series approximation was causing extreme inaccuracies at large angles,
    // leading to degenerate MVP matrices and blank frames at specific rotation angles
    float x = f32_to_native(radians);
    float result = sinf(x);
    return f32_from_native(result);
}

static inline f32 f32_cos(f32 radians) {
    // Use accurate standard library function instead of Taylor series approximation
    // The previous Taylor series approximation was causing extreme inaccuracies at large angles,
    // leading to degenerate MVP matrices and blank frames at specific rotation angles
    float x = f32_to_native(radians);
    float result = cosf(x);
    return f32_from_native(result);
}

// Rotation matrix around Y axis (for cube spinning)
static inline Mat4f mat4f_rotate_y(f32 radians) {
    f32 cos_theta = f32_cos(radians);
    f32 sin_theta = f32_sin(radians);

    Mat4f result = mat4f_identity();
    result.cols[0].x = cos_theta;   // [0][0] =  cos(θ)
    result.cols[0].z = sin_theta;   // [0][2] =  sin(θ)
    result.cols[2].x = f32_neg(sin_theta); // [2][0] = -sin(θ)
    result.cols[2].z = cos_theta;   // [2][2] =  cos(θ)
    return result;
}

// Perspective projection matrix (field of view in radians)
static inline Mat4f mat4f_perspective(f32 fov_radians, f32 aspect_ratio, f32 near_plane, f32 far_plane) {
    f32 tan_half_fov = f32_div(f32_sin(f32_div(fov_radians, f32_from_native(2.0f))),
                               f32_cos(f32_div(fov_radians, f32_from_native(2.0f))));

    f32 range = f32_sub(far_plane, near_plane);

    Mat4f result = {.cols = {{F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                     {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                     {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                     {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO}}};

    result.cols[0].x = f32_div(F32_ONE, f32_mul(aspect_ratio, tan_half_fov));  // [0][0] = 1/(ar*tan(fov/2))
    result.cols[1].y = f32_div(F32_ONE, tan_half_fov);                         // [1][1] = 1/tan(fov/2)
    result.cols[2].z = f32_neg(f32_div(f32_add(far_plane, near_plane), range)); // [2][2] = -(f+n)/(f-n)
    result.cols[2].w = f32_from_native(-1.0f);                                 // [2][3] = -1 (perspective divide)
    result.cols[3].z = f32_neg(f32_div(f32_mul(f32_from_native(2.0f), f32_mul(far_plane, near_plane)), range)); // [3][2] = -2*f*n/(f-n)

    return result;
}

// Look-at view matrix (camera transformation)
static inline Mat4f mat4f_look_at(Vec3f eye, Vec3f center, Vec3f up) {
    Vec3f f = vec3f_sub(center, eye);  // Forward vector (unnormalized)
    Vec3f s = vec3f_cross(f, up);      // Side vector (unnormalized)
    Vec3f u = vec3f_cross(s, f);       // Up vector (unnormalized)

    // Normalize vectors (simplified normalization)
    f32 f_len = f32_from_native(sqrtf(f32_to_native(vec3f_length_squared(f))));
    f32 s_len = f32_from_native(sqrtf(f32_to_native(vec3f_length_squared(s))));
    f32 u_len = f32_from_native(sqrtf(f32_to_native(vec3f_length_squared(u))));

    f = vec3f_mul_scalar(f, f32_div(F32_ONE, f_len));
    s = vec3f_mul_scalar(s, f32_div(F32_ONE, s_len));
    u = vec3f_mul_scalar(u, f32_div(F32_ONE, u_len));

    Mat4f result = {
        .cols = {
            {s.x, u.x, f32_neg(f.x), F32_ZERO},                                              // Right vector column
            {s.y, u.y, f32_neg(f.y), F32_ZERO},                                              // Up vector column
            {s.z, u.z, f32_neg(f.z), F32_ZERO},                                              // Forward vector column (negated)
            {f32_neg(vec3f_dot(s, eye)), f32_neg(vec3f_dot(u, eye)), f32_neg(vec3f_dot(f, eye)), F32_ONE}  // Translation column
        }
    };

    return result;
}

#endif // GIBGOCRAFT_MATH_H