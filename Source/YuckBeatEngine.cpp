#include "YuckBeatEngineAPI.h"
#include "YuckBeatIDs.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

class Engine
{
public:
	void reset (double newSampleRate, int32_t newChannels)
	{
		sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
		const auto channels = std::max<int32_t> (1, newChannels);
		historyLength =
		    std::max<uint32_t> (2, static_cast<uint32_t> (std::ceil (sampleRate * maxHistorySeconds)) + 4);
		history.assign (static_cast<size_t> (channels), std::vector<float> (historyLength, 0.0f));
		writePosition = 0;
		phase = 0.0f;
		smoothedDelaySamples = 0.0f;
	}

	void process (const YuckBeatEngineParams& params, const YuckBeatEngineProcessBlock& block)
	{
		if (block.numSamples <= 0 || block.outputChannels <= 0 || !block.outputs)
			return;

		if (history.empty () || block.sampleRate != sampleRate ||
		    block.outputChannels > static_cast<int32_t> (history.size ()))
			reset (block.sampleRate, block.outputChannels);

		if (block.tempo >= 20.0 && block.tempo <= 320.0)
			tempo = block.tempo;

		const auto currentCycle = Steinberg::Vst::YuckBeat::cycleBeatsFromNormalized (params.cycle);
		if (block.playing && block.projectTimeMusicValid)
		{
			phase = static_cast<float> (std::fmod (block.projectTimeMusic / currentCycle, 1.0));
			if (phase < 0.0f)
				phase += 1.0f;
		}

		const auto channels = std::min<int32_t> (block.outputChannels, static_cast<int32_t> (history.size ()));
		const auto secondsPerBeat = 60.0 / std::clamp (tempo, 20.0, 320.0);
		const auto mixAmount = static_cast<float> (Steinberg::Vst::YuckBeat::mixFromNormalized (params.mix));
		const auto recallBeats =
		    static_cast<float> (Steinberg::Vst::YuckBeat::recallBeatsFromNormalized (params.recall));
		const auto curveAmount =
		    static_cast<float> (Steinberg::Vst::YuckBeat::curveFromNormalized (params.curve));
		const auto smoothMs =
		    static_cast<float> (Steinberg::Vst::YuckBeat::smoothMsFromNormalized (params.smooth));
		const auto feedbackAmount =
		    static_cast<float> (Steinberg::Vst::YuckBeat::feedbackFromNormalized (params.feedback));
		const auto trimGain = static_cast<float> (
		    std::pow (10.0, Steinberg::Vst::YuckBeat::trimDbFromNormalized (params.trim) / 20.0));
		const auto phaseIncrement =
		    static_cast<float> ((tempo / 60.0) / sampleRate /
		                        Steinberg::Vst::YuckBeat::cycleBeatsFromNormalized (params.cycle));

		for (int32_t sample = 0; sample < block.numSamples; ++sample)
		{
			const auto shaped = shapePhase (phase, curveAmount);
			const auto targetDelaySamples =
			    static_cast<float> ((1.0f - shaped) * recallBeats * secondsPerBeat * sampleRate);

			if (smoothMs <= 0.001f)
			{
				smoothedDelaySamples = targetDelaySamples;
			}
			else
			{
				const auto coefficient =
				    1.0f - std::exp (-1.0f / (smoothMs * 0.001f * static_cast<float> (sampleRate)));
				smoothedDelaySamples += coefficient * (targetDelaySamples - smoothedDelaySamples);
			}

			const auto delaySamples =
			    std::clamp (smoothedDelaySamples, 0.0f, static_cast<float> (historyLength - 2));

			for (int32_t channel = 0; channel < channels; ++channel)
			{
				auto* output = block.outputs[channel];
				const auto* input = channel < block.inputChannels && block.inputs ? block.inputs[channel] : nullptr;
				const auto inputSample = input ? input[sample] : 0.0f;

				const auto recalled = delaySamples < 1.0f ? inputSample : readHistorySample (channel, delaySamples);
				history[static_cast<size_t> (channel)][writePosition] =
				    std::clamp (inputSample + recalled * feedbackAmount, -4.0f, 4.0f);

				output[sample] =
				    params.bypass ? inputSample : (inputSample + (recalled - inputSample) * mixAmount) * trimGain;
			}

			writePosition = (writePosition + 1) % historyLength;
			phase += phaseIncrement;
			phase -= std::floor (phase);
		}
	}

private:
	static constexpr double maxHistorySeconds = 20.0;

	float readHistorySample (int32_t channel, float delaySamples) const
	{
		const auto safeChannel = std::clamp<int32_t> (channel, 0, static_cast<int32_t> (history.size ()) - 1);
		auto readPosition = static_cast<float> (writePosition) - delaySamples;

		while (readPosition < 0.0f)
			readPosition += static_cast<float> (historyLength);

		while (readPosition >= static_cast<float> (historyLength))
			readPosition -= static_cast<float> (historyLength);

		const auto index0 =
		    std::clamp<uint32_t> (static_cast<uint32_t> (readPosition), 0, historyLength - 1);
		const auto index1 = (index0 + 1) % historyLength;
		const auto fraction = readPosition - static_cast<float> (index0);
		const auto& channelHistory = history[static_cast<size_t> (safeChannel)];

		return channelHistory[index0] + (channelHistory[index1] - channelHistory[index0]) * fraction;
	}

	static float shapePhase (float phaseToShape, float curveValue)
	{
		const auto x = std::clamp (phaseToShape, 0.0f, 1.0f);
		const auto bend = std::clamp (curveValue, -1.0f, 1.0f);

		if (std::abs (bend) < 0.0001f)
			return x;

		const auto exponent = 1.0f + std::abs (bend) * 4.0f;
		if (bend > 0.0f)
			return std::pow (x, 1.0f / exponent);

		return 1.0f - std::pow (1.0f - x, 1.0f / exponent);
	}

	std::vector<std::vector<float>> history;
	uint32_t historyLength = 1;
	uint32_t writePosition = 0;
	double sampleRate = 44100.0;
	double tempo = 120.0;
	float phase = 0.0f;
	float smoothedDelaySamples = 0.0f;
};

} // namespace

YUCKBEAT_ENGINE_EXPORT int32_t yuckbeat_engine_api_version ()
{
	return YUCKBEAT_ENGINE_API_VERSION;
}

YUCKBEAT_ENGINE_EXPORT YuckBeatEngineHandle yuckbeat_engine_create ()
{
	return new Engine ();
}

YUCKBEAT_ENGINE_EXPORT void yuckbeat_engine_destroy (YuckBeatEngineHandle handle)
{
	delete static_cast<Engine*> (handle);
}

YUCKBEAT_ENGINE_EXPORT void yuckbeat_engine_reset (YuckBeatEngineHandle handle, double sampleRate,
                                                   int32_t channels)
{
	if (auto* engine = static_cast<Engine*> (handle))
		engine->reset (sampleRate, channels);
}

YUCKBEAT_ENGINE_EXPORT void yuckbeat_engine_process (YuckBeatEngineHandle handle,
                                                     const YuckBeatEngineParams* params,
                                                     const YuckBeatEngineProcessBlock* block)
{
	if (auto* engine = static_cast<Engine*> (handle); engine && params && block)
		engine->process (*params, *block);
}

YUCKBEAT_ENGINE_EXPORT const char* yuckbeat_engine_version ()
{
	return "YuckBeatEngine dev";
}
