#pragma once
/** @file SpriteRenderer.h
 * @brief Visual component for drawing sprite textures.
 */

#include "Behaviour.h"
#include "Assets.h"
#include "BaseTypes.h"

/** @brief Renders a sprite from a non-owning Texture pointer and its Transform.
 *
 * Pixel dimensions divided by Texture::PixelsPerUnit() determine the world
 * dimensions before the Transform's scale. Rotation is around `pivot01` and
 * a local offset is projected with the sprite pose.
 * @ingroup xengine_render
 */
class SpriteRenderer : public Behaviour
{
private:
    // --- Resource / crop rectangle (texture pixels) ---
    const Texture* texture = nullptr;
    int  srcX = 0;
    int  srcY = 0;
    int  srcW = 0;   // 0 => Texture::Width()
    int  srcH = 0;   // 0 => Texture::Height()

    // --- Apariencia ---
    Color tint_ = { 255, 255, 255, 255 };
    bool  flipX_ = false;
    bool  flipY_ = false;

    // --- Posicionamiento avanzado ---
    // Sprite LOCAL-SPACE offset (world units BEFORE rotation).
    // This offset rotates with the Transform (rotation applied in Render()).
    Vec2 offset_{ 0.f, 0.f };

    // Rotation/scaling pivot relative to the sprite rectangle (0..1).
    // (0,0)=esquina superior izq, (0.5,0.5)=centro, (1,1)=inferior dcha.
    Vec2 pivot01_{ 0.5f, 0.5f };

    // Conveniencia
    void SetTexture(const Texture* tex) noexcept;
    void SetSource(RectI src) noexcept;
    void SetTint(Color tint) noexcept;
	void SetFlipX(bool fx) noexcept { flipX_ = fx; }
	void SetFlipY(bool fy) noexcept { flipY_ = fy; }
    void SetOffset(Vec2 off) noexcept { offset_ = off; }
    void SetPivot01(Vec2 p) noexcept { pivot01_ = p; }

	Color GetTint() const noexcept { return tint_; }
	Vec2 GetOffset() const noexcept { return offset_; }
	bool IsFlipX() const noexcept { return flipX_; }
	bool IsFlipY() const noexcept { return flipY_; }
	Vec2 GetPivot01() const noexcept { return pivot01_; }
	const Texture* GetTexture() const noexcept { return texture; }
	RectI GetSource() const noexcept { return { srcX, srcY, srcW, srcH }; }

protected:

    // Ciclo
    /** @brief Lifecycle hook for render; the engine controls invocation order.
     */
    void Render() override; // Apply texture PPU, Transform.sx/sy, rotated offsetLocal and Transform.rotDeg

public:
    /** @brief Constructs a SpriteRenderer instance.
     */
    SpriteRenderer() = default;
    /** @brief Releases the SpriteRenderer instance and any owned resources.
     */
    ~SpriteRenderer() override = default;

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `tint`; use the corresponding instance member in gameplay code.
     */
    using TintProp = Property<SpriteRenderer, Color,
        &SpriteRenderer::GetTint,
        &SpriteRenderer::SetTint>;
    /** @endcond */
    /** @brief RGBA tint multiplied with rendered sprite pixels.
     */
    TintProp tint{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `offset`; use the corresponding instance member in gameplay code.
     */
    using OffsetProp = Property<SpriteRenderer, Vec2,
        &SpriteRenderer::GetOffset,
		&SpriteRenderer::SetOffset>;
    /** @endcond */
	/** @brief Local offset applied when placing this sprite relative to its transform.
	 */
	OffsetProp offset{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Public type alias for the flip xprop accessor.
     */
    using FlipXProp = Property<SpriteRenderer, bool,
        &SpriteRenderer::IsFlipX,
		&SpriteRenderer::SetFlipX>;
    /** @endcond */
    /** @brief Flip the sprite horizontally without changing the object transform.
     */
    FlipXProp flipX{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Public type alias for the flip yprop accessor.
     */
    using FlipYProp = Property<SpriteRenderer, bool,
		&SpriteRenderer::IsFlipY,
		&SpriteRenderer::SetFlipY>;
    /** @endcond */
    /** @brief Flip the sprite vertically without changing the object transform.
     */
    FlipYProp flipY{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `pivot01`; use the corresponding instance member in gameplay code.
     */
    using Pivot01Prop = Property<SpriteRenderer, Vec2,
        &SpriteRenderer::GetPivot01,
		&SpriteRenderer::SetPivot01>;
    /** @endcond */
	/** @brief Normalized sprite pivot: (0,0) at one corner and (1,1) at the opposite.
	 */
	Pivot01Prop pivot01{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `texture`; use the corresponding instance member in gameplay code.
     */
    using TextureProp = Property<SpriteRenderer, const Texture*,
        &SpriteRenderer::GetTexture,
		&SpriteRenderer::SetTexture>;
    /** @endcond */
	/** @brief Non-owning texture pointer rendered by the component.
	 * @warning The current setter ignores `nullptr`; assigning null does not
	 *          clear an already configured sprite. Keep the asset alive.
	 */
	TextureProp sprite{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `source`; use the corresponding instance member in gameplay code.
     */
    using SourceProp = Property<SpriteRenderer, RectI,
        &SpriteRenderer::GetSource,
		&SpriteRenderer::SetSource>;
    /** @endcond */
	/** @brief Source rectangle in texture pixels, clamped against the texture on assignment.
	 * @note Requires a texture to have been assigned first. A nonpositive source
	 *       width/height selects the full texture dimension at render time.
	 */
	SourceProp source{ this };
};
