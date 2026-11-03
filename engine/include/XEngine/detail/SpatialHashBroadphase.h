#pragma once
/** @file SpatialHashBroadphase.h
 * @brief Deterministic broad-phase candidate enumeration through an AABB grid.
 */
// Spatial hash broadphase: deterministic candidate enumeration, no SDL dependency.
// AABB cell ranges are inclusive so touching boundaries cannot lose candidates.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

/** @brief CPU-only collision-grid helper used by CollisionManager. */
namespace xengine::detail::spatial_hash {
/** @brief Inclusive world AABB edges (touching boundaries may be candidates). */
struct Bounds { double minX, minY, maxX, maxY; };

/** @var double xengine::detail::spatial_hash::Bounds::minX
 * @brief Left edge of the world-space axis-aligned bounds.
 */
/** @var double xengine::detail::spatial_hash::Bounds::minY
 * @brief Upper edge of the world-space axis-aligned bounds.
 */
/** @var double xengine::detail::spatial_hash::Bounds::maxX
 * @brief Right edge of the world-space axis-aligned bounds.
 */
/** @var double xengine::detail::spatial_hash::Bounds::maxY
 * @brief Lower edge of the world-space axis-aligned bounds.
 */

/** @brief Hash-grid accumulator retaining oversized/invalid bounds globally.
 * @note Query candidate lists are sorted/unique; callers still perform
 *       actual narrow-phase tests. Not a public scene or collider container.
 */
class Grid {
public:
    /** @brief Construct a Grid instance.
     * @param cellSize Cell size used to bound or select the operation.
     */
    explicit Grid(double cellSize = 2.0) noexcept
        : mCellSize(std::isfinite(cellSize) && cellSize > 0.0 ? cellSize : 2.0) {}

    /** @brief Rebuilds cell occupancy from a snapshot of collider AABBs.
     * @param bounds Bounding rectangle of the object or region.
     */
    void Build(const std::vector<Bounds>& bounds) {
        mCells.clear(); mGlobal.clear(); mRanges.clear();
        mRanges.resize(bounds.size());
        mCells.reserve(bounds.size() * 2);
        for (std::size_t i=0; i<bounds.size(); ++i) {
            Range r;
            if (!MakeRange(bounds[i], r)) { mGlobal.push_back(i); continue; }
            mRanges[i] = r;
            for (std::int64_t x = r.x0; x <= r.x1; ++x)
                for (std::int64_t y = r.y0; y <= r.y1; ++y)
                    mCells[{x,y}].push_back(i);
        }
    }

    // Exact superset of overlapping AABBs, in the same ascending pair order
    // as nested loops (i,j), j>i. Global/oversized colliders never disappear.
    /** @brief Produces candidate indices j > i with deterministic ascending order.
     * @param i Index of the querying collider in the last Build input.
     * @param count Total collider count used to enumerate global/fallback entries.
     * @param out Replaced with candidate indices, not confirmed collisions.
     */
    void Candidates(std::size_t i, std::size_t count, std::vector<std::size_t>& out) const {
        out.clear();
        if (i >= mRanges.size()) return;
        const Range& r = mRanges[i];
        if (!r.valid) {
            out.reserve(count - i - 1);
            for (std::size_t j=i+1; j<count; ++j) out.push_back(j);
            return;
        }
        for (std::int64_t x=r.x0; x<=r.x1; ++x)
            for (std::int64_t y=r.y0; y<=r.y1; ++y) {
                const auto it=mCells.find({x,y});
                if (it == mCells.end()) continue;
                for (std::size_t j: it->second) if (j>i) out.push_back(j);
            }
        for (std::size_t j: mGlobal) if (j>i) out.push_back(j);
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(),out.end()),out.end());
    }

