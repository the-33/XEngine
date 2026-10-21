#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// SDL-independent layout for the optional F3 diagnostic overlay.
// Blocks are kept together when possible and overflow into additional columns/pages.
namespace XEngineStatsOverlay
{
    struct Placement
    {
        std::size_t line = 0;
        float x = 0.f;
        float y = 0.f;
    };

    struct Page
    {
        std::vector<Placement> lines;
        float usedHeight = 0.f;
        unsigned usedColumns = 0;
    };

    struct Layout
    {
        std::vector<Page> pages;
        unsigned columnsPerPage = 1;
        float columnWidth = 0.f;
    };

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
