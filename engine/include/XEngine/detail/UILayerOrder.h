#pragma once

#include <cstddef>

/** @brief Namespace collecting ui functionality for XEngine.
 */
namespace xengine::detail::ui {

// Draw commands are stable-sorted by ascending layer. Within one layer the
// later submission is on top, so pointer ownership uses this same comparison.
/** @brief Test whether ui is above.
 * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
 * @param sequence Submission sequence used to break equal-layer ties.
 * @param otherLayer Layer against which the first element is compared.
 * @param otherSequence Submission sequence of the other element.
 * @return True when is above; otherwise false.
 */
inline constexpr bool IsAbove(int layer, std::size_t sequence,
                              int otherLayer, std::size_t otherSequence) noexcept
{
    return layer > otherLayer ||
           (layer == otherLayer && sequence > otherSequence);
}

} // namespace xengine::detail::ui
