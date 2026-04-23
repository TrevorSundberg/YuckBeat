#include "YuckBeatEditor.h"

#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <windowsx.h>

#include "base/source/fstring.h"
#include "pluginterfaces/gui/iplugview.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Steinberg {
namespace Vst {
namespace YuckBeat {

namespace {

constexpr int32 kEditorWidth = 960;
constexpr int32 kEditorHeight = 640;
constexpr UINT_PTR kRefreshTimer = 1001;

ViewRect kEditorRect (0, 0, kEditorWidth, kEditorHeight);
const wchar_t* kWindowClassName = L"YuckBeatNativeEditor";

constexpr COLORREF rgb (int red, int green, int blue)
{
	return static_cast<COLORREF> ((blue << 16) | (green << 8) | red);
}

constexpr COLORREF kBack = rgb (18, 21, 21);
constexpr COLORREF kPanel = rgb (28, 32, 31);
constexpr COLORREF kPanelDeep = rgb (11, 14, 15);
constexpr COLORREF kInk = rgb (231, 231, 212);
constexpr COLORREF kMuted = rgb (132, 145, 132);
constexpr COLORREF kGrid = rgb (50, 63, 58);
constexpr COLORREF kAcid = rgb (165, 255, 69);
constexpr COLORREF kHeat = rgb (255, 130, 45);
constexpr COLORREF kBlue = rgb (94, 202, 224);
constexpr COLORREF kGold = rgb (255, 207, 77);

struct GdiObject
{
	explicit GdiObject (HGDIOBJ object = nullptr)
	: object (object)
	{
	}

	~GdiObject ()
	{
		if (object)
			DeleteObject (object);
	}

	operator HGDIOBJ () const { return object; }
	HGDIOBJ object {};
};

struct SelectGuard
{
	SelectGuard (HDC dc, HGDIOBJ object)
	: dc (dc)
	, old (SelectObject (dc, object))
	{
	}

	~SelectGuard ()
	{
		if (old)
			SelectObject (dc, old);
	}

