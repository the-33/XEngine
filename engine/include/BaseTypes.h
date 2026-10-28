#pragma once
/** @file BaseTypes.h
 * @brief Numeric vectors, rectangles, colors and cross-type math helpers.
 */

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <type_traits>
#include <concepts>

/** @namespace math
 * @brief Numeric concepts and scalar/vector geometry utilities.
 * @ingroup xengine_math
 */
namespace math
{
    // -------------------- Concepts --------------------
    template<class T>
    concept Arithmetic = std::integral<T> || std::floating_point<T>;

    template<class T>
    concept Float = std::floating_point<T>;

    template<class T>
    concept Int = std::integral<T>;

    template<class A, class B>
    /** @brief Public type alias for the common2 accessor.
     */
    using Common2 = std::common_type_t<A, B>;

    template<class A, class B, class C>
    /** @brief Public type alias for the common3 accessor.
     */
    using Common3 = std::common_type_t<A, B, C>;

    template<class T>
    /** @brief Returns a small floating-point tolerance, or zero for integral types.
     */
    constexpr T Epsilon() noexcept
    {
        if constexpr (Float<T>) return static_cast<T>(1e-6);
        else return T{ 0 };
    }

    // -------------------- Scalar utils --------------------
    template<Arithmetic T>
    /** @brief Returns the smaller of the two scalar arguments.
     * @param a First input operand.
     * @param b Second input operand.
     */
    constexpr T Min(T a, T b) noexcept { return (a < b) ? a : b; }

    template<Arithmetic T>
    /** @brief Returns the larger of the two scalar arguments.
     * @param a First input operand.
     * @param b Second input operand.
     */
    constexpr T Max(T a, T b) noexcept { return (a > b) ? a : b; }

    template<Arithmetic T>
    /** @brief Returns the nonnegative magnitude of a scalar or vector.
     * @param v Value, vector or input to this operation.
     */
    constexpr T Abs(T v) noexcept { return (v < T{ 0 }) ? -v : v; }

    template<Arithmetic T>
    /** @brief Restricts a scalar or vector to the supplied lower and upper bounds.
     * @param v Value, vector or input to this operation.
     * @param lo Lower permitted boundary.
     * @param hi Upper permitted boundary.
     */
    constexpr T Clamp(T v, T lo, T hi) noexcept { return std::clamp(v, lo, hi); }

    template<Arithmetic T>
    /** @brief Clamps a scalar into the interval from zero to one.
     * @param v Value, vector or input to this operation.
     */
    constexpr T Saturate(T v) noexcept { return Clamp(v, T{ 0 }, T{ 1 }); }

    template<Arithmetic T>
    /** @brief Interpolates between the supplied endpoints by factor t.
     * @param a First input operand.
     * @param b Second input operand.
     * @param t Interpolation factor.
     */
    constexpr T Lerp(T a, T b, T t) noexcept { return a + (b - a) * t; }

    template<Float T>
    /** @brief Compares floating-point values using an absolute tolerance.
     * @param a First input operand.
     * @param b Second input operand.
     * @param eps Numeric tolerance used for this comparison.
     */
    constexpr bool NearlyEqual(T a, T b, T eps = Epsilon<T>()) noexcept
    {
        return Abs(a - b) <= eps;
    }

    template<Float T>
    /** @brief Converts degrees to radians.
     * @param d Distance or angular value.
     */
    constexpr T Deg2Rad(T d) noexcept { return d * (static_cast<T>(3.14159265358979323846) / static_cast<T>(180)); }

    template<Float T>
    /** @brief Converts radians to degrees.
     * @param r Angle in radians.
     */
    constexpr T Rad2Deg(T r) noexcept { return r * (static_cast<T>(180) / static_cast<T>(3.14159265358979323846)); }

    template<Float T>
    /** @brief Normalizes an angle in radians into (-pi, pi].
     * @param a First input operand.
     */
    constexpr T WrapAngleRad(T a) noexcept
    {
        // (-pi, pi]
        const T twoPi = static_cast<T>(6.28318530717958647692);
        const T pi = static_cast<T>(3.14159265358979323846);
        while (a <= -pi) a += twoPi;
        while (a > pi) a -= twoPi;
        return a;
    }

    template<Float T>
    /** @brief Computes cubic smoothstep interpolation between two boundaries.
     * @param edge0 Lower edge of the transition interval.
     * @param edge1 Upper edge of the transition interval.
     * @param x Horizontal coordinate or component.
     */
    constexpr T SmoothStep(T edge0, T edge1, T x) noexcept
    {
        const T t = Saturate((x - edge0) / (edge1 - edge0));
        return t * t * (static_cast<T>(3) - static_cast<T>(2) * t);
    }
} // namespace math

// =====================================================================================
// TVec2
// =====================================================================================
template<math::Arithmetic T>
/** @brief Two-component numeric vector with arithmetic, accessors and geometry helpers.
 * @ingroup xengine_math
 */
struct TVec2
{
    /** @brief X coordinate value. Y coordinate value.
     */
    T x{}, y{};

    /** @brief Constructs a TVec2 instance.
     */
    constexpr TVec2() = default;
    /** @brief Constructs a TVec2 instance.
     * @param X Initial horizontal vector component.
     * @param Y Initial vertical vector component.
     */
    constexpr TVec2(T X, T Y) noexcept : x(X), y(Y) {}

    /** @brief Implements operator[] for this value or property type.
     * @param i Index of the requested component or element.
     */
    constexpr T& operator[](int i) noexcept { return (i == 0) ? x : y; }
    /** @brief Implements operator[] for this value or property type.
     * @param i Index of the requested component or element.
     */
    constexpr const T& operator[](int i) const noexcept { return (i == 0) ? x : y; }

    /** @brief Implements operator- for this value or property type.
     */
    constexpr TVec2 operator-() const noexcept { return { -x, -y }; }

    /** @brief Implements operator+= for this value or property type.
     * @param v Value, vector or input to this operation.
     */
    constexpr TVec2& operator+=(TVec2 v) noexcept { x += v.x; y += v.y; return *this; }
    /** @brief Implements operator-= for this value or property type.
     * @param v Value, vector or input to this operation.
     */
    constexpr TVec2& operator-=(TVec2 v) noexcept { x -= v.x; y -= v.y; return *this; }
    /** @brief Implements operator*= for this value or property type.
     * @param s Scalar multiplier, divisor or input value.
     */
    constexpr TVec2& operator*=(T s) noexcept { x *= s; y *= s; return *this; }
    /** @brief Implements operator/= for this value or property type.
     * @param s Scalar multiplier, divisor or input value.
     */
    constexpr TVec2& operator/=(T s) noexcept { x /= s; y /= s; return *this; }

