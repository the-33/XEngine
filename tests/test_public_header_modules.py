"""Guard XEngine's gameplay umbrella, optional modules, and internal namespaces."""

from pathlib import Path
import shutil
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
INCLUDE = ROOT / "engine" / "include"
MODULES = INCLUDE / "XEngine"


class PublicHeaderModulesTests(unittest.TestCase):
    def test_umbrella_doxygen_comment_has_no_accidental_nested_openers(self):
        umbrella = (INCLUDE / "XEngine.h").read_text(encoding="utf-8")
        first_comment = umbrella.split("/** @file XEngine.h", 1)[1].split("*/", 1)[0]
        # Doxygen treats slash-star in a documentation comment as a nested
        # comment opener, including text inside Markdown backticks.
        self.assertNotIn("/*", first_comment)

    def test_gameplay_umbrella_does_not_directly_import_specialized_io(self):
        umbrella = (INCLUDE / "XEngine.h").read_text(encoding="utf-8")
        for header in ("FileSystem.h", "AtomicFile.h", "BinaryIO.h",
                       "Serialization.h", "JsonSerializer.h"):
            self.assertNotIn(f'#include "{header}"', umbrella)
        self.assertIn('#include "PlayerPrefs.h"', umbrella)
        self.assertIn('#include "XEngine/Services.h"', umbrella)

    def test_manager_shortcuts_are_not_preprocessor_macros(self):
        umbrella = (INCLUDE / "XEngine.h").read_text(encoding="utf-8")
        services = (MODULES / "Services.h").read_text(encoding="utf-8")
        for name in ("Time", "Input", "Assets", "Sound", "Scenes", "Renderer",
                     "UI", "Window", "Physics", "Random", "CurrentCamera", "FixedDT"):
            self.assertNotIn(f"#define {name}", umbrella)
            self.assertIn(f" {name}{{}};", services)
        self.assertIn("struct ServiceAccess final", services)
        self.assertIn("T* operator->() const noexcept", services)

    def test_private_headers_moved_but_legacy_include_paths_preserved(self):
        for header in (
            "AdaptiveBroadphase.h", "SpatialHashBroadphase.h",
            "SpatialContextClassifier.h", "ParallelExecutor.h",
            "SpriteProjection.h", "RenderPreparation.h",
            "RendererBackendPolicy.h", "StatsOverlayLayout.h",
        ):
            self.assertTrue((MODULES / "detail" / header).is_file(), header)
            shim = (INCLUDE / header).read_text(encoding="utf-8")
            self.assertIn(f'"XEngine/detail/{header}"', shim)
        doxygen = (ROOT / "Doxyfile_Games").read_text(encoding="utf-8")
        self.assertIn("engine/include/XEngine/detail", doxygen)

    @unittest.skipUnless(shutil.which("g++"), "GCC is unavailable")
    def test_optional_io_and_internal_compatibility_compile_without_sdl(self):
        code = r"""
#include <XEngine/Storage.h>
#include <XEngine/BinaryIO.h>
#include <XEngine/detail/AdaptiveBroadphase.h>
#include <XEngine/detail/SpatialHashBroadphase.h>
#include <XEngine/detail/SpatialContextClassifier.h>
#include <XEngine/detail/StatsOverlayLayout.h>
#include <XEngine/detail/ParallelExecutor.h>
#include <XEngine/detail/SpriteProjection.h>
#include <XEngine/detail/RenderPreparation.h>
#include <XEngine/detail/RendererBackendPolicy.h>
#include <Property.h>
#include <type_traits>
static_assert(std::is_same_v<xengine::io::FileSystem, FileSystem>);
static_assert(std::is_same_v<xengine::binary::BinaryWriter, BinaryWriter>);
static_assert(std::is_same_v<AdaptiveBroadphase, xengine::detail::AdaptiveBroadphase>);
static_assert(std::is_same_v<ParallelExecutor, xengine::detail::ParallelExecutor>);
static_assert(std::is_same_v<SpatialContextClassifier,
                             xengine::detail::SpatialContextClassifier>);
static_assert(std::is_same_v<SpatialHashBroadphase::Grid,
                             xengine::detail::spatial_hash::Grid>);
static_assert(std::is_same_v<RenderPreparation::Prepared,
                             xengine::detail::render_preparation::Prepared>);
static_assert(std::is_same_v<SpriteProjection::Pose,
                             xengine::detail::sprite_projection::Pose>);
static_assert(std::is_same_v<RendererBackendPolicy::Plan,
                             xengine::detail::renderer_backend_policy::Plan>);
static_assert(std::is_same_v<XEngineStatsOverlay::Layout,
                             xengine::detail::stats_overlay::Layout>);
int main() { return 0; }
"""
        process = subprocess.run(
            ["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
             "-fsyntax-only", "-x", "c++", "-I", str(INCLUDE), "-"],
            input=code, text=True, capture_output=True, cwd=ROOT,
            check=False, timeout=60,
        )
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)


if __name__ == "__main__":
    unittest.main()
