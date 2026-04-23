#include "YuckBeatEngineLoader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

int main ()
{
	using Steinberg::Vst::YuckBeat::EngineLoader;

	constexpr int numSamples = 128;
	std::array<float, numSamples> inputLeft {};
	std::array<float, numSamples> inputRight {};
	std::array<float, numSamples> outputLeft {};
	std::array<float, numSamples> outputRight {};

	for (int index = 0; index < numSamples; ++index)
	{
		inputLeft[static_cast<size_t> (index)] = index == 0 ? 1.0f : 0.25f;
		inputRight[static_cast<size_t> (index)] = std::sin (static_cast<float> (index) * 0.05f);
	}

	const float* inputs[] = {inputLeft.data (), inputRight.data ()};
	float* outputs[] = {outputLeft.data (), outputRight.data ()};

	YuckBeatEngineParams params {};
	params.mix = 0.75;
	params.recall = 0.25;
	params.cycle = 0.5;
	params.curve = 0.5;
	params.smooth = 0.1;
	params.feedback = 0.0;
	params.trim = 0.5;
	params.bypass = 0;

	YuckBeatEngineProcessBlock block {};
	block.inputs = inputs;
	block.outputs = outputs;
	block.inputChannels = 2;
	block.outputChannels = 2;
	block.numSamples = numSamples;
	block.sampleRate = 44100.0;
	block.tempo = 140.0;
	block.projectTimeMusic = 0.0;
	block.playing = 1;
	block.projectTimeMusicValid = 1;

	EngineLoader loader;
	loader.reset (block.sampleRate, block.outputChannels);
	if (!loader.process (params, block))
	{
		std::cerr << "EngineLoader did not load an engine\n";
		return 1;
	}

	const auto validSample = [] (float value) { return std::isfinite (value) && std::abs (value) < 8.0f; };
	const auto leftOk = std::all_of (outputLeft.begin (), outputLeft.end (), validSample);
	const auto rightOk = std::all_of (outputRight.begin (), outputRight.end (), validSample);
	if (!leftOk || !rightOk)
	{
		std::cerr << "Engine output failed sanity checks\n";
		return 1;
	}

	std::cout << "Loaded hot-reload engine: " << loader.version () << "\n";
	return 0;
}
