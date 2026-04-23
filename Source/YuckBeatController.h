#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

class Controller final : public EditController
{
public:
	static FUnknown* createInstance (void*)
	{
		return static_cast<IEditController*> (new Controller ());
	}

	tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;
	tresult PLUGIN_API setComponentState (IBStream* state) SMTG_OVERRIDE;
	IPlugView* PLUGIN_API createView (FIDString name) SMTG_OVERRIDE;
	tresult PLUGIN_API getParamStringByValue (ParamID tag, ParamValue valueNormalized,
	                                          String128 string) SMTG_OVERRIDE;

	OBJ_METHODS (Controller, EditController)
	REFCOUNT_METHODS (EditController)
};

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
