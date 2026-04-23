#include "YuckBeatEngineAPI.h"
#include "YuckBeatIDs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

namespace YB = Steinberg::Vst::YuckBeat;

constexpr double kPi = 3.14159265358979323846;
constexpr double kReferenceSampleRate = 44100.0;
constexpr int kPitchVoiceCount = 4;

float dbToGain (double db)
{
	return static_cast<float> (std::pow (10.0, db / 20.0));
}

float sanitize (float value)
{
	return std::isfinite (value) ? std::clamp (value, -8.0f, 8.0f) : 0.0f;
}

class Biquad
{
public:
	void reset ()
	{
		z1 = 0.0f;
		z2 = 0.0f;
	}

	void setBypassed ()
	{
		enabled = false;
		b0 = 1.0f;
		b1 = 0.0f;
		b2 = 0.0f;
		a1 = 0.0f;
		a2 = 0.0f;
	}

	void setHighPass (double sampleRate, double frequency)
	{
		if (frequency <= 0.0 || sampleRate <= 0.0)
		{
			setBypassed ();
			return;
		}

		setCoefficients (sampleRate, std::clamp (frequency, 10.0, sampleRate * 0.45), true);
	}

	void setLowPass (double sampleRate, double frequency)
	{
		if (frequency <= 0.0 || sampleRate <= 0.0 || frequency >= sampleRate * 0.45)
		{
			setBypassed ();
			return;
		}

		setCoefficients (sampleRate, std::clamp (frequency, 10.0, sampleRate * 0.45), false);
	}

	float process (float input)
	{
		if (!enabled)
			return input;

		const auto output = b0 * input + z1;
		z1 = b1 * input - a1 * output + z2;
		z2 = b2 * input - a2 * output;
		return sanitize (output);
	}

private:
	void setCoefficients (double sampleRate, double frequency, bool highPass)
	{
		constexpr double q = 0.7071067811865476;
		const auto omega = 2.0 * kPi * frequency / sampleRate;
		const auto sinOmega = std::sin (omega);
		const auto cosOmega = std::cos (omega);
		const auto alpha = sinOmega / (2.0 * q);

		double rawB0 = 0.0;
		double rawB1 = 0.0;
		double rawB2 = 0.0;
		const auto rawA0 = 1.0 + alpha;
		const auto rawA1 = -2.0 * cosOmega;
		const auto rawA2 = 1.0 - alpha;

		if (highPass)
		{
			rawB0 = (1.0 + cosOmega) * 0.5;
			rawB1 = -(1.0 + cosOmega);
			rawB2 = (1.0 + cosOmega) * 0.5;
		}
		else
		{
			rawB0 = (1.0 - cosOmega) * 0.5;
			rawB1 = 1.0 - cosOmega;
			rawB2 = (1.0 - cosOmega) * 0.5;
		}

		b0 = static_cast<float> (rawB0 / rawA0);
		b1 = static_cast<float> (rawB1 / rawA0);
		b2 = static_cast<float> (rawB2 / rawA0);
		a1 = static_cast<float> (rawA1 / rawA0);
		a2 = static_cast<float> (rawA2 / rawA0);
		enabled = true;
	}

	bool enabled = false;
	float b0 = 1.0f;
	float b1 = 0.0f;
	float b2 = 0.0f;
	float a1 = 0.0f;
	float a2 = 0.0f;
	float z1 = 0.0f;
	float z2 = 0.0f;
};

class PitchShifterChannel
{
public:
	void reset (double newSampleRate)
	{
		sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
		bufferLength = std::max<uint32_t> (4096, static_cast<uint32_t> (sampleRate * 0.75));
		buffer.assign (bufferLength, 0.0f);
		writePosition = 0;
		for (int voice = 0; voice < kPitchVoiceCount; ++voice)
			phases[static_cast<size_t> (voice)] = static_cast<float> (voice) / kPitchVoiceCount;
	}

