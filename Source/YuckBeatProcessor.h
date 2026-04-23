#pragma once

#include "YuckBeatEngineLoader.h"
#include "YuckBeatIDs.h"
#include "public.sdk/source/vst/vstaudioeffect.h"

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

class Processor final : public AudioEffect
{
public:
	Processor ();

	tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;
	tresult PLUGIN_API setupProcessing (ProcessSetup& setup) SMTG_OVERRIDE;
	tresult PLUGIN_API setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
	                                       SpeakerArrangement* outputs, int32 numOuts) SMTG_OVERRIDE;
	tresult PLUGIN_API canProcessSampleSize (int32 symbolicSampleSize) SMTG_OVERRIDE;
	tresult PLUGIN_API setActive (TBool state) SMTG_OVERRIDE;
	tresult PLUGIN_API process (ProcessData& data) SMTG_OVERRIDE;
	tresult PLUGIN_API setState (IBStream* state) SMTG_OVERRIDE;
	tresult PLUGIN_API getState (IBStream* state) SMTG_OVERRIDE;

	static FUnknown* createInstance (void*)
	{
		return static_cast<IAudioProcessor*> (new Processor ());
	}

private:
	void applyParameterChanges (IParameterChanges& changes);
	void copyInputToOutput (ProcessData& data);

	ParamValue volume = DefaultVolume;
	ParamValue highPass = DefaultHighPass;
	ParamValue lowPass = DefaultLowPass;
	ParamValue pitch = DefaultPitch;
	ParamValue pitchMix = DefaultPitchMix;
	ParamValue echoMix = DefaultEchoMix;
	ParamValue echoTime = DefaultEchoTime;
	ParamValue echoFeedback = DefaultEchoFeedback;
	ParamValue reverbMix = DefaultReverbMix;
	ParamValue roomSize = DefaultRoomSize;
	ParamValue damping = DefaultDamping;
	ParamValue preDelay = DefaultPreDelay;
	ParamValue fractalShape = DefaultFractalShape;
	ParamValue fractalFold = DefaultFractalFold;
	ParamValue fractalPower = DefaultFractalPower;
	ParamValue fractalScale = DefaultFractalScale;
	ParamValue fractalSpin = DefaultFractalSpin;
	ParamValue fractalSize = DefaultFractalSize;
	ParamValue fractalHue = DefaultFractalHue;
	ParamValue fractalLight = DefaultFractalLight;
	ParamValue fractalRays = DefaultFractalRays;
	ParamValue fractalBloom = DefaultFractalBloom;
	bool bypass = false;

	EngineLoader engine;
	double tempo = 120.0;
	double sampleRate = 44100.0;
	int32 outputChannels = 2;
};

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
