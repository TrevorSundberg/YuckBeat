#include "YuckBeatVisualLoader.h"

#include "YuckBeatBuildConfig.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

#if defined(_WIN32)
#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

namespace {

bool sourceStamp (std::uint64_t& stamp)
{
#if defined(_WIN32)
	WIN32_FILE_ATTRIBUTE_DATA attrs {};
	if (!GetFileAttributesExA (YUCKBEAT_VISUAL_BINARY_PATH, GetFileExInfoStandard, &attrs))
		return false;

	const std::uint64_t writeTime =
	    (static_cast<std::uint64_t> (attrs.ftLastWriteTime.dwHighDateTime) << 32u) |
	    attrs.ftLastWriteTime.dwLowDateTime;
	const std::uint64_t size =
	    (static_cast<std::uint64_t> (attrs.nFileSizeHigh) << 32u) | attrs.nFileSizeLow;
	stamp = writeTime ^ (size << 1u);
	return true;
#else
	struct stat attrs {};
	if (stat (YUCKBEAT_VISUAL_BINARY_PATH, &attrs) != 0)
		return false;

#if defined(__APPLE__)
	const auto seconds = static_cast<std::uint64_t> (attrs.st_mtimespec.tv_sec);
	const auto nanos = static_cast<std::uint64_t> (attrs.st_mtimespec.tv_nsec);
#else
	const auto seconds = static_cast<std::uint64_t> (attrs.st_mtim.tv_sec);
	const auto nanos = static_cast<std::uint64_t> (attrs.st_mtim.tv_nsec);
#endif
	stamp = seconds * 1000000000ull + nanos;
	stamp ^= static_cast<std::uint64_t> (attrs.st_size) << 1u;
	return true;
#endif
}

bool copyFile (const std::string& source, const std::string& destination)
{
#if defined(_WIN32)
	return CopyFileA (source.c_str (), destination.c_str (), FALSE) != 0;
#else
	std::ifstream input (source, std::ios::binary);
	std::ofstream output (destination, std::ios::binary | std::ios::trunc);
	if (!input || !output)
		return false;

	output << input.rdbuf ();
	return output.good ();
#endif
}

void deleteFile (const std::string& path)
{
	if (path.empty ())
		return;

#if defined(_WIN32)
	DeleteFileA (path.c_str ());
#else
	std::remove (path.c_str ());
#endif
}

void* loadModule (const std::string& path)
{
#if defined(_WIN32)
	return reinterpret_cast<void*> (LoadLibraryA (path.c_str ()));
#else
	return dlopen (path.c_str (), RTLD_NOW | RTLD_LOCAL);
#endif
}

void unloadModule (void* module)
{
	if (!module)
		return;

#if defined(_WIN32)
	FreeLibrary (reinterpret_cast<HMODULE> (module));
#else
	dlclose (module);
#endif
}

template <typename Fn>
Fn loadSymbol (void* module, const char* name)
{
#if defined(_WIN32)
	return reinterpret_cast<Fn> (GetProcAddress (reinterpret_cast<HMODULE> (module), name));
#else
	return reinterpret_cast<Fn> (dlsym (module, name));
#endif
}

void fallbackRender (const FractalRenderParams& params, std::uint32_t* pixels)
{
	if (!pixels)
		return;

	const auto hue = static_cast<int> ((params.hue - static_cast<int> (params.hue)) * 255.0f);
	for (int y = 0; y < FractalRenderHeight; ++y)
	{
		for (int x = 0; x < FractalRenderWidth; ++x)
		{
			const auto r = static_cast<std::uint32_t> ((x * 255) / (FractalRenderWidth - 1));
			const auto g = static_cast<std::uint32_t> ((y * 255) / (FractalRenderHeight - 1));
			const auto b = static_cast<std::uint32_t> ((hue + x + y) & 255);
			pixels[y * FractalRenderWidth + x] = (r << 16u) | (g << 8u) | b;
		}
	}
}

} // namespace

VisualLoader::VisualLoader () = default;

VisualLoader::~VisualLoader ()
{
	unload ();
}

bool VisualLoader::render (const FractalRenderParams& params, std::uint32_t* pixels)
{
	maybeReload ();
	if (!renderFn)
	{
		fallbackRender (params, pixels);
		return false;
	}

	renderFn (&params, pixels);
	return true;
}

const char* VisualLoader::version () const
{
	return versionFn ? versionFn () : "visual missing";
}

void VisualLoader::maybeReload ()
{
	std::uint64_t currentStamp = 0;
	if (!sourceStamp (currentStamp))
		return;

	if (module && loadedSourceStamp == currentStamp)
		return;

	loadCurrentSource (currentStamp);
}

bool VisualLoader::loadCurrentSource (std::uint64_t currentStamp)
{
	const auto newShadowPath = makeShadowPath (currentStamp);
	if (!copyFile (YUCKBEAT_VISUAL_BINARY_PATH, newShadowPath))
		return false;

	auto* newModule = loadModule (newShadowPath);
	if (!newModule)
	{
		deleteFile (newShadowPath);
		return false;
	}

	const auto apiVersion =
	    loadSymbol<YuckBeatVisualApiVersionFn> (newModule, "yuckbeat_visual_api_version");
	const auto render = loadSymbol<YuckBeatVisualRenderFn> (newModule, "yuckbeat_visual_render");
	const auto version = loadSymbol<YuckBeatVisualVersionFn> (newModule, "yuckbeat_visual_version");

	if (!apiVersion || apiVersion () != YUCKBEAT_VISUAL_API_VERSION || !render)
	{
		unloadModule (newModule);
		deleteFile (newShadowPath);
		return false;
	}

	unload ();

	module = newModule;
	renderFn = render;
	versionFn = version;
	loadedSourceStamp = currentStamp;
	shadowPath = newShadowPath;
	return true;
}

void VisualLoader::unload ()
{
	if (module)
		unloadModule (module);
	module = nullptr;

	deleteFile (shadowPath);
	shadowPath.clear ();
	renderFn = nullptr;
	versionFn = nullptr;
	loadedSourceStamp = {};
}

std::string VisualLoader::makeShadowPath (std::uint64_t currentStamp) const
{
#if defined(_WIN32)
	char tempPath[MAX_PATH] {};
	GetTempPathA (MAX_PATH, tempPath);
	const auto* tempPathText = tempPath;
	const auto processId = static_cast<unsigned long> (GetCurrentProcessId ());
#else
	auto* temp = std::getenv ("TMPDIR");
	std::string tempPath = temp && *temp ? temp : "/tmp";
	if (!tempPath.empty () && tempPath.back () != '/')
		tempPath.push_back ('/');
	const auto* tempPathText = tempPath.c_str ();
	const auto processId = static_cast<unsigned long> (getpid ());
#endif

	char fileName[1024] {};
	std::snprintf (fileName, sizeof (fileName), "%sYuckBeatVisual_%lu_%llu%s", tempPathText,
	               processId, static_cast<unsigned long long> (currentStamp),
	               YUCKBEAT_VISUAL_BINARY_EXTENSION);
	return fileName;
}

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
