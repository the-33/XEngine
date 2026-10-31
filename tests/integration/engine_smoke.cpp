// Real SDL2 integration checks, linked to XEngine's normal static library.
// Exercise the main gameplay umbrella and opt-in modules in the same TU.
#include <XEngine.h>
#include <XEngine/Storage.h>
#include <XEngine/BinaryIO.h>
#include <XEngine/Serialization.h>
#include <XEngine/Diagnostics.h>
#include <XEngine/Platform.h>
#include "Engine.h"
#include "SceneManager.h"
#include "Scene.h"
#include "GameObject.h"
#include "Behaviour.h"
#include "Transform.h"
#include "AssetManager.h"
#include "InputManager.h"
#include "UIManager.h"
#include "SoundManager.h"
#include "RigidBody2D.h"
#include "Camera2D.h"
#include "WindowManager.h"
#include <SDL.h> // Uses the same SDL_main compatibility mapping as BlankGame.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <vector>

#if defined(Input) || defined(Time) || defined(Assets) || defined(Renderer)
#error "Gameplay manager shortcuts must not be preprocessor macros"
#endif

static_assert(std::is_convertible_v<decltype(Input), InputManager*>);
static_assert(std::is_convertible_v<decltype(CurrentCamera), Camera2D*>);
static_assert(std::is_convertible_v<decltype(FixedDT), float>);
static_assert(std::is_same_v<xengine::io::FileSystem, FileSystem>);
static_assert(std::is_same_v<xengine::binary::BinaryWriter, BinaryWriter>);
static_assert(std::is_same_v<xengine::serialization::BinarySerializer, BinarySerializer>);
static_assert(std::is_same_v<xengine::diagnostics::Path, RenderDiagnostics::Path>);

// These identifiers were unintentionally rewritten by the historical
// #define shortcuts. They must now be legal members in user code.
struct UserConfigNames {
    int Input = 0;
    int Time = 0;
    int Renderer = 0;
    int Assets = 0;
};