	HDC dc {};
	HGDIOBJ old {};
};

RECT makeRect (int x, int y, int width, int height)
{
	return RECT {x, y, x + width, y + height};
}

std::wstring widen (const char* text)
{
	std::wstring result;
	if (!text)
		return result;

	while (*text)
		result.push_back (static_cast<unsigned char> (*text++));

	return result;
}

std::wstring widen (const std::string& text)
{
	return widen (text.c_str ());
}

HFONT makeFont (int height, int weight = FW_NORMAL)
{
	return CreateFontW (-height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
	                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
	                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

void fillRect (HDC dc, const RECT& rect, COLORREF color)
{
	GdiObject brush (CreateSolidBrush (color));
	FillRect (dc, &rect, static_cast<HBRUSH> (brush.object));
}

void roundRect (HDC dc, const RECT& rect, COLORREF fill, COLORREF stroke, int radius = 8,
                int strokeWidth = 1)
{
	GdiObject brush (CreateSolidBrush (fill));
	GdiObject pen (CreatePen (PS_SOLID, strokeWidth, stroke));
	SelectGuard brushGuard (dc, brush);
	SelectGuard penGuard (dc, pen);
	RoundRect (dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
}

void drawText (HDC dc, const RECT& rect, const std::wstring& text, COLORREF color, HFONT font,
               UINT format)
{
	SetBkMode (dc, TRANSPARENT);
	SetTextColor (dc, color);
	SelectGuard fontGuard (dc, font);
	auto mutableRect = rect;
	DrawTextW (dc, text.c_str (), static_cast<int> (text.size ()), &mutableRect,
	           format | DT_NOPREFIX);
}

void drawLine (HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1)
{
	GdiObject pen (CreatePen (PS_SOLID, width, color));
	SelectGuard guard (dc, pen);
	MoveToEx (dc, x1, y1, nullptr);
	LineTo (dc, x2, y2);
}

POINT polarPoint (int cx, int cy, double radius, double radians)
{
	return POINT {static_cast<LONG> (std::lround (cx + std::cos (radians) * radius)),
	              static_cast<LONG> (std::lround (cy + std::sin (radians) * radius))};
}

void drawArcPolyline (HDC dc, int cx, int cy, int radius, double start, double sweep,
                      double value, COLORREF color, int width)
{
	const auto clamped = std::clamp (value, 0.0, 1.0);
	const int count = std::max (2, static_cast<int> (std::ceil (36 * clamped)));
	std::vector<POINT> points;
	points.reserve (static_cast<size_t> (count + 1));

	for (int i = 0; i <= count; ++i)
	{
		const auto t = count == 0 ? 0.0 : static_cast<double> (i) / count;
		points.push_back (polarPoint (cx, cy, radius, start + sweep * clamped * t));
	}

	GdiObject pen (CreatePen (PS_SOLID, width, color));
	SelectGuard guard (dc, pen);
	Polyline (dc, points.data (), static_cast<int> (points.size ()));
}

void drawKnob (HDC dc, const Editor::Binding& binding, ParamValue value)
{
	const int cx = binding.x + binding.width / 2;
	const int cy = binding.y + binding.height / 2;
	const int radius = binding.width / 2 - 6;
	const auto pitchAccent = binding.id == kPitchId || binding.id == kPitchMixId;
	const auto echoAccent = binding.id == kEchoMixId || binding.id == kEchoTimeId ||
	                        binding.id == kEchoFeedbackId || binding.id == kPreDelayId;
	const auto visualAccent = binding.id >= kFractalShapeId && binding.id <= kFractalBloomId;
	const auto accent = visualAccent ? kGold : (pitchAccent ? kHeat : (echoAccent ? kBlue : kAcid));

	GdiObject fill (CreateSolidBrush (kPanel));
	GdiObject outline (CreatePen (PS_SOLID, 1, rgb (65, 76, 70)));
	SelectGuard fillGuard (dc, fill);
	SelectGuard outlineGuard (dc, outline);
	Ellipse (dc, cx - radius, cy - radius, cx + radius, cy + radius);

	constexpr double start = 2.35;
	constexpr double sweep = 4.72;
	drawArcPolyline (dc, cx, cy, radius + 5, start, sweep, 1.0, rgb (58, 67, 62), 3);
	drawArcPolyline (dc, cx, cy, radius + 5, start, sweep, value, accent, 4);

	const auto pointer = polarPoint (cx, cy, radius - 4, start + sweep * value);
	drawLine (dc, cx, cy, pointer.x, pointer.y, kInk, 3);

	GdiObject capBrush (CreateSolidBrush (accent));
	GdiObject capPen (CreatePen (PS_SOLID, 1, accent));
	SelectGuard capBrushGuard (dc, capBrush);
	SelectGuard capPenGuard (dc, capPen);
	Ellipse (dc, cx - 4, cy - 4, cx + 4, cy + 4);
}

void drawSection (HDC dc, const RECT& bounds, const wchar_t* title, const wchar_t* description,
                  COLORREF accent, HFONT titleFont, HFONT smallFont)
{
	roundRect (dc, bounds, kPanelDeep, rgb (60, 73, 67), 9, 1);

	GdiObject accentBrush (CreateSolidBrush (accent));
	RECT strip = makeRect (bounds.left, bounds.top, 5, bounds.bottom - bounds.top);
	FillRect (dc, &strip, static_cast<HBRUSH> (accentBrush.object));

	drawText (dc, makeRect (bounds.left + 18, bounds.top + 11, 190, 18), title, kInk, titleFont,
	          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	drawText (dc, makeRect (bounds.left + 18, bounds.top + 30, bounds.right - bounds.left - 36, 18),
	          description, kMuted, smallFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void registerWindowClass ()
{
	static bool registered = false;
	if (registered)
		return;

	WNDCLASSEXW wc {};
	wc.cbSize = sizeof (wc);
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = &Editor::windowProc;
	wc.hInstance = GetModuleHandleW (nullptr);
	wc.hCursor = LoadCursorW (nullptr, IDC_ARROW);
	wc.hbrBackground = nullptr;
	wc.lpszClassName = kWindowClassName;
	RegisterClassExW (&wc);
	registered = true;
}

} // namespace

Editor::Editor (EditController* controller)
: EditorView (controller, &kEditorRect)
{
	initializeBindings ();
}

Editor::~Editor ()
{
	if (window)
		DestroyWindow (window);
}

void Editor::initializeBindings ()
{
	constexpr int audioKnob = 58;
	constexpr int visualKnob = 44;
	constexpr int x0 = 350;
	constexpr int x1 = 495;
	constexpr int x2 = 640;
	constexpr int x3 = 785;
	constexpr int row1 = 125;
	constexpr int row2 = 265;
	constexpr int row3 = 405;
	constexpr int visualRow = 536;
	constexpr int vx0 = 44;
	constexpr int visualStep = 88;
	bindings = {
		Binding {kVolumeId, x0, row1, audioKnob, audioKnob, DefaultVolume, "VOLUME", "output level", false},
		Binding {kHighPassId, x1, row1, audioKnob, audioKnob, DefaultHighPass, "HIGH PASS", "remove lows", false},
		Binding {kLowPassId, x2, row1, audioKnob, audioKnob, DefaultLowPass, "LOW PASS", "remove highs", false},
		Binding {kPitchId, x3, row1, audioKnob, audioKnob, DefaultPitch, "PITCH", "semitones", false},
		Binding {kPitchMixId, x0, row2, audioKnob, audioKnob, DefaultPitchMix, "PITCH MIX", "shift blend", false},
		Binding {kEchoMixId, x1, row2, audioKnob, audioKnob, DefaultEchoMix, "ECHO MIX", "delay level", false},
		Binding {kEchoTimeId, x2, row2, audioKnob, audioKnob, DefaultEchoTime, "ECHO TIME", "BPM note", false},
		Binding {kEchoFeedbackId, x3, row2, audioKnob, audioKnob, DefaultEchoFeedback, "ECHO FB", "repeats", false},
		Binding {kReverbMixId, x0, row3, audioKnob, audioKnob, DefaultReverbMix, "VERB MIX", "room level", false},
		Binding {kRoomSizeId, x1, row3, audioKnob, audioKnob, DefaultRoomSize, "ROOM", "space size", false},
		Binding {kDampingId, x2, row3, audioKnob, audioKnob, DefaultDamping, "DAMPING", "darkness", false},
		Binding {kPreDelayId, x3, row3, audioKnob, audioKnob, DefaultPreDelay, "PRE-DELAY", "BPM note", false},
		Binding {kFractalShapeId, vx0 + visualStep * 0, visualRow, visualKnob, visualKnob,
		         DefaultFractalShape, "SHAPE", "object mix", false},
		Binding {kFractalFoldId, vx0 + visualStep * 1, visualRow, visualKnob, visualKnob,
		         DefaultFractalFold, "FOLD", "mirror fold", false},
		Binding {kFractalPowerId, vx0 + visualStep * 2, visualRow, visualKnob, visualKnob,
		         DefaultFractalPower, "POWER", "detail push", false},
		Binding {kFractalScaleId, vx0 + visualStep * 3, visualRow, visualKnob, visualKnob,
		         DefaultFractalScale, "SCALE", "fractal gain", false},
		Binding {kFractalSpinId, vx0 + visualStep * 4, visualRow, visualKnob, visualKnob,
		         DefaultFractalSpin, "SPIN", "motion", false},
		Binding {kFractalSizeId, vx0 + visualStep * 5, visualRow, visualKnob, visualKnob,
		         DefaultFractalSize, "SIZE", "zoom mass", false},
		Binding {kFractalHueId, vx0 + visualStep * 6, visualRow, visualKnob, visualKnob,
		         DefaultFractalHue, "HUE", "palette", false},
		Binding {kFractalLightId, vx0 + visualStep * 7, visualRow, visualKnob, visualKnob,
		         DefaultFractalLight, "LIGHT", "key light", false},
		Binding {kFractalRaysId, vx0 + visualStep * 8, visualRow, visualKnob, visualKnob,
		         DefaultFractalRays, "RAYS", "god rays", false},
		Binding {kFractalBloomId, vx0 + visualStep * 9, visualRow, visualKnob, visualKnob,
		         DefaultFractalBloom, "BLOOM", "glow", false},
		Binding {kBypassId, 846, 24, 88, 26, 0.0, "BYPASS", "", true},
	};
}

tresult PLUGIN_API Editor::isPlatformTypeSupported (FIDString type)
{
	return type && std::strcmp (type, kPlatformTypeHWND) == 0 ? kResultTrue : kInvalidArgument;
}

tresult PLUGIN_API Editor::attached (void* parent, FIDString type)
{
	if (isPlatformTypeSupported (type) != kResultTrue)
		return kInvalidArgument;

	const auto result = EditorView::attached (parent, type);
	if (result != kResultOk)
		return result;

	registerWindowClass ();
	window = CreateWindowExW (0, kWindowClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
	                          0, 0, kEditorWidth, kEditorHeight, static_cast<HWND> (parent),
	                          nullptr, GetModuleHandleW (nullptr), this);
	if (!window)
	{
		EditorView::removed ();
		return kResultFalse;
	}

	animationStartMs = GetTickCount64 ();
	SetTimer (window, kRefreshTimer, 50, nullptr);
	return kResultTrue;
}

tresult PLUGIN_API Editor::removed ()
{
	if (window)
	{
		KillTimer (window, kRefreshTimer);
		DestroyWindow (window);
		window = nullptr;
	}

	return EditorView::removed ();
}

tresult PLUGIN_API Editor::onSize (ViewRect* newSize)
{
	const auto result = EditorView::onSize (newSize);
	if (window && newSize)
	{
		MoveWindow (window, 0, 0, newSize->right - newSize->left,
		            newSize->bottom - newSize->top, TRUE);
	}
	return result;
}

void Editor::syncFromController ()
{
	if (!window)
		return;

	bool changed = !hasLastValues;
	for (size_t i = 0; i < bindings.size () && i < lastValues.size (); ++i)
	{
		const auto value = getParameter (bindings[i].id);
		if (std::abs (lastValues[i] - value) > 0.00001)
		{
			lastValues[i] = value;
			changed = true;
		}
	}

	hasLastValues = true;
	if (changed)
		InvalidateRect (window, nullptr, FALSE);
}

void Editor::setParameter (ParamID id, ParamValue value)
{
	auto* editController = getController ();
	if (!editController)
		return;

	value = clamp01 (value);
	if (editController->setParamNormalized (id, value) == kResultTrue)
		editController->performEdit (id, editController->getParamNormalized (id));

	if (window)
		InvalidateRect (window, nullptr, FALSE);
}

void Editor::beginParameterEdit (ParamID id)
{
	if (auto* editController = getController ())
		editController->beginEdit (id);
}

void Editor::endParameterEdit (ParamID id)
{
	if (auto* editController = getController ())
		editController->endEdit (id);
}

ParamValue Editor::getParameter (ParamID id) const
{
	if (auto* editController = getController ())
		return editController->getParamNormalized (id);

	return 0.0;
}

std::string Editor::parameterToString (ParamID id, ParamValue value) const
{
	if (!getController ())
		return {};

	String128 utf16 {};
	if (getController ()->getParamStringByValue (id, value, utf16) == kResultTrue)
	{
		String converted (utf16);
		converted.toMultiByte (kCP_Utf8);
		return converted.text8 () ? std::string (converted.text8 ()) : std::string {};
	}

	char text[32] {};
	std::snprintf (text, sizeof (text), "%.2f", value);
	return text;
}

int Editor::findBindingAt (int x, int y) const
{
	for (int i = static_cast<int> (bindings.size ()) - 1; i >= 0; --i)
	{
		const auto& binding = bindings[static_cast<size_t> (i)];
		if (x >= binding.x && x <= binding.x + binding.width && y >= binding.y &&
		    y <= binding.y + binding.height)
			return i;
	}
	return -1;
}

FractalRenderParams Editor::makeFractalRenderParams () const
{
	const auto elapsedMs =
	    animationStartMs > 0 ? GetTickCount64 () - animationStartMs : static_cast<ULONGLONG> (0);
	const auto time = static_cast<float> (static_cast<double> (elapsedMs) * 0.001);
	const auto volume = static_cast<float> (getParameter (kVolumeId));
	const auto highPass = static_cast<float> (getParameter (kHighPassId));
	const auto lowPass = static_cast<float> (getParameter (kLowPassId));
	const auto pitch = static_cast<float> (getParameter (kPitchId));
	const auto pitchMix = static_cast<float> (getParameter (kPitchMixId));
	const auto echoMix = static_cast<float> (getParameter (kEchoMixId));
	const auto echoTime = static_cast<float> (getParameter (kEchoTimeId));
	const auto echoFeedback = static_cast<float> (getParameter (kEchoFeedbackId));
	const auto reverbMix = static_cast<float> (getParameter (kReverbMixId));
	const auto roomSize = static_cast<float> (getParameter (kRoomSizeId));
	const auto damping = static_cast<float> (getParameter (kDampingId));
	const auto preDelay = static_cast<float> (getParameter (kPreDelayId));

	const auto echoBeat = static_cast<float> (syncBeatsFromNormalized (echoTime));
	const auto preDelayBeat = static_cast<float> (syncBeatsFromNormalized (preDelay));
	const auto pulseSeconds = std::max (0.125f, (echoBeat + preDelayBeat) * 0.5f);
	const auto pulse =
	    0.5f + 0.5f * std::sin (time * 2.0f * 3.14159265358979323846f / pulseSeconds);
	auto hue = static_cast<float> (getParameter (kFractalHueId)) + pitchMix * 0.08f +
	           echoMix * 0.05f + pulse * 0.025f;
	hue -= std::floor (hue);

	FractalRenderParams params {};
	params.time = time;
	params.shape = static_cast<float> (clamp01 (getParameter (kFractalShapeId) * 0.70 + highPass * 0.30));
	params.fold = static_cast<float> (clamp01 (getParameter (kFractalFoldId) * 0.64 +
	                                           echoFeedback * 0.24 + reverbMix * 0.12));
	params.power = static_cast<float> (clamp01 (getParameter (kFractalPowerId) * 0.70 +
	                                            std::fabs (pitch - 0.5f) * 0.60));
	params.scale = static_cast<float> (clamp01 (getParameter (kFractalScaleId) * 0.62 +
	                                            roomSize * 0.26 + (1.0f - lowPass) * 0.12));
	params.spin = static_cast<float> (clamp01 (getParameter (kFractalSpinId) * 0.68 +
	                                           pitchMix * 0.14 +
	                                           std::min (1.0f, 1.0f / echoBeat / 8.0f) * 0.18));
	params.size = static_cast<float> (clamp01 (getParameter (kFractalSizeId) * 0.70 +
	                                           roomSize * 0.20 + preDelay * 0.10));
	params.hue = hue;
	params.light = static_cast<float> (clamp01 (getParameter (kFractalLightId) * 0.70 +
	                                            volume * 0.20 + pulse * 0.10));
	params.roughness = damping;
	params.audioDrive =
	    static_cast<float> (clamp01 (std::max ({echoMix, reverbMix, pitchMix * 0.55f})));
	params.ao = static_cast<float> (clamp01 (0.42 + highPass * 0.28 + damping * 0.30));
	params.bloom = static_cast<float> (clamp01 (getParameter (kFractalBloomId) * 0.68 +
	                                            echoFeedback * 0.20 + volume * 0.12));
	params.rays = static_cast<float> (clamp01 (getParameter (kFractalRaysId) * 0.66 +
	                                           echoMix * 0.18 + reverbMix * 0.16));
	params.bpmPulse = pulse;
	params.bypass = getParameter (kBypassId) > 0.5 ? 1.0f : 0.0f;
	return params;
}

void Editor::drawFractalPreview (HDC dc, const RECT& rect)
{
	renderFractal (makeFractalRenderParams (), fractalPixels.data ());

	BITMAPINFO bitmapInfo {};
	bitmapInfo.bmiHeader.biSize = sizeof (bitmapInfo.bmiHeader);
	bitmapInfo.bmiHeader.biWidth = FractalRenderWidth;
	bitmapInfo.bmiHeader.biHeight = -FractalRenderHeight;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;

	StretchDIBits (dc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 0, 0,
	               FractalRenderWidth, FractalRenderHeight, fractalPixels.data (), &bitmapInfo,
	               DIB_RGB_COLORS, SRCCOPY);

	GdiObject border (CreatePen (PS_SOLID, 1, rgb (95, 112, 101)));
	SelectGuard borderGuard (dc, border);
	MoveToEx (dc, rect.left, rect.top, nullptr);
	LineTo (dc, rect.right, rect.top);
	LineTo (dc, rect.right, rect.bottom);
	LineTo (dc, rect.left, rect.bottom);
	LineTo (dc, rect.left, rect.top);
}

LRESULT CALLBACK Editor::windowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto* editor = reinterpret_cast<Editor*> (
	    GetWindowLongPtrW (hwnd, GWLP_USERDATA));

	if (message == WM_NCCREATE)
	{
		auto* create = reinterpret_cast<CREATESTRUCTW*> (lParam);
		editor = static_cast<Editor*> (create->lpCreateParams);
		SetWindowLongPtrW (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (editor));
	}

	if (editor)
		return editor->handleMessage (hwnd, message, wParam, lParam);

	return DefWindowProcW (hwnd, message, wParam, lParam);
}

LRESULT Editor::handleMessage (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_TIMER:
			if (wParam == kRefreshTimer)
			{
				syncFromController ();
				InvalidateRect (window, nullptr, FALSE);
				return 0;
			}
			break;

		case WM_ERASEBKGND:
			return 1;

		case WM_PAINT:
		{
			PAINTSTRUCT ps {};
			const auto paintDc = BeginPaint (hwnd, &ps);
			RECT client {};
			GetClientRect (hwnd, &client);

			const auto memoryDc = CreateCompatibleDC (paintDc);
			const auto bitmap = CreateCompatibleBitmap (paintDc, client.right - client.left,
			                                           client.bottom - client.top);
			const auto oldBitmap = SelectObject (memoryDc, bitmap);

			fillRect (memoryDc, client, kBack);

			GdiObject titleFont (makeFont (29, FW_BOLD));
			GdiObject smallFont (makeFont (11, FW_SEMIBOLD));
			GdiObject tinyFont (makeFont (10));
			drawText (memoryDc, makeRect (24, 18, 220, 34), L"YUCKBEAT", kInk,
			          static_cast<HFONT> (titleFont.object), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
			drawText (memoryDc, makeRect (250, 25, 500, 20),
			          L"compact multi-effect with 256x256 SDF raymarch monitor", kMuted,
			          static_cast<HFONT> (smallFont.object), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

			drawSection (memoryDc, makeRect (22, 82, 280, 408), L"SDF RAYMARCH",
			             L"Audio controls push color, light, motion and surface detail.", kGold,
			             static_cast<HFONT> (smallFont.object),
			             static_cast<HFONT> (tinyFont.object));
			drawFractalPreview (memoryDc, makeRect (34, 124, FractalRenderWidth, FractalRenderHeight));
			drawText (memoryDc, makeRect (34, 392, 252, 22), L"PBR-ish light, AO, bloom and rays",
			          kInk, static_cast<HFONT> (smallFont.object),
			          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			drawText (memoryDc, makeRect (34, 418, 252, 36),
			          L"Effect knobs and the SDF knobs below all feed the renderer.", kMuted,
			          static_cast<HFONT> (tinyFont.object), DT_CENTER | DT_WORDBREAK);

			drawSection (memoryDc, makeRect (322, 82, 616, 122), L"FILTER / GAIN",
			             L"Set level, clean lows and highs, then tune pitch in semitones.", kAcid,
			             static_cast<HFONT> (smallFont.object),
			             static_cast<HFONT> (tinyFont.object));
			drawSection (memoryDc, makeRect (322, 222, 616, 122), L"PITCH / ECHO",
			             L"Pitch mix blends the shifter. Echo time follows the host BPM.", kBlue,
			             static_cast<HFONT> (smallFont.object),
			             static_cast<HFONT> (tinyFont.object));
			drawSection (memoryDc, makeRect (322, 362, 616, 122), L"REVERB",
			             L"Room, damping and BPM-synced pre-delay shape the space.", kHeat,
			             static_cast<HFONT> (smallFont.object),
			             static_cast<HFONT> (tinyFont.object));
			drawSection (memoryDc, makeRect (22, 498, 916, 126), L"VISUAL SDF CONTROLS",
			             L"Shape, fold, orbit power, scale, spin, size, hue, light, rays and bloom.",
			             kGold, static_cast<HFONT> (smallFont.object),
			             static_cast<HFONT> (tinyFont.object));

			for (const auto& binding : bindings)
			{
				const auto value = getParameter (binding.id);
				if (binding.button)
				{
					const auto enabled = value > 0.5;
					roundRect (memoryDc, makeRect (binding.x, binding.y, binding.width, binding.height),
					           enabled ? rgb (80, 39, 24) : kPanel, enabled ? kHeat : rgb (78, 89, 82),
					           7, 1);
					drawText (memoryDc, makeRect (binding.x, binding.y, binding.width, binding.height),
					          widen (binding.name), enabled ? kInk : kMuted,
					          static_cast<HFONT> (smallFont.object),
					          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
					drawText (memoryDc, makeRect (binding.x, binding.y + 31, binding.width, 18),
					          widen (parameterToString (binding.id, value)), kMuted,
					          static_cast<HFONT> (tinyFont.object),
					          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
					continue;
				}

				drawText (memoryDc, makeRect (binding.x - 2, binding.y - 27, 72, 18),
				          widen (binding.name), kInk, static_cast<HFONT> (smallFont.object),
				          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				drawKnob (memoryDc, binding, value);
				roundRect (memoryDc, makeRect (binding.x - 8, binding.y + 48, 84, 22), kPanel,
				           rgb (54, 62, 57), 5, 1);
				drawText (memoryDc, makeRect (binding.x - 8, binding.y + 49, 84, 20),
				          widen (parameterToString (binding.id, value)), kInk,
				          static_cast<HFONT> (tinyFont.object),
				          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				drawText (memoryDc, makeRect (binding.x - 7, binding.y + 71, 82, 16),
				          widen (binding.hint), kMuted, static_cast<HFONT> (tinyFont.object),
				          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}

			drawText (memoryDc, makeRect (26, 626, 900, 14),
			          L"Drag knobs or use the mouse wheel. Echo Time and Pre-delay are musical note values locked to host BPM.",
			          kMuted, static_cast<HFONT> (tinyFont.object),
			          DT_LEFT | DT_VCENTER | DT_SINGLELINE);

			BitBlt (paintDc, 0, 0, client.right - client.left, client.bottom - client.top, memoryDc, 0,
			        0, SRCCOPY);
			SelectObject (memoryDc, oldBitmap);
			DeleteObject (bitmap);
			DeleteDC (memoryDc);
			EndPaint (hwnd, &ps);
			return 0;
		}

		case WM_LBUTTONDBLCLK:
		{
			const auto x = GET_X_LPARAM (lParam);
			const auto y = GET_Y_LPARAM (lParam);
			const auto index = findBindingAt (x, y);
			if (index >= 0 && !bindings[static_cast<size_t> (index)].button)
			{
				const auto& binding = bindings[static_cast<size_t> (index)];
				beginParameterEdit (binding.id);
				setParameter (binding.id, binding.defaultValue);
				endParameterEdit (binding.id);
				return 0;
			}
			break;
		}

		case WM_LBUTTONDOWN:
		{
			SetFocus (hwnd);
			const auto x = GET_X_LPARAM (lParam);
			const auto y = GET_Y_LPARAM (lParam);
			const auto index = findBindingAt (x, y);
			if (index < 0)
				break;

			const auto& binding = bindings[static_cast<size_t> (index)];
			if (binding.button)
			{
				beginParameterEdit (binding.id);
				setParameter (binding.id, getParameter (binding.id) > 0.5 ? 0.0 : 1.0);
				endParameterEdit (binding.id);
				return 0;
			}

			activeBinding = index;
			dragStartY = y;
			dragStartValue = getParameter (binding.id);
			beginParameterEdit (binding.id);
			SetCapture (hwnd);
			return 0;
		}

		case WM_MOUSEMOVE:
			if (activeBinding >= 0 && (wParam & MK_LBUTTON) != 0)
			{
				const auto y = GET_Y_LPARAM (lParam);
				const auto& binding = bindings[static_cast<size_t> (activeBinding)];
				const auto fine = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
				const auto scale = fine ? 0.0015 : 0.005;
				setParameter (binding.id, dragStartValue + (dragStartY - y) * scale);
				return 0;
			}
			break;

		case WM_LBUTTONUP:
			if (activeBinding >= 0)
			{
				const auto id = bindings[static_cast<size_t> (activeBinding)].id;
				activeBinding = -1;
				ReleaseCapture ();
				endParameterEdit (id);
				return 0;
			}
			break;

		case WM_MOUSEWHEEL:
		{
			POINT point {GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam)};
			ScreenToClient (hwnd, &point);
			const auto index = findBindingAt (point.x, point.y);
			if (index >= 0 && !bindings[static_cast<size_t> (index)].button)
			{
				const auto& binding = bindings[static_cast<size_t> (index)];
				const auto fine = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
				const auto step = fine ? 0.005 : 0.02;
				const auto delta = static_cast<short> (HIWORD (wParam)) / static_cast<double> (WHEEL_DELTA);
				beginParameterEdit (binding.id);
				setParameter (binding.id, getParameter (binding.id) + delta * step);
				endParameterEdit (binding.id);
				return 0;
			}
			break;
		}
	}

	return DefWindowProcW (hwnd, message, wParam, lParam);
}

} // namespace YuckBeat
} // namespace Vst
} // namespace Steinberg
