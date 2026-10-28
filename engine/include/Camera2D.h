#pragma once
/** @file Camera2D.h
 * @brief World/screen projection, visible rectangle and camera following.
 */

#include <algorithm>
#include <cmath>
#include <memory>

#include "BaseTypes.h"
#include "Property.h"
#include "InputManager.h"
#include "Scene.h"
#include "RenderManager.h"

/** @brief Maps world positions into the window; may match the current window aspect ratio.
 * @ingroup xengine_render
 */
class Camera2D
{
    friend class Scene;
    // Scene can construct a camera; only the unique_ptr deleter may destroy it.
    /** @brief Grants the standard deleter access to the scene-owned camera destructor.
     * @note This is an ownership implementation detail, not a gameplay factory.
     */
    friend struct std::default_delete<Camera2D>;
	friend class InputManager;
	friend class RenderManager;

private:
    float mCenterX = 0.f, mCenterY = 0.f;
    float mBaseViewWidth = 20.f, mBaseViewHeight = 11.25f;
    float mZoom = 1.f;
    bool mMatchWindowAspect = true;

    Camera2D() = default;
    ~Camera2D() = default;

    // --- Setters ---
    void SetCenter(Vec2 center) noexcept;
    void Move(Vec2 delta) noexcept;
    void SetZoom(float zoom) noexcept;
    void SetViewBase(Vec2 viewBase) noexcept;

    // --- Getters ---
    Vec2 GetCenter() const noexcept;
    float GetZoom() const noexcept;
    Vec2 GetViewBase() const noexcept;
    bool GetMatchWindowAspect() const noexcept { return mMatchWindowAspect; }
    void SetMatchWindowAspect(bool enabled) noexcept { mMatchWindowAspect = enabled; }
    Rect ViewRect() const noexcept;

    // --- Conversiones ---
    Vec2 WorldToScreen(float wx, float wy,
        int winW, int winH) const noexcept;
    Vec2 ScreenToWorld(float sx, float sy,
        int winW, int winH) const noexcept;

public:
    // --- Comportamientos avanzados ---
    // Follow: smooth defaults to 0. The dt parameter must be supplied explicitly;
    // do not rely on its default value in time-dependent logic.
    /** @brief Moves the camera toward a target with optional exponential smoothing and explicit delta time.
     * @param target Target point, vector or transform for this operation.
     * @param smooth Follow speed; nonpositive values move instantly.
     * @param dt Elapsed frame time in seconds.
     */
    void Follow(const Vec2& target, float smooth = 0.f, float dt = 0.f /* dt must be supplied explicitly */) noexcept;
    /** @brief Clamps the visible camera rectangle within world bounds, centering if the world is smaller.
     * @param worldBounds World-space rectangle used to constrain camera movement.
     */
    void ClampToWorld(const Rect& worldBounds) noexcept;

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `center`; use the corresponding instance member in gameplay code.
     */
    using CenterProperty = Property<Camera2D, Vec2,
        &Camera2D::GetCenter,
        &Camera2D::SetCenter>;
    /** @endcond */
    /** @brief World-space point at the center of the camera view.
     */
    CenterProperty center{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `zoom`; use the corresponding instance member in gameplay code.
     */
    using ZoomProperty = Property<Camera2D, float,
        &Camera2D::GetZoom,
        &Camera2D::SetZoom>;
    /** @endcond */
    /** @brief Camera zoom factor; higher values show a smaller world area.
     */
    ZoomProperty zoom{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `view`; use the corresponding instance member in gameplay code.
     */
    using ViewProperty = Property<Camera2D, Vec2,
        &Camera2D::GetViewBase,
        &Camera2D::SetViewBase>;
    /** @endcond */
    /** @brief Base world-space dimensions of the view before zoom and aspect adjustment.
     */
    ViewProperty viewBase{ this };

    /** @cond XENGINE_INTERNAL */
    /** Keep the vertical world span fixed, adjusting the horizontal span to the window aspect. */
    using MatchWindowAspectProp = Property<Camera2D, bool, &Camera2D::GetMatchWindowAspect,
                                            &Camera2D::SetMatchWindowAspect>;
    /** @endcond */
    /** @brief When true, adapt the horizontal world span to the current window aspect ratio.
     */
    MatchWindowAspectProp matchWindowAspect{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `view rect`; use the corresponding instance member in gameplay code.
     */
    using ViewRectProperty = PropertyRO<Camera2D, 
        Rect, 
        &Camera2D::ViewRect>;
    /** @endcond */
    /** @brief Read-only world-space rectangle currently visible through this camera.
     */
    ViewRectProperty viewRect{ this };

	/** @brief Converts world-space coordinates to screen-space coordinates.
	 * @param worldPos Point in world coordinates.
	 */
	Vec2 WorldToScreen(const Vec2& worldPos) const noexcept;
	/** @brief Converts screen-space coordinates to world-space coordinates.
	 * @param screenPos Point in logical screen coordinates.
	 */
	Vec2 ScreenToWorld(const Vec2& screenPos) const noexcept;
};
