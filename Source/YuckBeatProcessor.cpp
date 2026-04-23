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
				case kVolumeId: volume = value; break;
				case kHighPassId: highPass = value; break;
				case kLowPassId: lowPass = value; break;
				case kPitchId: pitch = value; break;
				case kPitchMixId: pitchMix = value; break;
				case kEchoMixId: echoMix = value; break;
				case kEchoTimeId: echoTime = value; break;
				case kEchoFeedbackId: echoFeedback = value; break;
				case kReverbMixId: reverbMix = value; break;
				case kRoomSizeId: roomSize = value; break;
				case kDampingId: damping = value; break;
				case kPreDelayId: preDelay = value; break;
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
	params.volume = volume;
	params.highPass = highPass;
	params.lowPass = lowPass;
	params.pitch = pitch;
	params.pitchMix = pitchMix;
	params.echoMix = echoMix;
	params.echoTime = echoTime;
	params.echoFeedback = echoFeedback;
	params.reverbMix = reverbMix;
	params.roomSize = roomSize;
	params.damping = damping;
	params.preDelay = preDelay;
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
	int32 magic = 0;
	int32 version = 0;
	if (!streamer.readInt32 (magic) || magic != StateMagic)
		return kResultOk;
	if (!streamer.readInt32 (version) || version != StateVersion)
		return kResultOk;

	bool savedBypass = false;

	if (!streamer.readDouble (volume) || !streamer.readDouble (highPass) ||
	    !streamer.readDouble (lowPass) || !streamer.readDouble (pitch) ||
	    !streamer.readDouble (pitchMix) || !streamer.readDouble (echoMix) ||
	    !streamer.readDouble (echoTime) || !streamer.readDouble (echoFeedback) ||
	    !streamer.readDouble (reverbMix) || !streamer.readDouble (roomSize) ||
	    !streamer.readDouble (damping) || !streamer.readDouble (preDelay) ||
	    !streamer.readBool (savedBypass))
		return kResultFalse;

	volume = clamp01 (volume);
	highPass = clamp01 (highPass);
	lowPass = clamp01 (lowPass);
	pitch = clamp01 (pitch);
	pitchMix = clamp01 (pitchMix);
	echoMix = clamp01 (echoMix);
	echoTime = clamp01 (echoTime);
	echoFeedback = clamp01 (echoFeedback);
	reverbMix = clamp01 (reverbMix);
	roomSize = clamp01 (roomSize);
	damping = clamp01 (damping);
	preDelay = clamp01 (preDelay);
	bypass = savedBypass;
	return kResultOk;
}

tresult PLUGIN_API Processor::getState (IBStream* state)
{
	if (!state)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);
	streamer.writeInt32 (StateMagic);
	streamer.writeInt32 (StateVersion);
	streamer.writeDouble (volume);
	streamer.writeDouble (highPass);
	streamer.writeDouble (lowPass);
	streamer.writeDouble (pitch);
	streamer.writeDouble (pitchMix);
	streamer.writeDouble (echoMix);
	streamer.writeDouble (echoTime);
	streamer.writeDouble (echoFeedback);
	streamer.writeDouble (reverbMix);
	streamer.writeDouble (roomSize);
	streamer.writeDouble (damping);
	streamer.writeDouble (preDelay);
	streamer.writeBool (bypass);

	return kResultOk;
}

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
