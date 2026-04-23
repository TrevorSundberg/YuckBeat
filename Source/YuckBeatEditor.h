#pragma once

#include "YuckBeatIDs.h"

#include "public.sdk/source/vst/vsteditcontroller.h"

#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

#include <array>
#include <string>
#include <vector>

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

class Editor final : public EditorView
{
public:
	explicit Editor (EditController* controller);
	~Editor () override;

	tresult PLUGIN_API isPlatformTypeSupported (FIDString type) override;
	tresult PLUGIN_API attached (void* parent, FIDString type) override;
	tresult PLUGIN_API removed () override;
	tresult PLUGIN_API onSize (ViewRect* newSize) override;

	struct Binding
	{
		ParamID id {};
		int x {};
		int y {};
		int width {};
		int height {};
		ParamValue defaultValue {};
		const char* name {};
		const char* hint {};
		bool button {};
	};

private:
	void initializeBindings ();
	void syncFromController ();
	void setParameter (ParamID id, ParamValue value);
	void beginParameterEdit (ParamID id);
	void endParameterEdit (ParamID id);
	ParamValue getParameter (ParamID id) const;
	std::string parameterToString (ParamID id, ParamValue value) const;
	int findBindingAt (int x, int y) const;

public:
	static LRESULT CALLBACK windowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	LRESULT handleMessage (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	HWND window {};
	std::vector<Binding> bindings;
	int activeBinding {-1};
	int dragStartY {};
	ParamValue dragStartValue {};
	std::array<ParamValue, ParameterCount> lastValues {};
	bool hasLastValues {false};
};

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
