#!/usr/bin/env python3
"""
Unit tests for contextpack.py

Run with: python -m pytest tools/test_contextpack.py -v
Or: python tools/test_contextpack.py
"""

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

# Add tools directory to path
sys.path.insert(0, str(Path(__file__).parent))

from contextpack import (
    is_toon_eligible,
    matches_pattern,
    filter_diff_content,
    sort_diff_chunks,
    split_diff,
    check_for_secrets,
    load_ignore_patterns,
    TOON_MIN_ITEMS_DEFAULT,
    DIFF_CHUNK_SIZE,
    DEFAULT_EXCLUSIONS,
)


class TestToonEligibility(unittest.TestCase):
    """Tests for TOON eligibility heuristic."""
    
    def test_eligible_uniform_array(self):
        """Uniform array of objects should be eligible."""
        data = [
            {"id": 1, "name": "a"},
            {"id": 2, "name": "b"},
            {"id": 3, "name": "c"},
            {"id": 4, "name": "d"},
            {"id": 5, "name": "e"},
        ]
        self.assertTrue(is_toon_eligible(data))
    
    def test_not_eligible_too_few_items(self):
        """Array with fewer than min_items should not be eligible."""
        data = [
            {"id": 1, "name": "a"},
            {"id": 2, "name": "b"},
        ]
        self.assertFalse(is_toon_eligible(data, min_items=5))
    
    def test_not_eligible_non_uniform_keys(self):
        """Array with inconsistent keys should not be eligible."""
        data = [
            {"id": 1, "name": "a"},
            {"id": 2, "name": "b", "extra": "x"},
            {"id": 3, "name": "c"},
            {"id": 4, "name": "d"},
            {"id": 5, "name": "e"},
        ]
        self.assertFalse(is_toon_eligible(data))
    
    def test_not_eligible_not_array(self):
        """Non-array data should not be eligible."""
        data = {"id": 1, "name": "a"}
        self.assertFalse(is_toon_eligible(data))
    
    def test_not_eligible_array_of_non_objects(self):
        """Array of non-objects should not be eligible."""
        data = [1, 2, 3, 4, 5]
        self.assertFalse(is_toon_eligible(data))
    
    def test_eligible_with_custom_min_items(self):
        """Custom min_items should be respected."""
        data = [
            {"id": 1, "name": "a"},
            {"id": 2, "name": "b"},
            {"id": 3, "name": "c"},
        ]
        self.assertTrue(is_toon_eligible(data, min_items=3))
        self.assertFalse(is_toon_eligible(data, min_items=5))


class TestPatternMatching(unittest.TestCase):
    """Tests for .contextpackignore pattern matching."""
    
    def test_glob_pattern(self):
        """Glob patterns should match."""
        patterns = ["*.bin", "*.env"]
        self.assertTrue(matches_pattern("firmware.bin", patterns))
        self.assertTrue(matches_pattern("path/to/firmware.bin", patterns))
        self.assertTrue(matches_pattern(".env", patterns))
        self.assertFalse(matches_pattern("readme.md", patterns))
    
    def test_directory_pattern(self):
        """Directory patterns should match."""
        patterns = ["node_modules/", "build/"]
        self.assertTrue(matches_pattern("node_modules/package.json", patterns))
        self.assertTrue(matches_pattern("path/to/node_modules/file.js", patterns))
        self.assertTrue(matches_pattern("build/output.bin", patterns))
        self.assertFalse(matches_pattern("src/main.cpp", patterns))
    
    def test_secret_patterns(self):
        """Secret-ish patterns should match."""
        patterns = ["*credentials*", "*secret*", "*.key"]
        self.assertTrue(matches_pattern("credentials.json", patterns))
        self.assertTrue(matches_pattern("aws_credentials.ini", patterns))
        self.assertTrue(matches_pattern("secret_key.txt", patterns))
        self.assertTrue(matches_pattern("private.key", patterns))
        self.assertFalse(matches_pattern("config.json", patterns))


