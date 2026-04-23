#pragma once

#include <cstdint>

#define YUCKBEAT_VISUAL_API_VERSION 1

#if defined(_WIN32)
#define YUCKBEAT_VISUAL_EXPORT extern "C" __declspec(dllexport)
#else
#define YUCKBEAT_VISUAL_EXPORT extern "C" __attribute__ ((visibility ("default")))
#endif

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

// Keep the displayed preview 256x256, but render a smaller internal buffer so FL stays responsive.
constexpr int FractalDisplayWidth = 256;
constexpr int FractalDisplayHeight = 256;
constexpr int FractalRenderWidth = 128;
constexpr int FractalRenderHeight = 128;

struct FractalRenderParams
{
	float time {};
	float shape {};
	float fold {};
	float power {};
	float scale {};
	float spin {};
	float size {};
	float hue {};
	float light {};
	float roughness {};
	float audioDrive {};
	float ao {};
	float bloom {};
	float rays {};
	float bpmPulse {};
	float bypass {};
};

using YuckBeatVisualApiVersionFn = std::int32_t (*) ();
using YuckBeatVisualRenderFn = void (*) (const FractalRenderParams*, std::uint32_t*);
using YuckBeatVisualVersionFn = const char* (*) ();

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
