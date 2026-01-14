#pragma once

#include <algorithm>
#include <cmath>

#include "BaseTypes.h"
#include "Property.h"
#include "InputManager.h"
#include "Scene.h"
#include "RenderManager.h"

class Camera2D
{
    friend class Scene;
	friend class InputManager;
	friend class RenderManager;

private:
    float mCenterX = 0.f, mCenterY = 0.f;
    float mBaseViewWidth = 20.f, mBaseViewHeight = 11.25f;
    float mZoom = 1.f;

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
    Rect ViewRect() const noexcept;

    // --- Conversiones ---
    Vec2 WorldToScreen(float wx, float wy,
        int winW, int winH) const noexcept;
    Vec2 ScreenToWorld(float sx, float sy,
        int winW, int winH) const noexcept;

public:
    // --- Comportamientos avanzados ---
    // Follow: smooth = 0 por defecto. El parámetro dt debe proporcionarse explícitamente;
    // no se debe confiar en el valor por defecto para la lógica dependiente del tiempo.
    void Follow(const Vec2& target, float smooth = 0.f, float dt = 0.f /* dt debe proporcionarse explícitamente */) noexcept;
    void ClampToWorld(const Rect& worldBounds) noexcept;

    using CenterProperty = Property<Camera2D, Vec2,
        &Camera2D::GetCenter,
        &Camera2D::SetCenter>;
    CenterProperty center{ this };

    using ZoomProperty = Property<Camera2D, float,
        &Camera2D::GetZoom,
        &Camera2D::SetZoom>;
    ZoomProperty zoom{ this };

    using ViewProperty = Property<Camera2D, Vec2,
        &Camera2D::GetViewBase,
        &Camera2D::SetViewBase>;
    CenterProperty viewBase{ this };

    using ViewRectProperty = PropertyRO<Camera2D, 
        Rect, 
        &Camera2D::ViewRect>;
    ViewRectProperty viewRect{ this };

	Vec2 WorldToScreen(const Vec2& worldPos) const noexcept;
	Vec2 ScreenToWorld(const Vec2& screenPos) const noexcept;
};