private:
    /** @brief Key data and utilities used by xengine::detail::spatial_hash::Grid.
     */
    struct Key {
        /** @brief Configured x value used by Key.
         */
        std::int64_t x, y;
        /** @brief Implement the operator== operation for Key.
         * @return Result of applying the overloaded operator.
         * @param other Source object whose copy, move or equality is being requested.
         */
        bool operator==(const Key& other) const noexcept = default;
    };
    /** @brief Hash data and utilities used by xengine::detail::spatial_hash::Grid.
     */
    struct Hash {
        /** @brief Implement the operator() operation for Hash.
         * @param k K supplied to Hash.
         * @return Result of applying the overloaded operator.
         */
        std::size_t operator()(const Key& k) const noexcept {
            auto a=static_cast<std::uint64_t>(k.x);
            auto b=static_cast<std::uint64_t>(k.y);
            a ^= a >> 30; a *= 0xbf58476d1ce4e5b9ULL;
            b ^= b >> 27; b *= 0x94d049bb133111ebULL;
            return static_cast<std::size_t>(a ^ (b + 0x9e3779b97f4a7c15ULL + (a<<6) + (a>>2)));
        }
    };
    /** @brief Configured x0 value used by Range.
     */
    struct Range { std::int64_t x0=0, y0=0, x1=0, y1=0; bool valid=false; };
    /** @brief Configured limit value used by Grid.
     */
    static constexpr double kLimit = 1000000000.0;
    /** @brief Configured max occupied cells value used by Grid.
     */
    static constexpr std::uint64_t kMaxOccupiedCells = 128;
    /** @brief Make range in Grid.
     * @param b Second input operand.
     * @param out Caller-provided output destination.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool MakeRange(const Bounds& b, Range& out) const noexcept {
        if (!std::isfinite(b.minX) || !std::isfinite(b.maxX) ||
            !std::isfinite(b.minY) || !std::isfinite(b.maxY) ||
            b.minX>b.maxX || b.minY>b.maxY) return false;
        const double x0=std::floor(b.minX/mCellSize), x1=std::floor(b.maxX/mCellSize);
        const double y0=std::floor(b.minY/mCellSize), y1=std::floor(b.maxY/mCellSize);
        if (!std::isfinite(x0) || !std::isfinite(x1) || !std::isfinite(y0) || !std::isfinite(y1) ||
            x0 < -kLimit || x1 > kLimit || y0 < -kLimit || y1 > kLimit) return false;
        const double cells=(x1-x0+1.0)*(y1-y0+1.0);
        if (cells>static_cast<double>(kMaxOccupiedCells)) return false;
        out = {static_cast<std::int64_t>(x0),static_cast<std::int64_t>(y0),
               static_cast<std::int64_t>(x1),static_cast<std::int64_t>(y1),true};
        return true;
    }
    /** @brief Stored cell size value used by Grid.
     */
    double mCellSize;
    /** @brief Stored cells value used by Grid.
     */
    std::unordered_map<Key,std::vector<std::size_t>,Hash> mCells;
    /** @brief Stored global value used by Grid.
     */
    std::vector<std::size_t> mGlobal;
    /** @brief Stored ranges value used by Grid.
     */
    std::vector<Range> mRanges;
};
} // namespace SpatialHashBroadphase

// Compatibility namespace: existing engine code and internal tests may still
// include the historical forwarding header and use its original spelling.
namespace SpatialHashBroadphase = xengine::detail::spatial_hash;

/** @name Co-declared engine member descriptions
 * @brief Members sharing declarations with other fields.
 * @{ */
/** @var std::int64_t xengine::detail::spatial_hash::Grid::Key::y
 * @brief Y used by Key.
 */

/** @var std::int64_t xengine::detail::spatial_hash::Grid::Range::x0
 * @brief X0 used by Range.
 */

/** @var std::int64_t xengine::detail::spatial_hash::Grid::Range::y0
 * @brief Y0 used by Range.
 */

/** @var std::int64_t xengine::detail::spatial_hash::Grid::Range::x1
 * @brief X1 coordinate of the line endpoint.
 */

/** @var std::int64_t xengine::detail::spatial_hash::Grid::Range::y1
 * @brief Y1 coordinate of the line endpoint.
 */

/** @var bool xengine::detail::spatial_hash::Grid::Range::valid
 * @brief Valid used by Range.
 */
/** @} */