    /** @brief Returns a vector with both components zero.
     */
    static constexpr TVec2 Zero()  noexcept { return { T{0}, T{0} }; }
    /** @brief Returns a vector with all components equal to one.
     */
    static constexpr TVec2 One()   noexcept { return { T{1}, T{1} }; }
    /** @brief Returns a unit vector along positive X.
     */
    static constexpr TVec2 Right() noexcept { return { T{1}, T{0} }; }
    /** @brief Returns a unit vector along positive Y.
     */
    static constexpr TVec2 Up()    noexcept { return { T{0}, T{1} }; }
};

// ---- Vec2 operators (mixed) ----
/** @brief Adds two 2D vectors component-wise, preserving their common numeric type.
 * @param a First vector.
 * @param b Second vector.
 */
template<math::Arithmetic A, math::Arithmetic B>
    requires (!std::is_same_v<A, bool> && !std::is_same_v<B, bool>)
constexpr auto operator+(TVec2<A> a, TVec2<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec2<R>{ static_cast<R>(a.x) + static_cast<R>(b.x),
        static_cast<R>(a.y) + static_cast<R>(b.y) };
}

/** @brief Subtracts two 2D vectors component-wise.
 * @param a Left operand.
 * @param b Right operand.
 */
template<math::Arithmetic A, math::Arithmetic B>
    requires (!std::is_same_v<A, bool> && !std::is_same_v<B, bool>)
constexpr auto operator-(TVec2<A> a, TVec2<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec2<R>{ static_cast<R>(a.x) - static_cast<R>(b.x),
        static_cast<R>(a.y) - static_cast<R>(b.y) };
}

/** @brief Multiplies both 2D vector components by a scalar.
 * @param v Input vector.
 * @param s Scalar multiplier.
 */
template<math::Arithmetic A, math::Arithmetic S>
    requires (!std::is_same_v<S, bool>)
constexpr auto operator*(TVec2<A> v, S s) noexcept
{
    using R = math::Common2<A, S>;
    return TVec2<R>{ static_cast<R>(v.x)* static_cast<R>(s),
        static_cast<R>(v.y)* static_cast<R>(s) };
}

/** @brief Multiplies a scalar by both 2D vector components.
 * @param s Scalar multiplier.
 * @param v Input vector.
 */
template<math::Arithmetic S, math::Arithmetic A>
    requires (!std::is_same_v<S, bool>)
constexpr auto operator*(S s, TVec2<A> v) noexcept { return v * s; }

/** @brief Divides both 2D vector components by a scalar.
 * @param v Input vector.
 * @param s Scalar divisor; must not be zero.
 */
template<math::Arithmetic A, math::Arithmetic S>
    requires (!std::is_same_v<S, bool>)
constexpr auto operator/(TVec2<A> v, S s) noexcept
{
    using R = math::Common2<A, S>;
    return TVec2<R>{ static_cast<R>(v.x) / static_cast<R>(s),
        static_cast<R>(v.y) / static_cast<R>(s) };
}

// Component-wise mul/div (useful for per-axis scaling)
template<math::Arithmetic A, math::Arithmetic B>
/** @brief Multiplies vector components pairwise.
 * @param a First input operand.
 * @param b Second input operand.
 */
constexpr auto Hadamard(TVec2<A> a, TVec2<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec2<R>{ static_cast<R>(a.x)* static_cast<R>(b.x),
        static_cast<R>(a.y)* static_cast<R>(b.y) };
}

template<math::Arithmetic A, math::Arithmetic B>
/** @brief Implements operator== for this value or property type.
 * @param a First input operand.
 * @param b Second input operand.
 */
constexpr bool operator==(TVec2<A> a, TVec2<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return static_cast<R>(a.x) == static_cast<R>(b.x) &&
        static_cast<R>(a.y) == static_cast<R>(b.y);
}
template<math::Arithmetic A, math::Arithmetic B>
/** @brief Implements operator!= for this value or property type.
 * @param a First input operand.
 * @param b Second input operand.
 */
constexpr bool operator!=(TVec2<A> a, TVec2<B> b) noexcept { return !(a == b); }

// ---- Vec2 functions ----
namespace math
{
    template<Arithmetic A, Arithmetic B>
    /** @brief Computes the dot product of the input vectors.
     * @param a First input operand.
     * @param b Second input operand.
     */
    constexpr auto Dot(TVec2<A> a, TVec2<B> b) noexcept
    {
        using R = Common2<A, B>;
        return static_cast<R>(a.x) * static_cast<R>(b.x) +
            static_cast<R>(a.y) * static_cast<R>(b.y);
    }

    template<Arithmetic T>
    /** @brief Returns the squared Euclidean vector length without taking a square root.
     * @param v Value, vector or input to this operation.
     */
    constexpr auto LengthSq(TVec2<T> v) noexcept { return Dot(v, v); }

    template<Arithmetic T>
    /** @brief Returns the Euclidean vector length.
     * @param v Value, vector or input to this operation.
     */
    inline auto Length(TVec2<T> v) noexcept
    {
        using R = std::conditional_t<Float<T>, T, float>;
        return static_cast<R>(std::sqrt(static_cast<R>(LengthSq(v))));
    }

    template<Arithmetic T>
    /** @brief Returns Euclidean distance between the two points.
     * @param a First input operand.
     * @param b Second input operand.
     */
    inline auto Distance(TVec2<T> a, TVec2<T> b) noexcept { return Length(a - b); }

    template<Arithmetic T>
    /** @brief Returns a vector scaled to unit length where possible.
     * @param v Value, vector or input to this operation.
     */
    inline auto Normalized(TVec2<T> v) noexcept
    {
        using R = std::conditional_t<Float<T>, T, float>;
        const R len = Length(v);
        if (len <= Epsilon<R>()) return TVec2<R>{};
        return TVec2<R>{ static_cast<R>(v.x) / len, static_cast<R>(v.y) / len };
    }

    template<Arithmetic A, Arithmetic B>
    /** @brief Returns the smaller of the two scalar arguments.
     * @param a First input operand.
     * @param b Second input operand.
     */
    constexpr auto Min(TVec2<A> a, TVec2<B> b) noexcept
    {
        using R = Common2<A, B>;
        return TVec2<R>{ math::Min<R>(static_cast<R>(a.x), static_cast<R>(b.x)),
            math::Min<R>(static_cast<R>(a.y), static_cast<R>(b.y)) };
    }

    template<Arithmetic A, Arithmetic B>
    /** @brief Returns the larger of the two scalar arguments.
     * @param a First input operand.
     * @param b Second input operand.
     */
    constexpr auto Max(TVec2<A> a, TVec2<B> b) noexcept
    {
        using R = Common2<A, B>;
        return TVec2<R>{ math::Max<R>(static_cast<R>(a.x), static_cast<R>(b.x)),
            math::Max<R>(static_cast<R>(a.y), static_cast<R>(b.y)) };
    }

