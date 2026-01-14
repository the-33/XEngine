#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <type_traits>
#include <concepts>

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
    using Common2 = std::common_type_t<A, B>;

    template<class A, class B, class C>
    using Common3 = std::common_type_t<A, B, C>;

    template<class T>
    constexpr T Epsilon() noexcept
    {
        if constexpr (Float<T>) return static_cast<T>(1e-6);
        else return T{ 0 };
    }

    // -------------------- Scalar utils --------------------
    template<Arithmetic T>
    constexpr T Min(T a, T b) noexcept { return (a < b) ? a : b; }

    template<Arithmetic T>
    constexpr T Max(T a, T b) noexcept { return (a > b) ? a : b; }

    template<Arithmetic T>
    constexpr T Abs(T v) noexcept { return (v < T{ 0 }) ? -v : v; }

    template<Arithmetic T>
    constexpr T Clamp(T v, T lo, T hi) noexcept { return std::clamp(v, lo, hi); }

    template<Arithmetic T>
    constexpr T Saturate(T v) noexcept { return Clamp(v, T{ 0 }, T{ 1 }); }

    template<Arithmetic T>
    constexpr T Lerp(T a, T b, T t) noexcept { return a + (b - a) * t; }

    template<Float T>
    constexpr bool NearlyEqual(T a, T b, T eps = Epsilon<T>()) noexcept
    {
        return Abs(a - b) <= eps;
    }

    template<Float T>
    constexpr T Deg2Rad(T d) noexcept { return d * (static_cast<T>(3.14159265358979323846) / static_cast<T>(180)); }

    template<Float T>
    constexpr T Rad2Deg(T r) noexcept { return r * (static_cast<T>(180) / static_cast<T>(3.14159265358979323846)); }

    template<Float T>
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
struct TVec2
{
    T x{}, y{};

    constexpr TVec2() = default;
    constexpr TVec2(T X, T Y) noexcept : x(X), y(Y) {}

    constexpr T& operator[](int i) noexcept { return (i == 0) ? x : y; }
    constexpr const T& operator[](int i) const noexcept { return (i == 0) ? x : y; }

    constexpr TVec2 operator-() const noexcept { return { -x, -y }; }

    constexpr TVec2& operator+=(TVec2 v) noexcept { x += v.x; y += v.y; return *this; }
    constexpr TVec2& operator-=(TVec2 v) noexcept { x -= v.x; y -= v.y; return *this; }
    constexpr TVec2& operator*=(T s) noexcept { x *= s; y *= s; return *this; }
    constexpr TVec2& operator/=(T s) noexcept { x /= s; y /= s; return *this; }

    static constexpr TVec2 Zero()  noexcept { return { T{0}, T{0} }; }
    static constexpr TVec2 One()   noexcept { return { T{1}, T{1} }; }
    static constexpr TVec2 Right() noexcept { return { T{1}, T{0} }; }
    static constexpr TVec2 Up()    noexcept { return { T{0}, T{1} }; }
};

// ---- Vec2 operators (mixed) ----
template<math::Arithmetic A, math::Arithmetic B>
    requires (!std::is_same_v<A, bool> && !std::is_same_v<B, bool>)
constexpr auto operator+(TVec2<A> a, TVec2<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec2<R>{ static_cast<R>(a.x) + static_cast<R>(b.x),
        static_cast<R>(a.y) + static_cast<R>(b.y) };
}

template<math::Arithmetic A, math::Arithmetic B>
    requires (!std::is_same_v<A, bool> && !std::is_same_v<B, bool>)
constexpr auto operator-(TVec2<A> a, TVec2<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec2<R>{ static_cast<R>(a.x) - static_cast<R>(b.x),
        static_cast<R>(a.y) - static_cast<R>(b.y) };
}

template<math::Arithmetic A, math::Arithmetic S>
    requires (!std::is_same_v<S, bool>)
