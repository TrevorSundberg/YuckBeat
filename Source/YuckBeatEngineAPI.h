#pragma once

#include <cstdint>

#define YUCKBEAT_ENGINE_API_VERSION 1

struct YuckBeatEngineParams
{
	double mix {};
	double recall {};
	double cycle {};
	double curve {};
	double smooth {};
	double feedback {};
	double trim {};
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
