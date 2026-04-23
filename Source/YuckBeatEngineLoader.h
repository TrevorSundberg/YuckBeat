#pragma once

#include "YuckBeatEngineAPI.h"

#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

#include <cstdint>
#include <string>

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

class EngineLoader
{
public:
	EngineLoader ();
	~EngineLoader ();

	void reset (double sampleRate, int32_t channels);
	bool process (const YuckBeatEngineParams& params, const YuckBeatEngineProcessBlock& block);
	const char* version () const;
	bool loaded () const { return module != nullptr && instance != nullptr; }

private:
	void maybeReload (double sampleRate, int32_t channels);
	bool loadCurrentSource (double sampleRate, int32_t channels, const WIN32_FILE_ATTRIBUTE_DATA& attrs);
	void unload ();
	std::wstring makeShadowPath (const FILETIME& writeTime) const;

	HMODULE module {};
	YuckBeatEngineHandle instance {};
	YuckBeatEngineDestroyFn destroyFn {};
	YuckBeatEngineResetFn resetFn {};
	YuckBeatEngineProcessFn processFn {};
	YuckBeatEngineVersionFn versionFn {};
	FILETIME loadedWriteTime {};
	std::wstring shadowPath;
	uint64_t processCounter {};
	double currentSampleRate = 44100.0;
	int32_t currentChannels = 2;
};

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
