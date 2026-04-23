#include <iostream>

#if defined(_WIN32)
#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

using GetPluginFactoryFn = void* (*)();

int wmain (int argc, wchar_t** argv)
{
	if (argc != 2)
	{
		std::wcerr << L"usage: smoke_load_vst3.exe <plugin.vst3>\n";
		return 2;
	}

	HMODULE module = LoadLibraryW (argv[1]);
	if (module == nullptr)
	{
		std::wcerr << L"LoadLibraryW failed: " << GetLastError () << L"\n";
		return 1;
	}

	auto* symbol = GetProcAddress (module, "GetPluginFactory");
	if (symbol == nullptr)
	{
		std::wcerr << L"GetProcAddress failed: " << GetLastError () << L"\n";
		FreeLibrary (module);
		return 1;
	}

	auto* factory = reinterpret_cast<GetPluginFactoryFn> (symbol) ();
	if (factory == nullptr)
	{
		std::wcerr << L"GetPluginFactory returned null\n";
		FreeLibrary (module);
		return 1;
	}

	std::wcout << L"Loaded plugin and resolved GetPluginFactory\n";
	FreeLibrary (module);
	return 0;
}
#else
#include <dlfcn.h>

using GetPluginFactoryFn = void* (*)();

int main (int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "usage: smoke_load_vst3 <plugin-module>\n";
		return 2;
	}

	void* module = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
	if (module == nullptr)
	{
		std::cerr << "dlopen failed: " << dlerror () << "\n";
		return 1;
	}

	auto* symbol = dlsym (module, "GetPluginFactory");
	if (symbol == nullptr)
	{
		std::cerr << "dlsym failed: " << dlerror () << "\n";
		dlclose (module);
		return 1;
	}

	auto* factory = reinterpret_cast<GetPluginFactoryFn> (symbol) ();
	if (factory == nullptr)
	{
		std::cerr << "GetPluginFactory returned null\n";
		dlclose (module);
		return 1;
	}

	std::cout << "Loaded plugin and resolved GetPluginFactory\n";
	dlclose (module);
	return 0;
}
#endif
