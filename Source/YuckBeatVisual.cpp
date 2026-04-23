#include "YuckBeatFractalRenderer.h"
#include "YuckBeatVisualAPI.h"

YUCKBEAT_VISUAL_EXPORT std::int32_t yuckbeat_visual_api_version ()
{
	return YUCKBEAT_VISUAL_API_VERSION;
}

YUCKBEAT_VISUAL_EXPORT void
yuckbeat_visual_render (const Steinberg::Vst::YuckBeat::FractalRenderParams* params,
                        std::uint32_t* pixels)
{
	const auto safeParams = params ? *params : Steinberg::Vst::YuckBeat::FractalRenderParams {};
	Steinberg::Vst::YuckBeat::renderFractal (safeParams, pixels);
}

YUCKBEAT_VISUAL_EXPORT const char* yuckbeat_visual_version ()
{
	return "YuckBeatVisual fast-sdf";
}
