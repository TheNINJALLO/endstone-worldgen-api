from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class TestNativeSourceGuards(unittest.TestCase):
    def test_linux_plugin_preserves_host_imports_and_gates_private_bedrock_symbols(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        link_options = re.findall(
            r"target_link_options\(\s*worldgen_api\b[^)]*\)",
            cmake,
            flags=re.DOTALL,
        )
        self.assertTrue(link_options)
        for option in link_options:
            self.assertNotIn("--no-undefined", option)
            self.assertNotIn("-z,defs", option)

        post_build = re.search(
            r"add_custom_command\(TARGET worldgen_api POST_BUILD(?P<body>.*?)\n\s*\)",
            cmake,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(post_build)
        body = post_build.group("body")
        self.assertIn("$<TARGET_FILE:worldgen_api>", body)
        self.assertIn("verify_no_undefined_bedrock_symbols.cmake", body)

        clean_symbols = (ROOT / "tests/cmake/nm_clean.txt").read_text(
            encoding="utf-8"
        )
        self.assertIn("_ZN8endstone6Server8getLevelEv U", clean_symbols)


if __name__ == "__main__":
    unittest.main()