constexpr auto operator*(TVec2<A> v, S s) noexcept
{
    using R = math::Common2<A, S>;
    return TVec2<R>{ static_cast<R>(v.x)* static_cast<R>(s),
        static_cast<R>(v.y)* static_cast<R>(s) };
}

template<math::Arithmetic S, math::Arithmetic A>
    requires (!std::is_same_v<S, bool>)
constexpr auto operator*(S s, TVec2<A> v) noexcept { return v * s; }

template<math::Arithmetic A, math::Arithmetic S>
    requires (!std::is_same_v<S, bool>)
constexpr auto operator/(TVec2<A> v, S s) noexcept
{
    using R = math::Common2<A, S>;
    return TVec2<R>{ static_cast<R>(v.x) / static_cast<R>(s),
        static_cast<R>(v.y) / static_cast<R>(s) };
}

// Component-wise mul/div (útil para escalados por ejes)
template<math::Arithmetic A, math::Arithmetic B>
constexpr auto Hadamard(TVec2<A> a, TVec2<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec2<R>{ static_cast<R>(a.x)* static_cast<R>(b.x),
        static_cast<R>(a.y)* static_cast<R>(b.y) };
}

template<math::Arithmetic A, math::Arithmetic B>
constexpr bool operator==(TVec2<A> a, TVec2<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return static_cast<R>(a.x) == static_cast<R>(b.x) &&
        static_cast<R>(a.y) == static_cast<R>(b.y);
}
template<math::Arithmetic A, math::Arithmetic B>
constexpr bool operator!=(TVec2<A> a, TVec2<B> b) noexcept { return !(a == b); }

// ---- Vec2 functions ----
namespace math
{
    template<Arithmetic A, Arithmetic B>
    constexpr auto Dot(TVec2<A> a, TVec2<B> b) noexcept
    {
        using R = Common2<A, B>;
        return static_cast<R>(a.x) * static_cast<R>(b.x) +
            static_cast<R>(a.y) * static_cast<R>(b.y);
    }

    template<Arithmetic T>
    constexpr auto LengthSq(TVec2<T> v) noexcept { return Dot(v, v); }

    template<Arithmetic T>
    inline auto Length(TVec2<T> v) noexcept
    {
        using R = std::conditional_t<Float<T>, T, float>;
        return static_cast<R>(std::sqrt(static_cast<R>(LengthSq(v))));
    }

    template<Arithmetic T>
    inline auto Distance(TVec2<T> a, TVec2<T> b) noexcept { return Length(a - b); }

    template<Arithmetic T>
    inline auto Normalized(TVec2<T> v) noexcept
    {
        using R = std::conditional_t<Float<T>, T, float>;
        const R len = Length(v);
        if (len <= Epsilon<R>()) return TVec2<R>{};
        return TVec2<R>{ static_cast<R>(v.x) / len, static_cast<R>(v.y) / len };
    }

    template<Arithmetic A, Arithmetic B>
    constexpr auto Min(TVec2<A> a, TVec2<B> b) noexcept
    {
        using R = Common2<A, B>;
        return TVec2<R>{ math::Min<R>(static_cast<R>(a.x), static_cast<R>(b.x)),
            math::Min<R>(static_cast<R>(a.y), static_cast<R>(b.y)) };
    }

    template<Arithmetic A, Arithmetic B>
    constexpr auto Max(TVec2<A> a, TVec2<B> b) noexcept
    {
        using R = Common2<A, B>;
        return TVec2<R>{ math::Max<R>(static_cast<R>(a.x), static_cast<R>(b.x)),
            math::Max<R>(static_cast<R>(a.y), static_cast<R>(b.y)) };
    }