namespace {
int failures = 0;
int updates = 0;
int destroyedControllers = 0;
int targetFrames = 16;
GameObject* movingBody = nullptr;

class CapsuleContactProbe final : public Behaviour {
public:
    int triggerEnters = 0;
protected:
    void OnTriggerEnter(const CollisionInfo2D& info) override {
        if (info.other) ++triggerEnters;
    }
};

struct CapsuleContactCase {
    CapsuleContactProbe* probe = nullptr;
    bool expectContact = false;
};
std::vector<CapsuleContactCase> capsuleContacts;

void expect(bool result, const char* message) {
    if (!result) {
        ++failures;
        std::fprintf(stderr, "[INTEGRATION] FAIL: %s\n", message);
    }
}
// Avoid `near`: Windows compatibility headers may define it as a macro.
bool almostEqual(float x, float y) { return std::fabs(x - y) < 0.001f; }

void checkOptionalSerialization() {
    const std::vector<int> original{4, 8, 15};
    const std::string json = JsonSerializer::Serialize(original);
    const auto fromJson = JsonSerializer::Deserialize<std::vector<int>>(json);
    expect(fromJson == original, "opt-in JSON serializer round-trips values");

    const auto bytes = BinarySerializer::Serialize(original);
    const auto fromBinary = BinarySerializer::Deserialize<std::vector<int>>(bytes);
    expect(fromBinary == original, "opt-in binary serializer round-trips values");
}

void checkCapsuleGeometry(Scene* scene) {
    const auto addShape = [scene](const std::string& name, Collider2D::Shape shape,
                                  Vec3 position, Vec2 size, float radius = 0.5f) {
        auto* object = scene->CreateObject(name);
        object->transform->position = position;
        auto* collider = object->AddComponent<Collider2D>();
        collider->shape = shape;
        collider->size = size;
        collider->radius = radius;
        collider->isTrigger = true;
        return collider;
    };
    const auto addCase = [&](const char* name, float x, Collider2D::Shape otherShape,
                             Vec2 otherOffset, bool hit, Vec2 capsuleSize = {1.f, 3.f},
                             Vec2 otherSize = {1.f, 1.f}, float rotationZ = 0.f,
                             Vec2 capsuleScale = {1.f, 1.f}) {
        auto* capsule = addShape(std::string(name) + "-capsule", Collider2D::Shape::Capsule,
                                 {x, 500.f, 0.f}, capsuleSize);
        expect(capsule->shape.get() == Collider2D::Shape::Capsule,
               "capsule shape property preserves the new enum");
        capsule->gameObject->transform->rotation = Vec3{0.f, 0.f, rotationZ};
        capsule->gameObject->transform->scale = capsuleScale;
        auto* probe = capsule->gameObject->AddComponent<CapsuleContactProbe>();
        capsuleContacts.push_back({probe, hit});
        addShape(std::string(name) + "-other", otherShape,
                 {x + otherOffset.x, 500.f + otherOffset.y, 0.f}, otherSize, 0.4f);
    };

    addCase("capsule-circle-hit", 100.f, Collider2D::Shape::Circle, {0.65f, 0.f}, true);
    addCase("capsule-box-hit", 200.f, Collider2D::Shape::Box, {0.7f, 0.f}, true);
    addCase("capsule-capsule-hit", 300.f, Collider2D::Shape::Capsule, {0.7f, 0.f}, true);
    addCase("capsule-circle-miss", 400.f, Collider2D::Shape::Circle, {3.f, 0.f}, false);
    addCase("capsule-box-miss", 500.f, Collider2D::Shape::Box, {3.f, 0.f}, false);
    addCase("horizontal-capsule-hit", 600.f, Collider2D::Shape::Circle,
            {1.65f, 0.f}, true, {3.f, 1.f});

    // These pairs exercise the transformed world geometry and AABB indirectly,
    // without depending on Collider2D's protected internal geometry helpers.
    addCase("rotated-capsule-hit", 700.f, Collider2D::Shape::Circle,
            {1.6f, 0.f}, true, {1.f, 3.f}, {1.f, 1.f}, 90.f);
    addCase("rotated-capsule-miss", 800.f, Collider2D::Shape::Circle,
            {0.f, 1.5f}, false, {1.f, 3.f}, {1.f, 1.f}, 90.f);
    addCase("scaled-capsule-hit", 900.f, Collider2D::Shape::Circle,
            {1.3f, 0.f}, true, {1.f, 3.f}, {1.f, 1.f}, 0.f, {2.f, 1.f});
}

void checkSpriteFitting(Scene* scene, Texture* texture) {
    if (!scene || !texture) return;
    auto* object = scene->CreateObject("fit-to-sprite");
    auto* renderer = object->AddComponent<SpriteRenderer>();
    renderer->sprite = texture;
    renderer->offset = Vec2{0.25f, -0.5f};
    renderer->pivot01 = Vec2{0.f, 1.f};
    renderer->source = RectI{0, 0, std::max(1, texture->Width() / 2),
                             std::max(1, texture->Height() / 2)};
    const RectI src = renderer->source.get();
    const float spriteWidth = src.w / texture->PixelsPerUnit();
    const float spriteHeight = src.h / texture->PixelsPerUnit();
    auto* collider = object->AddComponent<Collider2D>();
    collider->shape = Collider2D::Shape::Capsule;

    expect(collider->FitToSprite(), "FitToSprite fits both dimensions by default");
    const Vec2 full = collider->size;
    const Vec2 center = collider->localOffset;
    expect(almostEqual(full.x, spriteWidth) && almostEqual(full.y, spriteHeight),
           "FitToSprite uses the selected source region and pixels per unit");
    expect(almostEqual(center.x, 0.25f + 0.5f * spriteWidth) &&
           almostEqual(center.y, -0.5f - 0.5f * spriteHeight),
           "FitToSprite matches the sprite offset and noncentered pivot");

    collider->size = Vec2{5.f, 6.f};
    collider->localOffset = Vec2{7.f, 8.f};
    expect(collider->FitToSprite(Collider2D::FitMode::Width, 0.9f),
           "FitToSprite supports width-only adjustment");
    const Vec2 widthOnly = collider->size;
    const Vec2 widthOffset = collider->localOffset;
    expect(almostEqual(widthOnly.x, spriteWidth * 0.9f) && almostEqual(widthOnly.y, 6.f) &&
           almostEqual(widthOffset.x, center.x) && almostEqual(widthOffset.y, 8.f),
           "width-only adjustment preserves height and vertical offset");

    expect(collider->FitToSprite(Collider2D::FitMode::Height, 0.8f),
           "FitToSprite supports height-only adjustment");
    const Vec2 heightOnly = collider->size;
    expect(almostEqual(heightOnly.x, spriteWidth * 0.9f) &&
           almostEqual(heightOnly.y, spriteHeight * 0.8f),
           "height-only adjustment preserves width");

    expect(collider->FitToSprite(0.95f, 0.75f),
           "FitToSprite supports independent factors");
    const Vec2 independent = collider->size;
    expect(almostEqual(independent.x, spriteWidth * 0.95f) &&
           almostEqual(independent.y, spriteHeight * 0.75f),
           "independent factors are applied to their respective axes");
    expect(!collider->FitToSprite(0.f), "FitToSprite rejects zero percentages");
    const Vec2 unchanged = collider->size;
    expect(almostEqual(unchanged.x, independent.x) && almostEqual(unchanged.y, independent.y),
           "invalid FitToSprite parameters do not change the collider");

    collider->shape = Collider2D::Shape::Circle;
    expect(collider->FitToSprite(0.9f, 0.8f), "FitToSprite supports circle geometry");
    expect(almostEqual(collider->radius,
           0.5f * std::min(spriteWidth * 0.9f, spriteHeight * 0.8f)),
           "circle fits the smaller selected diameter");

    auto* empty = scene->CreateObject("fit-without-sprite")->AddComponent<Collider2D>();
    expect(!empty->FitToSprite(), "FitToSprite fails safely without a SpriteRenderer");
}

void checkScene(Scene* scene) {
    checkOptionalSerialization();
    checkCapsuleGeometry(scene);
    auto* parent = scene->CreateObject("parent");
    auto* child = scene->CreateObject("child", parent);
    expect(parent && child, "scene creates entities");
    if (!parent || !child) return;

    expect(static_cast<GameObject*>(child->parent) == parent, "parent is linked");
    parent->SetParent(child);
    expect(static_cast<GameObject*>(parent->parent) == nullptr, "ancestor cycle is rejected");
    child->SetParent(child);
    expect(static_cast<GameObject*>(child->parent) == parent, "self-parenting is rejected");

    parent->name = std::string("renamed-parent");
    expect(scene->Find("parent") == nullptr, "old name leaves scene index");
    expect(scene->Find("renamed-parent") == parent, "new name finds the same entity");
    child->name = std::string("renamed-parent");
    expect(scene->Find("renamed-parent") == parent, "duplicate rename is rejected");
    expect(scene->Find("child") == child, "rejected rename preserves original name");

    parent->transform->position = Vec3{5.f, 6.f, 0.f};
    parent->transform->rotation = Vec3{0.f, 0.f, 90.f};
    parent->transform->scale = Vec2{2.f, 2.f};
    child->transform->localPosition = Vec3{1.f, 0.f, 0.f};
    const Vec3 before = child->transform->position;
    child->transform->Translate(Vec3{2.f, -3.f, 1.f});
    const Vec3 after = child->transform->position;
    expect(almostEqual(after.x, before.x + 2.f) && almostEqual(after.y, before.y - 3.f) &&
           almostEqual(after.z, before.z + 1.f), "Translate moves in world units under a rotated parent");
    child->SetParent(nullptr, true);
    const Vec3 detached = child->transform->position;
    expect(almostEqual(detached.x, after.x) && almostEqual(detached.y, after.y),
           "detaching with keepWorld preserves world position");

    movingBody = scene->CreateObject("moving-body");
    expect(movingBody != nullptr, "physics test body is created");
    auto* rigidBody = movingBody ? movingBody->AddComponent<RigidBody2D>() : nullptr;
    expect(rigidBody != nullptr, "dynamic rigid body is created");
    if (rigidBody) {
        rigidBody->gravityScale = 0.f;
        rigidBody->velocity = Vec2{2.f, 0.f};
    }

    auto* assets = AssetManager::GetInstancePtr();
    expect(assets != nullptr, "asset manager exists");
    expect(Assets.get() == assets, "typed Assets shortcut resolves current manager");
    expect(Scenes.get() == SceneManager::GetInstancePtr(),
           "typed Scenes shortcut resolves current manager");
    expect(CurrentCamera.get() == nullptr,
           "typed CurrentCamera is null while the scene builder is running");
    expect(FixedDT.get() >= 0.f, "typed FixedDT reads live engine state");
    if (assets) {
        auto* texture = assets->LoadTexture("textures/pixel.png");
        expect(texture != nullptr, "SDL_image loads a real PNG");
        expect(assets->LoadTexture("textures/pixel.png", "integration-pixel") == texture,
               "texture alias reuses cached resource");
        expect(assets->GetTextureByKey("integration-pixel") == texture,
               "texture alias resolves correctly");
        expect(assets->LoadTexture("textures/circle.png", "integration-pixel") == nullptr,
               "colliding texture alias is rejected");
        checkSpriteFitting(scene, assets->LoadTexture("textures/circle.png"));

        auto* font = assets->LoadFont("fonts/ui-default.ttf", 18);
        expect(font != nullptr, "SDL_ttf loads a real font");
        expect(assets->LoadFont("fonts/ui-default.ttf", "integration-font", 18) == font,
               "font alias reuses path and size");
        expect(assets->GetFontByKey("integration-font") == font,
               "font alias resolves correctly");
        expect(assets->LoadFont("fonts/ui-default.ttf", "integration-font", 24) == nullptr,
               "colliding font alias is rejected");

        auto* sfx = assets->LoadSFX("audio/tone.wav");
        expect(sfx != nullptr, "SDL_mixer loads generated WAV effect");
        expect(assets->LoadSFX("audio/tone.wav", "integration-sfx") == sfx,
               "effect alias shares ownership with path cache");
        expect(assets->GetSFXByKey("integration-sfx") == sfx,
               "effect alias resolves to the cached resource");
        expect(assets->LoadSFX("audio/other.wav", "integration-sfx") == nullptr,
               "colliding effect alias is rejected");

        auto* music = assets->LoadMusic("audio/tone.wav");
        expect(music != nullptr, "SDL_mixer loads generated WAV music");
        expect(assets->LoadMusic("audio/tone.wav", "integration-music") == music,
               "music alias shares ownership with path cache");
        expect(assets->GetMusicByKey("integration-music") == music,
               "music alias resolves to the cached resource");
        expect(assets->LoadMusic("audio/other.wav", "integration-music") == nullptr,
               "colliding music alias is rejected");

        auto* sound = SoundManager::GetInstancePtr();
        expect(sound != nullptr, "sound manager exists");
        if (sound && sfx && music) {
            expect(sound->PlaySFX(sfx) >= 0, "dummy audio plays a real SFX sample");
            sound->StopAllSFX();
            expect(sound->PlayMusic(music, 0), "dummy audio plays WAV music");
            sound->StopMusic();
            sound->SetMasterVolume(0.35f);
            expect(almostEqual(sound->MasterVolume(), 0.35f),
                   "master volume round-trips through the running mixer");
        }
    }
}

class FrameProbe final : public Behaviour {
    EntityID previousId = 0;
    float previousBodyX = 0.f;
    Vec2I mousePositionBeforeMotion{0, 0};
    std::string firstButtonId = "integration-button";
    std::string secondButtonId = "integration-button";
    std::string editableName = "Tester";

