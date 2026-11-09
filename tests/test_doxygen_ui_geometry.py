"""Prevent parameter-name mistakes in the SDL-free UI geometry Doxygen API."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
GEOMETRY_HEADERS = (
    ROOT / "engine/include/XEngine/detail/UIVisualGeometry.h",
    ROOT / "engine/include/XEngine/detail/UICornerRadii.h",
)


class UIGeometryDoxygenTests(unittest.TestCase):
    def test_all_geometry_doxygen_parameter_names_match_cpp_signatures(self):
        signature = re.compile(
            r"/\*\*((?:(?!/\*|\*/).)*)\*/\s*"
            r"(?:inline\s+)?(?:constexpr\s+)?"
            r"\w+\s+(\w+)\((.*?)\)\s*noexcept\s*\{",
            re.DOTALL,
        )
        checked = 0
        for header in GEOMETRY_HEADERS:
            source = header.read_text(encoding="utf-8")
            for comment, function, parameters in signature.findall(source):
                documented = re.findall(r"@param\s+(\w+)", comment)
                named = [
                    argument.split()[-1].lstrip("&*")
                    for argument in parameters.split(",") if argument.strip()
                ]
                with self.subTest(header=header.name, function=function, named=named):
                    self.assertEqual(documented, named)
                checked += 1
        self.assertGreaterEqual(checked, 7)


if __name__ == "__main__":
    unittest.main()