    template<Arithmetic V, Arithmetic L, Arithmetic H>
    /** @brief Restricts a scalar or vector to the supplied lower and upper bounds.
     * @param v Value, vector or input to this operation.
     * @param lo Lower permitted boundary.
     * @param hi Upper permitted boundary.
     */
    constexpr auto Clamp(TVec2<V> v, TVec2<L> lo, TVec2<H> hi) noexcept
    {
        using R = Common3<V, L, H>;
        return TVec2<R>{
            math::Clamp<R>(static_cast<R>(v.x), static_cast<R>(lo.x), static_cast<R>(hi.x)),
                math::Clamp<R>(static_cast<R>(v.y), static_cast<R>(lo.y), static_cast<R>(hi.y))
        };
    }

    template<Arithmetic V, Arithmetic S>
    /** @brief Restricts a scalar or vector to the supplied lower and upper bounds.
     * @param v Value, vector or input to this operation.
     * @param lo Lower permitted boundary.
     * @param hi Upper permitted boundary.
     */
    constexpr auto Clamp(TVec2<V> v, S lo, S hi) noexcept
    {
        using R = Common2<V, S>;
        return TVec2<R>{
            math::Clamp<R>(static_cast<R>(v.x), static_cast<R>(lo), static_cast<R>(hi)),
                math::Clamp<R>(static_cast<R>(v.y), static_cast<R>(lo), static_cast<R>(hi))
        };
    }

    template<Arithmetic V, Arithmetic Tt>
    /** @brief Interpolates between the supplied endpoints by factor t.
     * @param a First input operand.
     * @param b Second input operand.
     * @param t Interpolation factor.
     */
    constexpr auto Lerp(TVec2<V> a, TVec2<V> b, Tt t) noexcept
    {
        using R = Common2<V, Tt>;
        return TVec2<R>{
            math::Lerp<R>(static_cast<R>(a.x), static_cast<R>(b.x), static_cast<R>(t)),
                math::Lerp<R>(static_cast<R>(a.y), static_cast<R>(b.y), static_cast<R>(t))
        };
    }

    template<Arithmetic T>
    /** @brief Returns the nonnegative magnitude of a scalar or vector.
     * @param v Value, vector or input to this operation.
     */
    constexpr auto Abs(TVec2<T> v) noexcept
    {
        using R = T;
        return TVec2<R>{ math::Abs<R>(v.x), math::Abs<R>(v.y) };
    }

    template<Float T>
    /** @brief Rounds each component down to the nearest integer.
     * @param v Value, vector or input to this operation.
     */
    inline TVec2<T> Floor(TVec2<T> v) noexcept { return { std::floor(v.x), std::floor(v.y) }; }

    template<Float T>
    /** @brief Rounds each component up to the nearest integer.
     * @param v Value, vector or input to this operation.
     */
    inline TVec2<T> Ceil(TVec2<T> v) noexcept { return { std::ceil(v.x), std::ceil(v.y) }; }

    template<Float T>
    /** @brief Rounds each component to the nearest integer.
     * @param v Value, vector or input to this operation.
     */
    inline TVec2<T> Round(TVec2<T> v) noexcept { return { std::round(v.x), std::round(v.y) }; }

    template<Arithmetic T>
    /** @brief Returns a perpendicular vector rotated by a quarter-turn.
     * @param v Value, vector or input to this operation.
     */
    constexpr auto Perp(TVec2<T> v) noexcept { return TVec2<T>{ -v.y, v.x }; } // 90 degrees counterclockwise

    template<Float T>
    /** @brief Returns the unsigned angle between vectors, in radians.
     * @param a First input operand.
     * @param b Second input operand.
     */
    inline T AngleRad(TVec2<T> a, TVec2<T> b) noexcept
    {
        const T denom = Length(a) * Length(b);
        if (denom <= Epsilon<T>()) return T{ 0 };
        const T c = Clamp(Dot(a, b) / denom, T{ -1 }, T{ 1 });
        return std::acos(c);
    }

    template<Float T>
    /** @brief Returns the signed angle between vectors, in radians.
     * @param a First input operand.
     * @param b Second input operand.
     */
    inline T SignedAngleRad(TVec2<T> a, TVec2<T> b) noexcept
    {
        // sign via 2D cross (scalar)
        const T ang = AngleRad(a, b);
        const T cross = a.x * b.y - a.y * b.x;
        return (cross < T{ 0 }) ? -ang : ang;
    }

    template<Float T>
    /** @brief Returns the vector rotated by the supplied angle.
     * @param v Value, vector or input to this operation.
     * @param angleRad Angle in radians.
     */
    inline TVec2<T> Rotate(TVec2<T> v, T angleRad) noexcept
    {
        const T c = std::cos(angleRad);
        const T s = std::sin(angleRad);
        return { v.x * c - v.y * s, v.x * s + v.y * c };
    }

    template<Float T>
    /** @brief Creates a unit direction vector from an angle in radians.
     * @param angleRad Angle in radians.
     * @param length Requested number of elements or bytes.
     */
    inline TVec2<T> FromAngleRad(T angleRad, T length = T{ 1 }) noexcept
    {
        return { std::cos(angleRad) * length, std::sin(angleRad) * length };
    }

    template<Float T>
    /** @brief Reflects the input vector about the supplied normal.
     * @param v Value, vector or input to this operation.
     * @param nUnit Unit-length normal vector.
     */
    inline TVec2<T> Reflect(TVec2<T> v, TVec2<T> nUnit) noexcept
    {
        // v - 2*dot(v,n)*n
        return v - (T{ 2 } *Dot(v, nUnit)) * nUnit;
    }

    template<Float T>
    /** @brief Projects one vector onto another vector or axis.
     * @param v Value, vector or input to this operation.
     * @param onto Vector or axis onto which the input is projected.
     */
    inline TVec2<T> Project(TVec2<T> v, TVec2<T> onto) noexcept
    {
        const T d = Dot(onto, onto);
        if (d <= Epsilon<T>()) return {};
        return (Dot(v, onto) / d) * onto;
    }

    template<Float T>
    /** @brief Moves a value or vector towards its target without overshooting.
     * @param current Current value before the requested transition.
     * @param target Target point, vector or transform for this operation.
     * @param maxDelta Largest allowed movement or change per update.
     */
    inline TVec2<T> MoveTowards(TVec2<T> current, TVec2<T> target, T maxDelta) noexcept
    {
        const auto d = target - current;
        const T len = Length(d);
        if (len <= maxDelta || len <= Epsilon<T>()) return target;
        return current + (d / len) * maxDelta;
    }
} // namespace math

// =====================================================================================
// TVec3
// =====================================================================================
template<math::Arithmetic T>
/** @brief Three-component numeric vector with arithmetic and spatial helper functions.
 * @ingroup xengine_math
 */
struct TVec3
{
    /** @brief X coordinate value. Y coordinate value. Z coordinate value.
     */
    T x{}, y{}, z{};

