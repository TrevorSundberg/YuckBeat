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
	parameters.addParameter (STR16 ("SDF Shape"), STR16 ("%"), 0, DefaultFractalShape,
	                         ParameterInfo::kCanAutomate, kFractalShapeId);
	parameters.addParameter (STR16 ("SDF Fold"), STR16 ("%"), 0, DefaultFractalFold,
	                         ParameterInfo::kCanAutomate, kFractalFoldId);
	parameters.addParameter (STR16 ("SDF Power"), STR16 ("%"), 0, DefaultFractalPower,
	                         ParameterInfo::kCanAutomate, kFractalPowerId);
	parameters.addParameter (STR16 ("SDF Scale"), STR16 ("%"), 0, DefaultFractalScale,
	                         ParameterInfo::kCanAutomate, kFractalScaleId);
	parameters.addParameter (STR16 ("SDF Spin"), STR16 ("x"), 0, DefaultFractalSpin,
	                         ParameterInfo::kCanAutomate, kFractalSpinId);
	parameters.addParameter (STR16 ("SDF Size"), STR16 ("%"), 0, DefaultFractalSize,
	                         ParameterInfo::kCanAutomate, kFractalSizeId);
	parameters.addParameter (STR16 ("SDF Hue"), STR16 ("deg"), 0, DefaultFractalHue,
	                         ParameterInfo::kCanAutomate, kFractalHueId);
	parameters.addParameter (STR16 ("SDF Light"), STR16 ("%"), 0, DefaultFractalLight,
	                         ParameterInfo::kCanAutomate, kFractalLightId);
	parameters.addParameter (STR16 ("God Rays"), STR16 ("%"), 0, DefaultFractalRays,
	                         ParameterInfo::kCanAutomate, kFractalRaysId);
	parameters.addParameter (STR16 ("Bloom"), STR16 ("%"), 0, DefaultFractalBloom,
	                         ParameterInfo::kCanAutomate, kFractalBloomId);
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
		case kFractalShapeId:
		case kFractalFoldId:
		case kFractalPowerId:
		case kFractalScaleId:
		case kFractalSizeId:
		case kFractalLightId:
		case kFractalRaysId:
		case kFractalBloomId:
			std::snprintf (text, sizeof (text), "%.0f%%", percentFromNormalized (value));
			break;
		case kFractalSpinId:
			std::snprintf (text, sizeof (text), "%.2fx", spinRateFromNormalized (value));
			break;
		case kFractalHueId:
			std::snprintf (text, sizeof (text), "%.0f deg", hueDegreesFromNormalized (value));
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
	if (!streamer.readInt32 (version) || version < 1 || version > StateVersion)
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
	ParamValue savedFractalShape = DefaultFractalShape;
	ParamValue savedFractalFold = DefaultFractalFold;
	ParamValue savedFractalPower = DefaultFractalPower;
	ParamValue savedFractalScale = DefaultFractalScale;
	ParamValue savedFractalSpin = DefaultFractalSpin;
	ParamValue savedFractalSize = DefaultFractalSize;
	ParamValue savedFractalHue = DefaultFractalHue;
	ParamValue savedFractalLight = DefaultFractalLight;
	ParamValue savedFractalRays = DefaultFractalRays;
	ParamValue savedFractalBloom = DefaultFractalBloom;
	bool savedBypass = false;

	if (!streamer.readDouble (savedVolume) || !streamer.readDouble (savedHighPass) ||
	    !streamer.readDouble (savedLowPass) || !streamer.readDouble (savedPitch) ||
	    !streamer.readDouble (savedPitchMix) || !streamer.readDouble (savedEchoMix) ||
	    !streamer.readDouble (savedEchoTime) || !streamer.readDouble (savedEchoFeedback) ||
	    !streamer.readDouble (savedReverbMix) || !streamer.readDouble (savedRoomSize) ||
	    !streamer.readDouble (savedDamping) || !streamer.readDouble (savedPreDelay) ||
	    !streamer.readBool (savedBypass))
		return kResultFalse;
	if (version >= 2 &&
	    (!streamer.readDouble (savedFractalShape) || !streamer.readDouble (savedFractalFold) ||
	     !streamer.readDouble (savedFractalPower) || !streamer.readDouble (savedFractalScale) ||
	     !streamer.readDouble (savedFractalSpin) || !streamer.readDouble (savedFractalSize) ||
	     !streamer.readDouble (savedFractalHue) || !streamer.readDouble (savedFractalLight) ||
	     !streamer.readDouble (savedFractalRays) || !streamer.readDouble (savedFractalBloom)))
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
	setParamNormalized (kFractalShapeId, clamp01 (savedFractalShape));
	setParamNormalized (kFractalFoldId, clamp01 (savedFractalFold));
	setParamNormalized (kFractalPowerId, clamp01 (savedFractalPower));
	setParamNormalized (kFractalScaleId, clamp01 (savedFractalScale));
	setParamNormalized (kFractalSpinId, clamp01 (savedFractalSpin));
	setParamNormalized (kFractalSizeId, clamp01 (savedFractalSize));
	setParamNormalized (kFractalHueId, clamp01 (savedFractalHue));
	setParamNormalized (kFractalLightId, clamp01 (savedFractalLight));
	setParamNormalized (kFractalRaysId, clamp01 (savedFractalRays));
	setParamNormalized (kFractalBloomId, clamp01 (savedFractalBloom));
	setParamNormalized (kBypassId, savedBypass ? 1.0 : 0.0);

	return kResultOk;
}

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
