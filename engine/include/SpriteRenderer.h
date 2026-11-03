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
    /** @brief Configured texture value used by SpriteRenderer.
     */
    const Texture* texture = nullptr;
    /** @brief Configured src x value used by SpriteRenderer.
     */
    int  srcX = 0;
    /** @brief Configured src y value used by SpriteRenderer.
     */
    int  srcY = 0;
    /** @brief Configured src w value used by SpriteRenderer.
     */
    int  srcW = 0;   // 0 => Texture::Width()
    /** @brief Configured src h value used by SpriteRenderer.
     */
    int  srcH = 0;   // 0 => Texture::Height()

    // --- Apariencia ---
    /** @brief Stored tint value used by SpriteRenderer.
     */
    Color tint_ = { 255, 255, 255, 255 };
    /** @brief Stored flip x value used by SpriteRenderer.
     */
    bool  flipX_ = false;
    /** @brief Stored flip y value used by SpriteRenderer.
     */
    bool  flipY_ = false;

    // --- Posicionamiento avanzado ---
    // Sprite LOCAL-SPACE offset (world units BEFORE rotation).
    // This offset rotates with the Transform (rotation applied in Render()).
    /** @brief Stored offset value used by SpriteRenderer.
     */
    Vec2 offset_{ 0.f, 0.f };

    // Rotation/scaling pivot relative to the sprite rectangle (0..1).
    // (0,0)=esquina superior izq, (0.5,0.5)=centro, (1,1)=inferior dcha.
    /** @brief Stored pivot01 value used by SpriteRenderer.
     */
    Vec2 pivot01_{ 0.5f, 0.5f };

    // Temporary second sprite used to blend animation transitions.
    /** @brief Stored animation previous texture value used by SpriteRenderer.
     */
    const Texture* animationPreviousTexture_ = nullptr;
    /** @brief Stored animation previous source value used by SpriteRenderer.
     */
    RectI animationPreviousSource_{};
    /** @brief Stored animation blend value used by SpriteRenderer.
     */
    float animationBlend_ = 1.f;

    // Conveniencia
    /** @brief Set texture in SpriteRenderer.
     * @param tex Texture asset to read or draw.
     */
    void SetTexture(const Texture* tex) noexcept;
    /** @brief Set source in SpriteRenderer.
     * @param src Src supplied to SpriteRenderer.
     */
    void SetSource(RectI src) noexcept;
    /** @brief Set tint in SpriteRenderer.
     * @param tint RGBA tint applied to the rendered sprite or text.
     */
    void SetTint(Color tint) noexcept;
	/** @brief Set flip x in SpriteRenderer.
	 * @param fx Fx supplied to SpriteRenderer.
	 */
	void SetFlipX(bool fx) noexcept { flipX_ = fx; }
	/** @brief Set flip y in SpriteRenderer.
	 * @param fy Fy supplied to SpriteRenderer.
	 */
	void SetFlipY(bool fy) noexcept { flipY_ = fy; }
    /** @brief Set offset in SpriteRenderer.
     * @param off Off supplied to SpriteRenderer.
     */
    void SetOffset(Vec2 off) noexcept { offset_ = off; }
    /** @brief Set pivot01 in SpriteRenderer.
     * @param p P supplied to SpriteRenderer.
     */
    void SetPivot01(Vec2 p) noexcept { pivot01_ = p; }

	/** @brief Retrieve the tint from SpriteRenderer.
	 * @return Requested get tint value or reference.
	 */
	Color GetTint() const noexcept { return tint_; }
	/** @brief Retrieve the offset from SpriteRenderer.
	 * @return Requested get offset value or reference.
	 */
	Vec2 GetOffset() const noexcept { return offset_; }
	/** @brief Test whether SpriteRenderer is flip x.
	 * @return True when is flip x; otherwise false.
	 */
	bool IsFlipX() const noexcept { return flipX_; }
	/** @brief Test whether SpriteRenderer is flip y.
	 * @return True when is flip y; otherwise false.
	 */
	bool IsFlipY() const noexcept { return flipY_; }
	/** @brief Retrieve the pivot01 from SpriteRenderer.
	 * @return Requested get pivot01 value or reference.
	 */
	Vec2 GetPivot01() const noexcept { return pivot01_; }
	/** @brief Retrieve the texture from SpriteRenderer.
	 * @return Requested get texture value or reference.
	 */
	const Texture* GetTexture() const noexcept { return texture; }
	/** @brief Retrieve the source from SpriteRenderer.
	 * @return Requested get source value or reference.
	 */
	RectI GetSource() const noexcept { return { srcX, srcY, srcW, srcH }; }

protected:

    // Ciclo
    /** @brief Lifecycle hook for render; the engine controls invocation order.
     */
    void Render() override; // Apply texture PPU, Transform.sx/sy, rotated offsetLocal and Transform.rotDeg

public:
    /** @brief Crossfade the last animation sprite into the current sprite.
     * The previous texture is borrowed from AssetManager; it must outlive
     * the transition. A blend of 0 shows the previous sprite, 1 the current.
     * @param previous Previous texture owned by AssetManager.
     * @param source Previous texture's pixel source rectangle.
     * @param blend Destination opacity fraction, clamped to [0,1].
     */
    void SetAnimationBlend(const Texture* previous, RectI source, float blend) noexcept;
    /** @brief Disable the temporary animation crossfade overlay. */
    void ClearAnimationBlend() noexcept;
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
