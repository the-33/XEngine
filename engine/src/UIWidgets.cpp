/** @file UIWidgets.cpp
 * @brief Canvas registration, scene cleanup and automatic retained UI submission.
 */
#include "UIWidgets.h"

#include <algorithm>
#include <stdexcept>

UIManager::~UIManager() noexcept = default;

UIWidgetCanvas& UIManager::CreateCanvas(const std::string& key)
{
    if (key.empty() || key.find('/') != std::string::npos)
        throw std::invalid_argument("UIManager::CreateCanvas: key must be nonempty and contain no '/'");
    auto existing = mWidgetCanvases.find(key);
    if (existing != mWidgetCanvases.end()) return *existing->second;

    auto canvas = std::make_unique<UIWidgetCanvas>(key, mCanvas);
    UIWidgetCanvas& result = *canvas;
    mWidgetCanvases.emplace(key, std::move(canvas));
    mWidgetCanvasOrder.push_back(key);
    return result;
}

UIWidgetCanvas* UIManager::FindCanvas(std::string_view key) noexcept
{
    const auto found = mWidgetCanvases.find(std::string(key));
    return found == mWidgetCanvases.end() ? nullptr : found->second.get();
}

const UIWidgetCanvas* UIManager::FindCanvas(std::string_view key) const noexcept
{
    const auto found = mWidgetCanvases.find(std::string(key));
    return found == mWidgetCanvases.end() ? nullptr : found->second.get();
}

bool UIManager::DestroyCanvas(std::string_view key) noexcept
{
    const std::string name(key);
    if (mWidgetCanvases.erase(name) == 0) return false;
    mWidgetCanvasOrder.erase(std::remove(mWidgetCanvasOrder.begin(), mWidgetCanvasOrder.end(), name),
                            mWidgetCanvasOrder.end());
    return true;
}

void UIManager::ClearWidgetCanvases() noexcept
{
    mWidgetCanvases.clear();
    mWidgetCanvasOrder.clear();
    mWidgetInteractions.clear();
}

void UIManager::ClearSceneWidgetCanvases() noexcept
{
    const auto oldOrder = mWidgetCanvasOrder;
    for (const auto& key : oldOrder) {
        const auto found = mWidgetCanvases.find(key);
        if (found != mWidgetCanvases.end() && !found->second->IsPersistent())
            DestroyCanvas(key);
    }
}

void UIManager::DrawWidgetCanvases_() noexcept
{
    if (mWidgetCanvases.empty()) return;

    // No user callback is run while traversing widgets. This allows an OnClick
    // listener to remove its own element/canvas or change scenes safely.
    UIWidgetEvents events;
    for (const auto& key : mWidgetCanvasOrder) {
        auto it = mWidgetCanvases.find(key);
        if (it != mWidgetCanvases.end()) it->second->Submit(*this, events);
    }

    for (auto& callback : events)
        if (callback) callback();
}