    /** @brief Constructs a TVec3 instance.
     */
    constexpr TVec3() = default;
    /** @brief Constructs a TVec3 instance.
     * @param X Initial horizontal vector component.
     * @param Y Initial vertical vector component.
     * @param Z Initial depth vector component.
     */
    constexpr TVec3(T X, T Y, T Z) noexcept : x(X), y(Y), z(Z) {}

    /** @brief Implements operator[] for this value or property type.
     * @param i Index of the requested component or element.
     */
    constexpr T& operator[](int i) noexcept { return (i == 0) ? x : (i == 1 ? y : z); }
    /** @brief Implements operator[] for this value or property type.
     * @param i Index of the requested component or element.
     */
    constexpr const T& operator[](int i) const noexcept { return (i == 0) ? x : (i == 1 ? y : z); }

    /** @brief Implements operator- for this value or property type.
     */
    constexpr TVec3 operator-() const noexcept { return { -x, -y, -z }; }

    /** @brief Implements operator+= for this value or property type.
     * @param v Value, vector or input to this operation.
     */
    constexpr TVec3& operator+=(TVec3 v) noexcept { x += v.x; y += v.y; z += v.z; return *this; }
    /** @brief Implements operator-= for this value or property type.
     * @param v Value, vector or input to this operation.
     */
    constexpr TVec3& operator-=(TVec3 v) noexcept { x -= v.x; y -= v.y; z -= v.z; return *this; }
    /** @brief Implements operator*= for this value or property type.
     * @param s Scalar multiplier, divisor or input value.
     */
    constexpr TVec3& operator*=(T s) noexcept { x *= s; y *= s; z *= s; return *this; }
    /** @brief Implements operator/= for this value or property type.
     * @param s Scalar multiplier, divisor or input value.
     */
    constexpr TVec3& operator/=(T s) noexcept { x /= s; y /= s; z /= s; return *this; }

    /** @brief Implements operator tvec2< t > for this value or property type.
     */
    explicit constexpr operator TVec2<T>() const noexcept { return { x, y }; }
    template<math::Arithmetic U>
    /** @brief Implements operator tvec2< u > for this value or property type.
     */
    explicit constexpr operator TVec2<U>() const noexcept
    {
        return { static_cast<U>(x), static_cast<U>(y) };
    }

    /** @brief Returns a zero-filled vector.
     */
    static constexpr TVec3 Zero()    noexcept { return { T{0}, T{0}, T{0} }; }
    /** @brief Returns a vector with all components equal to one.
     */
    static constexpr TVec3 One()     noexcept { return { T{1}, T{1}, T{1} }; }
    /** @brief Returns the positive X unit direction.
     */
    static constexpr TVec3 Right()   noexcept { return { T{1}, T{0}, T{0} }; }
    /** @brief Returns the up unit direction.
     */
    static constexpr TVec3 Up()      noexcept { return { T{0}, T{1}, T{0} }; }
    /** @brief Returns the forward unit direction.
     */
    static constexpr TVec3 Forward() noexcept { return { T{0}, T{0}, T{1} }; }
};

template<math::Arithmetic A, math::Arithmetic B>
/** @brief Implements operator+ for this value or property type.
 * @param a First input operand.
 * @param b Second input operand.
 */
constexpr auto operator+(TVec3<A> a, TVec3<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec3<R>{ static_cast<R>(a.x) + static_cast<R>(b.x),
        static_cast<R>(a.y) + static_cast<R>(b.y),
        static_cast<R>(a.z) + static_cast<R>(b.z) };
}
template<math::Arithmetic A, math::Arithmetic B>
/** @brief Implements operator- for this value or property type.
 * @param a First input operand.
 * @param b Second input operand.
 */
constexpr auto operator-(TVec3<A> a, TVec3<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec3<R>{ static_cast<R>(a.x) - static_cast<R>(b.x),
        static_cast<R>(a.y) - static_cast<R>(b.y),
        static_cast<R>(a.z) - static_cast<R>(b.z) };
}
template<math::Arithmetic A, math::Arithmetic S>
/** @brief Implements operator* for this value or property type.
 * @param v Value, vector or input to this operation.
 * @param s Scalar multiplier, divisor or input value.
 */
constexpr auto operator*(TVec3<A> v, S s) noexcept
{
    using R = math::Common2<A, S>;
    return TVec3<R>{ static_cast<R>(v.x)* static_cast<R>(s),
        static_cast<R>(v.y)* static_cast<R>(s),
        static_cast<R>(v.z)* static_cast<R>(s) };
}
template<math::Arithmetic S, math::Arithmetic A>
/** @brief Implements operator* for this value or property type.
 * @param s Scalar multiplier, divisor or input value.
 * @param v Value, vector or input to this operation.
 */
constexpr auto operator*(S s, TVec3<A> v) noexcept { return v * s; }

template<math::Arithmetic A, math::Arithmetic S>
/** @brief Implements operator/ for this value or property type.
 * @param v Value, vector or input to this operation.
 * @param s Scalar multiplier, divisor or input value.
 */
constexpr auto operator/(TVec3<A> v, S s) noexcept
{
    using R = math::Common2<A, S>;
    return TVec3<R>{ static_cast<R>(v.x) / static_cast<R>(s),
        static_cast<R>(v.y) / static_cast<R>(s),
        static_cast<R>(v.z) / static_cast<R>(s) };
}

template<math::Arithmetic A, math::Arithmetic B>
/** @brief Multiplies vector components pairwise.
 * @param a First input operand.
 * @param b Second input operand.
 */
constexpr auto Hadamard(TVec3<A> a, TVec3<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec3<R>{ static_cast<R>(a.x)* static_cast<R>(b.x),
        static_cast<R>(a.y)* static_cast<R>(b.y),
        static_cast<R>(a.z)* static_cast<R>(b.z) };
}

template<math::Arithmetic A, math::Arithmetic B>
/** @brief Implements operator== for this value or property type.
 * @param a First input operand.
 * @param b Second input operand.
 */
constexpr bool operator==(TVec3<A> a, TVec3<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return static_cast<R>(a.x) == static_cast<R>(b.x) &&
        static_cast<R>(a.y) == static_cast<R>(b.y) &&
        static_cast<R>(a.z) == static_cast<R>(b.z);
}
template<math::Arithmetic A, math::Arithmetic B>
/** @brief Implements operator!= for this value or property type.
 * @param a First input operand.
 * @param b Second input operand.
 */
constexpr bool operator!=(TVec3<A> a, TVec3<B> b) noexcept { return !(a == b); }

namespace math
{
    template<Arithmetic A, Arithmetic B>
    /** @brief Computes the dot product of the input vectors.
     * @param a First input operand.
     * @param b Second input operand.
     */
    constexpr auto Dot(TVec3<A> a, TVec3<B> b) noexcept
    {
        using R = Common2<A, B>;
        return static_cast<R>(a.x) * static_cast<R>(b.x) +
            static_cast<R>(a.y) * static_cast<R>(b.y) +
            static_cast<R>(a.z) * static_cast<R>(b.z);
    }

