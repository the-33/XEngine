#pragma once
/** @file StatsOverlayLayout.h
 * @brief Viewport-constrained F3 statistics overlay column/page arrangement.
 */

#include <algorithm>
#include <cstddef>
#include <vector>

/** @brief SDL-independent layout of the optional F3 diagnostic overlay.
 * @details Diagnostic lines are grouped into blocks. Blocks that fit a fresh
 * column remain intact; oversized blocks split across columns or pages.
 */
namespace XEngineStatsOverlay
{
    /** @brief One logical line's output position in the overlay.
     */
    struct Placement
    {
        std::size_t line = 0; ///< Original line index supplied by the caller.
        float x = 0.f;       ///< Horizontal pixel position within the window.
        float y = 0.f;       ///< Vertical pixel position within the window.
    };

    /** @brief Lines assigned to a single overlay page.
     */
    struct Page
    {
        std::vector<Placement> lines; ///< Positions in presentation order.
        float usedHeight = 0.f;       ///< Maximum occupied vertical coordinate.
        unsigned usedColumns = 0;    ///< Number of columns containing at least one line.
    };

    /** @brief Complete page/column packing result.
     */
    struct Layout
    {
        std::vector<Page> pages; ///< Includes one empty page when no lines are supplied.
        unsigned columnsPerPage = 1; ///< At most three columns per page.
        float columnWidth = 0.f;     ///< Effective width clamped to the available space.
    };

    /** @brief Packs lines into viewport columns and overflow pages.
     * @param blocks Ordered sets of line indices; each set is kept together
     *               where room permits, but can split if taller than a column.
     * @param windowWidth Viewport width, in pixels.
     * @param windowHeight Viewport height, in pixels.
     * @param desiredColumnWidth Preferred width per text column.
     * @param lineHeight Requested per-line advance; clamped to at least 1.
     * @param margin Left/right and bottom padding.
     * @param headerHeight Reserved height above the overlay lines.
     * @param footerHeight Reserved height below the overlay lines.
     * @param columnGap Horizontal distance between columns.
     * @param blockGap Vertical distance between distinct blocks.
     * @return Deterministic positions; rendering and page navigation are
     *         handled by the overlay renderer, not this utility.
     */
    inline Layout Pack(const std::vector<std::vector<std::size_t>>& blocks,
                       float windowWidth, float windowHeight,
                       float desiredColumnWidth, float lineHeight,
                       float margin = 12.f, float headerHeight = 48.f,
                       float footerHeight = 36.f, float columnGap = 20.f,
                       float blockGap = 10.f)
    {
        Layout out;
        const float usableWidth = std::max(1.f, windowWidth - 2.f * margin);
        const float usableHeight = std::max(1.f, windowHeight - headerHeight - footerHeight - margin);
        const float rowHeight = std::max(1.f, lineHeight);
        out.columnWidth = std::clamp(desiredColumnWidth, 1.f, usableWidth);
        out.columnsPerPage = std::min(3u, std::max(1u, static_cast<unsigned>(
            (usableWidth + columnGap) / (out.columnWidth + columnGap))));

        out.pages.emplace_back();
        unsigned column = 0;
        float y = 0.f;

        const auto nextColumn = [&]()
        {
            ++column;
            y = 0.f;
            if (column >= out.columnsPerPage)
            {
                column = 0;
                out.pages.emplace_back();
            }
        };

        for (const auto& block : blocks)
        {
            if (block.empty()) continue;
            const float fullHeight = static_cast<float>(block.size()) * rowHeight;
            // Keep entire blocks together when they fit in a fresh column.
            if (y > 0.f && fullHeight <= usableHeight && y + blockGap + fullHeight > usableHeight)
                nextColumn();
            else if (y > 0.f && y + blockGap + rowHeight > usableHeight)
                nextColumn();
            else if (y > 0.f)
                y += blockGap;

            for (const std::size_t lineIndex : block)
            {
                if (y > 0.f && y + rowHeight > usableHeight)
                    nextColumn();
                Page& page = out.pages.back();
                page.lines.push_back({lineIndex,
                    margin + static_cast<float>(column) * (out.columnWidth + columnGap),
                    headerHeight + y});
                y += rowHeight;
                page.usedHeight = std::max(page.usedHeight, headerHeight + y);
                page.usedColumns = std::max(page.usedColumns, column + 1);
            }
        }
        // Keep a single valid page even when nothing is registered yet.
        return out;
    }
}
