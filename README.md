# YuckBeat

YuckBeat is a VST3 effect template with a practical starter chain: gain, high-pass and low-pass filters, pitch shifting, BPM-synced echo, and room reverb with BPM-synced pre-delay.

The plugin has a stable VST3 shell and a separate hot-reloadable DSP engine binary. Windows builds include a native Win32/GDI editor for FL Studio; Linux and macOS builds currently compile the same processor and hot-reload engine while falling back to the host's generic parameter editor.

## Controls

- `Bypass`: pass audio through unchanged.
- `Volume`: output gain from `-24 dB` to `+12 dB`.
- `High Pass`: removes low frequencies, with `Off` at minimum.
- `Low Pass`: removes high frequencies, with `Off` at maximum.
- `Pitch`: pitch shift in semitones from `-12` to `+12`.
- `Pitch Mix`: dry/shifted blend for the pitch shifter.
- `Echo Mix`: dry/echo blend.
- `Echo Time`: BPM-synced note value from `1/64` to `2 bars`.
- `Echo Feedback`: repeat amount.
- `Reverb Mix`: dry/reverb blend.
- `Room Size`: reverb space size.
- `Damping`: reverb brightness/darkness.
- `Pre-delay`: BPM-synced reverb pre-delay note value.

## Windows From Linux

```sh
export LLVM_MINGW_ROOT=/home/trevor/.local/toolchains/llvm-mingw-20260421-ucrt-ubuntu-22.04-x86_64
export VST3_SDK_ROOT=/home/trevor/vst3sdk
export YUCKBEAT_COPY_VST3_AFTER_BUILD=ON
export YUCKBEAT_VST3_COPY_DIR="/home/trevor/.wine/drive_c/Program Files/Common Files/VST3"
./scripts/build-windows.sh
```

The copied VST3 shell should appear at:

```text
/home/trevor/.wine/drive_c/Program Files/Common Files/VST3/YuckBeat.vst3
```

In FL Studio, use `Options > Manage plugins > Find installed plugins` if it does not appear immediately. After shell, UI, parameter, or factory changes, rebuild the `YuckBeat` target and restart FL Studio or reload the plugin.

## Linux

```sh
export VST3_SDK_ROOT=/home/trevor/vst3sdk
./scripts/build-linux.sh
```

The Linux VST3 bundle is written to:

```text
build-linux/VST3/Release/YuckBeat.vst3
```

Set `YUCKBEAT_COPY_VST3_AFTER_BUILD=ON` to copy it to the platform default `~/.vst3/YuckBeat.vst3`.

## macOS

Run this on macOS with CMake, Ninja, a native compiler, and the Steinberg VST3 SDK:

```sh
export VST3_SDK_ROOT="$HOME/vst3sdk"
./scripts/build-macos.sh
```

Set `YUCKBEAT_COPY_VST3_AFTER_BUILD=ON` to copy the VST3 bundle to `~/Library/Audio/Plug-Ins/VST3/YuckBeat.vst3`.

## Docker

The Docker image supplies CMake/Ninja/build tools. Mount the VST3 SDK and, for Windows cross-builds, mount an llvm-mingw toolchain:

```sh
docker build -f docker/Dockerfile -t yuckbeat-build .

docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$PWD":"$PWD" \
  -v /home/trevor/vst3sdk:/opt/vst3sdk:ro \
  -v /home/trevor/.local/toolchains/llvm-mingw-20260421-ucrt-ubuntu-22.04-x86_64:/opt/llvm-mingw:ro \
  -w "$PWD" \
  yuckbeat-build ./scripts/build-windows.sh
```

Docker can build Linux and Windows targets. Mounting the repo at the same path inside the container keeps the generated hot-reload engine path valid for Wine on the host. macOS VST3 builds need a macOS host or runner because Apple's SDK/toolchain and bundle signing are macOS-specific.

## Hot Reload

For DSP-only changes inside `YuckBeatEngine.cpp`, rebuild only the engine for the active build directory:

```sh
cmake --build /home/trevor/YuckBeat/build-win --config Release --target YuckBeatEngine
```

The loaded VST shell checks the engine binary timestamp during processing. When the build output changes, it copies the engine to a temporary shadow file, loads that copy, swaps the DSP instance, and deletes the previous shadow copy. This keeps the host from locking the real build output, so iterative DSP rebuilds can be picked up without reinstalling the VST3 shell.

Hot reload covers DSP implementation changes only. Changes to the VST3 shell, editor, parameters, component IDs, or build path still require rebuilding `YuckBeat` and reloading the plugin in the host.

## Smoke Test

Windows under Wine:

```sh
cmake --build /home/trevor/YuckBeat/build-win --config Release --target smoke_load_vst3
wine /home/trevor/YuckBeat/build-win/bin/smoke_load_vst3.exe 'C:\Program Files\Common Files\VST3\YuckBeat.vst3'

cmake --build /home/trevor/YuckBeat/build-win --config Release --target smoke_hot_reload_engine
wine /home/trevor/YuckBeat/build-win/bin/smoke_hot_reload_engine.exe
```

Linux:

```sh
/home/trevor/YuckBeat/build-linux/bin/Release/smoke_load_vst3 \
  /home/trevor/YuckBeat/build-linux/VST3/Release/YuckBeat.vst3/Contents/x86_64-linux/YuckBeat.so

/home/trevor/YuckBeat/build-linux/bin/Release/smoke_hot_reload_engine
```