    template<Arithmetic A, Arithmetic B>
    /** @brief Computes the cross product of two vectors.
     * @param a First input operand.
     * @param b Second input operand.
     */
    constexpr auto Cross(TVec3<A> a, TVec3<B> b) noexcept
    {
        using R = Common2<A, B>;
        return TVec3<R>{
            static_cast<R>(a.y)* static_cast<R>(b.z) - static_cast<R>(a.z) * static_cast<R>(b.y),
                static_cast<R>(a.z)* static_cast<R>(b.x) - static_cast<R>(a.x) * static_cast<R>(b.z),
                static_cast<R>(a.x)* static_cast<R>(b.y) - static_cast<R>(a.y) * static_cast<R>(b.x)
        };
    }

    template<Arithmetic T>
    /** @brief Returns the squared Euclidean vector length without taking a square root.
     * @param v Value, vector or input to this operation.
     */
    constexpr auto LengthSq(TVec3<T> v) noexcept { return Dot(v, v); }

    template<Arithmetic T>
    /** @brief Returns the Euclidean vector length.
     * @param v Value, vector or input to this operation.
     */
    inline auto Length(TVec3<T> v) noexcept
    {
        using R = std::conditional_t<Float<T>, T, float>;
        return static_cast<R>(std::sqrt(static_cast<R>(LengthSq(v))));
    }

    template<Arithmetic T>
    /** @brief Returns a vector scaled to unit length where possible.
     * @param v Value, vector or input to this operation.
     */
    inline auto Normalized(TVec3<T> v) noexcept
    {
        using R = std::conditional_t<Float<T>, T, float>;
        const R len = Length(v);
        if (len <= Epsilon<R>()) return TVec3<R>{};
        return TVec3<R>{ static_cast<R>(v.x) / len, static_cast<R>(v.y) / len, static_cast<R>(v.z) / len };
    }

    template<Arithmetic V, Arithmetic L, Arithmetic H>
    /** @brief Restricts a scalar or vector to the supplied lower and upper bounds.
     * @param v Value, vector or input to this operation.
     * @param lo Lower permitted boundary.
     * @param hi Upper permitted boundary.
     */
    constexpr auto Clamp(TVec3<V> v, TVec3<L> lo, TVec3<H> hi) noexcept
    {
        using R = Common3<V, L, H>;
        return TVec3<R>{
            math::Clamp<R>(static_cast<R>(v.x), static_cast<R>(lo.x), static_cast<R>(hi.x)),
                math::Clamp<R>(static_cast<R>(v.y), static_cast<R>(lo.y), static_cast<R>(hi.y)),
                math::Clamp<R>(static_cast<R>(v.z), static_cast<R>(lo.z), static_cast<R>(hi.z))
        };
    }

    template<Arithmetic V, Arithmetic S>
    /** @brief Restricts a scalar or vector to the supplied lower and upper bounds.
     * @param v Value, vector or input to this operation.
     * @param lo Lower permitted boundary.
     * @param hi Upper permitted boundary.
     */
    constexpr auto Clamp(TVec3<V> v, S lo, S hi) noexcept
    {
        using R = Common2<V, S>;
        return TVec3<R>{
            math::Clamp<R>(static_cast<R>(v.x), static_cast<R>(lo), static_cast<R>(hi)),
                math::Clamp<R>(static_cast<R>(v.y), static_cast<R>(lo), static_cast<R>(hi)),
                math::Clamp<R>(static_cast<R>(v.z), static_cast<R>(lo), static_cast<R>(hi))
        };
    }

    template<Float T>
    /** @brief Reflects the input vector about the supplied normal.
     * @param v Value, vector or input to this operation.
     * @param nUnit Unit-length normal vector.
     */
    inline TVec3<T> Reflect(TVec3<T> v, TVec3<T> nUnit) noexcept
    {
        return v - (T{ 2 } *Dot(v, nUnit)) * nUnit;
    }

    template<Float T>
    /** @brief Projects one vector onto another vector or axis.
     * @param v Value, vector or input to this operation.
     * @param onto Vector or axis onto which the input is projected.
     */
    inline TVec3<T> Project(TVec3<T> v, TVec3<T> onto) noexcept
    {
        const T d = Dot(onto, onto);
        if (d <= Epsilon<T>()) return {};
        return (Dot(v, onto) / d) * onto;
    }

    template<Float T>
    /** @brief Moves a value or vector towards its target without overshooting.
     * @param current Current value before the requested transition.
     * @param target Target point, vector or transform for this operation.
     * @param maxDelta Largest allowed movement or change per update.
     */
    inline TVec3<T> MoveTowards(TVec3<T> current, TVec3<T> target, T maxDelta) noexcept
    {
        const auto d = target - current;
        const T len = Length(d);
        if (len <= maxDelta || len <= Epsilon<T>()) return target;
        return current + (d / len) * maxDelta;
    }
} // namespace math

// =====================================================================================
// TRect
// =====================================================================================
template<math::Arithmetic T>
/** @brief Axis-aligned rectangle parameterized by a numeric coordinate type.
 * @ingroup xengine_math
 */
struct TRect
{
    /** @brief Rectangle left coordinate. Rectangle top coordinate. Rectangle width. Rectangle height.
     */
    T x{}, y{}, w{}, h{};

    /** @brief Constructs a TRect instance.
     */
    constexpr TRect() = default;
    /** @brief Constructs a TRect instance.
     * @param X Initial horizontal vector component.
     * @param Y Initial vertical vector component.
     * @param W Requested width.
     * @param H Requested height.
     */
    constexpr TRect(T X, T Y, T W, T H) noexcept : x(X), y(Y), w(W), h(H) {}

    /** @brief Returns the left edge of this rectangle.
     */
    constexpr T Left()   const noexcept { return x; }
    /** @brief Returns the right edge of this rectangle.
     */
    constexpr T Right()  const noexcept { return x + w; }
    /** @brief Returns the top edge of this rectangle.
     */
    constexpr T Top()    const noexcept { return y; }
    /** @brief Returns the bottom edge of this rectangle.
     */
    constexpr T Bottom() const noexcept { return y + h; }

    /** @brief Returns the position of the rectangle origin.
     */
    constexpr TVec2<T> Pos()    const noexcept { return { x, y }; }
    /** @brief Returns the number of bytes or the dimensions of this value, as appropriate.
     */
    constexpr TVec2<T> Size()   const noexcept { return { w, h }; }
    /** @brief Returns the center point of this rectangle.
     */
    constexpr TVec2<T> Center() const noexcept { return { x + w * T{0.5}, y + h * T{0.5} }; }