	float process (float input, double ratio, double mix)
	{
		if (buffer.empty ())
			reset (sampleRate);

		buffer[writePosition] = input;

		float shifted = input;
		const auto blend = static_cast<float> (YB::clamp01 (mix));
		if (blend > 0.0001f && std::abs (ratio - 1.0) > 0.0001)
		{
			const auto grainSamples = std::clamp<float> (
			    static_cast<float> (sampleRate * 0.090), 512.0f,
			    static_cast<float> (std::max<uint32_t> (bufferLength - 8, 512)));
			const auto phaseIncrement =
			    std::clamp<float> (static_cast<float> (std::abs (ratio - 1.0) / grainSamples), 0.0f, 0.25f);

			float sum = 0.0f;
			float weightSum = 0.0f;
			for (auto& phase : phases)
			{
				const auto delay = ratio >= 1.0 ? (1.0f - phase) * grainSamples : phase * grainSamples;
				const auto window = 0.5f - 0.5f * std::cos (static_cast<float> (2.0 * kPi) * phase);
				sum += readInterpolated (delay + 4.0f) * window;
				weightSum += window;

				phase += phaseIncrement;
				if (phase >= 1.0f)
					phase -= 1.0f;
			}

			if (weightSum > 0.0001f)
				shifted = sum / weightSum;
		}

		writePosition = (writePosition + 1) % bufferLength;
		return input + (sanitize (shifted) - input) * blend;
	}

private:
	float readInterpolated (float delaySamples) const
	{
		const auto safeDelay = std::clamp (delaySamples, 1.0f, static_cast<float> (bufferLength - 2));
		auto readPosition = static_cast<float> (writePosition) - safeDelay;
		while (readPosition < 0.0f)
			readPosition += static_cast<float> (bufferLength);

		const auto index0 = static_cast<uint32_t> (readPosition) % bufferLength;
		const auto index1 = (index0 + 1) % bufferLength;
		const auto fraction = readPosition - static_cast<float> (index0);
		return buffer[index0] + (buffer[index1] - buffer[index0]) * fraction;
	}

	std::vector<float> buffer;
	uint32_t bufferLength = 1;
	uint32_t writePosition = 0;
	double sampleRate = 44100.0;
	std::array<float, kPitchVoiceCount> phases {};
};

class DelayChannel
{
public:
	void reset (double newSampleRate)
	{
		sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
		bufferLength = std::max<uint32_t> (2, static_cast<uint32_t> (sampleRate * 25.0) + 4);
		buffer.assign (bufferLength, 0.0f);
		writePosition = 0;
	}

	float process (float input, double delaySamples, double feedback, double mix)
	{
		if (buffer.empty ())
			reset (sampleRate);

		const auto delayed = readInterpolated (static_cast<float> (delaySamples));
		buffer[writePosition] =
		    sanitize (input + delayed * static_cast<float> (YB::echoFeedbackFromNormalized (feedback)));
		writePosition = (writePosition + 1) % bufferLength;

		const auto blend = static_cast<float> (YB::echoMixFromNormalized (mix));
		return input + (delayed - input) * blend;
	}

private:
	float readInterpolated (float delaySamples) const
	{
		const auto safeDelay = std::clamp (delaySamples, 1.0f, static_cast<float> (bufferLength - 2));
		auto readPosition = static_cast<float> (writePosition) - safeDelay;
		while (readPosition < 0.0f)
			readPosition += static_cast<float> (bufferLength);

		const auto index0 = static_cast<uint32_t> (readPosition) % bufferLength;
		const auto index1 = (index0 + 1) % bufferLength;
		const auto fraction = readPosition - static_cast<float> (index0);
		return buffer[index0] + (buffer[index1] - buffer[index0]) * fraction;
	}

	std::vector<float> buffer;
	uint32_t bufferLength = 1;
	uint32_t writePosition = 0;
	double sampleRate = 44100.0;
};

