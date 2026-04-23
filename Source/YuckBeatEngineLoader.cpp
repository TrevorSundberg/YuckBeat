#include "YuckBeatEngineLoader.h"

#include "YuckBeatBuildConfig.h"

#include <cstdio>

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

namespace {

bool sameFileTime (const FILETIME& left, const FILETIME& right)
{
	return left.dwLowDateTime == right.dwLowDateTime && left.dwHighDateTime == right.dwHighDateTime;
}

template <typename Fn>
Fn loadSymbol (HMODULE module, const char* name)
{
	return reinterpret_cast<Fn> (GetProcAddress (module, name));
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
	WIN32_FILE_ATTRIBUTE_DATA attrs {};
	if (!GetFileAttributesExW (YUCKBEAT_ENGINE_DLL_PATH, GetFileExInfoStandard, &attrs))
		return;

	if (module && sameFileTime (loadedWriteTime, attrs.ftLastWriteTime))
		return;

	loadCurrentSource (sampleRate, channels, attrs);
}

bool EngineLoader::loadCurrentSource (double sampleRate, int32_t channels,
                                      const WIN32_FILE_ATTRIBUTE_DATA& attrs)
{
	const auto newShadowPath = makeShadowPath (attrs.ftLastWriteTime);
	if (!CopyFileW (YUCKBEAT_ENGINE_DLL_PATH, newShadowPath.c_str (), FALSE))
		return false;

	auto* newModule = LoadLibraryW (newShadowPath.c_str ());
	if (!newModule)
	{
		DeleteFileW (newShadowPath.c_str ());
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
		FreeLibrary (newModule);
		DeleteFileW (newShadowPath.c_str ());
		return false;
	}

	auto* newInstance = create ();
	if (!newInstance)
	{
		FreeLibrary (newModule);
		DeleteFileW (newShadowPath.c_str ());
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
	loadedWriteTime = attrs.ftLastWriteTime;
	shadowPath = newShadowPath;
	return true;
}

void EngineLoader::unload ()
{
	if (instance && destroyFn)
		destroyFn (instance);
	instance = nullptr;

	if (module)
		FreeLibrary (module);
	module = nullptr;

	if (!shadowPath.empty ())
		DeleteFileW (shadowPath.c_str ());

	shadowPath.clear ();
	destroyFn = nullptr;
	resetFn = nullptr;
	processFn = nullptr;
	versionFn = nullptr;
	loadedWriteTime = {};
}

std::wstring EngineLoader::makeShadowPath (const FILETIME& writeTime) const
{
	wchar_t tempPath[MAX_PATH] {};
	GetTempPathW (MAX_PATH, tempPath);

	ULARGE_INTEGER stamp {};
	stamp.LowPart = writeTime.dwLowDateTime;
	stamp.HighPart = writeTime.dwHighDateTime;

	wchar_t fileName[MAX_PATH] {};
	std::swprintf (fileName, MAX_PATH, L"%sYuckBeatEngine_%lu_%llu.dll", tempPath,
	               static_cast<unsigned long> (GetCurrentProcessId ()),
	               static_cast<unsigned long long> (stamp.QuadPart));
	return fileName;
}

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