    template<Arithmetic V, Arithmetic L, Arithmetic H>
    constexpr auto Clamp(TVec2<V> v, TVec2<L> lo, TVec2<H> hi) noexcept
    {
        using R = Common3<V, L, H>;
        return TVec2<R>{
            math::Clamp<R>(static_cast<R>(v.x), static_cast<R>(lo.x), static_cast<R>(hi.x)),
                math::Clamp<R>(static_cast<R>(v.y), static_cast<R>(lo.y), static_cast<R>(hi.y))
        };
    }

    template<Arithmetic V, Arithmetic S>
    constexpr auto Clamp(TVec2<V> v, S lo, S hi) noexcept
    {
        using R = Common2<V, S>;
        return TVec2<R>{
            math::Clamp<R>(static_cast<R>(v.x), static_cast<R>(lo), static_cast<R>(hi)),
                math::Clamp<R>(static_cast<R>(v.y), static_cast<R>(lo), static_cast<R>(hi))
        };
    }

    template<Arithmetic V, Arithmetic Tt>
    constexpr auto Lerp(TVec2<V> a, TVec2<V> b, Tt t) noexcept
    {
        using R = Common2<V, Tt>;
        return TVec2<R>{
            math::Lerp<R>(static_cast<R>(a.x), static_cast<R>(b.x), static_cast<R>(t)),
                math::Lerp<R>(static_cast<R>(a.y), static_cast<R>(b.y), static_cast<R>(t))
        };
    }

    template<Arithmetic T>
    constexpr auto Abs(TVec2<T> v) noexcept
    {
        using R = T;
        return TVec2<R>{ math::Abs<R>(v.x), math::Abs<R>(v.y) };
    }

    template<Float T>
    inline TVec2<T> Floor(TVec2<T> v) noexcept { return { std::floor(v.x), std::floor(v.y) }; }

    template<Float T>
    inline TVec2<T> Ceil(TVec2<T> v) noexcept { return { std::ceil(v.x), std::ceil(v.y) }; }

    template<Float T>
    inline TVec2<T> Round(TVec2<T> v) noexcept { return { std::round(v.x), std::round(v.y) }; }

    template<Arithmetic T>
    constexpr auto Perp(TVec2<T> v) noexcept { return TVec2<T>{ -v.y, v.x }; } // 90º CCW

    template<Float T>
    inline T AngleRad(TVec2<T> a, TVec2<T> b) noexcept
    {
        const T denom = Length(a) * Length(b);
        if (denom <= Epsilon<T>()) return T{ 0 };
        const T c = Clamp(Dot(a, b) / denom, T{ -1 }, T{ 1 });
        return std::acos(c);
    }

    template<Float T>
    inline T SignedAngleRad(TVec2<T> a, TVec2<T> b) noexcept
    {
        // sign via 2D cross (scalar)
        const T ang = AngleRad(a, b);
        const T cross = a.x * b.y - a.y * b.x;
        return (cross < T{ 0 }) ? -ang : ang;
    }

    template<Float T>
    inline TVec2<T> Rotate(TVec2<T> v, T angleRad) noexcept
    {
        const T c = std::cos(angleRad);
        const T s = std::sin(angleRad);
        return { v.x * c - v.y * s, v.x * s + v.y * c };
    }

    template<Float T>
    inline TVec2<T> FromAngleRad(T angleRad, T length = T{ 1 }) noexcept
    {
        return { std::cos(angleRad) * length, std::sin(angleRad) * length };
    }

    template<Float T>
    inline TVec2<T> Reflect(TVec2<T> v, TVec2<T> nUnit) noexcept
    {
        // v - 2*dot(v,n)*n
        return v - (T{ 2 } *Dot(v, nUnit)) * nUnit;
    }

    template<Float T>
    inline TVec2<T> Project(TVec2<T> v, TVec2<T> onto) noexcept
    {
        const T d = Dot(onto, onto);
        if (d <= Epsilon<T>()) return {};
        return (Dot(v, onto) / d) * onto;
    }

    template<Float T>
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
struct TVec3
{
    T x{}, y{}, z{};