    /** @brief Tests whether the supplied point or rectangle is contained within these bounds.
     * @param p Point or position being processed.
     */
    constexpr bool Contains(TVec2<T> p) const noexcept
    {
        return p.x >= Left() && p.x <= Right() && p.y >= Top() && p.y <= Bottom();
    }

    /** @brief Tests whether two rectangles have a nonempty overlap.
     * @param r Rectangle in the coordinate space specified by the method.
     */
    constexpr bool Overlaps(TRect r) const noexcept
    {
        return !(Right() < r.Left() || r.Right() < Left() || Bottom() < r.Top() || r.Bottom() < Top());
    }

    /** @brief Returns a rectangle translated by the supplied offset.
     * @param d Distance or angular value.
     */
    constexpr TRect Translated(TVec2<T> d) const noexcept { return { x + d.x, y + d.y, w, h }; }

    /** @brief Returns a rectangle with its edges moved inward by the supplied amount.
     * @param dx Horizontal offset.
     * @param dy Vertical offset.
     */
    constexpr TRect Inset(T dx, T dy) const noexcept { return { x + dx, y + dy, w - T{2} *dx, h - T{2} *dy }; }
    /** @brief Returns a rectangle with its edges expanded outward by the supplied amount.
     * @param dx Horizontal offset.
     * @param dy Vertical offset.
     */
    constexpr TRect Outset(T dx, T dy) const noexcept { return { x - dx, y - dy, w + T{2} *dx, h + T{2} *dy }; }

    template<math::Arithmetic S>
    /** @brief Returns a rectangle scaled using the supplied factor.
     * @param sx Horizontal scale.
     * @param sy Vertical scale.
     */
    constexpr auto Scaled(S sx, S sy) const noexcept
    {
        using R = math::Common2<T, S>;
        return TRect<R>{
            static_cast<R>(x)* static_cast<R>(sx),
                static_cast<R>(y)* static_cast<R>(sy),
                static_cast<R>(w)* static_cast<R>(sx),
                static_cast<R>(h)* static_cast<R>(sy)
        };
    }

    // Expand rect to include a point
    /** @brief Expands these bounds to include the supplied point or rectangle.
     * @param p Point or position being processed.
     */
    constexpr void ExpandToInclude(TVec2<T> p) noexcept
    {
        const T l = std::min(Left(), p.x);
        const T t = std::min(Top(), p.y);
        const T r = std::max(Right(), p.x);
        const T b = std::max(Bottom(), p.y);
        x = l; y = t; w = r - l; h = b - t;
    }

    // Clamp a point into the rectangle
    /** @brief Restricts a point to the inside of this rectangle.
     * @param p Point or position being processed.
     */
    constexpr TVec2<T> ClampPoint(TVec2<T> p) const noexcept
    {
        return { std::clamp(p.x, Left(), Right()), std::clamp(p.y, Top(), Bottom()) };
    }

    /** @brief Returns the smallest rectangle containing both input rectangles.
     * @param a First input operand.
     * @param b Second input operand.
     */
    static constexpr TRect Union(TRect a, TRect b) noexcept
    {
        const T l = std::min(a.Left(), b.Left());
        const T t = std::min(a.Top(), b.Top());
        const T r = std::max(a.Right(), b.Right());
        const T bt = std::max(a.Bottom(), b.Bottom());
        return { l, t, r - l, bt - t };
    }

    /** @brief Returns the overlapping rectangle of the two inputs.
     * @param a First input operand.
     * @param b Second input operand.
     */
    static constexpr TRect Intersection(TRect a, TRect b) noexcept
    {
        const T l = std::max(a.Left(), b.Left());
        const T t = std::max(a.Top(), b.Top());
        const T r = std::min(a.Right(), b.Right());
        const T bt = std::min(a.Bottom(), b.Bottom());
        if (r < l || bt < t) return {};
        return { l, t, r - l, bt - t };
    }
};

template<math::Arithmetic T>
/** @brief Implements operator+ for this value or property type.
 * @param rc Rectangle to convert or compare.
 * @param d Distance or angular value.
 */
constexpr TRect<T> operator+(TRect<T> rc, TVec2<T> d) noexcept { return rc.Translated(d); }

template<math::Arithmetic T>
/** @brief Implements operator- for this value or property type.
 * @param rc Rectangle to convert or compare.
 * @param d Distance or angular value.
 */
constexpr TRect<T> operator-(TRect<T> rc, TVec2<T> d) noexcept { return { rc.x - d.x, rc.y - d.y, rc.w, rc.h }; }

// =====================================================================================
// Color (+ HSV/HSL helpers)
// =====================================================================================
/** @brief RGBA color channels and color conversion helpers.
 * @ingroup xengine_math
 */
struct Color
{
    /** @brief Red color channel (0..255). Green color channel (0..255). Blue color channel (0..255). Alpha color channel (0..255).
     */
    uint8_t r = 255, g = 255, b = 255, a = 255;

    /** @brief Constructs a Color instance.
     */
    constexpr Color() = default;
    /** @brief Constructs a Color instance.
     * @param R Red color channel.
     * @param G Green color channel.
     * @param B Blue color channel.
     * @param A Alpha color channel.
     */
    constexpr Color(uint8_t R, uint8_t G, uint8_t B, uint8_t A = 255) : r(R), g(G), b(B), a(A) {}

    /** @brief Returns the predefined black color.
     * @param a First input operand.
     */
    static constexpr Color Black(uint8_t a = 255)   noexcept { return { 0,   0,   0, a }; }
    /** @brief Returns the predefined white color.
     * @param a First input operand.
     */
    static constexpr Color White(uint8_t a = 255)   noexcept { return { 255, 255, 255, a }; }

    /** @brief Returns the predefined red color.
     * @param a First input operand.
     */
    static constexpr Color Red(uint8_t a = 255)     noexcept { return { 255,   0,   0, a }; }
    /** @brief Returns the predefined green color.
     * @param a First input operand.
     */
    static constexpr Color Green(uint8_t a = 255)   noexcept { return { 0, 255,   0, a }; }
    /** @brief Returns the predefined blue color.
     * @param a First input operand.
     */
    static constexpr Color Blue(uint8_t a = 255)    noexcept { return { 0,   0, 255, a }; }

    /** @brief Returns the predefined yellow color.
     * @param a First input operand.
     */
    static constexpr Color Yellow(uint8_t a = 255)  noexcept { return { 255, 255,   0, a }; }
    /** @brief Returns the predefined cyan color.
     * @param a First input operand.
     */
    static constexpr Color Cyan(uint8_t a = 255)    noexcept { return { 0, 255, 255, a }; }
    /** @brief Returns the predefined magenta color.
     * @param a First input operand.
     */
    static constexpr Color Magenta(uint8_t a = 255) noexcept { return { 255,   0, 255, a }; }

