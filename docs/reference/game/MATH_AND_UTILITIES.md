# Vectors, transforms, colors, properties and utilities

@ref TVec2 and @ref TVec3 represent two- and three-component numeric values;
@ref TRect represents rectangles; @ref Color is a color value. @ref Transform
is the scene component that maps local and world positions.

## Ordinary data versus engine state

```cpp
Vec2 direction{1.f, 0.f};
Vec3 position{2.f, 3.f, 0.f};
Color tint{255, 210, 140, 255};

Transform* tr = object->transform;
Vec3 current = tr->position.get();
tr->position = current + Vec3{1.f, 0.f, 0.f};
```

Math structs have plain numerical fields and can be copied like regular C++
values. Components and managers use `Property`/`PropertyRO` to invoke their
getters/setters, apply validation, and express read-only state. Do not copy
a property accessor or serialize its internal owner pointer: read `.get()`
and serialize the underlying value instead.

`Transform` exposes local and world coordinates and rotation. When parenting
objects, understand whether an API preserves local or world position.
Do not treat a `Vec2` of logical screen pixels as a world-space position;
use @ref Camera2D for conversions.

## Useful services

@ref RandomManager provides deterministic pseudo-random sampling after an
explicit seed, while @ref TimeManager exposes elapsed and fixed-step time.
These are game services, not persistence or cryptographic primitives.

For precise overloads and enum values, open the corresponding type in the
Classes list. Use the Modules navigation to find other related APIs.