struct CombFilter
{
	void reset (uint32_t delaySamples)
	{
		buffer.assign (std::max<uint32_t> (1, delaySamples), 0.0f);
		index = 0;
		filterStore = 0.0f;
	}

	float process (float input, float feedback, float damping)
	{
		const auto output = buffer[index];
		filterStore = output * (1.0f - damping) + filterStore * damping;
		buffer[index] = sanitize (input + filterStore * feedback);
		index = (index + 1) % static_cast<uint32_t> (buffer.size ());
		return output;
	}

	std::vector<float> buffer;
	uint32_t index = 0;
	float filterStore = 0.0f;
};

struct AllpassFilter
{
	void reset (uint32_t delaySamples)
	{
		buffer.assign (std::max<uint32_t> (1, delaySamples), 0.0f);
		index = 0;
	}

	float process (float input)
	{
		const auto buffered = buffer[index];
		const auto output = -input + buffered;
		buffer[index] = input + buffered * 0.5f;
		index = (index + 1) % static_cast<uint32_t> (buffer.size ());
		return sanitize (output);
	}

	std::vector<float> buffer;
	uint32_t index = 0;
};

class ReverbChannel
{
public:
	void reset (double newSampleRate, int channelIndex)
	{
		sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
		const auto scale = sampleRate / kReferenceSampleRate;
		const auto stereoSpread = channelIndex % 2 == 0 ? 0 : 23;

		constexpr std::array<int, 8> combDelays {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
		for (size_t index = 0; index < combs.size (); ++index)
		{
			combs[index].reset (static_cast<uint32_t> (
			    std::max (1.0, (combDelays[index] + stereoSpread) * scale)));
		}

		constexpr std::array<int, 4> allpassDelays {556, 441, 341, 225};
		for (size_t index = 0; index < allpasses.size (); ++index)
		{
			allpasses[index].reset (static_cast<uint32_t> (
			    std::max (1.0, (allpassDelays[index] + stereoSpread) * scale)));
		}

		preDelayLength = std::max<uint32_t> (2, static_cast<uint32_t> (sampleRate * 25.0) + 4);
		preDelayBuffer.assign (preDelayLength, 0.0f);
		preDelayWritePosition = 0;
	}

	float process (float input, double preDelaySamples, double roomSize, double damping)
	{
		if (preDelayBuffer.empty ())
			reset (sampleRate, 0);

		const auto predelayed = readPreDelay (static_cast<float> (preDelaySamples));
		preDelayBuffer[preDelayWritePosition] = input;
		preDelayWritePosition = (preDelayWritePosition + 1) % preDelayLength;

		const auto feedback = static_cast<float> (0.64 + YB::roomSizeFromNormalized (roomSize) * 0.30);
		const auto damp = static_cast<float> (YB::dampingFromNormalized (damping));

		float sum = 0.0f;
		for (auto& comb : combs)
			sum += comb.process (predelayed * 0.18f, feedback, damp);

		auto output = sum * 0.18f;
		for (auto& allpass : allpasses)
			output = allpass.process (output);

		return sanitize (output);
	}

private:
	float readPreDelay (float delaySamples) const
	{
		const auto safeDelay = std::clamp (delaySamples, 1.0f, static_cast<float> (preDelayLength - 2));
		auto readPosition = static_cast<float> (preDelayWritePosition) - safeDelay;
		while (readPosition < 0.0f)
			readPosition += static_cast<float> (preDelayLength);

		const auto index0 = static_cast<uint32_t> (readPosition) % preDelayLength;
		const auto index1 = (index0 + 1) % preDelayLength;
		const auto fraction = readPosition - static_cast<float> (index0);
		return preDelayBuffer[index0] + (preDelayBuffer[index1] - preDelayBuffer[index0]) * fraction;
	}

	std::array<CombFilter, 8> combs;
	std::array<AllpassFilter, 4> allpasses;
	std::vector<float> preDelayBuffer;
	uint32_t preDelayLength = 1;
	uint32_t preDelayWritePosition = 0;
	double sampleRate = 44100.0;
};

struct ChannelState
{
	void reset (double sampleRate, int channelIndex)
	{
		highPass.reset ();
		lowPass.reset ();
		pitch.reset (sampleRate);
		delay.reset (sampleRate);
		reverb.reset (sampleRate, channelIndex);
	}

