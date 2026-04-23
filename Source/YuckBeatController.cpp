#include "YuckBeatController.h"
#include "YuckBeatEditor.h"
#include "YuckBeatIDs.h"

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

	parameters.addParameter (STR16 ("Mix"), STR16 ("%"), 0, DefaultMix,
	                         ParameterInfo::kCanAutomate, kMixId);
	parameters.addParameter (STR16 ("Recall"), STR16 ("beats"), 0, DefaultRecall,
	                         ParameterInfo::kCanAutomate, kRecallId);
	parameters.addParameter (STR16 ("Cycle"), STR16 ("beats"), 0, DefaultCycle,
	                         ParameterInfo::kCanAutomate, kCycleId);
	parameters.addParameter (STR16 ("Curve"), nullptr, 0, DefaultCurve,
	                         ParameterInfo::kCanAutomate, kCurveId);
	parameters.addParameter (STR16 ("Smooth"), STR16 ("ms"), 0, DefaultSmooth,
	                         ParameterInfo::kCanAutomate, kSmoothId);
	parameters.addParameter (STR16 ("Feedback"), STR16 ("%"), 0, DefaultFeedback,
	                         ParameterInfo::kCanAutomate, kFeedbackId);
	parameters.addParameter (STR16 ("Trim"), STR16 ("dB"), 0, DefaultTrim,
	                         ParameterInfo::kCanAutomate, kTrimId);
	parameters.addParameter (STR16 ("Bypass"), nullptr, 1, 0,
	                         ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass, kBypassId);

	return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView (FIDString name)
{
	if (name && std::strcmp (name, ViewType::kEditor) == 0)
		return new Editor (this);

	return nullptr;
}

tresult PLUGIN_API Controller::getParamStringByValue (ParamID tag, ParamValue valueNormalized,
                                                      String128 string)
{
	char text[48] {};
	const auto value = clamp01 (valueNormalized);

	switch (tag)
	{
		case kMixId:
			std::snprintf (text, sizeof (text), "%.0f%%", mixFromNormalized (value) * 100.0);
			break;
		case kRecallId:
			std::snprintf (text, sizeof (text), "%.2f bt", recallBeatsFromNormalized (value));
			break;
		case kCycleId:
			std::snprintf (text, sizeof (text), "%.2f bt", cycleBeatsFromNormalized (value));
			break;
		case kCurveId:
			std::snprintf (text, sizeof (text), "%+.2f", curveFromNormalized (value));
			break;
		case kSmoothId:
			std::snprintf (text, sizeof (text), "%.1f ms", smoothMsFromNormalized (value));
			break;
		case kFeedbackId:
			std::snprintf (text, sizeof (text), "%.0f%%", feedbackFromNormalized (value) * 100.0);
			break;
		case kTrimId:
			std::snprintf (text, sizeof (text), "%+.1f dB", trimDbFromNormalized (value));
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
	ParamValue savedMix = DefaultMix;
	ParamValue savedRecall = DefaultRecall;
	ParamValue savedCycle = DefaultCycle;
	ParamValue savedCurve = DefaultCurve;
	ParamValue savedSmooth = DefaultSmooth;
	ParamValue savedFeedback = DefaultFeedback;
	ParamValue savedTrim = DefaultTrim;
	bool savedBypass = false;

	if (!streamer.readDouble (savedMix) || !streamer.readDouble (savedRecall) ||
	    !streamer.readDouble (savedCycle) || !streamer.readDouble (savedCurve) ||
	    !streamer.readDouble (savedSmooth) || !streamer.readDouble (savedFeedback) ||
	    !streamer.readDouble (savedTrim) || !streamer.readBool (savedBypass))
		return kResultFalse;

	setParamNormalized (kMixId, clamp01 (savedMix));
	setParamNormalized (kRecallId, clamp01 (savedRecall));
	setParamNormalized (kCycleId, clamp01 (savedCycle));
	setParamNormalized (kCurveId, clamp01 (savedCurve));
	setParamNormalized (kSmoothId, clamp01 (savedSmooth));
	setParamNormalized (kFeedbackId, clamp01 (savedFeedback));
	setParamNormalized (kTrimId, clamp01 (savedTrim));
	setParamNormalized (kBypassId, savedBypass ? 1.0 : 0.0);

	return kResultOk;
}

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
