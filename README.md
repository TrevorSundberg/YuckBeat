# YuckBeat

YuckBeat is a VST3 effect for FL Studio under Wine. It records incoming audio into a rolling history buffer, then reads back earlier material along a tempo-synced curve for Gross Beat-style time recall.

The plugin has a stable VST3 shell, a native Win32/GDI editor, and a separate hot-reloadable DSP engine DLL. FL Studio loads the shell from the Wine VST3 folder, while the shell shadows and reloads `YuckBeatEngine.dll` from the build directory when that engine changes.

## Controls

- `Bypass`: pass audio through unchanged.
- `Recall`: how far back in the rolling buffer to read.
- `Depth`: blend amount between dry input and recalled audio.
- `Curve`: shape of the time recall ramp.
- `Smooth`: crossfade smoothing for reduced clicks.
- `Grid`: tempo-synced step size.
- `Gate`: rhythmic gate amount.
- `Mix`: final wet/dry mix.

## Build

```sh
cmake -S /home/trevor/YuckBeat -B /home/trevor/YuckBeat/build-win \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/home/trevor/YuckBeat/cmake/llvm-mingw-x86_64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DYUCKBEAT_VST3_COPY_DIR="/home/trevor/.wine/drive_c/Program Files/Common Files/VST3"

cmake --build /home/trevor/YuckBeat/build-win --config Release --target YuckBeat
```

The copied VST3 shell should appear at:

```text
/home/trevor/.wine/drive_c/Program Files/Common Files/VST3/YuckBeat.vst3
```

In FL Studio, use `Options > Manage plugins > Find installed plugins` if it does not appear immediately. After shell, UI, parameter, or factory changes, rebuild the `YuckBeat` target and restart FL Studio or reload the plugin.

## Hot Reload

For DSP-only changes inside `YuckBeatEngine.cpp`, rebuild only the engine:

```sh
cmake --build /home/trevor/YuckBeat/build-win --config Release --target YuckBeatEngine
```

The loaded VST shell checks the engine DLL timestamp during processing. When the build output changes, it copies the engine DLL to a temporary shadow file, loads that copy, swaps the DSP instance, and deletes the previous shadow copy. This keeps FL Studio from locking the real build output, so iterative DSP rebuilds can be picked up without reinstalling the VST3 shell.

Hot reload covers DSP implementation changes only. Changes to the VST3 shell, editor, parameters, component IDs, or build path still require rebuilding `YuckBeat` and reloading the plugin in FL Studio.

## Smoke Test

```sh
cmake --build /home/trevor/YuckBeat/build-win --config Release --target smoke_load_vst3
wine /home/trevor/YuckBeat/build-win/bin/smoke_load_vst3.exe 'C:\Program Files\Common Files\VST3\YuckBeat.vst3'
```
