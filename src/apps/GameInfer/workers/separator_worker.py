#!/usr/bin/env python3
"""Persistent JSON-lines worker for audio-separator.

The worker keeps one Separator/model instance alive for a complete separation
phase. Protocol messages are written to stdout; library logs stay on stderr.
"""

from __future__ import annotations

import json
import logging
import os
import shutil
import sys
import tempfile
import traceback
from pathlib import Path
from typing import Any


def _reply(request_id: Any, event: str, **payload: Any) -> None:
    message = {"request_id": request_id, "event": event, **payload}
    sys.stdout.write(json.dumps(message, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def _ensure_ffmpeg_on_path() -> Path:
    existing = shutil.which("ffmpeg")
    if existing:
        return Path(existing).resolve()

    from imageio_ffmpeg import get_ffmpeg_exe

    bundled = Path(get_ffmpeg_exe()).resolve()
    if not bundled.is_file():
        raise FileNotFoundError(f"bundled FFmpeg executable not found: {bundled}")

    configured_tools_dir = os.environ.get("GAMEINFER_SEPARATOR_TOOLS_DIR", "").strip()
    tools_dir = (
        Path(configured_tools_dir)
        if configured_tools_dir
        else Path(tempfile.gettempdir()) / "gameinfer-separator-tools"
    )
    tools_dir.mkdir(parents=True, exist_ok=True)
    ffmpeg = tools_dir / ("ffmpeg.exe" if os.name == "nt" else "ffmpeg")
    if not ffmpeg.is_file() or ffmpeg.stat().st_size != bundled.stat().st_size:
        shutil.copy2(bundled, ffmpeg)
    if os.name != "nt":
        ffmpeg.chmod(ffmpeg.stat().st_mode | 0o111)

    current_path = os.environ.get("PATH", "")
    os.environ["PATH"] = str(tools_dir) + (os.pathsep + current_path if current_path else "")
    resolved = shutil.which("ffmpeg")
    if not resolved:
        raise FileNotFoundError(f"failed to expose bundled FFmpeg from: {ffmpeg}")
    return Path(resolved).resolve()


def _flatten_models(grouped: dict[str, Any]) -> list[dict[str, Any]]:
    models: list[dict[str, Any]] = []
    for architecture, entries in grouped.items():
        if not isinstance(entries, dict):
            continue
        for friendly_name, metadata in entries.items():
            if not isinstance(metadata, dict):
                continue
            scores = metadata.get("scores") or {}
            stems = [str(stem) for stem in scores] if isinstance(scores, dict) else []
            if stems and not any(stem.lower() == "vocals" for stem in stems):
                continue
            filename = metadata.get("filename")
            if not filename:
                continue
            models.append(
                {
                    "architecture": str(architecture),
                    "friendly_name": str(friendly_name),
                    "filename": str(filename),
                    "stems": stems,
                    "two_stem": len(stems) == 2 if stems else None,
                }
            )
    return sorted(models, key=lambda item: (item["architecture"], item["friendly_name"]))


class SeparatorWorker:
    def __init__(self) -> None:
        self.separator: Any = None
        self.output_mode = "vocals"

    @staticmethod
    def _separator_class() -> Any:
        _ensure_ffmpeg_on_path()
        from audio_separator.separator import Separator

        return Separator

    def list_models(self, request: dict[str, Any]) -> dict[str, Any]:
        options: dict[str, Any] = {"info_only": True}
        if request.get("model_file_dir"):
            options["model_file_dir"] = request["model_file_dir"]
        separator = self._separator_class()(**options)
        return {"models": _flatten_models(separator.list_supported_model_files())}

    def load_model(self, request: dict[str, Any]) -> dict[str, Any]:
        model_filename = str(request.get("model_filename") or "").strip()
        if not model_filename:
            raise ValueError("model_filename is required")

        self.output_mode = str(request.get("output_mode") or "vocals")
        if self.output_mode not in {"vocals", "vocals_instrumental"}:
            raise ValueError(f"unsupported output_mode: {self.output_mode}")

        parameters = request.get("parameters") or {}
        if not isinstance(parameters, dict):
            raise ValueError("parameters must be an object")

        output_dir = Path(str(request.get("output_dir") or ".")).resolve()
        output_dir.mkdir(parents=True, exist_ok=True)

        backend = str(request.get("backend") or "auto")
        if backend not in {"auto", "cpu", "cuda", "directml"}:
            raise ValueError(f"unsupported backend: {backend}")
        if backend in {"cpu", "directml"}:
            os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
        if backend in {"cpu", "cuda"}:
            import torch

            if backend == "cpu" and hasattr(torch.backends, "mps"):
                torch.backends.mps.is_available = lambda: False
            if backend == "cuda" and not torch.cuda.is_available():
                raise RuntimeError("CUDA backend was selected, but PyTorch cannot access CUDA")

        separator_kwargs = {
            "log_level": logging.INFO,
            "model_file_dir": str(request.get("model_file_dir") or "").strip() or None,
            "output_dir": str(output_dir),
            "output_format": "WAV",
            "output_single_stem": "Vocals" if self.output_mode == "vocals" else None,
            "normalization_threshold": float(parameters.get("normalization", 0.9)),
            "amplification_threshold": float(parameters.get("amplification", 0.0)),
            "sample_rate": int(parameters.get("sample_rate", 44100)),
            "use_soundfile": bool(parameters.get("use_soundfile", True)),
            "use_autocast": bool(parameters.get("use_autocast", False)),
            "use_directml": bool(parameters.get("use_directml", False)),
            "chunk_duration": int(parameters.get("chunk_duration", 0)) or None,
        }
        if separator_kwargs["model_file_dir"] is None:
            separator_kwargs.pop("model_file_dir")
        for key, parameter_key in (
            ("mdx_params", "mdx"),
            ("mdxc_params", "mdxc"),
            ("vr_params", "vr"),
            ("demucs_params", "demucs"),
        ):
            value = parameters.get(parameter_key)
            if isinstance(value, dict) and value:
                separator_kwargs[key] = value

        self.separator = self._separator_class()(**separator_kwargs)
        self.separator.load_model(model_filename=model_filename)
        model_instance = getattr(self.separator, "model_instance", None)
        return {
            "model_filename": model_filename,
            "model_type": type(model_instance).__name__ if model_instance is not None else "",
        }

    def separate(self, request: dict[str, Any]) -> dict[str, Any]:
        if self.separator is None:
            raise RuntimeError("separator model is not loaded")

        input_path = Path(str(request.get("input_path") or "")).resolve()
        if not input_path.is_file():
            raise FileNotFoundError(f"input audio not found: {input_path}")

        output_dir = Path(str(request.get("output_dir") or ".")).resolve()
        output_dir.mkdir(parents=True, exist_ok=True)
        output_basename = str(request.get("output_basename") or input_path.stem).strip()
        if not output_basename:
            raise ValueError("output_basename is required")

        # audio-separator stores output_dir both on the facade and the loaded
        # architecture instance. Updating both keeps the model resident while
        # allowing each queue item to write beside its own MIDI output.
        self.separator.output_dir = str(output_dir)
        model_instance = getattr(self.separator, "model_instance", None)
        if model_instance is not None:
            model_instance.output_dir = str(output_dir)

        output_names = {
            "Vocals": f"{output_basename}_vocals",
            "Instrumental": f"{output_basename}_instrumental",
            "Other": f"{output_basename}_instrumental",
        }
        raw_outputs = self.separator.separate(str(input_path), output_names)
        outputs = []
        for path in raw_outputs or []:
            candidate = Path(path)
            if not candidate.is_absolute():
                candidate = output_dir / candidate
            outputs.append(str(candidate.resolve()))
        existing_outputs = [path for path in outputs if Path(path).is_file()]

        vocals = [path for path in existing_outputs if "vocal" in Path(path).stem.lower()]
        non_vocals = [path for path in existing_outputs if path not in vocals]
        if len(vocals) != 1:
            raise RuntimeError(f"expected exactly one vocals output, found {len(vocals)}")
        if self.output_mode == "vocals_instrumental" and len(non_vocals) != 1:
            raise RuntimeError(
                "Vocals + Instrumental requires a two-stem model; "
                f"the selected model produced {len(non_vocals)} non-vocal stems"
            )

        return {
            "vocals_path": vocals[0],
            "instrumental_path": non_vocals[0] if non_vocals else None,
            "outputs": existing_outputs,
        }

    def handle(self, request: dict[str, Any]) -> dict[str, Any]:
        command = request.get("command")
        if command == "ping":
            return {"protocol_version": 1}
        if command == "list_models":
            return self.list_models(request)
        if command == "load_model":
            return self.load_model(request)
        if command == "separate":
            return self.separate(request)
        if command == "shutdown":
            return {"shutdown": True}
        raise ValueError(f"unknown command: {command}")


def main() -> int:
    worker = SeparatorWorker()
    for raw_line in sys.stdin:
        raw_line = raw_line.strip()
        if not raw_line:
            continue
        request_id: Any = None
        try:
            request = json.loads(raw_line)
            if not isinstance(request, dict):
                raise ValueError("request must be a JSON object")
            request_id = request.get("request_id")
            result = worker.handle(request)
            _reply(request_id, "result", **result)
            if result.get("shutdown"):
                return 0
        except Exception as exc:  # keep the worker alive after a job failure
            traceback.print_exc(file=sys.stderr)
            _reply(request_id, "error", error=str(exc), error_type=type(exc).__name__)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