    void ExerciseInputAndUI(Scene* owner) {
        auto* input = InputManager::GetInstancePtr();
        auto* ui = UIManager::GetInstancePtr();
        auto* window = WindowManager::GetInstancePtr();
        expect(input && ui && window, "input, UI and window services remain available");
        expect(Input.get() == input && UI.get() == ui && Window.get() == window,
               "typed manager shortcuts resolve current singleton instances");
        if (!input || !ui || !window || !owner) return;

        const Vec2I logicalSize = window->GetSize();
        const Vec2I drawableSize = window->GetDrawableSize();
        const float scaleX = logicalSize.x > 0 ?
            static_cast<float>(drawableSize.x) / logicalSize.x : 1.f;
        const float scaleY = logicalSize.y > 0 ?
            static_cast<float>(drawableSize.y) / logicalSize.y : 1.f;
        const Rect buttonRect{40.f * scaleX, 20.f * scaleY,
                              100.f * scaleX, 60.f * scaleY};
        if (updates == 1) {
            mousePositionBeforeMotion = input->GetMousePos();
            input->SetTextInput(true);
            SDL_Event motion{};
            motion.type = SDL_MOUSEMOTION;
            motion.motion.x = 64;
            motion.motion.y = 36;
            motion.motion.xrel = 64;
            motion.motion.yrel = 36;
            expect(SDL_PushEvent(&motion) == 1, "SDL accepts synthetic mouse motion");

            SDL_Event down{};
            down.type = SDL_MOUSEBUTTONDOWN;
            down.button.button = SDL_BUTTON_LEFT;
            down.button.x = 64;
            down.button.y = 36;
            expect(SDL_PushEvent(&down) == 1, "SDL accepts synthetic mouse press");

            SDL_Event wheel{};
            wheel.type = SDL_MOUSEWHEEL;
            wheel.wheel.x = -1;
            wheel.wheel.y = 2;
            expect(SDL_PushEvent(&wheel) == 1, "SDL accepts synthetic mouse wheel");

            SDL_Event textEvent{};
            textEvent.type = SDL_TEXTINPUT;
            std::snprintf(textEvent.text.text, sizeof(textEvent.text.text), "á");
            expect(SDL_PushEvent(&textEvent) == 1, "SDL accepts synthetic UTF-8 input");
        } else if (updates == 2) {
            expect(input->MousePressed(MouseButton::Left) && input->MouseDown(MouseButton::Left),
                   "mouse press edge is visible in the next real frame");
            const Vec2I cursor = input->GetMousePos();
            expect(cursor.x == 64 && cursor.y == 36, "SDL motion updates mouse position");
            const Vec2I delta = input->GetMouseDelta();
            const Vec2I expectedDelta{cursor.x - mousePositionBeforeMotion.x,
                                      cursor.y - mousePositionBeforeMotion.y};
            expect(delta.x == expectedDelta.x && delta.y == expectedDelta.y,
                   "mouse delta follows SDL motion relative to the previous frame");
            const Vec2I wheel = input->GetMouseWheel();
            expect(wheel.x == -1 && wheel.y == 2, "wheel event reaches input manager");
            expect(input->GetTextInput() == "á", "UTF-8 text reaches input manager");

            Camera2D* cam = owner->camera.get();
            const Vec2I logical = window->GetSize();
            if (cam && logical.x > 0 && logical.y > 0) {
                const Vec2 view = cam->viewBase;
                const Vec2 deltaWorld = input->GetMouseDeltaWorld();
                expect(almostEqual(deltaWorld.x, expectedDelta.x * view.x / logical.x) &&
                       almostEqual(deltaWorld.y, expectedDelta.y * view.y / logical.y),
                       "mouse world delta scales without including camera offset");
            }
            expect(!ui->Button(firstButtonId.c_str(), buttonRect),
                   "button does not activate on mouse press");

            SDL_Event up{};
            up.type = SDL_MOUSEBUTTONUP;
            up.button.button = SDL_BUTTON_LEFT;
            up.button.x = 64;
            up.button.y = 36;
            expect(SDL_PushEvent(&up) == 1, "SDL accepts synthetic mouse release");
        } else if (updates == 3) {
            expect(input->MouseReleased(MouseButton::Left) && !input->MouseDown(MouseButton::Left),
                   "mouse release edge is visible in the next real frame");
            expect(input->GetTextInput().empty(), "text input is cleared between frames");
            const Vec2I wheel = input->GetMouseWheel();
            expect(wheel.x == 0 && wheel.y == 0, "wheel input is cleared between frames");
            expect(firstButtonId.c_str() != secondButtonId.c_str(),
                   "button test uses distinct identifier addresses");
            expect(ui->Button(secondButtonId.c_str(), buttonRect),
                   "UI matches button ID text across press/release frames");
            input->SetTextInput(false);
        } else if (updates == 4) {
            expect(!input->MouseReleased(MouseButton::Left), "release edge is not repeated");
            expect(!ui->Button(firstButtonId.c_str(), buttonRect),
                   "completed button click is not repeated");
            expect(ui->Focus("integration-name"), "UI accepts a requested stable focus ID");
            expect(ui->IsFocused("integration-name"), "UI reports the focused widget");
            expect(!ui->TextInput("integration-name", Rect{160.f, 20.f, 180.f, 48.f},
                                  &editableName, "Player name", 24),
                   "UI text field preserves existing game-owned text without input");
            expect(editableName == "Tester", "UI text field does not overwrite caller data");
            ui->ClearFocus();
            expect(!ui->IsFocused("integration-name"), "UI can clear its focus");
        }
    }
public:
    void Update(float) override {
        ++updates;
        if (updates == 2) {
            for (const auto& test : capsuleContacts) {
                expect(test.probe && (test.probe->triggerEnters > 0) == test.expectContact,
                       "capsule trigger contacts match the expected narrowphase result");
            }
        }
        Scene* owner = scene;
        expect(owner != nullptr, "behaviour has a scene");
        if (owner)
            expect(CurrentCamera.get() == owner->camera.get(),
                   "typed CurrentCamera follows the active scene after activation");
        ExerciseInputAndUI(owner);
        if (movingBody) {
            const Vec3 pos = movingBody->transform->position;
            const float expectedStep = 2.f * Engine::GetInstancePtr()->fixedDelta.get();
            expect(almostEqual(pos.x - previousBodyX, expectedStep) && almostEqual(pos.y, 0.f),
                   "dynamic rigid body advances exactly one fixed step per frame");
            previousBodyX = pos.x;
        }
        if (owner) {
            if (previousId != 0)
                expect(owner->Find(previousId) == nullptr, "destroy queue flushes between frames");
            auto* transient = owner->CreateObject("transient");
            expect(transient != nullptr, "entities can be created inside Update");
            if (transient) {
                previousId = transient->ID;
                transient->Destroy();
            }
        }
        if (updates >= targetFrames) {
            // Regression: Stop cannot destroy this behaviour/scene mid-callback.
            Engine::Stop();
            expect(gameObject.get() != nullptr, "Stop defers teardown until Update returns");
            expect(SceneManager::GetInstancePtr() != nullptr, "managers survive the stopping frame");
        }
    }
    void OnDestroy() override { ++destroyedControllers; }
};

void buildScene(Scene* scene) {
    checkScene(scene);
    auto* controller = scene->CreateObject("integration-controller");
    expect(controller != nullptr, "lifecycle probe created");
    if (controller) controller->AddComponent<FrameProbe>();
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 2) {
        char* end = nullptr;
        const long parsed = std::strtol(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || parsed < 2 || parsed > 20000) {
            std::fprintf(stderr, "Expected 2..20000 frames\n");
            return 2;
        }
        targetFrames = static_cast<int>(parsed);
    } else if (argc != 1) return 2;

