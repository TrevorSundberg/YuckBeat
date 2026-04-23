#include "YuckBeatController.h"
#include "YuckBeatIDs.h"
#include "YuckBeatProcessor.h"

#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

BEGIN_FACTORY_DEF (Steinberg::Vst::YuckBeat::CompanyName,
                   Steinberg::Vst::YuckBeat::CompanyWeb,
                   Steinberg::Vst::YuckBeat::CompanyEmail)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (Steinberg::Vst::YuckBeat::ProcessorUID),
	            Steinberg::PClassInfo::kManyInstances,
	            kVstAudioEffectClass,
	            Steinberg::Vst::YuckBeat::PluginName,
	            Steinberg::Vst::kDistributable,
	            "Fx|Delay",
	            Steinberg::Vst::YuckBeat::Version,
	            kVstVersionString,
	            Steinberg::Vst::YuckBeat::Processor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (Steinberg::Vst::YuckBeat::ControllerUID),
	            Steinberg::PClassInfo::kManyInstances,
	            kVstComponentControllerClass,
	            "YuckBeat Controller",
	            0,
	            "",
	            Steinberg::Vst::YuckBeat::Version,
	            kVstVersionString,
	            Steinberg::Vst::YuckBeat::Controller::createInstance)

END_FACTORY
