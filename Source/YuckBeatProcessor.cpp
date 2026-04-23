#include "YuckBeatProcessor.h"
#include "YuckBeatController.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

#include <algorithm>
#include <cstring>

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

Processor::Processor ()
{
	setControllerClass (ControllerUID);
	processContextRequirements.needTempo ().needProjectTimeMusic ().needTransportState ();
}

tresult PLUGIN_API Processor::initialize (FUnknown* context)
{
	const auto result = AudioEffect::initialize (context);
	if (result != kResultOk)
		return result;

	addAudioInput (STR16 ("Stereo In"), SpeakerArr::kStereo);
	addAudioOutput (STR16 ("Stereo Out"), SpeakerArr::kStereo);
	return kResultOk;
}

tresult PLUGIN_API Processor::setupProcessing (ProcessSetup& setup)
{
	const auto result = AudioEffect::setupProcessing (setup);
	if (result == kResultOk)
	{
		sampleRate = setup.sampleRate > 0.0 ? setup.sampleRate : 44100.0;
		engine.reset (sampleRate, outputChannels);
	}

	return result;
}

tresult PLUGIN_API Processor::setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                                  SpeakerArrangement* outputs, int32 numOuts)
{
	if (numIns == 1 && numOuts == 1 && inputs[0] == outputs[0])
	{
		const auto channels = SpeakerArr::getChannelCount (inputs[0]);
		if (channels == 1 || channels == 2)
			return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
	}

	return kResultFalse;
}

tresult PLUGIN_API Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
	return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API Processor::setActive (TBool state)
{
	if (state)
	{
		SpeakerArrangement arrangement = SpeakerArr::kStereo;
		getBusArrangement (kOutput, 0, arrangement);
		outputChannels = std::max<int32> (1, SpeakerArr::getChannelCount (arrangement));
		sampleRate = processSetup.sampleRate > 0.0 ? processSetup.sampleRate : 44100.0;
		engine.reset (sampleRate, outputChannels);
	}

	return AudioEffect::setActive (state);
}

void Processor::applyParameterChanges (IParameterChanges& changes)
{
	const auto numParamsChanged = changes.getParameterCount ();

	for (int32 index = 0; index < numParamsChanged; ++index)
	{
		if (auto* queue = changes.getParameterData (index))
		{
			ParamValue value = 0.0;
			int32 sampleOffset = 0;
			const auto pointCount = queue->getPointCount ();

			if (pointCount <= 0 || queue->getPoint (pointCount - 1, sampleOffset, value) != kResultTrue)
				continue;

			value = clamp01 (value);

			switch (queue->getParameterId ())
			{
				case kMixId: mix = value; break;
				case kRecallId: recall = value; break;
				case kCycleId: cycle = value; break;
				case kCurveId: curve = value; break;
				case kSmoothId: smooth = value; break;
				case kFeedbackId: feedback = value; break;
				case kTrimId: trim = value; break;
				case kBypassId: bypass = value > 0.5; break;
			}
		}
	}
}

tresult PLUGIN_API Processor::process (ProcessData& data)
{
	if (data.inputParameterChanges)
		applyParameterChanges (*data.inputParameterChanges);

	if (data.numInputs == 0 || data.numOutputs == 0 || data.numSamples <= 0)
		return kResultOk;

	double projectTimeMusic = 0.0;
	bool playing = false;
	bool projectTimeMusicValid = false;
	if (data.processContext)
	{
		if ((data.processContext->state & ProcessContext::kTempoValid) != 0)
			tempo = std::clamp (data.processContext->tempo, 20.0, 320.0);

		playing = (data.processContext->state & ProcessContext::kPlaying) != 0;
		projectTimeMusicValid =
		    (data.processContext->state & ProcessContext::kProjectTimeMusicValid) != 0;
		projectTimeMusic = data.processContext->projectTimeMusic;
	}

	YuckBeatEngineParams params {};
	params.mix = mix;
	params.recall = recall;
	params.cycle = cycle;
	params.curve = curve;
	params.smooth = smooth;
	params.feedback = feedback;
	params.trim = trim;
	params.bypass = bypass ? 1 : 0;

	YuckBeatEngineProcessBlock block {};
	block.inputs = data.inputs[0].channelBuffers32;
	block.outputs = data.outputs[0].channelBuffers32;
	block.inputChannels = data.inputs[0].numChannels;
	block.outputChannels = data.outputs[0].numChannels;
	block.numSamples = data.numSamples;
	block.sampleRate = processSetup.sampleRate > 0.0 ? processSetup.sampleRate : sampleRate;
	block.tempo = tempo;
	block.projectTimeMusic = projectTimeMusic;
	block.playing = playing ? 1 : 0;
	block.projectTimeMusicValid = projectTimeMusicValid ? 1 : 0;

	if (!engine.process (params, block))
		copyInputToOutput (data);

	data.outputs[0].silenceFlags = 0;
	return kResultOk;
}

void Processor::copyInputToOutput (ProcessData& data)
{
	const auto channels = std::min<int32> (data.inputs[0].numChannels, data.outputs[0].numChannels);
	for (int32 channel = 0; channel < channels; ++channel)
	{
		std::memcpy (data.outputs[0].channelBuffers32[channel],
		             data.inputs[0].channelBuffers32[channel],
		             static_cast<size_t> (data.numSamples) * sizeof (float));
	}

	for (int32 channel = channels; channel < data.outputs[0].numChannels; ++channel)
	{
		std::memset (data.outputs[0].channelBuffers32[channel], 0,
		             static_cast<size_t> (data.numSamples) * sizeof (float));
	}
}

tresult PLUGIN_API Processor::setState (IBStream* state)
{
	if (!state)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);
	bool savedBypass = false;

	if (!streamer.readDouble (mix) || !streamer.readDouble (recall) || !streamer.readDouble (cycle) ||
	    !streamer.readDouble (curve) || !streamer.readDouble (smooth) || !streamer.readDouble (feedback) ||
	    !streamer.readDouble (trim) || !streamer.readBool (savedBypass))
		return kResultFalse;

	bypass = savedBypass;
	return kResultOk;
}

tresult PLUGIN_API Processor::getState (IBStream* state)
{
	if (!state)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);
	streamer.writeDouble (mix);
	streamer.writeDouble (recall);
	streamer.writeDouble (cycle);
	streamer.writeDouble (curve);
	streamer.writeDouble (smooth);
	streamer.writeDouble (feedback);
	streamer.writeDouble (trim);
	streamer.writeBool (bypass);

	return kResultOk;
}

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
