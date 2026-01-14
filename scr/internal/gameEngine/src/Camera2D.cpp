#include "Camera2D.h"

#include "WindowManager.h"

static inline float clampf(float v, float lo, float hi)
{
	return std::max(lo, std::min(v, hi));
}

void Camera2D::SetCenter(Vec2 center) noexcept
{
	mCenterX = center.x;
	mCenterY = center.y;
}

void Camera2D::Move(Vec2 delta) noexcept
{
	mCenterX += delta.x;
	mCenterY += delta.y;
}

void Camera2D::SetZoom(float zoom) noexcept
{
	// evita negativos o ceros extremos
	mZoom = clampf(zoom, 0.01f, 100.f);
}

void Camera2D::SetViewBase(Vec2 viewBase) noexcept
{
	// evita cero o negativo
	if (viewBase.x > 1e-3f) mBaseViewWidth = viewBase.x;
	if (viewBase.y > 1e-3f) mBaseViewHeight = viewBase.y;
}

Vec2 Camera2D::GetCenter() const noexcept
{
	return { mCenterX, mCenterY };
}

float Camera2D::GetZoom() const noexcept
{
	return mZoom;
}

Vec2 Camera2D::GetViewBase() const noexcept
{
	// ancho visible en mundo = base / zoom
	return (mZoom > 0.f) ? Vec2{(mBaseViewWidth / mZoom), (mBaseViewHeight / mZoom)} : Vec2{ mBaseViewWidth, mBaseViewHeight};
}

Rect Camera2D::ViewRect() const noexcept {
	// rectángulo visible en mundo (top-left + tamaño)
	const Vec2 view = GetViewBase();
	const float left = mCenterX - view.x * 0.5f;
	const float top = mCenterY - view.y * 0.5f;
	return { left, top, view.x, view.y };
}


Vec2 Camera2D::WorldToScreen(float wx, float wy, int winW, int winH) const noexcept
{
	const Vec2 view = GetViewBase();
	const float vw = view.x;
	const float vh = view.y;
	const float left = mCenterX - vw * 0.5f;
	const float top = mCenterY - vh * 0.5f;

	const float nx = (wx - left) / vw;  // 0..1
	const float ny = (wy - top) / vh;  // 0..1

	float sx = nx * static_cast<float>(winW);
	float sy = ny * static_cast<float>(winH);

	return { sx, sy };

	// Si el mundo tiene Y hacia ARRIBA, usa en su lugar:
	// sy = (1.0f - ny) * static_cast<float>(winH);
}

Vec2 Camera2D::ScreenToWorld(float sx, float sy, int winW, int winH) const noexcept
{
	const Vec2 view = GetViewBase();
	const float vw = view.x;
	const float vh = view.y;
	const float left = mCenterX - vw * 0.5f;
	const float top = mCenterY - vh * 0.5f;

	const float nx = (winW > 0) ? (sx / static_cast<float>(winW)) : 0.f; // 0..1
	const float ny = (winH > 0) ? (sy / static_cast<float>(winH)) : 0.f; // 0..1

	float wx = left + nx * vw;
	float wy = top + ny * vh;

	return { wx, wy };

	// Si el mundo tiene Y hacia ARRIBA, usa:
	// wy = top + (1.0f - ny) * vh;
}

void Camera2D::Follow(const Vec2& target, float smooth, float dt) noexcept
{
	// smooth = "velocidad de seguimiento" (por segundo)
	// cuanto mayor sea smooth, más rápido sigue al objetivo.
	if (smooth <= 0.f) {
		// snap instantáneo
		mCenterX = target.x;
		mCenterY = target.y;
		return;
	}

	// factor de interpolación dependiente del tiempo:
	// k = 1 - exp(-smooth * dt)
	// esto produce una transición exponencial suave, estable para FPS variables.
	const float k = 1.f - std::exp(-smooth * dt);

	mCenterX += (target.x - mCenterX) * k;
	mCenterY += (target.y - mCenterY) * k;
}

void Camera2D::ClampToWorld(const Rect& worldBounds) noexcept
{
	// Asegura que el rectángulo visible cabe dentro de worldBounds.
	// Si el mundo es más pequeño que la vista, centra la cámara.
	const Vec2 view = GetViewBase();
	const float vw = view.x;
	const float vh = view.y;

	float minLeft = worldBounds.x;
	float minTop = worldBounds.y;
	float maxLeft = worldBounds.x + worldBounds.w - vw;
	float maxTop = worldBounds.y + worldBounds.h - vh;

	float left, top;

	if (worldBounds.w <= vw) {
		// mundo más pequeño que la vista en X -> centrar
		left = worldBounds.x + (worldBounds.w - vw) * 0.5f;
	}
	else {
		const float curLeft = mCenterX - vw * 0.5f;
		left = clampf(curLeft, minLeft, maxLeft);
	}

	if (worldBounds.h <= vh) {
		top = worldBounds.y + (worldBounds.h - vh) * 0.5f;
	}
	else {
		const float curTop = mCenterY - vh * 0.5f;
		top = clampf(curTop, minTop, maxTop);
	}

	mCenterX = left + vw * 0.5f;
	mCenterY = top + vh * 0.5f;
}

Vec2 Camera2D::WorldToScreen(const Vec2& worldPos) const noexcept
{
	Vec2I drawableSize = WindowManager::GetInstancePtr()->GetDrawableSize();

	return WorldToScreen(worldPos.x, worldPos.y,
		drawableSize.x,
		drawableSize.y);
}

Vec2 Camera2D::ScreenToWorld(const Vec2& screenPos) const noexcept
{
	Vec2I drawableSize = WindowManager::GetInstancePtr()->GetDrawableSize();

	return ScreenToWorld(screenPos.x, screenPos.y,
		drawableSize.x,
		drawableSize.y);
}
