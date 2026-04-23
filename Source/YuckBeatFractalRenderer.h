#pragma once

#include "YuckBeatVisualAPI.h"

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

void renderFractal (const FractalRenderParams& params, std::uint32_t* pixels) noexcept;

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