    constexpr TVec3() = default;
    constexpr TVec3(T X, T Y, T Z) noexcept : x(X), y(Y), z(Z) {}

    constexpr T& operator[](int i) noexcept { return (i == 0) ? x : (i == 1 ? y : z); }
    constexpr const T& operator[](int i) const noexcept { return (i == 0) ? x : (i == 1 ? y : z); }

    constexpr TVec3 operator-() const noexcept { return { -x, -y, -z }; }

    constexpr TVec3& operator+=(TVec3 v) noexcept { x += v.x; y += v.y; z += v.z; return *this; }
    constexpr TVec3& operator-=(TVec3 v) noexcept { x -= v.x; y -= v.y; z -= v.z; return *this; }
    constexpr TVec3& operator*=(T s) noexcept { x *= s; y *= s; z *= s; return *this; }
    constexpr TVec3& operator/=(T s) noexcept { x /= s; y /= s; z /= s; return *this; }

    explicit constexpr operator TVec2<T>() const noexcept { return { x, y }; }
    template<math::Arithmetic U>
    explicit constexpr operator TVec2<U>() const noexcept
    {
        return { static_cast<U>(x), static_cast<U>(y) };
    }

    static constexpr TVec3 Zero()    noexcept { return { T{0}, T{0}, T{0} }; }
    static constexpr TVec3 One()     noexcept { return { T{1}, T{1}, T{1} }; }
    static constexpr TVec3 Right()   noexcept { return { T{1}, T{0}, T{0} }; }
    static constexpr TVec3 Up()      noexcept { return { T{0}, T{1}, T{0} }; }
    static constexpr TVec3 Forward() noexcept { return { T{0}, T{0}, T{1} }; }
};

template<math::Arithmetic A, math::Arithmetic B>
constexpr auto operator+(TVec3<A> a, TVec3<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec3<R>{ static_cast<R>(a.x) + static_cast<R>(b.x),
        static_cast<R>(a.y) + static_cast<R>(b.y),
        static_cast<R>(a.z) + static_cast<R>(b.z) };
}
template<math::Arithmetic A, math::Arithmetic B>
constexpr auto operator-(TVec3<A> a, TVec3<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec3<R>{ static_cast<R>(a.x) - static_cast<R>(b.x),
        static_cast<R>(a.y) - static_cast<R>(b.y),
        static_cast<R>(a.z) - static_cast<R>(b.z) };
}
template<math::Arithmetic A, math::Arithmetic S>
constexpr auto operator*(TVec3<A> v, S s) noexcept
{
    using R = math::Common2<A, S>;
    return TVec3<R>{ static_cast<R>(v.x)* static_cast<R>(s),
        static_cast<R>(v.y)* static_cast<R>(s),
        static_cast<R>(v.z)* static_cast<R>(s) };
}
template<math::Arithmetic S, math::Arithmetic A>
constexpr auto operator*(S s, TVec3<A> v) noexcept { return v * s; }

template<math::Arithmetic A, math::Arithmetic S>
constexpr auto operator/(TVec3<A> v, S s) noexcept
{
    using R = math::Common2<A, S>;
    return TVec3<R>{ static_cast<R>(v.x) / static_cast<R>(s),
        static_cast<R>(v.y) / static_cast<R>(s),
        static_cast<R>(v.z) / static_cast<R>(s) };
}

template<math::Arithmetic A, math::Arithmetic B>
constexpr auto Hadamard(TVec3<A> a, TVec3<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return TVec3<R>{ static_cast<R>(a.x)* static_cast<R>(b.x),
        static_cast<R>(a.y)* static_cast<R>(b.y),
        static_cast<R>(a.z)* static_cast<R>(b.z) };
}

template<math::Arithmetic A, math::Arithmetic B>
constexpr bool operator==(TVec3<A> a, TVec3<B> b) noexcept
{
    using R = math::Common2<A, B>;
    return static_cast<R>(a.x) == static_cast<R>(b.x) &&
        static_cast<R>(a.y) == static_cast<R>(b.y) &&
        static_cast<R>(a.z) == static_cast<R>(b.z);
}
template<math::Arithmetic A, math::Arithmetic B>
constexpr bool operator!=(TVec3<A> a, TVec3<B> b) noexcept { return !(a == b); }

namespace math
{
    template<Arithmetic A, Arithmetic B>
    constexpr auto Dot(TVec3<A> a, TVec3<B> b) noexcept
    {
        using R = Common2<A, B>;
        return static_cast<R>(a.x) * static_cast<R>(b.x) +
            static_cast<R>(a.y) * static_cast<R>(b.y) +
            static_cast<R>(a.z) * static_cast<R>(b.z);
    }

