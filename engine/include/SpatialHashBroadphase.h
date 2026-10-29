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
namespace SpatialHashBroadphase {
/** @brief Inclusive world AABB edges (touching boundaries may be candidates). */
struct Bounds { double minX, minY, maxX, maxY; };

/** @brief Hash-grid accumulator retaining oversized/invalid bounds globally.
 * @note Query candidate lists are sorted/unique; callers still perform
 *       actual narrow-phase tests. Not a public scene or collider container.
 */
class Grid {
public:
    explicit Grid(double cellSize = 2.0) noexcept
        : mCellSize(std::isfinite(cellSize) && cellSize > 0.0 ? cellSize : 2.0) {}

    /** @brief Rebuilds cell occupancy from a snapshot of collider AABBs. */
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
    struct Key {
        std::int64_t x, y;
        bool operator==(const Key&) const noexcept = default;
    };
    struct Hash {
        std::size_t operator()(const Key& k) const noexcept {
            auto a=static_cast<std::uint64_t>(k.x);
            auto b=static_cast<std::uint64_t>(k.y);
            a ^= a >> 30; a *= 0xbf58476d1ce4e5b9ULL;
            b ^= b >> 27; b *= 0x94d049bb133111ebULL;
            return static_cast<std::size_t>(a ^ (b + 0x9e3779b97f4a7c15ULL + (a<<6) + (a>>2)));
        }
    };
    struct Range { std::int64_t x0=0, y0=0, x1=0, y1=0; bool valid=false; };
    static constexpr double kLimit = 1000000000.0;
    static constexpr std::uint64_t kMaxOccupiedCells = 128;
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
    double mCellSize;
    std::unordered_map<Key,std::vector<std::size_t>,Hash> mCells;
    std::vector<std::size_t> mGlobal;
    std::vector<Range> mRanges;
};
} // namespace SpatialHashBroadphase
