# Physics bodies, colliders and contact callbacks

@ref PhysicsManager advances the simulation; @ref RigidBody2D represents
physical movement and @ref Collider2D supplies shape, collision layer and
trigger behavior. Both components are attached to scene-owned `GameObject`s.

## Choose a body type

`RigidBody2D::BodyType` distinguishes static geometry, kinematic movement
and dynamic simulation. Set parameters through their public property
accessors, not private cached values:

```cpp
auto* body = object->AddComponent<RigidBody2D>();
body->bodyType = RigidBody2D::BodyType::Dynamic;
body->mass = 2.f;
body->gravityScale = 1.f;
body->velocity = Vec2{3.f, 0.f};
```

The solver uses fixed simulation steps. Use the documented `AddForce`/
`AddTorque` operations for physical motion, with the appropriate `ForceMode`.
Directly setting velocity can be useful for controlled games but overrides
the solver's current motion state.

## Set up collisions and triggers

Attach a `Collider2D` and configure its `shape`, `size` or `radius` properties.
Supported shapes are `Box`, `Circle` and `Capsule`. A capsule is a line segment
with circular ends, suitable for characters:

```cpp
auto* collider = player->AddComponent<Collider2D>();
collider->shape = Collider2D::Shape::Capsule;
collider->size = Vec2{0.7f, 1.4f}; // overall width and height, including the rounded ends
```

To size the collider automatically from the `SpriteRenderer` on the same
`GameObject`, call `FitToSprite()` **after** assigning the sprite texture:

```cpp
collider->FitToSprite();                                 // both axes, 100%
collider->FitToSprite(0.95f);                            // both axes, 95%
collider->FitToSprite(0.90f, 0.80f);                     // width 90%, height 80%
collider->FitToSprite(Collider2D::FitMode::Width, 0.9f);  // width only, 90%
collider->FitToSprite(Collider2D::FitMode::Height, 0.8f); // height only, 80%
collider->FitToSprite(Collider2D::FitMode::Both, 0.9f, 0.8f); // both independently
```

Factors are multipliers: **1.0 means 100%**, not 100. Invalid factors or a
missing sprite return `false` without modifying the collider. Width-only and
height-only fits preserve the other dimension and the corresponding offset.
`FitToSprite()` uses the sprite's selected `source` region, texture
`PixelsPerUnit()`, `offset` and `pivot01`. It sizes `Box`/`Capsule` using
`size`; for `Circle`, the chosen dimension controls its diameter (if both
axes are fitted, the smaller diameter is used). The fit happens **once**:
call it again after changing the texture, source region, PPU or pivot. It
does not analyze transparent pixels; transparent padding remains inside the
fitted dimensions. To match a rotating sprite, leave collider
`inheritRotation=true` and `rotationOffset=0`; nonzero X/Y sprite rotations
introduce visual foreshortening that 2D colliders cannot reproduce.

`size` supplies the capsule's total local width and height; the longer scaled
dimension sets its axis (vertical or horizontal). `localOffset`, `rotationOffset`,
`inheritRotation` and world scale work as on boxes. Under nonuniform scaling,
the capsule retains circular world-space end caps. `radius` applies only to
`Circle` shapes. `WorldCapsule()` exposes the two world-space cap centers and
radius; `WorldAABB()` includes the entire capsule, including rotations.

In Debug, press **F3** to open the statistics overlay and see the actual
collider outlines, including capsules (green for colliders, yellow for triggers).
You can also enable an individual collider's `showGizmo` property.

For trigger volumes, set `isTrigger=true`. `layer` and `mask` are bitfields:
the collision system checks the layer/mask relationship before evaluating
geometry. Matching layers alone do not force a collision if their shapes do
not overlap.

Normal contacts and triggers are different lifecycle events. The game
receives `OnCollisionEnter/Stay/Exit` for physical pairs and
`OnTriggerEnter/Stay/Exit` for trigger overlaps. Do not mutate or destroy
objects directly through invalidated contact pointers; use supported
scene/object removal APIs.

## Physics guidance

- Use `FixedUpdate` for applying forces and other simulation work.
- Prefer colliders matching the sprite's gameplay shape, not necessarily its
  exact visible pixels.
- A dynamic body normally needs a nonzero valid mass; avoid negative size
  and other invalid geometry.
- Collision filters are game-defined and do not determine rendering order.
- Debug collider drawing is diagnostic only and may differ from the sprite.

See individual property and enum entries on @ref RigidBody2D and
@ref Collider2D for detailed constraints, defaults and units.
