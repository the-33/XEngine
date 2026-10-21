#pragma once
#include <type_traits>
#include <utility>
#include <concepts>

// ======================================================
// Helpers comunes (internos)
// ======================================================
namespace detail
{
    template<class P>
    concept PropLike = requires(const P & p) { p.get(); };

    template<class X>
    constexpr decltype(auto) unwrap(const X& x) noexcept
    {
        if constexpr (PropLike<X>) return x.get();
        else return x;
    }
}

// ======================================================
// PropertyRO (read-only)
// ======================================================
template <class Owner, class T, T(Owner::* Getter)() const noexcept>
struct PropertyRO
{
    Owner* self = nullptr;

    constexpr explicit PropertyRO(Owner* o) noexcept : self(o) {}

    // Lectura
    constexpr T get() const noexcept { return (self->*Getter)(); }
    constexpr operator T() const noexcept { return get(); }

    // operator->:
    constexpr auto operator->() const noexcept
        requires (std::is_pointer_v<T>)
    {
        return get();
    }

    constexpr const T* operator->() const noexcept
        requires (!std::is_pointer_v<T>)
    {
        cache = get();
        return &cache;
    }

    // Unary operators (valid here)
    friend constexpr auto operator+(const PropertyRO& x) noexcept
        requires requires { +detail::unwrap(x); }
    { return +detail::unwrap(x); }

    friend constexpr auto operator-(const PropertyRO& x) noexcept
        requires requires { -detail::unwrap(x); }
    { return -detail::unwrap(x); }

    friend constexpr auto operator~(const PropertyRO& x) noexcept
        requires requires { ~detail::unwrap(x); }
    { return ~detail::unwrap(x); }

    friend constexpr bool operator!(const PropertyRO& x) noexcept
        requires requires { !detail::unwrap(x); }
    { return !detail::unwrap(x); }

private:
    mutable T cache{};
};

// ======================================================
// Property (lectura/escritura)
// ======================================================
template <class Owner, class T,
    T(Owner::* Getter)() const noexcept,
    void(Owner::* Setter)(T) noexcept>
struct Property
{
    Owner* self = nullptr;

    constexpr explicit Property(Owner* o) noexcept : self(o) {}

    Property(const Property&) = delete;
    Property(Property&&) = delete;
    Property& operator=(Property&&) = delete;

    // Lectura
    constexpr T get() const noexcept { return (self->*Getter)(); }
    constexpr operator T() const noexcept { return get(); }

    // Escritura
    constexpr Property& operator=(T value) noexcept {
        (self->*Setter)(value);
        return *this;
    }

    constexpr Property& operator=(const Property& rhs) noexcept {
        return (*this = rhs.get());
    }

    // operator->:
    constexpr auto operator->() const noexcept
        requires (std::is_pointer_v<T>)
    {
        return get();
    }

    constexpr const T* operator->() const noexcept
        requires (!std::is_pointer_v<T>)
    {
        cache = get();
        return &cache;
    }

    // Shared Apply implementation (for op=)
    template<class U, class Op>
        requires requires (T a, const U& b, Op op) { op(a, b); }
    constexpr Property& Apply(const U& v, Op op) noexcept
    {
        T tmp = get();
        op(tmp, v);
        (self->*Setter)(tmp);
        return *this;
    }