    /** @brief Returns the predefined gray color.
     * @param a First input operand.
     */
    static constexpr Color Gray(uint8_t a = 255)       noexcept { return { 128, 128, 128, a }; }
    /** @brief Returns the predefined light gray color.
     * @param a First input operand.
     */
    static constexpr Color LightGray(uint8_t a = 255)  noexcept { return { 211, 211, 211, a }; }
    /** @brief Returns the predefined dark gray color.
     * @param a First input operand.
     */
    static constexpr Color DarkGray(uint8_t a = 255)   noexcept { return { 64,  64,  64, a }; }

    /** @brief Returns the predefined orange color.
     * @param a First input operand.
     */
    static constexpr Color Orange(uint8_t a = 255) noexcept { return { 255, 165,   0, a }; }
    /** @brief Returns the predefined pink color.
     * @param a First input operand.
     */
    static constexpr Color Pink(uint8_t a = 255)   noexcept { return { 255, 192, 203, a }; }
    /** @brief Returns the predefined purple color.
     * @param a First input operand.
     */
    static constexpr Color Purple(uint8_t a = 255) noexcept { return { 128,   0, 128, a }; }
    /** @brief Returns the predefined brown color.
     * @param a First input operand.
     */
    static constexpr Color Brown(uint8_t a = 255)  noexcept { return { 139,  69,  19, a }; }

    /** @brief Returns the predefined lime color.
     * @param a First input operand.
     */
    static constexpr Color Lime(uint8_t a = 255)   noexcept { return { 0, 255,   0, a }; }
    /** @brief Returns the predefined navy color.
     * @param a First input operand.
     */
    static constexpr Color Navy(uint8_t a = 255)   noexcept { return { 0,   0, 128, a }; }
    /** @brief Returns the predefined teal color.
     * @param a First input operand.
     */
    static constexpr Color Teal(uint8_t a = 255)   noexcept { return { 0, 128, 128, a }; }
    /** @brief Returns the predefined olive color.
     * @param a First input operand.
     */
    static constexpr Color Olive(uint8_t a = 255)  noexcept { return { 128, 128,   0, a }; }
    /** @brief Returns the predefined maroon color.
     * @param a First input operand.
     */
    static constexpr Color Maroon(uint8_t a = 255) noexcept { return { 128,   0,   0, a }; }
    /** @brief Returns the predefined silver color.
     * @param a First input operand.
     */
    static constexpr Color Silver(uint8_t a = 255) noexcept { return { 192, 192, 192, a }; }

    /** @brief Compares all four RGBA channels for equality.
     * @param A First color to compare.
     * @param B Second color to compare.
     */
    friend constexpr bool operator==(Color A, Color B) noexcept
    {
        return A.r == B.r && A.g == B.g && A.b == B.b && A.a == B.a;
    }
    /** @brief Checks whether any RGBA channel differs. */
    friend constexpr bool operator!=(Color A, Color B) noexcept { return !(A == B); }

    /** @brief Linearly interpolates between two values using the supplied factor.
     * @param c0 First input color.
     * @param c1 Second input color.
     * @param t Interpolation factor.
     */
    static inline Color Lerp(Color c0, Color c1, float t) noexcept
    {
        auto lerpU8 = [t](uint8_t A, uint8_t B)
            {
                float x = static_cast<float>(A) + (static_cast<float>(B) - static_cast<float>(A)) * t;
                x = std::clamp(x, 0.f, 255.f);
                return static_cast<uint8_t>(x + 0.5f);
            };
        return { lerpU8(c0.r,c1.r), lerpU8(c0.g,c1.g), lerpU8(c0.b,c1.b), lerpU8(c0.a,c1.a) };
    }

    // Add/sub saturating (0..255)
    /** @brief Adds a signed amount to RGB, clamped to [0, 255]; preserves alpha. */
    friend inline Color operator+(Color c, int v) noexcept
    {
        auto sat = [](int x) { return static_cast<uint8_t>(std::clamp(x, 0, 255)); };
        return { sat(int(c.r) + v), sat(int(c.g) + v), sat(int(c.b) + v), c.a };
    }
    /** @brief Subtracts a signed amount from RGB with channel saturation; preserves alpha. */
    friend inline Color operator-(Color c, int v) noexcept { return c + (-v); }

    // Multiply RGB by scalar
    /** @brief Multiplies RGB channels by a scalar with saturation; preserves alpha. */
    friend inline Color operator*(Color c, float s) noexcept
    {
        auto mul = [s](uint8_t v) {
            float x = static_cast<float>(v) * s;
            x = std::clamp(x, 0.f, 255.f);
            return static_cast<uint8_t>(x + 0.5f);
            };
        return { mul(c.r), mul(c.g), mul(c.b), c.a };
    }

    // Premultiply alpha (RGB *= a/255)
    /** @brief Returns a color whose RGB channels are multiplied by alpha.
     */
    inline Color Premultiplied() const noexcept
    {
        const float af = static_cast<float>(a) / 255.f;
        return (*this) * af;
    }

    // ---- HSV (H in [0,360), S,V in [0,1]) ----
    /** @brief Creates an RGB color from hue, saturation and value.
     * @param hDeg Hue measured in degrees.
     * @param s Scalar multiplier, divisor or input value.
     * @param v Value, vector or input to this operation.
     * @param a First input operand.
     */
    static inline Color FromHSV(float hDeg, float s, float v, uint8_t a = 255) noexcept
    {
        hDeg = std::fmod(hDeg, 360.f);
        if (hDeg < 0.f) hDeg += 360.f;
        s = std::clamp(s, 0.f, 1.f);
        v = std::clamp(v, 0.f, 1.f);

        const float c = v * s;
        const float x = c * (1.f - std::fabs(std::fmod(hDeg / 60.f, 2.f) - 1.f));
        const float m = v - c;

        float rf = 0, gf = 0, bf = 0;
        if (hDeg < 60) { rf = c; gf = x; bf = 0; }
        else if (hDeg < 120) { rf = x; gf = c; bf = 0; }
        else if (hDeg < 180) { rf = 0; gf = c; bf = x; }
        else if (hDeg < 240) { rf = 0; gf = x; bf = c; }
        else if (hDeg < 300) { rf = x; gf = 0; bf = c; }
        else { rf = c; gf = 0; bf = x; }

        auto toU8 = [](float f) { return static_cast<uint8_t>(std::clamp(f * 255.f, 0.f, 255.f) + 0.5f); };
        return { toU8(rf + m), toU8(gf + m), toU8(bf + m), a };
    }

    /** @brief Converts this color to hue, saturation and value.
     * @param hDeg Hue measured in degrees.
     * @param s Scalar multiplier, divisor or input value.
     * @param v Value, vector or input to this operation.
     */
    inline void ToHSV(float& hDeg, float& s, float& v) const noexcept
    {
        const float rf = r / 255.f, gf = g / 255.f, bf = b / 255.f;
        const float mx = std::max({ rf, gf, bf });
        const float mn = std::min({ rf, gf, bf });
        const float d = mx - mn;

        v = mx;
        s = (mx <= 0.f) ? 0.f : (d / mx);

        if (d <= 0.f) { hDeg = 0.f; return; }

        if (mx == rf) hDeg = 60.f * std::fmod(((gf - bf) / d), 6.f);
        else if (mx == gf) hDeg = 60.f * (((bf - rf) / d) + 2.f);
        else               hDeg = 60.f * (((rf - gf) / d) + 4.f);

        if (hDeg < 0.f) hDeg += 360.f;
    }
};

