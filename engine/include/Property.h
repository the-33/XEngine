#pragma once
/** @file Property.h
 * @brief Unity-like owner-bound read/write and read-only property adapters.
 */
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
    /** @brief Returns the underlying value represented by a property accessor.
     * @param x A regular value or owner-bound property accessor.
     */
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
/** @brief Owner-bound read-only property exposing the owner's getter.
 * @ingroup xengine_utilities
 */
struct PropertyRO
{
    /** @cond XENGINE_INTERNAL */
    /** @brief Non-owning pointer to the getter's owner; do not access from gameplay code.
     */
    Owner* self = nullptr;
    /** @endcond */

    /** @brief Constructs a PropertyRO instance.
     * @param o Object or owner associated with this operation.
     */
    constexpr explicit PropertyRO(Owner* o) noexcept : self(o) {}

    // Lectura
    /** @brief Reads the current value from its owning object.
     */
    constexpr T get() const noexcept { return (self->*Getter)(); }
    /** @brief Implicit conversion that reads the current value via its getter.
     */
    constexpr operator T() const noexcept { return get(); }

    // operator->:
    /** @brief For pointer values, returns the current pointer from the owner.
     */
    constexpr auto operator->() const noexcept
        requires (std::is_pointer_v<T>)
    {
        return get();
    }

    /** @brief For value types, returns a pointer to a cached copy of the getter result.
     * @warning The returned pointer is invalidated by the next property arrow
     *          access and must not be stored; modifying it does not call a setter.
     */
    constexpr const T* operator->() const noexcept
        requires (!std::is_pointer_v<T>)
    {
        cache = get();
        return &cache;
    }

    // Unary operators (valid here)
    /** @brief Applies unary plus to the current read-only value. */
    friend constexpr auto operator+(const PropertyRO& x) noexcept
        requires requires { +detail::unwrap(x); }
    { return +detail::unwrap(x); }

    /** @brief Applies unary minus to the current read-only value, if supported. */
    friend constexpr auto operator-(const PropertyRO& x) noexcept
        requires requires { -detail::unwrap(x); }
    { return -detail::unwrap(x); }

    /** @brief Applies bitwise complement to the current read-only value, if supported. */
    friend constexpr auto operator~(const PropertyRO& x) noexcept
        requires requires { ~detail::unwrap(x); }
    { return ~detail::unwrap(x); }

    /** @brief Computes logical negation of the current read-only value, if supported. */
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
/** @brief Owner-bound read/write property that delegates assignment to the owner's setter.
 * @ingroup xengine_utilities
 */
struct Property
{
    /** @cond XENGINE_INTERNAL */
    /** @brief Non-owning pointer to the accessor's owner; do not access from gameplay code.
     */
    Owner* self = nullptr;
    /** @endcond */

    /** @brief Constructs a Property instance.
     * @param o Object or owner associated with this operation.
     */
    constexpr explicit Property(Owner* o) noexcept : self(o) {}

    /** @brief Constructs a Property instance.
     */
    Property(const Property&) = delete;
    /** @brief Constructs a Property instance.
     */
    Property(Property&&) = delete;
    /** @brief Implements operator= for this value or property type.
     */
    Property& operator=(Property&&) = delete;

    // Lectura
    /** @brief Reads the current value from its owning object.
     */
    constexpr T get() const noexcept { return (self->*Getter)(); }
    /** @brief Implicit conversion that reads the current owner value.
     */
    constexpr operator T() const noexcept { return get(); }

    // Escritura
    /** @brief Assigns a value by invoking the owner's setter (which may validate it).
     * @param value Value passed to the owner setter.
     */
    constexpr Property& operator=(T value) noexcept {
        (self->*Setter)(value);
        return *this;
    }

    /** @brief Implements operator= for this value or property type.
     * @param rhs Right-hand operand.
     */
    constexpr Property& operator=(const Property& rhs) noexcept {
        return (*this = rhs.get());
    }

    // operator->:
    /** @brief For pointer values, returns the current pointer from the owner.
     */
    constexpr auto operator->() const noexcept
        requires (std::is_pointer_v<T>)
    {
        return get();
    }

    /** @brief For value types, returns a pointer to a cached copy of the getter result.
     * @warning Writing through this pointer does not invoke the owner's setter.
     */
    constexpr const T* operator->() const noexcept
        requires (!std::is_pointer_v<T>)
    {
        cache = get();
        return &cache;
    }

    // Shared Apply implementation (for op=)
    /** @brief Reads the current value, applies a compound operation and invokes the owner's setter.
     * @param v Right-hand operand of the compound operation.
     * @param op Callable that mutates the temporary value before assignment.
     * @return This property for chaining compound assignments.
     */
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
/** @brief Convenience alias for accessing prop op eq from gameplay code.
 */
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
        /** @brief Implements operator++ for this value or property type.
         */
        constexpr Property& operator++() noexcept
        requires requires (T a) { ++a; }
    {
        T tmp = get();
        ++tmp;
        (self->*Setter)(tmp);
        return *this;
    }

    /** @brief Implements operator++ for this value or property type.
     */
    constexpr T operator++(int) noexcept
        requires requires (T a) { a++; }
    {
        T old = get();
        T tmp = old;
        tmp++;
        (self->*Setter)(tmp);
        return old;
    }

    /** @brief Implements operator-- for this value or property type.
     */
    constexpr Property& operator--() noexcept
        requires requires (T a) { --a; }
    {
        T tmp = get();
        --tmp;
        (self->*Setter)(tmp);
        return *this;
    }

    /** @brief Implements operator-- for this value or property type.
     */
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
    /** @brief Applies unary plus to the current writable property value. */
    friend constexpr auto operator+(const Property& x) noexcept
        requires requires { +detail::unwrap(x); }
    { return +detail::unwrap(x); }

    /** @brief Applies unary minus to the current writable property value, if supported. */
    friend constexpr auto operator-(const Property& x) noexcept
        requires requires { -detail::unwrap(x); }
    { return -detail::unwrap(x); }

    /** @brief Applies bitwise complement to the current writable property value, if supported. */
    friend constexpr auto operator~(const Property& x) noexcept
        requires requires { ~detail::unwrap(x); }
    { return ~detail::unwrap(x); }

    /** @brief Computes logical negation of the current writable property value, if supported. */
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

/** @brief Convenience alias for accessing prop global bin from gameplay code.
 */
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


/** @brief Convenience alias for accessing prop global cmp from gameplay code.
 */
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
