"""Source-level regression contracts for engine safety and public API fixes.

These tests work without the SDL development SDK. A full engine integration
build and interactive input/HiDPI tests are still required on target systems.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


def source(name: str) -> str:
    return (ROOT / name).read_text(encoding="utf-8")


def body(text: str, signature: str, end: str) -> str:
    return text.split(signature, 1)[1].split(end, 1)[0]


class APISafetyRegressions(unittest.TestCase):
    def test_stop_is_deferred_until_run_returns(self):
        header = source("engine/include/Engine.h")
        cpp = source("engine/src/Engine.cpp")
        stop = body(header, "static void Stop() noexcept", "// Benchmark-only override")
        run = body(header, "static int Run() noexcept", "/** @brief Requests a safe engine shutdown")
        impl = body(cpp, "void Engine::StopImpl() noexcept", "bool Engine::ApplyStartingScene")
        loop = body(cpp, "int Engine::RunSingleThreadingImpl() noexcept", "int Engine::RunMultiThreadingImpl() noexcept")

        self.assertIn("if (e->mRunning || !e->SafeToQuit)", stop)
        self.assertIn("e->mRunning = false;", stop)
        self.assertLess(run.index("e->StopImpl();"), run.index("DestroySingleton();"))
        self.assertIn("mShuttingDown", impl)
        self.assertNotIn("DestroySingleton()", impl)
        self.assertNotIn("StopImpl();", loop)

    def test_parenting_rejects_cycles_and_cross_scene_links(self):
        cpp = source("engine/src/GameObject.cpp")
        parent = body(cpp, "void GameObject::SetParent(", "void GameObject::AddChild(")
        self.assertIn("newParent->mScene != mScene", parent)
        self.assertRegex(parent, r"for\s*\(auto\* ancestor = newParent; ancestor; ancestor = ancestor->mParent\)")
        self.assertIn("ancestor == this", parent)
        self.assertLess(parent.index("ancestor == this"), parent.index("mParent = newParent"))
        self.assertIn("mTransform->MarkDirty();", parent)

    def test_renaming_updates_scene_index_and_prevents_collisions(self):
        header = source("engine/include/GameObject.h")
        cpp = source("engine/src/GameObject.cpp")
        scene = source("engine/include/Scene.h")
        rename = body(cpp, "void GameObject::SetName(", "void GameObject::SetParent(")
        self.assertIn("void SetName(std::string name) noexcept;", header)
        self.assertIn("friend class GameObject;", scene)
        self.assertIn("existing->second != this", rename)
        self.assertLess(rename.index("mByName.erase(old)"), rename.index("mByName[mName] = this"))

    def test_world_translate_recomputes_local_pose(self):
        cpp = source("engine/src/Transform.cpp")
        translate = body(cpp, "void Transform::Translate(", "static inline float RadToDeg")
        self.assertIn("GetWorldPosition()", translate)
        self.assertIn("SetWorldPosition(", translate)
        for signature, following in (("Vec3 Transform::GetRight()", "Vec3 Transform::GetUp()"),
                                     ("Vec3 Transform::GetUp()", "void Transform::Translate(")):
            self.assertIn("RecalcWorld_();", body(cpp, signature, following))

    def test_mouse_delta_removes_camera_offset_and_scales_hidpi(self):
        cpp = source("engine/src/InputManager.cpp")
        delta = cpp.split("Vec2 InputManager::GetMouseDeltaWorld() const noexcept", 1)[1]
        pos = body(cpp, "Vec2 InputManager::GetMousePosWorld() const noexcept", "Vec2 InputManager::GetMouseDeltaWorld() const noexcept")
        self.assertIn("mMouseDeltaX += e.motion.xrel", cpp)
        self.assertIn("mMouseDeltaY += e.motion.yrel", cpp)
        for function in (pos, delta):
            self.assertIn("win->GetSize()", function)
            self.assertIn("win->GetDrawableSize()", function)
        self.assertIn("ScreenToWorld(0.f, 0.f", delta)
        self.assertIn("moved.x - origin.x", delta)
        self.assertIn("moved.y - origin.y", delta)

    def test_asset_aliases_share_path_owned_resources(self):
        cpp = source("engine/src/AssetManager.cpp")
        header = source("engine/include/AssetManager.h")
        for kind, cache in (("Texture", "mTextureAliases"), ("Font", "mFontAliases"),
                            ("SFX", "mSfxAliases"), ("Music", "mMusicAliases")):
            self.assertIn(cache, header)
            self.assertIn(f"{cache}[key] =", cpp)
            self.assertRegex(cpp, rf"Get{kind}ByKey\([^)]*\) const noexcept\s*\{{\s*if \(const auto alias = {cache}\.find\(key\)")
            self.assertGreaterEqual(cpp.count(f"{cache}.clear();"), 2)
        self.assertIn("Texture* texture = LoadTexture(path, pixelsPerUnit);", cpp)
        self.assertIn("Font* font = LoadFont(path, ptSize);", cpp)
        self.assertIn("SoundEffect* sfx = LoadSFX(path);", cpp)
        self.assertIn("Music* music = LoadMusic(path);", cpp)

    def test_ui_ids_compare_text_and_mouse_matches_drawable_space(self):
        cpp = source("engine/src/UIManager.cpp")
        self.assertIn("std::optional<std::string> gActiveID", cpp)
        self.assertIn("*gActiveID == id", cpp)
        self.assertNotIn("static const char* gActiveID", cpp)
        self.assertIn("ButtonBehavior_(id, r)", cpp)
        self.assertIn("window->GetDrawableSize()", cpp)
        self.assertIn("window->GetSize()", cpp)
        docs = source("docs/WINDOW_AND_CAMERA.md")
        self.assertIn("Window->drawableSize.get()", docs)


if __name__ == "__main__":
    unittest.main()
