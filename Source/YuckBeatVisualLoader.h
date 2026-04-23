#pragma once

#include "YuckBeatVisualAPI.h"

#include <cstdint>
#include <string>

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

class VisualLoader
{
public:
	VisualLoader ();
	~VisualLoader ();

	bool render (const FractalRenderParams& params, std::uint32_t* pixels);
	const char* version () const;
	bool loaded () const { return module != nullptr && renderFn != nullptr; }

private:
	void maybeReload ();
	bool loadCurrentSource (std::uint64_t sourceStamp);
	void unload ();
	std::string makeShadowPath (std::uint64_t sourceStamp) const;

	void* module {};
	YuckBeatVisualRenderFn renderFn {};
	YuckBeatVisualVersionFn versionFn {};
	std::uint64_t loadedSourceStamp {};
	std::string shadowPath;
};

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