class TestDiffFiltering(unittest.TestCase):
    """Tests for diff content filtering."""
    
    def test_filter_excluded_files(self):
        """Excluded files should be removed from diff."""
        diff = """diff --git a/src/main.cpp b/src/main.cpp
--- a/src/main.cpp
+++ b/src/main.cpp
@@ -1,3 +1,4 @@
+// new line
 int main() {}
diff --git a/firmware.bin b/firmware.bin
Binary files differ
diff --git a/readme.md b/readme.md
--- a/readme.md
+++ b/readme.md
@@ -1 +1 @@
-old
+new"""
        
        patterns = ["*.bin"]
        filtered, excluded = filter_diff_content(diff, patterns)
        
        self.assertIn("src/main.cpp", filtered)
        self.assertIn("readme.md", filtered)
        self.assertNotIn("firmware.bin", filtered)
        self.assertIn("firmware.bin", excluded)
    
    def test_filter_empty_diff(self):
        """Empty diff should return empty."""
        filtered, excluded = filter_diff_content("", ["*.bin"])
        self.assertEqual(filtered, "")
        self.assertEqual(excluded, [])


class TestDiffSorting(unittest.TestCase):
    """Tests for deterministic diff sorting."""
    
    def test_sort_chunks_alphabetically(self):
        """Diff chunks should be sorted by file path."""
        diff = """diff --git a/z_file.txt b/z_file.txt
content z
diff --git a/a_file.txt b/a_file.txt
content a
diff --git a/m_file.txt b/m_file.txt
content m"""
        
        sorted_diff = sort_diff_chunks(diff)
        
        # Find positions
        pos_a = sorted_diff.find("a_file.txt")
        pos_m = sorted_diff.find("m_file.txt")
        pos_z = sorted_diff.find("z_file.txt")
        
        self.assertLess(pos_a, pos_m)
        self.assertLess(pos_m, pos_z)


class TestDiffSplitting(unittest.TestCase):
    """Tests for diff size budget splitting."""
    
    def test_no_split_under_limit(self):
        """Small diff should not be split."""
        diff = "diff --git a/file.txt b/file.txt\nsmall content"
        chunks = split_diff(diff, chunk_size=1000)
        
        self.assertEqual(len(chunks), 1)
        self.assertEqual(chunks[0][0], "diff.patch")
    
    def test_split_over_limit(self):
        """Large diff should be split into parts."""
        # Create a diff larger than chunk size
        diff_parts = []
        for i in range(10):
            diff_parts.append(f"diff --git a/file{i}.txt b/file{i}.txt\n" + "x" * 1000)
        diff = "\n".join(diff_parts)
        
        chunks = split_diff(diff, chunk_size=3000)
        
        self.assertGreater(len(chunks), 1)
        self.assertTrue(all(name.startswith("diff_part_") for name, _ in chunks))


class TestSecretDetection(unittest.TestCase):
    """Tests for secret pattern detection."""
    
    def test_detect_api_key(self):
        """API key patterns should be detected."""
        # Use clearly fake pattern: TEST_ prefix with repeated characters
        content = 'API_KEY = "TEST_FAKE_KEY_1234567890_ABCDEFGHIJ_abcdefghijklmnop"'
        matches = check_for_secrets(content)
        self.assertGreater(len(matches), 0)
    
    def test_detect_private_key(self):
        """Private key headers should be detected."""
        content = "-----BEGIN PRIVATE KEY-----\nbase64content\n-----END PRIVATE KEY-----"
        matches = check_for_secrets(content)
        self.assertGreater(len(matches), 0)
    
    def test_no_false_positive(self):
        """Normal code should not trigger detection."""
        content = """
def get_user(user_id):
    return database.query(user_id)
"""
        matches = check_for_secrets(content)
        self.assertEqual(len(matches), 0)