    const std::string config = R"JSON({
        "Engine":{"multiThreading":false,"logStats":false,"logErrors":true},
        "Window":{"width":640,"height":360,"fullscreen":false,"title":"XEngine integration"},
        "Renderer":{"vsync":false,"hardwareAcceleration":false,
                    "backend":{"windows":"software","linux":"software"},
                    "backendFallback":true,"parallelPreparation":false},
        "Assets":{"assetsFolderPath":"./","defaultFontRelativePath":"fonts/ui-default.ttf"}
    })JSON";
    if (!Engine::StartFromJson(config)) {
        std::fprintf(stderr, "[INTEGRATION] Engine::StartFromJson failed\n");
        Engine::Stop();
        return 3;
    }
    expect(Engine::ConfigureDeterministicFixedSteps(1),
           "integration runs one deterministic physics step per rendered frame");
    Engine::SetInitCallback([](Engine&) {
        auto* scenes = SceneManager::GetInstancePtr();
        expect(scenes != nullptr, "scene manager exists");
        if (!scenes) { Engine::Stop(); return; }
        scenes->Register("integration", buildScene);
        expect(scenes->SetActive("integration"), "scene is registered");
    });
    const int status = Engine::Run();
    expect(status == 0, "engine loop exits successfully");
    expect(updates == targetFrames, "engine runs requested number of real frames");
    expect(destroyedControllers == 1, "controller receives exactly one OnDestroy");
    if (failures != 0) {
        std::fprintf(stderr, "[INTEGRATION] %d failures over %d frames\n", failures, updates);
        return 1;
    }
    std::printf("XENGINE_INTEGRATION_OK: %d frames\n", updates);
    return 0;
}
