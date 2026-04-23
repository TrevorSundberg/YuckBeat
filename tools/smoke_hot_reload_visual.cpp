#include "YuckBeatVisualLoader.h"

#include <array>
#include <cstdint>
#include <iostream>

int main ()
{
	using Steinberg::Vst::YuckBeat::FractalRenderHeight;
	using Steinberg::Vst::YuckBeat::FractalRenderParams;
	using Steinberg::Vst::YuckBeat::FractalRenderWidth;
	using Steinberg::Vst::YuckBeat::VisualLoader;

	VisualLoader loader;
	FractalRenderParams params {};
	params.shape = 0.4f;
	params.fold = 0.5f;
	params.power = 0.6f;
	params.scale = 0.5f;
	params.spin = 0.3f;
	params.size = 0.5f;
	params.hue = 0.28f;
	params.light = 0.6f;
	params.roughness = 0.45f;
	params.audioDrive = 0.35f;
	params.ao = 0.6f;
	params.bloom = 0.45f;
	params.rays = 0.4f;
	params.bpmPulse = 0.5f;

	std::array<std::uint32_t, FractalRenderWidth * FractalRenderHeight> pixels {};
	if (!loader.render (params, pixels.data ()))
	{
		std::cerr << "VisualLoader did not load visual module\n";
		return 1;
	}

	std::uint32_t checksum = 0;
	for (auto pixel : pixels)
		checksum = (checksum * 33u) ^ pixel;

	if (checksum == 0)
	{
		std::cerr << "Visual render produced an empty checksum\n";
		return 1;
	}

	std::cout << "Loaded hot-reload visual: " << loader.version () << " checksum=" << checksum
	          << "\n";
	return 0;
}