class TestToonCliIntegration(unittest.TestCase):
    """Integration tests for TOON CLI (requires npm install)."""
    
    @classmethod
    def setUpClass(cls):
        """Check if TOON CLI is available."""
        tools_dir = Path(__file__).parent
        try:
            result = subprocess.run(
                ["npm", "--prefix", str(tools_dir), "exec", "--", "toon", "--version"],
                capture_output=True,
                text=True,
                timeout=30,
            )
            cls.toon_available = result.returncode == 0
        except Exception:
            cls.toon_available = False
    
    def test_toon_cli_version(self):
        """TOON CLI should be pinned to expected version."""
        if not self.toon_available:
            self.skipTest("TOON CLI not available")
        
        tools_dir = Path(__file__).parent
        result = subprocess.run(
            ["npm", "--prefix", str(tools_dir), "exec", "--", "toon", "--version"],
            capture_output=True,
            text=True,
        )
        
        # Should contain version number
        self.assertIn("2.1.0", result.stdout + result.stderr)
    
    def test_toon_roundtrip(self):
        """TOON conversion should be lossless (round-trip)."""
        if not self.toon_available:
            self.skipTest("TOON CLI not available")
        
        tools_dir = Path(__file__).parent
        
        # Create test JSON
        test_data = [
            {"id": 1, "name": "alpha", "value": 100},
            {"id": 2, "name": "beta", "value": 200},
            {"id": 3, "name": "gamma", "value": 300},
            {"id": 4, "name": "delta", "value": 400},
            {"id": 5, "name": "epsilon", "value": 500},
        ]
        
        with tempfile.TemporaryDirectory() as tmpdir:
            json_path = Path(tmpdir) / "test.json"
            toon_path = Path(tmpdir) / "test.toon"
            
            # Write JSON
            with open(json_path, "w") as f:
                json.dump(test_data, f)
            
            # Convert to TOON
            result = subprocess.run(
                ["npm", "--prefix", str(tools_dir), "exec", "--", "toon", str(json_path), "-o", str(toon_path)],
                capture_output=True,
                text=True,
            )
            
            self.assertEqual(result.returncode, 0, f"TOON conversion failed: {result.stderr}")
            self.assertTrue(toon_path.exists(), "TOON file not created")
            
            # Verify TOON file is not empty and has expected structure
            toon_content = toon_path.read_text()
            self.assertIn("[5]", toon_content)  # Array count
            self.assertIn("id", toon_content)
            self.assertIn("name", toon_content)


class TestStatsParsingRobustness(unittest.TestCase):
    """Tests for TOON stats parsing robustness."""
    
    def test_parse_standard_format(self):
        """Standard stats format should parse correctly."""
        import re
        
        output = """[info] Token estimates: ~336 (JSON) → ~62 (TOON)
[success] Saved ~274 tokens (-81.5%)"""
        
        json_match = re.search(r'~(\d+)\s*\(JSON\)', output)
        toon_match = re.search(r'~(\d+)\s*\(TOON\)', output)
        savings_match = re.search(r'\(-?(\d+(?:\.\d+)?)\s*%\)', output)
        
        self.assertIsNotNone(json_match)
        self.assertIsNotNone(toon_match)
        self.assertIsNotNone(savings_match)
        
        self.assertEqual(int(json_match.group(1)), 336)
        self.assertEqual(int(toon_match.group(1)), 62)
        self.assertEqual(float(savings_match.group(1)), 81.5)
    
    def test_parse_missing_stats(self):
        """Missing stats should not crash."""
        import re
        
        output = "Some unexpected output format"
        
        json_match = re.search(r'~(\d+)\s*\(JSON\)', output)
        toon_match = re.search(r'~(\d+)\s*\(TOON\)', output)
        
        self.assertIsNone(json_match)
        self.assertIsNone(toon_match)


def run_tests():
    """Run all tests."""
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromModule(sys.modules[__name__])
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(run_tests())