    template<Arithmetic A, Arithmetic B>
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
    constexpr auto LengthSq(TVec3<T> v) noexcept { return Dot(v, v); }

    template<Arithmetic T>
    inline auto Length(TVec3<T> v) noexcept
    {
        using R = std::conditional_t<Float<T>, T, float>;
        return static_cast<R>(std::sqrt(static_cast<R>(LengthSq(v))));
    }

    template<Arithmetic T>
    inline auto Normalized(TVec3<T> v) noexcept
    {
        using R = std::conditional_t<Float<T>, T, float>;
        const R len = Length(v);
        if (len <= Epsilon<R>()) return TVec3<R>{};
        return TVec3<R>{ static_cast<R>(v.x) / len, static_cast<R>(v.y) / len, static_cast<R>(v.z) / len };
    }

    template<Arithmetic V, Arithmetic L, Arithmetic H>
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
    inline TVec3<T> Reflect(TVec3<T> v, TVec3<T> nUnit) noexcept
    {
        return v - (T{ 2 } *Dot(v, nUnit)) * nUnit;
    }

    template<Float T>
    inline TVec3<T> Project(TVec3<T> v, TVec3<T> onto) noexcept
    {
        const T d = Dot(onto, onto);
        if (d <= Epsilon<T>()) return {};
        return (Dot(v, onto) / d) * onto;
    }

    template<Float T>
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
struct TRect
{
    T x{}, y{}, w{}, h{};

    constexpr TRect() = default;
    constexpr TRect(T X, T Y, T W, T H) noexcept : x(X), y(Y), w(W), h(H) {}

    constexpr T Left()   const noexcept { return x; }
    constexpr T Right()  const noexcept { return x + w; }
    constexpr T Top()    const noexcept { return y; }
    constexpr T Bottom() const noexcept { return y + h; }

    constexpr TVec2<T> Pos()    const noexcept { return { x, y }; }
    constexpr TVec2<T> Size()   const noexcept { return { w, h }; }
    constexpr TVec2<T> Center() const noexcept { return { x + w * T{0.5}, y + h * T{0.5} }; }

    constexpr bool Contains(TVec2<T> p) const noexcept
    {
        return p.x >= Left() && p.x <= Right() && p.y >= Top() && p.y <= Bottom();
    }

    constexpr bool Overlaps(TRect r) const noexcept
    {
        return !(Right() < r.Left() || r.Right() < Left() || Bottom() < r.Top() || r.Bottom() < Top());
    }

    constexpr TRect Translated(TVec2<T> d) const noexcept { return { x + d.x, y + d.y, w, h }; }

    constexpr TRect Inset(T dx, T dy) const noexcept { return { x + dx, y + dy, w - T{2} *dx, h - T{2} *dy }; }
    constexpr TRect Outset(T dx, T dy) const noexcept { return { x - dx, y - dy, w + T{2} *dx, h + T{2} *dy }; }

    template<math::Arithmetic S>
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
    constexpr void ExpandToInclude(TVec2<T> p) noexcept
    {
        const T l = std::min(Left(), p.x);
        const T t = std::min(Top(), p.y);
        const T r = std::max(Right(), p.x);
        const T b = std::max(Bottom(), p.y);
        x = l; y = t; w = r - l; h = b - t;
    }

