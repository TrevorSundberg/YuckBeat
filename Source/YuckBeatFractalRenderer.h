#pragma once

#include <cstdint>

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

constexpr int FractalRenderWidth = 256;
constexpr int FractalRenderHeight = 256;

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

void renderFractal (const FractalRenderParams& params, std::uint32_t* pixels) noexcept;

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
