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

constexpr int32 kEditorWidth = 660;
constexpr int32 kEditorHeight = 420;
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

double shapePhaseForDisplay (double phase, double normalizedCurve)
{
	const auto x = std::clamp (phase, 0.0, 1.0);
	const auto bend = std::clamp (curveFromNormalized (normalizedCurve), -1.0, 1.0);

	if (std::abs (bend) < 0.0001)
		return x;

	const auto exponent = 1.0 + std::abs (bend) * 4.0;
	if (bend > 0.0)
		return std::pow (x, 1.0 / exponent);

	return 1.0 - std::pow (1.0 - x, 1.0 / exponent);
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
	const auto accent = binding.id == kCurveId ? kHeat : (binding.id == kTrimId ? kBlue : kAcid);

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

void drawCurve (HDC dc, ParamValue recall, ParamValue cycle, ParamValue curve)
{
	const auto bounds = makeRect (24, 64, 612, 170);
	const auto graph = makeRect (42, 104, 576, 88);
	roundRect (dc, bounds, kPanelDeep, rgb (60, 73, 67), 9, 1);

	GdiObject smallFont (makeFont (11, FW_SEMIBOLD));
	GdiObject tinyFont (makeFont (10));
	drawText (dc, makeRect (42, 72, 180, 18), L"PAST RECALL CURVE", kMuted,
	          static_cast<HFONT> (smallFont.object), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	char readout[96] {};
	std::snprintf (readout, sizeof (readout), "%.2f beat depth / %.2f beat cycle",
	               recallBeatsFromNormalized (recall), cycleBeatsFromNormalized (cycle));
	drawText (dc, makeRect (390, 72, 226, 18), widen (readout), kInk,
	          static_cast<HFONT> (smallFont.object), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

	for (int i = 1; i < 4; ++i)
	{
		const int x = graph.left + (graph.right - graph.left) * i / 4;
		drawLine (dc, x, graph.top, x, graph.bottom, kGrid);
	}
	for (int i = 1; i < 3; ++i)
	{
		const int y = graph.top + (graph.bottom - graph.top) * i / 3;
		drawLine (dc, graph.left, y, graph.right, y, kGrid);
	}

	constexpr int kSegments = 120;
	std::vector<POINT> line;
	std::vector<POINT> fill;
	line.reserve (kSegments + 1);
	fill.reserve (kSegments + 3);
	fill.push_back (POINT {graph.left, graph.bottom});

	for (int i = 0; i <= kSegments; ++i)
	{
		const auto phase = static_cast<double> (i) / kSegments;
		const auto shaped = shapePhaseForDisplay (phase, curve);
		const auto delay = (1.0 - shaped) * recallBeatsFromNormalized (recall) / 4.0;
		const int x = graph.left + static_cast<int> (std::lround (phase * (graph.right - graph.left)));
		const int y = graph.bottom - static_cast<int> (
		                               std::lround (std::clamp (delay, 0.0, 1.0) *
		                                            (graph.bottom - graph.top)));
		line.push_back (POINT {x, y});
		fill.push_back (POINT {x, y});
	}
	fill.push_back (POINT {graph.right, graph.bottom});

	GdiObject fillBrush (CreateSolidBrush (rgb (45, 87, 50)));
	GdiObject nullPen (CreatePen (PS_NULL, 0, 0));
	SelectGuard fillGuard (dc, fillBrush);
	SelectGuard penGuard (dc, nullPen);
	Polygon (dc, fill.data (), static_cast<int> (fill.size ()));

	GdiObject curvePen (CreatePen (PS_SOLID, 3, kAcid));
	SelectGuard curveGuard (dc, curvePen);
	Polyline (dc, line.data (), static_cast<int> (line.size ()));

	drawText (dc, makeRect (42, 198, 70, 16), L"NOW", kMuted, static_cast<HFONT> (tinyFont.object),
	          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	drawText (dc, makeRect (548, 198, 70, 16), L"PAST", kMuted, static_cast<HFONT> (tinyFont.object),
	          DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
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
	constexpr int knobY = 264;
	constexpr int gap = 86;
	bindings = {
		Binding {kMixId, 30, knobY, 68, 68, DefaultMix, "MIX", "wet amount", false},
		Binding {kRecallId, 30 + gap, knobY, 68, 68, DefaultRecall, "RECALL", "past depth", false},
		Binding {kCycleId, 30 + gap * 2, knobY, 68, 68, DefaultCycle, "CYCLE", "curve length", false},
		Binding {kCurveId, 30 + gap * 3, knobY, 68, 68, DefaultCurve, "CURVE", "shape bend", false},
		Binding {kSmoothId, 30 + gap * 4, knobY, 68, 68, DefaultSmooth, "SMOOTH", "slew time", false},
		Binding {kFeedbackId, 30 + gap * 5, knobY, 68, 68, DefaultFeedback, "FEED", "history return", false},
		Binding {kTrimId, 30 + gap * 6, knobY, 68, 68, DefaultTrim, "TRIM", "output gain", false},
		Binding {kBypassId, 548, 24, 88, 26, 0.0, "BYPASS", "", true},
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
			drawText (memoryDc, makeRect (255, 25, 360, 20),
			          L"tempo-synced history recall and curve scrubbing", kMuted,
			          static_cast<HFONT> (smallFont.object), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

			drawCurve (memoryDc, getParameter (kRecallId), getParameter (kCycleId),
			           getParameter (kCurveId));

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

			drawText (memoryDc, makeRect (26, 392, 608, 16),
			          L"Drag knobs or mouse-wheel them. Curve controls how far back the read head reaches over each cycle.",
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
