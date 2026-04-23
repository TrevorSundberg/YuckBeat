#include <windows.h>

#include <iostream>

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
