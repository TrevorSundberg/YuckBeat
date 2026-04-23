#pragma once

#include "YuckBeatEngineAPI.h"

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
	bool loadCurrentSource (double sampleRate, int32_t channels, uint64_t sourceStamp);
	void unload ();
	std::string makeShadowPath (uint64_t sourceStamp) const;

	void* module {};
	YuckBeatEngineHandle instance {};
	YuckBeatEngineDestroyFn destroyFn {};
	YuckBeatEngineResetFn resetFn {};
	YuckBeatEngineProcessFn processFn {};
	YuckBeatEngineVersionFn versionFn {};
	uint64_t loadedSourceStamp {};
	std::string shadowPath;
	uint64_t processCounter {};
	double currentSampleRate = 44100.0;
	int32_t currentChannels = 2;
};

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
