// Real SDL2 integration checks, linked to XEngine's normal static library.
#include "Engine.h"
#include "SceneManager.h"
#include "Scene.h"
#include "GameObject.h"
#include "Behaviour.h"
#include "Transform.h"
#include "AssetManager.h"
#include <SDL.h> // Uses the same SDL_main compatibility mapping as BlankGame.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
int failures = 0;
int updates = 0;
int destroyedControllers = 0;
int targetFrames = 16;

void expect(bool result, const char* message) {
    if (!result) {
        ++failures;
        std::fprintf(stderr, "[INTEGRATION] FAIL: %s\n", message);
    }
}
// Avoid `near`: Windows compatibility headers may define it as a macro.
bool almostEqual(float x, float y) { return std::fabs(x - y) < 0.001f; }

void checkScene(Scene* scene) {
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

    auto* assets = AssetManager::GetInstancePtr();
    expect(assets != nullptr, "asset manager exists");
    if (assets) {
        auto* texture = assets->LoadTexture("textures/pixel.png");
        expect(texture != nullptr, "SDL_image loads a real PNG");
        expect(assets->LoadTexture("textures/pixel.png", "integration-pixel") == texture,
               "texture alias reuses cached resource");
        expect(assets->GetTextureByKey("integration-pixel") == texture,
               "texture alias resolves correctly");
        expect(assets->LoadTexture("textures/circle.png", "integration-pixel") == nullptr,
               "colliding texture alias is rejected");

        auto* font = assets->LoadFont("fonts/ui-default.ttf", 18);
        expect(font != nullptr, "SDL_ttf loads a real font");
        expect(assets->LoadFont("fonts/ui-default.ttf", "integration-font", 18) == font,
               "font alias reuses path and size");
        expect(assets->GetFontByKey("integration-font") == font,
               "font alias resolves correctly");
        expect(assets->LoadFont("fonts/ui-default.ttf", "integration-font", 24) == nullptr,
               "colliding font alias is rejected");
    }
}

class FrameProbe final : public Behaviour {
    EntityID previousId = 0;
public:
    void Update(float) override {
        ++updates;
        Scene* owner = scene;
        expect(owner != nullptr, "behaviour has a scene");
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