    // op= (now constrained by requires, so invalid operations are excluded)
#define PROP_OP_EQ(OP) \
    template<class U> \
        requires requires (T a, const U& b) { a OP##= b; } \
    constexpr Property& operator OP##= (const U& v) noexcept { \
        return Apply(v, [](T& a, const U& b) { a OP##= b; }); \
    }

    PROP_OP_EQ(+)
        PROP_OP_EQ(-)
        PROP_OP_EQ(*)
        PROP_OP_EQ(/ )
        PROP_OP_EQ(%)
        PROP_OP_EQ(&)
        PROP_OP_EQ(| )
        PROP_OP_EQ(^)
        PROP_OP_EQ(<< )
        PROP_OP_EQ(>> )

#undef PROP_OP_EQ

        // ++ / -- (only if supported by T)
        constexpr Property& operator++() noexcept
        requires requires (T a) { ++a; }
    {
        T tmp = get();
        ++tmp;
        (self->*Setter)(tmp);
        return *this;
    }

    constexpr T operator++(int) noexcept
        requires requires (T a) { a++; }
    {
        T old = get();
        T tmp = old;
        tmp++;
        (self->*Setter)(tmp);
        return old;
    }

    constexpr Property& operator--() noexcept
        requires requires (T a) { --a; }
    {
        T tmp = get();
        --tmp;
        (self->*Setter)(tmp);
        return *this;
    }

    constexpr T operator--(int) noexcept
        requires requires (T a) { a--; }
    {
        T old = get();
        T tmp = old;
        tmp--;
        (self->*Setter)(tmp);
        return old;
    }

    // Unary operators (valid here)
    friend constexpr auto operator+(const Property& x) noexcept
        requires requires { +detail::unwrap(x); }
    { return +detail::unwrap(x); }

    friend constexpr auto operator-(const Property& x) noexcept
        requires requires { -detail::unwrap(x); }
    { return -detail::unwrap(x); }

    friend constexpr auto operator~(const Property& x) noexcept
        requires requires { ~detail::unwrap(x); }
    { return ~detail::unwrap(x); }

    friend constexpr bool operator!(const Property& x) noexcept
        requires requires { !detail::unwrap(x); }
    { return !detail::unwrap(x); }

private:
    mutable T cache{};
};

// ======================================================
// Operadores BINARIOS / COMPARACIONES (FUERA)
//  - Avoid Self-vs-Self ambiguity
//  - Operate between Property and PropertyRO
// ======================================================

#define PROP_GLOBAL_BIN(OP) \
template<detail::PropLike L, detail::PropLike R> \
constexpr auto operator OP (const L& lhs, const R& rhs) noexcept \
    requires requires { detail::unwrap(lhs) OP detail::unwrap(rhs); } \
{ return detail::unwrap(lhs) OP detail::unwrap(rhs); } \
\
template<detail::PropLike L, class R> \
constexpr auto operator OP (const L& lhs, const R& rhs) noexcept \
    requires (!detail::PropLike<R>) && requires { detail::unwrap(lhs) OP rhs; } \
{ return detail::unwrap(lhs) OP rhs; } \
\
template<class L, detail::PropLike R> \
constexpr auto operator OP (const L& lhs, const R& rhs) noexcept \
    requires (!detail::PropLike<L>) && requires { lhs OP detail::unwrap(rhs); } \
{ return lhs OP detail::unwrap(rhs); }

PROP_GLOBAL_BIN(+)
PROP_GLOBAL_BIN(-)
PROP_GLOBAL_BIN(*)
PROP_GLOBAL_BIN(/ )
PROP_GLOBAL_BIN(%)
PROP_GLOBAL_BIN(&)
PROP_GLOBAL_BIN(| )
PROP_GLOBAL_BIN(^)
PROP_GLOBAL_BIN(<< )
PROP_GLOBAL_BIN(>> )

#undef PROP_GLOBAL_BIN


#define PROP_GLOBAL_CMP(OP) \
template<detail::PropLike L, detail::PropLike R> \
constexpr bool operator OP (const L& lhs, const R& rhs) noexcept \
    requires requires { detail::unwrap(lhs) OP detail::unwrap(rhs); } \
{ return detail::unwrap(lhs) OP detail::unwrap(rhs); } \
\
template<detail::PropLike L, class R> \
constexpr bool operator OP (const L& lhs, const R& rhs) noexcept \
    requires (!detail::PropLike<R>) && requires { detail::unwrap(lhs) OP rhs; } \
{ return detail::unwrap(lhs) OP rhs; } \
\
template<class L, detail::PropLike R> \
constexpr bool operator OP (const L& lhs, const R& rhs) noexcept \
    requires (!detail::PropLike<L>) && requires { lhs OP detail::unwrap(rhs); } \
{ return lhs OP detail::unwrap(rhs); }

PROP_GLOBAL_CMP(== )
PROP_GLOBAL_CMP(!= )
PROP_GLOBAL_CMP(< )
    PROP_GLOBAL_CMP(<= )
    PROP_GLOBAL_CMP(> )
    PROP_GLOBAL_CMP(>= )

#undef PROP_GLOBAL_CMP
