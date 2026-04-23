#pragma once

#include <cstdint>

#define YUCKBEAT_ENGINE_API_VERSION 2

#if defined(_WIN32)
#define YUCKBEAT_ENGINE_EXPORT extern "C" __declspec (dllexport)
#else
#define YUCKBEAT_ENGINE_EXPORT extern "C" __attribute__ ((visibility ("default")))
#endif

struct YuckBeatEngineParams
{
	double volume {};
	double highPass {};
	double lowPass {};
	double pitch {};
	double pitchMix {};
	double echoMix {};
	double echoTime {};
	double echoFeedback {};
	double reverbMix {};
	double roomSize {};
	double damping {};
	double preDelay {};
	int bypass {};
};

struct YuckBeatEngineProcessBlock
{
	const float* const* inputs {};
	float** outputs {};
	int32_t inputChannels {};
	int32_t outputChannels {};
	int32_t numSamples {};
	double sampleRate {};
	double tempo {};
	double projectTimeMusic {};
	int playing {};
	int projectTimeMusicValid {};
};

using YuckBeatEngineHandle = void*;
using YuckBeatEngineApiVersionFn = int32_t (*) ();
using YuckBeatEngineCreateFn = YuckBeatEngineHandle (*) ();
using YuckBeatEngineDestroyFn = void (*) (YuckBeatEngineHandle);
using YuckBeatEngineResetFn = void (*) (YuckBeatEngineHandle, double, int32_t);
using YuckBeatEngineProcessFn = void (*) (YuckBeatEngineHandle, const YuckBeatEngineParams*,
                                         const YuckBeatEngineProcessBlock*);
using YuckBeatEngineVersionFn = const char* (*) ();