    // Clamp a point into the rectangle
    constexpr TVec2<T> ClampPoint(TVec2<T> p) const noexcept
    {
        return { std::clamp(p.x, Left(), Right()), std::clamp(p.y, Top(), Bottom()) };
    }

    static constexpr TRect Union(TRect a, TRect b) noexcept
    {
        const T l = std::min(a.Left(), b.Left());
        const T t = std::min(a.Top(), b.Top());
        const T r = std::max(a.Right(), b.Right());
        const T bt = std::max(a.Bottom(), b.Bottom());
        return { l, t, r - l, bt - t };
    }

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
constexpr TRect<T> operator+(TRect<T> rc, TVec2<T> d) noexcept { return rc.Translated(d); }

template<math::Arithmetic T>
constexpr TRect<T> operator-(TRect<T> rc, TVec2<T> d) noexcept { return { rc.x - d.x, rc.y - d.y, rc.w, rc.h }; }

// =====================================================================================
// Color (+ HSV/HSL helpers)
// =====================================================================================
struct Color
{
    uint8_t r = 255, g = 255, b = 255, a = 255;

    constexpr Color() = default;
    constexpr Color(uint8_t R, uint8_t G, uint8_t B, uint8_t A = 255) : r(R), g(G), b(B), a(A) {}

    static constexpr Color Black(uint8_t a = 255)   noexcept { return { 0,   0,   0, a }; }
    static constexpr Color White(uint8_t a = 255)   noexcept { return { 255, 255, 255, a }; }

    static constexpr Color Red(uint8_t a = 255)     noexcept { return { 255,   0,   0, a }; }
    static constexpr Color Green(uint8_t a = 255)   noexcept { return { 0, 255,   0, a }; }
    static constexpr Color Blue(uint8_t a = 255)    noexcept { return { 0,   0, 255, a }; }

    static constexpr Color Yellow(uint8_t a = 255)  noexcept { return { 255, 255,   0, a }; }
    static constexpr Color Cyan(uint8_t a = 255)    noexcept { return { 0, 255, 255, a }; }
    static constexpr Color Magenta(uint8_t a = 255) noexcept { return { 255,   0, 255, a }; }

    static constexpr Color Gray(uint8_t a = 255)       noexcept { return { 128, 128, 128, a }; }
    static constexpr Color LightGray(uint8_t a = 255)  noexcept { return { 211, 211, 211, a }; }
    static constexpr Color DarkGray(uint8_t a = 255)   noexcept { return { 64,  64,  64, a }; }

    static constexpr Color Orange(uint8_t a = 255) noexcept { return { 255, 165,   0, a }; }
    static constexpr Color Pink(uint8_t a = 255)   noexcept { return { 255, 192, 203, a }; }
    static constexpr Color Purple(uint8_t a = 255) noexcept { return { 128,   0, 128, a }; }
    static constexpr Color Brown(uint8_t a = 255)  noexcept { return { 139,  69,  19, a }; }

    static constexpr Color Lime(uint8_t a = 255)   noexcept { return { 0, 255,   0, a }; }
    static constexpr Color Navy(uint8_t a = 255)   noexcept { return { 0,   0, 128, a }; }
    static constexpr Color Teal(uint8_t a = 255)   noexcept { return { 0, 128, 128, a }; }
    static constexpr Color Olive(uint8_t a = 255)  noexcept { return { 128, 128,   0, a }; }
    static constexpr Color Maroon(uint8_t a = 255) noexcept { return { 128,   0,   0, a }; }
    static constexpr Color Silver(uint8_t a = 255) noexcept { return { 192, 192, 192, a }; }