	Biquad highPass;
	Biquad lowPass;
	PitchShifterChannel pitch;
	DelayChannel delay;
	ReverbChannel reverb;
};

class Engine
{
public:
	void reset (double newSampleRate, int32_t newChannels)
	{
		sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
		const auto channelCount = std::max<int32_t> (1, newChannels);
		channels.assign (static_cast<size_t> (channelCount), {});
		for (size_t index = 0; index < channels.size (); ++index)
			channels[index].reset (sampleRate, static_cast<int> (index));
	}

	void process (const YuckBeatEngineParams& params, const YuckBeatEngineProcessBlock& block)
	{
		if (block.numSamples <= 0 || block.outputChannels <= 0 || !block.outputs)
			return;

		const auto currentSampleRate = block.sampleRate > 0.0 ? block.sampleRate : sampleRate;
		if (channels.empty () || currentSampleRate != sampleRate ||
		    block.outputChannels > static_cast<int32_t> (channels.size ()))
			reset (currentSampleRate, block.outputChannels);

		if (block.tempo >= 20.0 && block.tempo <= 320.0)
			tempo = block.tempo;

		const auto channelCount =
		    std::min<int32_t> (block.outputChannels, static_cast<int32_t> (channels.size ()));
		const auto secondsPerBeat = 60.0 / std::clamp (tempo, 20.0, 320.0);
		const auto echoDelaySamples = YB::syncBeatsFromNormalized (params.echoTime) * secondsPerBeat * sampleRate;
		const auto preDelaySamples = YB::syncBeatsFromNormalized (params.preDelay) * secondsPerBeat * sampleRate;
		const auto pitchRatio = std::pow (2.0, YB::pitchSemitonesFromNormalized (params.pitch) / 12.0);
		const auto volumeGain = dbToGain (YB::volumeDbFromNormalized (params.volume));
		const auto highPassHz = YB::highPassHzFromNormalized (params.highPass);
		const auto lowPassHz = YB::lowPassHzFromNormalized (params.lowPass);
		const auto reverbBlend = static_cast<float> (YB::reverbMixFromNormalized (params.reverbMix));

		for (int32_t channel = 0; channel < channelCount; ++channel)
		{
			channels[static_cast<size_t> (channel)].highPass.setHighPass (sampleRate, highPassHz);
			channels[static_cast<size_t> (channel)].lowPass.setLowPass (sampleRate, lowPassHz);
		}

		for (int32_t sample = 0; sample < block.numSamples; ++sample)
		{
			for (int32_t channel = 0; channel < channelCount; ++channel)
			{
				auto& state = channels[static_cast<size_t> (channel)];
				auto* output = block.outputs[channel];
				const auto* input =
				    channel < block.inputChannels && block.inputs ? block.inputs[channel] : nullptr;
				const auto inputSample = input ? input[sample] : 0.0f;

				if (params.bypass)
				{
					output[sample] = inputSample;
					continue;
				}

				auto processed = state.highPass.process (inputSample);
				processed = state.lowPass.process (processed);
				processed = state.pitch.process (processed, pitchRatio, params.pitchMix);
				processed =
				    state.delay.process (processed, echoDelaySamples, params.echoFeedback, params.echoMix);

				const auto reverbWet =
				    state.reverb.process (processed, preDelaySamples, params.roomSize, params.damping);
				processed += (reverbWet - processed) * reverbBlend;

				output[sample] = sanitize (processed * volumeGain);
			}
		}
	}

private:
	std::vector<ChannelState> channels;
	double sampleRate = 44100.0;
	double tempo = 120.0;
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
	return "YuckBeatEngine template-fx";
}
