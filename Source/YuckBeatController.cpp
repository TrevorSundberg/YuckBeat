#include "YuckBeatController.h"
#include "YuckBeatIDs.h"

#if YUCKBEAT_HAS_NATIVE_EDITOR
#include "YuckBeatEditor.h"
#endif

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

namespace {

void writeAscii (String128 string, const char* text)
{
	UString (string, 128).fromAscii (text);
}

} // namespace

tresult PLUGIN_API Controller::initialize (FUnknown* context)
{
	const auto result = EditController::initialize (context);
	if (result != kResultOk)
		return result;

	parameters.addParameter (STR16 ("Volume"), STR16 ("dB"), 0, DefaultVolume,
	                         ParameterInfo::kCanAutomate, kVolumeId);
	parameters.addParameter (STR16 ("High Pass"), STR16 ("Hz"), 0, DefaultHighPass,
	                         ParameterInfo::kCanAutomate, kHighPassId);
	parameters.addParameter (STR16 ("Low Pass"), STR16 ("Hz"), 0, DefaultLowPass,
	                         ParameterInfo::kCanAutomate, kLowPassId);
	parameters.addParameter (STR16 ("Pitch"), STR16 ("st"), 0, DefaultPitch,
	                         ParameterInfo::kCanAutomate, kPitchId);
	parameters.addParameter (STR16 ("Pitch Mix"), STR16 ("%"), 0, DefaultPitchMix,
	                         ParameterInfo::kCanAutomate, kPitchMixId);
	parameters.addParameter (STR16 ("Echo Mix"), STR16 ("%"), 0, DefaultEchoMix,
	                         ParameterInfo::kCanAutomate, kEchoMixId);
	parameters.addParameter (STR16 ("Echo Time"), STR16 ("sync"), SyncDivisionStepCount,
	                         DefaultEchoTime, ParameterInfo::kCanAutomate, kEchoTimeId);
	parameters.addParameter (STR16 ("Echo Feedback"), STR16 ("%"), 0, DefaultEchoFeedback,
	                         ParameterInfo::kCanAutomate, kEchoFeedbackId);
	parameters.addParameter (STR16 ("Reverb Mix"), STR16 ("%"), 0, DefaultReverbMix,
	                         ParameterInfo::kCanAutomate, kReverbMixId);
	parameters.addParameter (STR16 ("Room Size"), STR16 ("%"), 0, DefaultRoomSize,
	                         ParameterInfo::kCanAutomate, kRoomSizeId);
	parameters.addParameter (STR16 ("Damping"), STR16 ("%"), 0, DefaultDamping,
	                         ParameterInfo::kCanAutomate, kDampingId);
	parameters.addParameter (STR16 ("Pre-delay"), STR16 ("sync"), SyncDivisionStepCount,
	                         DefaultPreDelay, ParameterInfo::kCanAutomate, kPreDelayId);
	parameters.addParameter (STR16 ("Bypass"), nullptr, 1, 0,
	                         ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass, kBypassId);

	return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView (FIDString name)
{
#if YUCKBEAT_HAS_NATIVE_EDITOR
	if (name && std::strcmp (name, ViewType::kEditor) == 0)
		return new Editor (this);
#endif

	return nullptr;
}

tresult PLUGIN_API Controller::getParamStringByValue (ParamID tag, ParamValue valueNormalized,
                                                      String128 string)
{
	char text[48] {};
	const auto value = clamp01 (valueNormalized);

	switch (tag)
	{
		case kVolumeId:
			std::snprintf (text, sizeof (text), "%+.1f dB", volumeDbFromNormalized (value));
			break;
		case kHighPassId:
			if (highPassHzFromNormalized (value) <= 0.0)
				std::snprintf (text, sizeof (text), "Off");
			else
				std::snprintf (text, sizeof (text), "%.0f Hz", highPassHzFromNormalized (value));
			break;
		case kLowPassId:
			if (lowPassHzFromNormalized (value) <= 0.0)
				std::snprintf (text, sizeof (text), "Off");
			else
				std::snprintf (text, sizeof (text), "%.0f Hz", lowPassHzFromNormalized (value));
			break;
		case kPitchId:
			std::snprintf (text, sizeof (text), "%+.1f st", pitchSemitonesFromNormalized (value));
			break;
		case kPitchMixId:
			std::snprintf (text, sizeof (text), "%.0f%%", pitchMixFromNormalized (value) * 100.0);
			break;
		case kEchoMixId:
			std::snprintf (text, sizeof (text), "%.0f%%", echoMixFromNormalized (value) * 100.0);
			break;
		case kEchoTimeId:
			std::snprintf (text, sizeof (text), "%s", syncLabelFromNormalized (value));
			break;
		case kEchoFeedbackId:
			std::snprintf (text, sizeof (text), "%.0f%%", echoFeedbackFromNormalized (value) * 100.0);
			break;
		case kReverbMixId:
			std::snprintf (text, sizeof (text), "%.0f%%", reverbMixFromNormalized (value) * 100.0);
			break;
		case kRoomSizeId:
			std::snprintf (text, sizeof (text), "%.0f%%", roomSizeFromNormalized (value) * 100.0);
			break;
		case kDampingId:
			std::snprintf (text, sizeof (text), "%.0f%%", dampingFromNormalized (value) * 100.0);
			break;
		case kPreDelayId:
			std::snprintf (text, sizeof (text), "%s", syncLabelFromNormalized (value));
			break;
		case kBypassId:
			std::snprintf (text, sizeof (text), "%s", value > 0.5 ? "OFFLINE" : "ACTIVE");
			break;
		default:
			return EditController::getParamStringByValue (tag, valueNormalized, string);
	}

	writeAscii (string, text);
	return kResultTrue;
}

tresult PLUGIN_API Controller::setComponentState (IBStream* state)
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

	ParamValue savedVolume = DefaultVolume;
	ParamValue savedHighPass = DefaultHighPass;
	ParamValue savedLowPass = DefaultLowPass;
	ParamValue savedPitch = DefaultPitch;
	ParamValue savedPitchMix = DefaultPitchMix;
	ParamValue savedEchoMix = DefaultEchoMix;
	ParamValue savedEchoTime = DefaultEchoTime;
	ParamValue savedEchoFeedback = DefaultEchoFeedback;
	ParamValue savedReverbMix = DefaultReverbMix;
	ParamValue savedRoomSize = DefaultRoomSize;
	ParamValue savedDamping = DefaultDamping;
	ParamValue savedPreDelay = DefaultPreDelay;
	bool savedBypass = false;

	if (!streamer.readDouble (savedVolume) || !streamer.readDouble (savedHighPass) ||
	    !streamer.readDouble (savedLowPass) || !streamer.readDouble (savedPitch) ||
	    !streamer.readDouble (savedPitchMix) || !streamer.readDouble (savedEchoMix) ||
	    !streamer.readDouble (savedEchoTime) || !streamer.readDouble (savedEchoFeedback) ||
	    !streamer.readDouble (savedReverbMix) || !streamer.readDouble (savedRoomSize) ||
	    !streamer.readDouble (savedDamping) || !streamer.readDouble (savedPreDelay) ||
	    !streamer.readBool (savedBypass))
		return kResultFalse;

	setParamNormalized (kVolumeId, clamp01 (savedVolume));
	setParamNormalized (kHighPassId, clamp01 (savedHighPass));
	setParamNormalized (kLowPassId, clamp01 (savedLowPass));
	setParamNormalized (kPitchId, clamp01 (savedPitch));
	setParamNormalized (kPitchMixId, clamp01 (savedPitchMix));
	setParamNormalized (kEchoMixId, clamp01 (savedEchoMix));
	setParamNormalized (kEchoTimeId, clamp01 (savedEchoTime));
	setParamNormalized (kEchoFeedbackId, clamp01 (savedEchoFeedback));
	setParamNormalized (kReverbMixId, clamp01 (savedReverbMix));
	setParamNormalized (kRoomSizeId, clamp01 (savedRoomSize));
	setParamNormalized (kDampingId, clamp01 (savedDamping));
	setParamNormalized (kPreDelayId, clamp01 (savedPreDelay));
	setParamNormalized (kBypassId, savedBypass ? 1.0 : 0.0);

	return kResultOk;
}

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
