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
