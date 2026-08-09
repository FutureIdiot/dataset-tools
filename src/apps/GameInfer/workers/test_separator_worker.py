from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import textwrap
import types
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import separator_worker


WORKER = Path(__file__).with_name("separator_worker.py")


class SeparatorWorkerBootstrapTest(unittest.TestCase):
    def test_bundled_ffmpeg_is_exposed_with_the_expected_name(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            bundled = root / ("imageio-ffmpeg.exe" if os.name == "nt" else "imageio-ffmpeg")
            bundled.write_bytes(b"bundled-ffmpeg")
            tools_dir = root / "tools"
            fake_imageio_ffmpeg = types.ModuleType("imageio_ffmpeg")
            fake_imageio_ffmpeg.get_ffmpeg_exe = lambda: str(bundled)

            with (
                patch.dict(
                    os.environ,
                    {"PATH": "", "GAMEINFER_SEPARATOR_TOOLS_DIR": str(tools_dir)},
                    clear=False,
                ),
                patch.dict(sys.modules, {"imageio_ffmpeg": fake_imageio_ffmpeg}),
            ):
                resolved = separator_worker._ensure_ffmpeg_on_path()

            expected = tools_dir / ("ffmpeg.exe" if os.name == "nt" else "ffmpeg")
            self.assertEqual(resolved, expected.resolve())
            self.assertEqual(expected.read_bytes(), b"bundled-ffmpeg")


class SeparatorWorkerProtocolTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        root = Path(self.temp_dir.name)
        package = root / "fake" / "audio_separator"
        (package / "separator").mkdir(parents=True)
        (package / "__init__.py").write_text("", encoding="utf-8")
        (package / "separator" / "__init__.py").write_text(
            textwrap.dedent(
                """
                from pathlib import Path

                class _Model:
                    def __init__(self, output_dir):
                        self.output_dir = output_dir

                class Separator:
                    loads = 0

                    def __init__(self, output_dir='.', output_single_stem=None, **kwargs):
                        self.output_dir = output_dir
                        self.output_single_stem = output_single_stem
                        self.model_instance = None

                    def list_supported_model_files(self):
                        return {
                            'MDXC': {
                                'Fake vocals model': {
                                    'filename': 'fake.ckpt',
                                    'scores': {'vocals': {}, 'instrumental': {}},
                                }
                            }
                        }

                    def load_model(self, model_filename):
                        Separator.loads += 1
                        self.model_instance = _Model(self.output_dir)

                    def separate(self, input_path, output_names):
                        if Separator.loads != 1:
                            raise RuntimeError(f'model was loaded {Separator.loads} times')
                        out = Path(self.model_instance.output_dir)
                        out.mkdir(parents=True, exist_ok=True)
                        vocals = out / (output_names['Vocals'] + '.wav')
                        vocals.write_bytes(b'vocals')
                        outputs = [str(vocals)]
                        if self.output_single_stem is None:
                            instrumental = out / (output_names['Instrumental'] + '.wav')
                            instrumental.write_bytes(b'instrumental')
                            outputs.append(str(instrumental))
                        return outputs
                """
            ),
            encoding="utf-8",
        )
        self.root = root
        env = os.environ.copy()
        env["PYTHONPATH"] = str(root / "fake")
        self.process = subprocess.Popen(
            [sys.executable, str(WORKER)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
        )

    def tearDown(self) -> None:
        if self.process.poll() is None:
            self._request("shutdown")
        self.process.wait(timeout=5)
        if self.process.stdin is not None:
            self.process.stdin.close()
        if self.process.stdout is not None:
            self.process.stdout.close()
        if self.process.stderr is not None:
            self.process.stderr.close()
        self.temp_dir.cleanup()

    def _request(self, command: str, **payload):
        assert self.process.stdin is not None
        assert self.process.stdout is not None
        request = {"request_id": command, "command": command, **payload}
        self.process.stdin.write(json.dumps(request) + "\n")
        self.process.stdin.flush()
        response = json.loads(self.process.stdout.readline())
        self.assertEqual(response["request_id"], command)
        return response

    def test_model_is_loaded_once_and_reused_for_two_files(self) -> None:
        source_a = self.root / "a.wav"
        source_b = self.root / "b.wav"
        source_a.write_bytes(b"a")
        source_b.write_bytes(b"b")
        output_dir = self.root / "out"

        response = self._request(
            "load_model",
            model_filename="fake.ckpt",
            output_mode="vocals_instrumental",
            output_dir=str(output_dir),
            parameters={},
        )
        self.assertEqual(response["event"], "result")

        first = self._request(
            "separate",
            input_path=str(source_a),
            output_dir=str(output_dir),
            output_basename="first",
        )
        second = self._request(
            "separate",
            input_path=str(source_b),
            output_dir=str(output_dir),
            output_basename="second",
        )
        self.assertEqual(Path(first["vocals_path"]).name, "first_vocals.wav")
        self.assertEqual(Path(first["instrumental_path"]).name, "first_instrumental.wav")
        self.assertEqual(Path(second["vocals_path"]).name, "second_vocals.wav")

    def test_list_models_returns_normalized_metadata(self) -> None:
        response = self._request("list_models", model_file_dir=str(self.root / "models"))
        self.assertEqual(response["models"][0]["filename"], "fake.ckpt")
        self.assertEqual(response["models"][0]["architecture"], "MDXC")
        self.assertTrue(response["models"][0]["two_stem"])

    def test_vocals_mode_returns_no_instrumental(self) -> None:
        source = self.root / "vocals-only.wav"
        source.write_bytes(b"audio")
        output_dir = self.root / "vocals-only-output"
        self._request(
            "load_model",
            model_filename="fake.ckpt",
            output_mode="vocals",
            output_dir=str(output_dir),
            parameters={},
        )
        response = self._request(
            "separate",
            input_path=str(source),
            output_dir=str(output_dir),
            output_basename="vocals-only",
        )
        self.assertEqual(Path(response["vocals_path"]).name, "vocals-only_vocals.wav")
        self.assertIsNone(response["instrumental_path"])


if __name__ == "__main__":
    unittest.main()
