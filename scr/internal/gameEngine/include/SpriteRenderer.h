#pragma once

#include "Behaviour.h"
#include "Assets.h"
#include "BaseTypes.h"

class SpriteRenderer : public Behaviour
{
private:
    // --- Recurso / recorte (en píxeles de la textura) ---
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
    // Offset en ESPACIO LOCAL del sprite (unidades de mundo ANTES de aplicar rotación).
    // Este offset “gira” junto con el Transform (se rota en Render()).
    Vec2 offset_{ 0.f, 0.f };

    // Pivot de rotación/escala relativo al rectángulo del sprite (0..1).
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
    void Render() override; // Aplica PPU de texture, Transform.sx/sy, offsetLocal rotado y Transform.rotDeg

public:
    SpriteRenderer() = default;
    ~SpriteRenderer() override = default;

    using TintProp = Property<SpriteRenderer, Color,
        &SpriteRenderer::GetTint,
        &SpriteRenderer::SetTint>;
    TintProp tint{ this };

    using OffsetProp = Property<SpriteRenderer, Vec2,
        &SpriteRenderer::GetOffset,
		&SpriteRenderer::SetOffset>;
	OffsetProp offset{ this };

    using FlipXProp = Property<SpriteRenderer, bool,
        &SpriteRenderer::IsFlipX,
		&SpriteRenderer::SetFlipX>;
    FlipXProp flipX{ this };

    using FlipYProp = Property<SpriteRenderer, bool,
		&SpriteRenderer::IsFlipY,
		&SpriteRenderer::SetFlipY>;
    FlipYProp flipY{ this };

    using Pivot01Prop = Property<SpriteRenderer, Vec2,
        &SpriteRenderer::GetPivot01,
		&SpriteRenderer::SetPivot01>;
	Pivot01Prop pivot01{ this };

    using TextureProp = Property<SpriteRenderer, const Texture*,
        &SpriteRenderer::GetTexture,
		&SpriteRenderer::SetTexture>;
	TextureProp sprite{ this };

    using SourceProp = Property<SpriteRenderer, RectI,
        &SpriteRenderer::GetSource,
		&SpriteRenderer::SetSource>;
	SourceProp source{ this };
};