#include "YuckBeatEngineLoader.h"

#include "YuckBeatBuildConfig.h"

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <string>

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

bool sourceStamp (uint64_t& stamp)
{
#if defined(_WIN32)
	WIN32_FILE_ATTRIBUTE_DATA attrs {};
	if (!GetFileAttributesExA (YUCKBEAT_ENGINE_BINARY_PATH, GetFileExInfoStandard, &attrs))
		return false;

	const uint64_t writeTime =
	    (static_cast<uint64_t> (attrs.ftLastWriteTime.dwHighDateTime) << 32u) |
	    attrs.ftLastWriteTime.dwLowDateTime;
	const uint64_t size = (static_cast<uint64_t> (attrs.nFileSizeHigh) << 32u) | attrs.nFileSizeLow;
	stamp = writeTime ^ (size << 1u);
	return true;
#else
	struct stat attrs {};
	if (stat (YUCKBEAT_ENGINE_BINARY_PATH, &attrs) != 0)
		return false;

#if defined(__APPLE__)
	const auto seconds = static_cast<uint64_t> (attrs.st_mtimespec.tv_sec);
	const auto nanos = static_cast<uint64_t> (attrs.st_mtimespec.tv_nsec);
#else
	const auto seconds = static_cast<uint64_t> (attrs.st_mtim.tv_sec);
	const auto nanos = static_cast<uint64_t> (attrs.st_mtim.tv_nsec);
#endif
	stamp = seconds * 1000000000ull + nanos;
	stamp ^= static_cast<uint64_t> (attrs.st_size) << 1u;
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

} // namespace

EngineLoader::EngineLoader () = default;

EngineLoader::~EngineLoader ()
{
	unload ();
}

void EngineLoader::reset (double sampleRate, int32_t channels)
{
	currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
	currentChannels = channels > 0 ? channels : 2;
	maybeReload (currentSampleRate, currentChannels);
	if (instance && resetFn)
		resetFn (instance, currentSampleRate, currentChannels);
}

bool EngineLoader::process (const YuckBeatEngineParams& params, const YuckBeatEngineProcessBlock& block)
{
	currentSampleRate = block.sampleRate > 0.0 ? block.sampleRate : currentSampleRate;
	currentChannels = block.outputChannels > 0 ? block.outputChannels : currentChannels;

	if ((processCounter++ & 31u) == 0)
		maybeReload (currentSampleRate, currentChannels);

	if (!instance || !processFn)
		return false;

	processFn (instance, &params, &block);
	return true;
}

const char* EngineLoader::version () const
{
	return versionFn ? versionFn () : "engine missing";
}

void EngineLoader::maybeReload (double sampleRate, int32_t channels)
{
	uint64_t currentStamp = 0;
	if (!sourceStamp (currentStamp))
		return;

	if (module && loadedSourceStamp == currentStamp)
		return;

	loadCurrentSource (sampleRate, channels, currentStamp);
}

bool EngineLoader::loadCurrentSource (double sampleRate, int32_t channels, uint64_t currentStamp)
{
	const auto newShadowPath = makeShadowPath (currentStamp);
	if (!copyFile (YUCKBEAT_ENGINE_BINARY_PATH, newShadowPath))
		return false;

	auto* newModule = loadModule (newShadowPath);
	if (!newModule)
	{
		deleteFile (newShadowPath);
		return false;
	}

	const auto apiVersion = loadSymbol<YuckBeatEngineApiVersionFn> (newModule, "yuckbeat_engine_api_version");
	const auto create = loadSymbol<YuckBeatEngineCreateFn> (newModule, "yuckbeat_engine_create");
	const auto destroy = loadSymbol<YuckBeatEngineDestroyFn> (newModule, "yuckbeat_engine_destroy");
	const auto reset = loadSymbol<YuckBeatEngineResetFn> (newModule, "yuckbeat_engine_reset");
	const auto process = loadSymbol<YuckBeatEngineProcessFn> (newModule, "yuckbeat_engine_process");
	const auto version = loadSymbol<YuckBeatEngineVersionFn> (newModule, "yuckbeat_engine_version");

	if (!apiVersion || apiVersion () != YUCKBEAT_ENGINE_API_VERSION || !create || !destroy || !reset ||
	    !process)
	{
		unloadModule (newModule);
		deleteFile (newShadowPath);
		return false;
	}

	auto* newInstance = create ();
	if (!newInstance)
	{
		unloadModule (newModule);
		deleteFile (newShadowPath);
		return false;
	}

	reset (newInstance, sampleRate, channels);
	unload ();

	module = newModule;
	instance = newInstance;
	destroyFn = destroy;
	resetFn = reset;
	processFn = process;
	versionFn = version;
	loadedSourceStamp = currentStamp;
	shadowPath = newShadowPath;
	return true;
}

void EngineLoader::unload ()
{
	if (instance && destroyFn)
		destroyFn (instance);
	instance = nullptr;

	if (module)
		unloadModule (module);
	module = nullptr;

	deleteFile (shadowPath);
	shadowPath.clear ();
	destroyFn = nullptr;
	resetFn = nullptr;
	processFn = nullptr;
	versionFn = nullptr;
	loadedSourceStamp = {};
}

std::string EngineLoader::makeShadowPath (uint64_t currentStamp) const
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
	std::snprintf (fileName, sizeof (fileName), "%sYuckBeatEngine_%lu_%llu%s", tempPathText,
	               processId, static_cast<unsigned long long> (currentStamp),
	               YUCKBEAT_ENGINE_BINARY_EXTENSION);
	return fileName;
}

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
