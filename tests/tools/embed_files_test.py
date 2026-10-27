"""Embedded text stays byte-exact without oversized compiler string literals."""

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COMPILER = sys.argv.pop() if len(sys.argv) > 1 else "c++"
# Larger than Clang's required string-literal limit, with UTF-8 and a raw-string terminator.
PAYLOAD = ('é )EMBEDDED_FILE"\\\n' * 5000).encode()


class EmbedFilesTest(unittest.TestCase):
    def test_large_arbitrary_text_and_empty_file_compile_byte_exactly(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "large.txt").write_bytes(PAYLOAD)
            (root / "empty.txt").write_bytes(b"")
            subprocess.run([
                "cmake", f"-DOUTPUT={root / 'embedded.hpp'}", f"-DROOT={root}", "-DNAME=kFiles",
                f"-DSOURCES={root / 'large.txt'};{root / 'empty.txt'}",
                "-P", str(ROOT / "cmake/EmbedFiles.cmake"),
            ], check=True, capture_output=True)
            assertions = [
                "static_assert(std::char_traits<char>::length(redgem::generated::kFiles[0].contents)"
                f" == {len(PAYLOAD)});",
                "static_assert(redgem::generated::kFiles[1].contents[0] == '\\0');",
            ]
            for index in (0, 1, 2, 65535, len(PAYLOAD) - 1):
                assertions.append(
                    f"static_assert(static_cast<unsigned char>(redgem::generated::kFiles[0].contents[{index}])"
                    f" == {PAYLOAD[index]});")
            source = root / "test.cpp"
            source.write_text('#include <string>\n#include "embedded.hpp"\n' + "\n".join(assertions), encoding="utf-8")
            subprocess.run([
                COMPILER, "-std=c++17", "-Werror", "-Woverlength-strings", "-c", str(source),
                "-o", str(root / "test.o"),
            ], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
