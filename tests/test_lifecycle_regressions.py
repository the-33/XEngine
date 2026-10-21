"""SDL-independent source-contract checks for lifetime and activation order.

Full runtime integration still requires SDL2 headers/libraries; these checks
specifically prevent reintroduction of the three regressions in this patch.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LifecycleRegressions(unittest.TestCase):
    def test_scene_owns_camera(self):
        scene = (ROOT/'engine/include/Scene.h').read_text()
        source = (ROOT/'engine/src/Scene.cpp').read_text()
        self.assertIn('std::unique_ptr<Camera2D> mCamera', scene)
        camera_header = (ROOT/'engine/include/Camera2D.h').read_text()
        self.assertIn('mCamera.reset(new Camera2D());', source)
        self.assertIn('friend class Scene;', camera_header)
        self.assertIn('friend struct std::default_delete<Camera2D>;', camera_header)
        self.assertNotIn('std::make_unique<Camera2D>()', source)
        self.assertIn('~Scene() noexcept;', scene)

    def test_gameobject_destroy_returns_unqualified_void(self):
        header = (ROOT/'engine/include/GameObject.h').read_text()
        source = (ROOT/'engine/src/GameObject.cpp').read_text()
        self.assertIn('void Destroy() noexcept;', header)
        self.assertIn('void GameObject::Destroy() noexcept', source)
        self.assertNotIn('const void', header + source)

    def test_on_enable_comes_after_initial_awake_and_start(self):
        text = (ROOT/'engine/src/SceneManager.cpp').read_text()
        section = text.split('// --- 3) Arranque limpio ---', 1)[1].split('void SceneManager::FixedUpdate', 1)[0]
        self.assertLess(section.index('mActive->AwakeAll();'), section.index('mActive->StartAll();'))
        self.assertLess(section.index('mActive->StartAll();'), section.index('mActive->OnEnableAll();'))
        self.assertLess(section.index('mActive->Register();'), section.index('mActive->OnEnableAll();'))

    def test_added_component_uses_lifecycle_wrappers(self):
        component = (ROOT/'engine/include/Component.h').read_text()
        behaviour = (ROOT/'engine/include/Behaviour.h').read_text()
        for text in (component, behaviour):
            section = text.split('void OnAddedToGameObject', 1)[1].split('\n    }', 1)[0]
            self.assertIn('Awake_()', section)
            self.assertNotIn(') Awake();', section)
            self.assertIn('Start_()', section)
        self.assertIn('if (!mStarted) Start_();', behaviour)

if __name__ == '__main__':
    unittest.main()
