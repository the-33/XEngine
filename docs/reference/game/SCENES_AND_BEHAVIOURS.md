# Scenes, GameObjects and gameplay scripts

Use @ref SceneManager to register level builders, @ref Scene to own the
objects in a level, @ref GameObject to attach components and @ref Behaviour
to write gameplay. The engine controls when lifecycle callbacks run.

The BlankGame starter files use one public gameplay include,
`#include <XEngine.h>`, without forward declarations or separate engine
headers. To scaffold your own header from a game directory:

```bash
./new_asset.sh behaviour Mover
./new_asset.sh object Player --sprite square --collider box --behaviour Mover
./new_asset.sh scene Level1 --dir scenes
```

Use `new_asset.bat` on Windows. The default destination is the root of
`assets/`, but `--dir scripts`, `--dir gameobjects` and `--dir scenes` keep
the traditional layout. The `--sprite` option restores a missing selected
default PNG from the central BlankGame template. The `--collider` option
supports box, circle or capsule colliders; it does not generate
polygon colliders for arbitrary sprite silhouettes.

## Register a scene

Your generated `src/InitCallback.hpp` is called after the engine starts:

```cpp
#include <XEngine.h>
#include "StartScene.hpp"

inline void InitGame(Engine& engine) {
    (void)engine;
    Scenes->Register("StartScene", BuildStartScene);
    Scenes->SetActive("StartScene");
}
```

`BuildStartScene(Scene*)` creates its contents and returns. Scene activation
is scheduled at an engine-controlled boundary; do not manually call any
scene or component update function. Use `Scenes->SetPersistent(id, true)`
only when you intentionally want to retain an existing scene instance.

## Attach a Behaviour to a GameObject

```cpp
#include <XEngine.h>

class Mover : public Behaviour {
public:
    void Update(float dt) override {
        Vec3 p = transform->position.get();
        p.x += 2.f * dt;
        transform->position = p;
    }
};

inline void BuildStartScene(Scene* scene) {
    auto* entity = scene->CreateObject("Player");
    entity->AddComponent<Mover>();
}
```

The object is not drawn until you add a @ref SpriteRenderer or issue custom
draw commands. `dt` is elapsed time in seconds; physics work that must run
at a fixed rate belongs in `FixedUpdate(float)`.

## Lifecycle and object references

| Callback | Typical responsibility |
|---|---|
| `Awake` | Initialize local state once after attachment. |
| `Start` | Initialize play state before the first update. |
| `Update` | Input, ordinary movement and game rules per frame. |
| `FixedUpdate` | Physics-related state at the fixed time step. |
| `Render` | Immediate-mode UI or custom rendering. |
| `OnEnable` / `OnDisable` | Register or pause activity when activation changes. |
| `OnCollisionEnter/Stay/Exit` | Respond to physical contacts. |
| `OnTriggerEnter/Stay/Exit` | Respond to overlaps without physical response. |
| `OnDestroy` | Release any resources that are not owned by the scene. |

The `gameObject`, `transform` and `scene` properties are **non-owning**.
`GameObject::Destroy()` schedules object removal. References to destroyed
objects and previous scenes must not be dereferenced. For reusable entities,
put an `inline` factory function in `assets/gameobjects/` and invoke it
from each scene builder.
