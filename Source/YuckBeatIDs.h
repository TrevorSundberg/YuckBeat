#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

enum ParamIDs : ParamID
{
	kMixId = 100,
	kRecallId,
	kCycleId,
	kCurveId,
	kSmoothId,
	kFeedbackId,
	kTrimId,
	kBypassId
};

static const FUID ProcessorUID (0x9F16DA45, 0xF2EC4795, 0xB6D2E897, 0xFA40207C);
static const FUID ControllerUID (0xA727A55A, 0x8BB04252, 0x8CD0168B, 0x125CFEA1);

constexpr auto PluginName = "YuckBeat";
constexpr auto CompanyName = "YuckTools";
constexpr auto CompanyWeb = "https://local.yuckbeat";
constexpr auto CompanyEmail = "trevor@localhost";
constexpr auto Version = "0.1.0";

constexpr ParamValue DefaultMix = 0.65;
constexpr ParamValue DefaultRecall = 0.25;
constexpr ParamValue DefaultCycle = 0.20;
constexpr ParamValue DefaultCurve = 0.50;
constexpr ParamValue DefaultSmooth = 0.10;
constexpr ParamValue DefaultFeedback = 0.0;
constexpr ParamValue DefaultTrim = 0.50;

inline double clamp01 (double value)
{
	return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

inline double mixFromNormalized (ParamValue value) { return clamp01 (value); }
inline double recallBeatsFromNormalized (ParamValue value) { return clamp01 (value) * 4.0; }
inline double cycleBeatsFromNormalized (ParamValue value) { return 0.25 + clamp01 (value) * 3.75; }
inline double curveFromNormalized (ParamValue value) { return clamp01 (value) * 2.0 - 1.0; }
inline double smoothMsFromNormalized (ParamValue value) { return clamp01 (value) * 80.0; }
inline double feedbackFromNormalized (ParamValue value) { return clamp01 (value) * 0.95; }
inline double trimDbFromNormalized (ParamValue value) { return -12.0 + clamp01 (value) * 24.0; }

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
