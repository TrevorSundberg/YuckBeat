#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

#include <cmath>
#include <cstdint>

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

enum ParamIDs : ParamID
{
	kVolumeId = 100,
	kHighPassId,
	kLowPassId,
	kPitchId,
	kPitchMixId,
	kEchoMixId,
	kEchoTimeId,
	kEchoFeedbackId,
	kReverbMixId,
	kRoomSizeId,
	kDampingId,
	kPreDelayId,
	kBypassId,
	kFractalShapeId,
	kFractalFoldId,
	kFractalPowerId,
	kFractalScaleId,
	kFractalSpinId,
	kFractalSizeId,
	kFractalHueId,
	kFractalLightId,
	kFractalRaysId,
	kFractalBloomId
};

constexpr int32 ParameterCount = 23;
constexpr int32 SyncDivisionCount = 8;
constexpr int32 SyncDivisionStepCount = SyncDivisionCount - 1;
constexpr int32 StateMagic = 0x59423032; // YB02
constexpr int32 StateVersion = 2;

static const FUID ProcessorUID (0x9F16DA45, 0xF2EC4795, 0xB6D2E897, 0xFA40207C);
static const FUID ControllerUID (0xA727A55A, 0x8BB04252, 0x8CD0168B, 0x125CFEA1);

constexpr auto PluginName = "YuckBeat";
constexpr auto CompanyName = "YuckTools";
constexpr auto CompanyWeb = "https://local.yuckbeat";
constexpr auto CompanyEmail = "trevor@localhost";
constexpr auto Version = "0.2.0";

constexpr ParamValue DefaultVolume = 24.0 / 36.0;
constexpr ParamValue DefaultHighPass = 0.0;
constexpr ParamValue DefaultLowPass = 1.0;
constexpr ParamValue DefaultPitch = 0.5;
constexpr ParamValue DefaultPitchMix = 1.0;
constexpr ParamValue DefaultEchoMix = 0.0;
constexpr ParamValue DefaultEchoTime = 4.0 / 7.0;
constexpr ParamValue DefaultEchoFeedback = 0.25;
constexpr ParamValue DefaultReverbMix = 0.0;
constexpr ParamValue DefaultRoomSize = 0.45;
constexpr ParamValue DefaultDamping = 0.45;
constexpr ParamValue DefaultPreDelay = 1.0 / 7.0;
constexpr ParamValue DefaultFractalShape = 0.38;
constexpr ParamValue DefaultFractalFold = 0.52;
constexpr ParamValue DefaultFractalPower = 0.57;
constexpr ParamValue DefaultFractalScale = 0.46;
constexpr ParamValue DefaultFractalSpin = 0.32;
constexpr ParamValue DefaultFractalSize = 0.52;
constexpr ParamValue DefaultFractalHue = 0.28;
constexpr ParamValue DefaultFractalLight = 0.62;
constexpr ParamValue DefaultFractalRays = 0.48;
constexpr ParamValue DefaultFractalBloom = 0.55;

inline double clamp01 (double value)
{
	return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

inline double volumeDbFromNormalized (ParamValue value) { return -24.0 + clamp01 (value) * 36.0; }

inline double highPassHzFromNormalized (ParamValue value)
{
	const auto normalized = clamp01 (value);
	if (normalized <= 0.001)
		return 0.0;

	return 20.0 * std::pow (100.0, normalized);
}

inline double lowPassHzFromNormalized (ParamValue value)
{
	const auto normalized = clamp01 (value);
	if (normalized >= 0.999)
		return 0.0;

	return 200.0 * std::pow (100.0, normalized);
}

inline double pitchSemitonesFromNormalized (ParamValue value)
{
	return -12.0 + clamp01 (value) * 24.0;
}

inline double pitchMixFromNormalized (ParamValue value) { return clamp01 (value); }
inline double echoMixFromNormalized (ParamValue value) { return clamp01 (value); }
inline double echoFeedbackFromNormalized (ParamValue value) { return clamp01 (value) * 0.95; }
inline double reverbMixFromNormalized (ParamValue value) { return clamp01 (value); }
inline double roomSizeFromNormalized (ParamValue value) { return 0.10 + clamp01 (value) * 0.90; }
inline double dampingFromNormalized (ParamValue value) { return clamp01 (value) * 0.95; }
inline double percentFromNormalized (ParamValue value) { return clamp01 (value) * 100.0; }
inline double hueDegreesFromNormalized (ParamValue value) { return clamp01 (value) * 360.0; }
inline double spinRateFromNormalized (ParamValue value) { return 0.05 + clamp01 (value) * 3.95; }

inline int32 syncDivisionIndexFromNormalized (ParamValue value)
{
	const auto scaled = clamp01 (value) * static_cast<double> (SyncDivisionCount - 1);
	const auto rounded = static_cast<int32> (scaled + 0.5);
	if (rounded < 0)
		return 0;
	if (rounded >= SyncDivisionCount)
		return SyncDivisionCount - 1;
	return rounded;
}

inline double syncBeatsFromIndex (int32 index)
{
	switch (index)
	{
		case 0: return 0.0625; // 1/64 note
		case 1: return 0.125;  // 1/32 note
		case 2: return 0.25;   // 1/16 note
		case 3: return 0.5;    // 1/8 note
		case 4: return 1.0;    // 1/4 note
		case 5: return 2.0;    // 1/2 note
		case 6: return 4.0;    // 1 bar in 4/4
		default: return 8.0;   // 2 bars in 4/4
	}
}

inline double syncBeatsFromNormalized (ParamValue value)
{
	return syncBeatsFromIndex (syncDivisionIndexFromNormalized (value));
}

inline const char* syncLabelFromIndex (int32 index)
{
	switch (index)
	{
		case 0: return "1/64";
		case 1: return "1/32";
		case 2: return "1/16";
		case 3: return "1/8";
		case 4: return "1/4";
		case 5: return "1/2";
		case 6: return "1 bar";
		default: return "2 bars";
	}
}

inline const char* syncLabelFromNormalized (ParamValue value)
{
	return syncLabelFromIndex (syncDivisionIndexFromNormalized (value));
}

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