    friend constexpr bool operator==(Color A, Color B) noexcept
    {
        return A.r == B.r && A.g == B.g && A.b == B.b && A.a == B.a;
    }
    friend constexpr bool operator!=(Color A, Color B) noexcept { return !(A == B); }

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
    friend inline Color operator+(Color c, int v) noexcept
    {
        auto sat = [](int x) { return static_cast<uint8_t>(std::clamp(x, 0, 255)); };
        return { sat(int(c.r) + v), sat(int(c.g) + v), sat(int(c.b) + v), c.a };
    }
    friend inline Color operator-(Color c, int v) noexcept { return c + (-v); }

    // Multiply RGB by scalar
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
    inline Color Premultiplied() const noexcept
    {
        const float af = static_cast<float>(a) / 255.f;
        return (*this) * af;
    }

    // ---- HSV (H in [0,360), S,V in [0,1]) ----
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

// =====================================================================================
// Aliases (FLOAT-first)
// =====================================================================================
using Vec2I = TVec2<int>;
using Vec2 = TVec2<float>;

using Vec3I = TVec3<int>;
using Vec3 = TVec3<float>;

using RectI = TRect<int>;
using Rect = TRect<float>;

// =====================================================================================
// Conversions (explicit + helpers "round to int")
// =====================================================================================
template<math::Arithmetic TO, math::Arithmetic FROM>
constexpr TVec2<TO> Vec2Cast(TVec2<FROM> v) noexcept { return { static_cast<TO>(v.x), static_cast<TO>(v.y) }; }

template<math::Arithmetic TO, math::Arithmetic FROM>
constexpr TVec3<TO> Vec3Cast(TVec3<FROM> v) noexcept { return { static_cast<TO>(v.x), static_cast<TO>(v.y), static_cast<TO>(v.z) }; }

template<math::Arithmetic TO, math::Arithmetic FROM>
constexpr TRect<TO> RectCast(TRect<FROM> r) noexcept
{
    return { static_cast<TO>(r.x), static_cast<TO>(r.y), static_cast<TO>(r.w), static_cast<TO>(r.h) };
}

inline Vec2I  ToVec2Round(Vec2 v) noexcept { return { (int)std::lround(v.x), (int)std::lround(v.y) }; }
inline Vec3I  ToVec3Round(Vec3 v) noexcept { return { (int)std::lround(v.x), (int)std::lround(v.y), (int)std::lround(v.z) }; }
inline RectI  ToRectRound(Rect r) noexcept { return { (int)std::lround(r.x), (int)std::lround(r.y), (int)std::lround(r.w), (int)std::lround(r.h) }; }

// =====================================================================================
// Constants
// =====================================================================================
namespace Vector2
{
    inline constexpr Vec2I  Zero{ Vec2I::Zero() };
    inline constexpr Vec2I  One{ Vec2I::One() };
    inline constexpr Vec2I  Right{ Vec2I::Right() };
    inline constexpr Vec2I  Up{ Vec2I::Up() };

    inline constexpr Vec2 Zerof{ Vec2::Zero() };
    inline constexpr Vec2 Onef{ Vec2::One() };
    inline constexpr Vec2 Rightf{ Vec2::Right() };
    inline constexpr Vec2 Upf{ Vec2::Up() };
}

namespace Vector3
{
    inline constexpr Vec3I  Zero{ Vec3I::Zero() };
    inline constexpr Vec3I  One{ Vec3I::One() };
    inline constexpr Vec3I  Right{ Vec3I::Right() };
    inline constexpr Vec3I  Up{ Vec3I::Up() };
    inline constexpr Vec3I  Forward{ Vec3I::Forward() };

    inline constexpr Vec3 Zerof{ Vec3::Zero() };
    inline constexpr Vec3 Onef{ Vec3::One() };
    inline constexpr Vec3 Rightf{ Vec3::Right() };
    inline constexpr Vec3 Upf{ Vec3::Up() };
    inline constexpr Vec3 Forwardf{ Vec3::Forward() };
}