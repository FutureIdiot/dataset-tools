# DiffSinger Dataset Tools

DiffSinger dataset processing tools for singing voice synthesis data preparation, including audio slicing, labeling, forced alignment, and audio-to-MIDI transcription.

## Applications

| Application | Description |
|---|---|
| **MinLabel** | Audio labeling tool with G2P conversion (Mandarin/Cantonese/Japanese) |
| **SlurCutter** | DiffSinger sentence/MIDI editor with piano roll F0 visualization |
| **AudioSlicer** | RMS-based automatic audio slicing with Audacity CSV marker support |
| **LyricFA** | Lyric forced alignment using FunASR Paraformer (Chinese) |
| **HubertFA** | HuBERT phoneme forced alignment with Praat TextGrid output |
| **GameInfer** | Optional source separation followed by GAME audio-to-MIDI transcription |

## Supported Platforms

+ Microsoft Windows (10 ~ 11) — primary, with DirectML GPU acceleration
+ Apple macOS (11+)
+ Linux (Tested on Ubuntu)

## Models

### AsrModel

[AsrModel](https://github.com/openvpi/dataset-tools/releases/tag/AsrModel)

Used for LyricFA, only supports Chinese. [jp&&en version(beta)](https://github.com/wolfgitpr/LyricFA)

### SomeModel

[SomeModel](https://github.com/openvpi/dataset-tools/releases/tag/SomeModel)

### FblModel

[FblModel](https://github.com/openvpi/dataset-tools/releases/tag/FblModel)

Currently, FoxBreatheLabeler only supports annotating breathing using TextGrid files output from SOFA (i.e. overlaying
new "AP" annotations on intervals already marked as "SP").

### GAME Model

Required for GameInfer. Download an archive with `onnx` in its filename from the
[GAME releases](https://github.com/openvpi/GAME/releases), then extract the model directory under
`<app_dir>/model/` or select it from GameInfer. The selected directory must contain `config.json`,
`encoder.onnx`, `segmenter.onnx`, `bd2dur.onnx`, `dur2bd.onnx`, and `estimator.onnx`.

### GameInfer Usage

1. Configure **Source separation** when the inputs are full mixes. Select the model cache, separator model,
   optional GPU acceleration, and either `Vocals` (default) or `Vocals + Instrumental`. Disable separation for
   files that are already clean vocal stems.
2. Select the GAME model directory and optionally enable GPU acceleration.
3. Add one or more WAV, FLAC, or MP3 files, or drag them into the task queue.
4. Choose the default `Language` and `Tempo` for new tasks. Use **Apply to all editable tasks** when
   existing pending or failed tasks should use the same values.
5. Adjust input/output paths, output names, `Language`, or `Tempo` directly in individual queue rows.
   MIDI output defaults to the input directory with the same base file name.
6. Select **Start conversion**. GameInfer separates every pending input first with one persistent separator
   model, closes that worker to release its memory, then loads GAME once and generates MIDI for every successful
   vocal stem. A failed task does not stop later tasks.

Only the vocals stem is passed to GAME. `Vocals + Instrumental` also keeps `<midi-name>_instrumental.wav` for
the user; an instrumental-only mode is intentionally not provided. Separated files are written beside each MIDI
as `<midi-name>_vocals.wav` and, when selected, `<midi-name>_instrumental.wav`.

GameInfer uses a persistent worker backed by
[`audio-separator`](https://github.com/nomadkaraoke/python-audio-separator). The worker runtime is managed by
the application: its Python and locked dependencies are installed into GameInfer's application-data directory
on first use and reused afterwards. Users do not need to install Python or configure an interpreter. Release
packages include the `uv` runtime launcher; source builds bundle it automatically when `uv` is available during
CMake configuration (`GAMEINFER_SEPARATOR_UV_EXECUTABLE` may be set explicitly by packagers, and release builds
can enforce it with `GAMEINFER_REQUIRE_SEPARATOR_UV=ON`).

The model selector is editable. Cached `.ckpt`, `.onnx`, `.pth`, and `.yaml` models are listed automatically;
an `audio-separator` model filename may also be entered directly and will be downloaded into the selected cache
when first loaded. Architecture-specific MDX, MDXC/RoFormer, VR, and Demucs options are available under
**Advanced parameters**.

Release packages bundle `Kim_Vocal_2.onnx` (about 67 MB) as the small default separator model. The model list
also includes `vocals_mel_band_roformer.ckpt`, a larger optional RoFormer model (about 913 MB); when it is not
already cached, GameInfer shows that the first run will download and install it automatically. More models can
be selected by filename from the
[`audio-separator` model list](https://github.com/nomadkaraoke/python-audio-separator#listing-and-filtering-available-models)
and are downloaded from the upstream UVR model repositories on first use.

The interface language can be changed between English and Simplified Chinese from **Settings**.

## Build from Source

### Requirements

| Component | Requirement |             Detailed             |
|:---------:|:-----------:|:--------------------------------:|
|    Qt     |  \>=6.8.0   | Core, Gui, Widgets, Svg, Network |
| Compiler  |  \>=C++17   |      MSVC 2022, GCC, Clang       |
|   CMake   |   \>=3.17   |      >=3.20 is recommended       |

> Tested with Qt 6.8.3 and Qt 6.9.3. CI builds use Qt 6.9.3.

### Setup Environment

You need to install Qt libraries first.

#### Windows

```sh
cd /D src/libs
cmake -Dep=dml -P ../../scripts/setup-onnxruntime.cmake

cd ../../
set QT_DIR=<dir> # directory `Qt6Config.cmake` locates
set Qt6_DIR=%QT_DIR%
set VCPKG_KEEP_ENV_VARS=QT_DIR;Qt6_DIR

git clone https://github.com/microsoft/vcpkg.git
cd /D vcpkg
bootstrap-vcpkg.bat

vcpkg install ^
    --x-manifest-root=../scripts/vcpkg-manifest ^
    --x-install-root=./installed ^
    --triplet=x64-windows
```

#### Unix

```sh
cd src/libs
cmake -Dep=cpu -P ../../scripts/setup-onnxruntime.cmake

cd ../../
export QT_DIR=<dir> # directory `Qt6Config.cmake` locates
export Qt6_DIR=$QT_DIR
export VCPKG_KEEP_ENV_VARS="QT_DIR;Qt6_DIR"

git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

./vcpkg install \
    --x-manifest-root=../scripts/vcpkg-manifest \
    --x-install-root=./installed \
    --triplet=<triplet>

# triplet:
#   Mac:   `x64-osx` or `arm64-osx`
#   Linux: `x64-linux` or `arm64-linux`
```

### Build & Install

```sh
cmake -B build -G Ninja \
    -DCMAKE_INSTALL_PREFIX=<dir> \
    -DCMAKE_PREFIX_PATH=<dir> \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build --target all

cmake --build build --target install
```

### CMake Build Options

| Option | Default | Description |
|---|---|---|
| `BUILD_TESTS` | `ON` | Build `src/tests/` subdirectory (currently empty placeholder) |
| `AUDIO_UTIL_BUILD_TESTS` | `ON` | Build TestAudioUtil |
| `GAME_INFER_BUILD_TESTS` | `ON` | Build TestGame |
| `SOME_INFER_BUILD_TESTS` | `ON` | Build TestSome |
| `RMVPE_INFER_BUILD_TESTS` | `ON` | Build TestRmvpe |
| `ONNXRUNTIME_ENABLE_DML` | `ON` (Windows) | Enable DirectML GPU acceleration |
| `ONNXRUNTIME_ENABLE_CUDA` | `OFF` | Enable CUDA GPU acceleration |

### Build Outputs

| Type | Files |
|---|---|
| Applications | `MinLabel.exe`, `SlurCutter.exe`, `AudioSlicer.exe`, `LyricFA.exe`, `HubertFA.exe`, `GameInfer.exe` |
| Test executables | `TestGame.exe`, `TestRmvpe.exe`, `TestSome.exe`, `TestAudioUtil.exe` |
| Shared libraries | `game-infer.dll`, `rmvpe-infer.dll`, `some-infer.dll`, `audio-util.dll` |

## Libraries

### Related Projects

+ [DiffSinger](https://github.com/openvpi/DiffSinger)
    + Apache 2.0 License

+ [ChorusKit](https://github.com/SineStriker/qsynthesis-revenge)
    + Apache 2.0 License

### Dependencies

+ [Qt 6](https://www.qt.io/) (6.8+)
    + GNU LGPL v2.1 or later
+ [ONNX Runtime](https://github.com/microsoft/onnxruntime)
    + MIT License
+ [FFmpeg](https://github.com/FFmpeg/FFmpeg)
    + GNU LGPL v2.1 or later
+ [LAME](https://lame.sourceforge.io/)
    + GNU LGPL v2.0
+ [SDL](https://github.com/libsdl-org/SDL)
    + Zlib License
+ [SndFile](https://github.com/libsndfile/libsndfile)
    + GNU LGPL v2.1 or later
+ [vcpkg](https://github.com/microsoft/vcpkg)
    + MIT License
+ [r8brain-free-src](https://github.com/avaneev/r8brain-free-src)
    + MIT License
+ [FunASR](https://github.com/alibaba-damo-academy/FunASR)
    + MIT License
+ [fftw3](https://github.com/FFTW/fftw3)
    + GNU GPL v2.0
+ [yaml-cpp](https://github.com/jbeder/yaml-cpp)
    + MIT License
+ [wolf-midi](https://github.com/user/wolf-midi)
    + MIT License
+ [nlohmann/json](https://github.com/nlohmann/json)
    + MIT License
+ [FoxBreatheLabeler](https://github.com/autumn-DL/FoxBreatheLabeler)
    + GNU AGPL v3.0
+ [textgrid.hpp](https://github.com/eiichiroi/textgrid.hpp)
    + MIT License
+ [soxr](https://sourceforge.net/projects/soxr/)
    + GNU LGPL v2.1
+ [mpg123](https://www.mpg123.de/)
    + GNU LGPL v2.1

## License

This repository is licensed under the Apache 2.0 License.