/** @var uint8_t Color::g
 * @brief Green channel (0..255), independent of alpha.
 */
/** @var uint8_t Color::b
 * @brief Blue channel (0..255), independent of alpha.
 */
/** @var uint8_t Color::a
 * @brief Alpha channel: 0 is transparent, 255 is opaque.
 */

// =====================================================================================
// Aliases (FLOAT-first)
// =====================================================================================
/** @brief Public type alias for the vec2 i accessor.
 */
using Vec2I = TVec2<int>;
/** @brief Public type alias for the vec2 accessor.
 */
using Vec2 = TVec2<float>;

/** @brief Public type alias for the vec3 i accessor.
 */
using Vec3I = TVec3<int>;
/** @brief Public type alias for the vec3 accessor.
 */
using Vec3 = TVec3<float>;

/** @brief Public type alias for the rect i accessor.
 */
using RectI = TRect<int>;
/** @brief Public type alias for the rect accessor.
 */
using Rect = TRect<float>;

// =====================================================================================
// Conversions (explicit + helpers "round to int")
// =====================================================================================
template<math::Arithmetic TO, math::Arithmetic FROM>
/** @brief Converts vector components to the requested numeric type.
 * @param v Value, vector or input to this operation.
 */
constexpr TVec2<TO> Vec2Cast(TVec2<FROM> v) noexcept { return { static_cast<TO>(v.x), static_cast<TO>(v.y) }; }

template<math::Arithmetic TO, math::Arithmetic FROM>
/** @brief Converts vector components to the requested numeric type.
 * @param v Value, vector or input to this operation.
 */
constexpr TVec3<TO> Vec3Cast(TVec3<FROM> v) noexcept { return { static_cast<TO>(v.x), static_cast<TO>(v.y), static_cast<TO>(v.z) }; }

template<math::Arithmetic TO, math::Arithmetic FROM>
/** @brief Converts rectangle components to the requested numeric type.
 * @param r Rectangle in the coordinate space specified by the method.
 */
constexpr TRect<TO> RectCast(TRect<FROM> r) noexcept
{
    return { static_cast<TO>(r.x), static_cast<TO>(r.y), static_cast<TO>(r.w), static_cast<TO>(r.h) };
}

/** @brief Converts to integral Vec2 coordinates with rounding.
 * @param v Value, vector or input to this operation.
 */
inline Vec2I  ToVec2Round(Vec2 v) noexcept { return { (int)std::lround(v.x), (int)std::lround(v.y) }; }
/** @brief Converts to integral Vec3 coordinates with rounding.
 * @param v Value, vector or input to this operation.
 */
inline Vec3I  ToVec3Round(Vec3 v) noexcept { return { (int)std::lround(v.x), (int)std::lround(v.y), (int)std::lround(v.z) }; }
/** @brief Converts rectangle coordinates with rounding.
 * @param r Rectangle in the coordinate space specified by the method.
 */
inline RectI  ToRectRound(Rect r) noexcept { return { (int)std::lround(r.x), (int)std::lround(r.y), (int)std::lround(r.w), (int)std::lround(r.h) }; }

// =====================================================================================
// Constants
// =====================================================================================
/** @namespace Vector2
 * @brief Named common directions and zero/one constants for 2D vectors.
 * @ingroup xengine_math
 */
namespace Vector2
{
    /** @brief Value named Zero used by Vector2.
     */
    inline constexpr Vec2I  Zero{ Vec2I::Zero() };
    /** @brief Value named One used by Vector2.
     */
    inline constexpr Vec2I  One{ Vec2I::One() };
    /** @brief Value named Right used by Vector2.
     */
    inline constexpr Vec2I  Right{ Vec2I::Right() };
    /** @brief Value named Up used by Vector2.
     */
    inline constexpr Vec2I  Up{ Vec2I::Up() };

    /** @brief Value named Zerof used by Vector2.
     */
    inline constexpr Vec2 Zerof{ Vec2::Zero() };
    /** @brief Value named Onef used by Vector2.
     */
    inline constexpr Vec2 Onef{ Vec2::One() };
    /** @brief Value named Rightf used by Vector2.
     */
    inline constexpr Vec2 Rightf{ Vec2::Right() };
    /** @brief Value named Upf used by Vector2.
     */
    inline constexpr Vec2 Upf{ Vec2::Up() };
}

/** @namespace Vector3
 * @brief Named common directions and zero/one constants for 3D vectors.
 * @ingroup xengine_math
 */
namespace Vector3
{
    /** @brief Value named Zero used by Vector3.
     */
    inline constexpr Vec3I  Zero{ Vec3I::Zero() };
    /** @brief Value named One used by Vector3.
     */
    inline constexpr Vec3I  One{ Vec3I::One() };
    /** @brief Value named Right used by Vector3.
     */
    inline constexpr Vec3I  Right{ Vec3I::Right() };
    /** @brief Value named Up used by Vector3.
     */
    inline constexpr Vec3I  Up{ Vec3I::Up() };
    /** @brief Value named Forward used by Vector3.
     */
    inline constexpr Vec3I  Forward{ Vec3I::Forward() };

    /** @brief Value named Zerof used by Vector3.
     */
    inline constexpr Vec3 Zerof{ Vec3::Zero() };
    /** @brief Value named Onef used by Vector3.
     */
    inline constexpr Vec3 Onef{ Vec3::One() };
    /** @brief Value named Rightf used by Vector3.
     */
    inline constexpr Vec3 Rightf{ Vec3::Right() };
    /** @brief Value named Upf used by Vector3.
     */
    inline constexpr Vec3 Upf{ Vec3::Up() };
    /** @brief Value named Forwardf used by Vector3.
     */
    inline constexpr Vec3 Forwardf{ Vec3::Forward() };
}

// These members intentionally share C++ declarations. Explicit @var blocks
// make each axis visible in Doxygen without altering the C++ data layout.
/** @var T TVec2<T>::y
 * @brief Y component of the two-dimensional vector.
 */
/** @var T TVec3<T>::y
 * @brief Y component of the three-dimensional vector.
 */
/** @var T TVec3<T>::z
 * @brief Z component of the three-dimensional vector.
 */
/** @var T TRect<T>::y
 * @brief Rectangle top coordinate.
 */
/** @var T TRect<T>::w
 * @brief Rectangle width or horizontal extent.
 */
/** @var T TRect<T>::h
 * @brief Rectangle height or vertical extent.
 */
