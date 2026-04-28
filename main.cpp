#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <math.h>
#include "kiero/kiero.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "cheat.h"
#include "offsets.h"
#include <vector>

typedef HRESULT(WINAPI* Present)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(WINAPI* ResizeBuffers)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

Present oPresent = NULL;
ResizeBuffers oResizeBuffers = NULL;

ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;
HWND window = NULL;
WNDPROC oWndProc = NULL;

bool init = false;
bool show_menu = true;

static float tab_anim[2] = { 1.f, 0.f };
static int active_tab = 0;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT __stdcall hkWndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (show_menu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;
	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
	if (mainRenderTargetView) {
		pContext->OMSetRenderTargets(0, 0, 0);
		mainRenderTargetView->Release();
		mainRenderTargetView = NULL;
	}
	HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
	if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice))) {
		pDevice->GetImmediateContext(&pContext);
		ID3D11Texture2D* pBackBuffer;
		pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
		if (pBackBuffer) {
			pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
		}
	}
	return hr;
}

static ImU32 ColU32(float r, float g, float b, float a = 1.f) {
	return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
}

static void ApplyTheme()
{
	ImGuiStyle& s = ImGui::GetStyle();
	s.WindowRounding    = 14.f;
	s.ChildRounding     = 10.f;
	s.FrameRounding     = 6.f;
	s.GrabRounding      = 6.f;
	s.PopupRounding     = 8.f;
	s.ScrollbarRounding = 6.f;
	s.TabRounding       = 6.f;
	s.WindowBorderSize  = 0.f;
	s.FrameBorderSize   = 0.f;
	s.ChildBorderSize   = 0.f;
	s.ItemSpacing       = ImVec2(9.f, 8.f);
	s.ItemInnerSpacing  = ImVec2(7.f, 5.f);
	s.FramePadding      = ImVec2(11.f, 6.f);
	s.WindowPadding     = ImVec2(0.f, 0.f);
	s.ScrollbarSize     = 8.f;
	s.GrabMinSize       = 12.f;

	ImVec4* c = s.Colors;
	c[ImGuiCol_WindowBg]             = ImVec4(0.055f, 0.047f, 0.082f, 0.98f);
	c[ImGuiCol_ChildBg]              = ImVec4(0.085f, 0.070f, 0.125f, 1.00f);
	c[ImGuiCol_PopupBg]              = ImVec4(0.075f, 0.060f, 0.110f, 0.98f);
	c[ImGuiCol_Border]               = ImVec4(0.28f, 0.15f, 0.45f, 0.00f);
	c[ImGuiCol_FrameBg]              = ImVec4(0.115f, 0.090f, 0.170f, 1.00f);
	c[ImGuiCol_FrameBgHovered]       = ImVec4(0.180f, 0.130f, 0.270f, 1.00f);
	c[ImGuiCol_FrameBgActive]        = ImVec4(0.250f, 0.160f, 0.390f, 1.00f);
	c[ImGuiCol_TitleBg]              = ImVec4(0.085f, 0.060f, 0.130f, 1.00f);
	c[ImGuiCol_TitleBgActive]        = ImVec4(0.140f, 0.080f, 0.220f, 1.00f);
	c[ImGuiCol_ScrollbarBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.35f, 0.18f, 0.60f, 0.75f);
	c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.55f, 0.30f, 0.80f, 0.90f);
	c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.70f, 0.40f, 0.95f, 1.00f);
	c[ImGuiCol_CheckMark]            = ImVec4(0.92f, 0.55f, 1.00f, 1.00f);
	c[ImGuiCol_SliderGrab]           = ImVec4(0.78f, 0.38f, 1.00f, 1.00f);
	c[ImGuiCol_SliderGrabActive]     = ImVec4(0.95f, 0.55f, 1.00f, 1.00f);
	c[ImGuiCol_Button]               = ImVec4(0.180f, 0.110f, 0.320f, 1.00f);
	c[ImGuiCol_ButtonHovered]        = ImVec4(0.350f, 0.200f, 0.580f, 1.00f);
	c[ImGuiCol_ButtonActive]         = ImVec4(0.520f, 0.300f, 0.820f, 1.00f);
	c[ImGuiCol_Header]               = ImVec4(0.28f, 0.14f, 0.48f, 0.60f);
	c[ImGuiCol_HeaderHovered]        = ImVec4(0.44f, 0.24f, 0.72f, 0.70f);
	c[ImGuiCol_HeaderActive]         = ImVec4(0.60f, 0.35f, 0.90f, 0.80f);
	c[ImGuiCol_Separator]            = ImVec4(0.30f, 0.15f, 0.50f, 0.35f);
	c[ImGuiCol_SeparatorHovered]     = ImVec4(0.50f, 0.25f, 0.75f, 0.60f);
	c[ImGuiCol_SeparatorActive]      = ImVec4(0.65f, 0.35f, 0.90f, 0.80f);
	c[ImGuiCol_Text]                 = ImVec4(0.96f, 0.93f, 1.00f, 1.00f);
	c[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.40f, 0.60f, 1.00f);
	c[ImGuiCol_ResizeGrip]           = ImVec4(0.40f, 0.20f, 0.60f, 0.25f);
	c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.60f, 0.30f, 0.80f, 0.55f);
	c[ImGuiCol_ResizeGripActive]     = ImVec4(0.80f, 0.45f, 0.95f, 0.85f);
}

static void SectionHeader(const char* label) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	float w = ImGui::GetContentRegionAvail().x;
	dl->AddRectFilled(ImVec2(p.x, p.y + 4.f), ImVec2(p.x + 3.f, p.y + 18.f), ColU32(0.78f, 0.35f, 1.0f, 1.f), 2.f);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.f);
	ImGui::TextColored(ImVec4(0.88f, 0.75f, 1.00f, 1.0f), "%s", label);
	ImGui::Spacing();
	p = ImGui::GetCursorScreenPos();
	dl->AddRectFilledMultiColor(
		ImVec2(p.x, p.y), ImVec2(p.x + w, p.y + 1.f),
		ColU32(0.45f, 0.20f, 0.70f, 0.55f),
		ColU32(0.45f, 0.20f, 0.70f, 0.00f),
		ColU32(0.45f, 0.20f, 0.70f, 0.00f),
		ColU32(0.45f, 0.20f, 0.70f, 0.55f)
	);
	ImGui::Dummy(ImVec2(0.f, 3.f));
}

static void BeginCard() {
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.105f, 0.082f, 0.155f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 10.f));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
}
static void EndCard() {
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}

static void ToggleSwitch(const char* id, bool* v) {
	float h  = ImGui::GetTextLineHeight() + 4.f;
	float w  = h * 1.9f;
	float r  = h * 0.5f;
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton(id, ImVec2(w, h));
	if (ImGui::IsItemClicked()) *v = !*v;
	bool hovered = ImGui::IsItemHovered();

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec4 on  = ImVec4(0.58f, 0.32f, 0.90f, 1.0f);
	ImVec4 onH = ImVec4(0.68f, 0.42f, 1.00f, 1.0f);
	ImVec4 off = ImVec4(0.13f, 0.10f, 0.18f, 1.0f);
	ImVec4 offH= ImVec4(0.18f, 0.15f, 0.24f, 1.0f);
	ImVec4 col = *v ? (hovered ? onH : on) : (hovered ? offH : off);
	dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h),
		IM_COL32((int)(col.x*255),(int)(col.y*255),(int)(col.z*255),255), r);
	float cx = *v ? (p.x + w - r) : (p.x + r);
	dl->AddCircleFilled(ImVec2(cx, p.y + r), r - 2.5f,
		IM_COL32(248, 245, 255, 240), 16);
}

static void ToggleRow(const char* label, bool* v, bool enabled = true) {
	float avail = ImGui::GetContentRegionAvail().x;
	float h     = ImGui::GetTextLineHeight() + 4.f;
	float tw    = h * 1.9f;

	ImVec2 start = ImGui::GetCursorPos();
	ImGui::PushStyleColor(ImGuiCol_Text,
		enabled ? ImVec4(0.92f, 0.88f, 1.00f, 1.0f)
		        : ImVec4(0.45f, 0.38f, 0.55f, 1.0f));
	ImGui::SetCursorPosY(start.y + 2.f);
	ImGui::TextUnformatted(label);
	ImGui::PopStyleColor();

	ImGui::SetCursorPos(ImVec2(start.x + avail - tw, start.y));
	char id[64]; sprintf_s(id, "##tg_%s", label);
	if (!enabled) {
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 p = ImGui::GetCursorScreenPos();
		dl->AddRectFilled(p, ImVec2(p.x + tw, p.y + h), IM_COL32(30, 25, 40, 255), h * 0.5f);
		dl->AddCircleFilled(ImVec2(p.x + h * 0.5f, p.y + h * 0.5f), h * 0.5f - 2.5f,
			IM_COL32(90, 80, 110, 200), 16);
		ImGui::Dummy(ImVec2(tw, h));
	} else {
		ToggleSwitch(id, v);
	}
	ImGui::Dummy(ImVec2(0.f, 3.f));
}

static void ModernCardBegin(const char* title) {
	ImGui::PushStyleColor(ImGuiCol_ChildBg,  ImVec4(0.070f, 0.055f, 0.105f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBg,  ImVec4(0.045f, 0.035f, 0.075f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   12.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(14.f, 12.f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,     ImVec2(8.f, 6.f));

	char id[96]; sprintf_s(id, "##card_%s", title);
	ImGui::BeginChild(id, ImVec2(0, 0),
		ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
		ImGuiWindowFlags_NoScrollbar);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p  = ImGui::GetCursorScreenPos();
	float  cw = ImGui::GetContentRegionAvail().x;

	dl->AddRectFilled(ImVec2(p.x, p.y + 1.f), ImVec2(p.x + 16.f, p.y + 17.f),
		IM_COL32(55, 35, 90, 255), 4.f);
	dl->AddRectFilled(ImVec2(p.x + 4.f, p.y + 5.f), ImVec2(p.x + 12.f, p.y + 13.f),
		IM_COL32(200, 140, 255, 255), 2.f);
	ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x + 24.f, ImGui::GetCursorPos().y));
	ImGui::TextColored(ImVec4(0.93f, 0.85f, 1.0f, 1.f), "%s", title);
	ImGui::Spacing();
	ImVec2 sp = ImGui::GetCursorScreenPos();
	dl->AddLine(ImVec2(sp.x, sp.y), ImVec2(sp.x + cw, sp.y),
		IM_COL32(60, 40, 90, 180), 1.f);
	ImGui::Dummy(ImVec2(0.f, 4.f));
}
static void ModernCardEnd() {
	ImGui::EndChild();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(2);
}

static void TopTab(int idx, const char* icon, const char* label, int& active) {
	ImVec2 p = ImGui::GetCursorScreenPos();
	float  h = 44.f;
	float  w = 126.f;
	char   id[24]; sprintf_s(id, "##tt%d", idx);

	ImGui::InvisibleButton(id, ImVec2(w, h));
	bool clicked = ImGui::IsItemClicked();
	bool hovered = ImGui::IsItemHovered();
	if (clicked) active = idx;

	bool is_active = (active == idx);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImU32 txt = is_active ? IM_COL32(245, 230, 255, 255)
	          : hovered   ? IM_COL32(200, 180, 230, 255)
	                      : IM_COL32(130, 115, 160, 255);

	dl->AddText(ImVec2(p.x + 18.f, p.y + 14.f), txt, icon);
	dl->AddText(ImVec2(p.x + 42.f, p.y + 14.f), txt, label);

	if (is_active) {
		dl->AddRectFilled(ImVec2(p.x + 14.f, p.y + h - 3.f),
		                  ImVec2(p.x + w - 14.f, p.y + h - 1.f),
		                  IM_COL32(180, 110, 255, 255), 1.f);
	}
	ImGui::SameLine(0.f, 0.f);
}

static void DrawEsp() {
	if (!Cheat::esp_players && !Cheat::esp_ball) return;

	Cheat::Vector3 camLoc;
	Cheat::Rotator camRot;
	float fov = 0.0f;
	if (!Cheat::GetCamera(camLoc, camRot, fov)) return;

	ImGuiIO& io = ImGui::GetIO();
	float sw = io.DisplaySize.x;
	float sh = io.DisplaySize.y;
	if (sw <= 0 || sh <= 0) return;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	static std::vector<Cheat::EspEntry> entries;
	Cheat::GatherEsp(entries);

	for (auto& e : entries) {
		float baseOffset   = (e.team == -1) ? -20.0f : -90.0f;
		float worldHeight  = (e.team == -1) ?  40.0f : 180.0f;

		Cheat::Vector3 baseP = { e.worldPos.x, e.worldPos.y, e.worldPos.z + baseOffset };
		Cheat::Vector3 topP  = { e.worldPos.x, e.worldPos.y, e.worldPos.z + baseOffset + worldHeight };

		float sx, sy, sxTop, syTop;
		if (!Cheat::WorldToScreen(baseP, camLoc, camRot, fov, sw, sh, sx, sy)) continue;
		if (!Cheat::WorldToScreen(topP,  camLoc, camRot, fov, sw, sh, sxTop, syTop)) continue;

		if (sx < -100 || sx > sw + 100 || sy < -100 || sy > sh + 100) continue;

		float boxH = sy - syTop;
		if (boxH < 6.f) continue;
		float boxW = boxH * 0.45f;

		ImU32 color;
		switch (e.team) {
			case -1: color = IM_COL32(255, 220,  60, 255); break;
			case  0: color = IM_COL32( 80, 180, 255, 255); break;
			case  1: color = IM_COL32(255,  80,  80, 255); break;
			case  2: color = IM_COL32(120, 255, 120, 255); break;
			default: color = IM_COL32(200, 200, 200, 255); break;
		}

		dl->AddRect(ImVec2(sx - boxW - 1, syTop - 1), ImVec2(sx + boxW + 1, sy + 1),
					IM_COL32(0, 0, 0, 200), 0.f, 0, 3.f);
		dl->AddRect(ImVec2(sx - boxW, syTop), ImVec2(sx + boxW, sy),
					color, 0.f, 0, 1.5f);

		char txt[64]; txt[0] = 0;
		if (Cheat::esp_names && Cheat::esp_distance)
			sprintf_s(txt, "%s  %.0fm", e.label, e.distance / 100.0f);
		else if (Cheat::esp_names)
			sprintf_s(txt, "%s", e.label);
		else if (Cheat::esp_distance)
			sprintf_s(txt, "%.0fm", e.distance / 100.0f);

		if (txt[0]) {
			ImVec2 ts = ImGui::CalcTextSize(txt);
			dl->AddRectFilled(ImVec2(sx - ts.x*0.5f - 3, syTop - ts.y - 5),
							  ImVec2(sx + ts.x*0.5f + 3, syTop - 2),
							  IM_COL32(0, 0, 0, 170), 3.f);
			dl->AddText(ImVec2(sx - ts.x*0.5f, syTop - ts.y - 4), color, txt);
		}

		if (Cheat::esp_lines) {
			dl->AddLine(ImVec2(sw * 0.5f, sh), ImVec2(sx, sy), color, 1.2f);
		}
	}
}

static void DrawCrosshair() {
	if (!Cheat::crosshair) return;
	ImGuiIO& io = ImGui::GetIO();
	float cx = io.DisplaySize.x * 0.5f;
	float cy = io.DisplaySize.y * 0.5f;
	if (cx <= 0 || cy <= 0) return;

	ImDrawList* dl = ImGui::GetForegroundDrawList();
	float s = Cheat::crosshair_size;
	ImU32 col = IM_COL32(240, 240, 255, 255);
	ImU32 outline = IM_COL32(0, 0, 0, 200);

	switch (Cheat::crosshair_style) {
		case 0:
			dl->AddCircleFilled(ImVec2(cx, cy), s * 0.40f + 1.f, outline, 16);
			dl->AddCircleFilled(ImVec2(cx, cy), s * 0.40f, col, 16);
			break;
		case 1:
			dl->AddLine(ImVec2(cx - s, cy), ImVec2(cx + s, cy), outline, 3.f);
			dl->AddLine(ImVec2(cx, cy - s), ImVec2(cx, cy + s), outline, 3.f);
			dl->AddLine(ImVec2(cx - s, cy), ImVec2(cx + s, cy), col, 1.5f);
			dl->AddLine(ImVec2(cx, cy - s), ImVec2(cx, cy + s), col, 1.5f);
			break;
		case 2:
			dl->AddLine(ImVec2(cx - s, cy), ImVec2(cx - s * 0.30f, cy), outline, 3.f);
			dl->AddLine(ImVec2(cx + s * 0.30f, cy), ImVec2(cx + s, cy), outline, 3.f);
			dl->AddLine(ImVec2(cx, cy - s), ImVec2(cx, cy - s * 0.30f), outline, 3.f);
			dl->AddLine(ImVec2(cx, cy + s * 0.30f), ImVec2(cx, cy + s), outline, 3.f);
			dl->AddLine(ImVec2(cx - s, cy), ImVec2(cx - s * 0.30f, cy), col, 1.5f);
			dl->AddLine(ImVec2(cx + s * 0.30f, cy), ImVec2(cx + s, cy), col, 1.5f);
			dl->AddLine(ImVec2(cx, cy - s), ImVec2(cx, cy - s * 0.30f), col, 1.5f);
			dl->AddLine(ImVec2(cx, cy + s * 0.30f), ImVec2(cx, cy + s), col, 1.5f);
			dl->AddCircleFilled(ImVec2(cx, cy), 1.8f, col, 12);
			break;
		case 3:
			dl->AddCircle(ImVec2(cx, cy), s, outline, 32, 3.f);
			dl->AddCircle(ImVec2(cx, cy), s, col, 32, 1.5f);
			dl->AddCircleFilled(ImVec2(cx, cy), 1.5f, col, 12);
			break;
	}
}

static void DrawRadar() {
	if (!Cheat::radar) return;

	Cheat::Vector3 camLoc;
	Cheat::Rotator camRot;
	float fov = 0.0f;
	if (!Cheat::GetCamera(camLoc, camRot, fov)) return;

	ImGuiIO& io = ImGui::GetIO();
	float r = Cheat::radar_size * 0.5f;
	float cx = io.DisplaySize.x - r - 20.f;
	float cy = r + 20.f;

	ImDrawList* dl = ImGui::GetForegroundDrawList();
	dl->AddCircleFilled(ImVec2(cx, cy), r + 4.f, IM_COL32(20, 15, 35, 220), 48);
	dl->AddCircle(ImVec2(cx, cy), r + 4.f, IM_COL32(140, 80, 220, 200), 48, 2.f);
	dl->AddCircle(ImVec2(cx, cy), r * 0.66f, IM_COL32(100, 60, 160, 90), 48, 1.f);
	dl->AddCircle(ImVec2(cx, cy), r * 0.33f, IM_COL32(100, 60, 160, 90), 48, 1.f);
	dl->AddLine(ImVec2(cx - r, cy), ImVec2(cx + r, cy), IM_COL32(100, 60, 160, 80), 1.f);
	dl->AddLine(ImVec2(cx, cy - r), ImVec2(cx, cy + r), IM_COL32(100, 60, 160, 80), 1.f);

	static std::vector<Cheat::EspEntry> entries;
	bool saved_p = Cheat::esp_players, saved_b = Cheat::esp_ball;
	Cheat::esp_players = true; Cheat::esp_ball = true;
	Cheat::GatherEsp(entries);
	Cheat::esp_players = saved_p; Cheat::esp_ball = saved_b;

	float yawRad = (float)camRot.yaw * (3.14159265358979323846f / 180.0f);
	float csY = cosf(yawRad);
	float snY = sinf(yawRad);

	float range = Cheat::radar_range;
	if (range < 100.f) range = 100.f;

	for (auto& e : entries) {
		float dx = (float)(e.worldPos.x - camLoc.x);
		float dy = (float)(e.worldPos.y - camLoc.y);
		float fwd   =  dx * csY + dy * snY;
		float right = -dx * snY + dy * csY;
		float px =  right / range * r;
		float py = -fwd   / range * r;
		float dist = sqrtf(px * px + py * py);
		bool edge = false;
		if (dist > r) {
			px = px / dist * r;
			py = py / dist * r;
			edge = true;
		}
		ImU32 color;
		switch (e.team) {
			case -1: color = IM_COL32(255, 220,  60, 255); break;
			case  0: color = IM_COL32( 80, 180, 255, 255); break;
			case  1: color = IM_COL32(255,  80,  80, 255); break;
			default: color = IM_COL32(220, 220, 220, 255); break;
		}
		float sz = (e.team == -1) ? 3.f : 4.f;
		dl->AddCircleFilled(ImVec2(cx + px, cy + py), sz + 1.f, IM_COL32(0, 0, 0, 220), 12);
		dl->AddCircleFilled(ImVec2(cx + px, cy + py), sz,
			edge ? (color & IM_COL32(255, 255, 255, 170)) : color, 12);
	}

	dl->AddTriangleFilled(
		ImVec2(cx, cy - 10.f),
		ImVec2(cx - 5.f, cy - 1.f),
		ImVec2(cx + 5.f, cy - 1.f),
		IM_COL32(255, 255, 255, 240));
	dl->AddCircleFilled(ImVec2(cx, cy), 3.f, IM_COL32(255, 255, 255, 255), 16);
}

static void DrawBallTrail() {
	if (!Cheat::ball_trail) return;

	Cheat::Vector3 camLoc;
	Cheat::Rotator camRot;
	float fov = 0.0f;
	if (!Cheat::GetCamera(camLoc, camRot, fov)) return;

	ImGuiIO& io = ImGui::GetIO();
	float sw = io.DisplaySize.x;
	float sh = io.DisplaySize.y;
	if (sw <= 0 || sh <= 0) return;

	static std::vector<Cheat::EspEntry> entries;
	bool saved_p = Cheat::esp_players, saved_b = Cheat::esp_ball;
	Cheat::esp_players = false; Cheat::esp_ball = true;
	Cheat::GatherEsp(entries);
	Cheat::esp_players = saved_p; Cheat::esp_ball = saved_b;

	Cheat::Vector3 ballPos = { 0, 0, 0 };
	bool have_ball = false;
	for (auto& e : entries) {
		if (e.team == -1) { ballPos = e.worldPos; have_ball = true; break; }
	}

	static std::vector<Cheat::Vector3> history;
	const size_t max_pts = 64;
	if (have_ball) {
		bool add = history.empty();
		if (!add) {
			const auto& last = history.back();
			double dx = ballPos.x - last.x;
			double dy = ballPos.y - last.y;
			double dz = ballPos.z - last.z;
			if (dx*dx + dy*dy + dz*dz > 100.0) add = true;
		}
		if (add) history.push_back(ballPos);
		while (history.size() > max_pts) history.erase(history.begin());
	} else {
		history.clear();
	}
	if (history.size() < 2) return;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	ImVec2 prev; bool prev_ok = false;
	for (size_t i = 0; i < history.size(); i++) {
		float sx, sy;
		bool ok = Cheat::WorldToScreen(history[i], camLoc, camRot, fov, sw, sh, sx, sy);
		if (!ok) { prev_ok = false; continue; }
		if (prev_ok) {
			float t = (float)i / (float)history.size();
			int a = (int)(40.f + t * 200.f);
			if (a > 240) a = 240;
			dl->AddLine(prev, ImVec2(sx, sy), IM_COL32(255, 220, 60, a), 2.2f);
		}
		prev = ImVec2(sx, sy);
		prev_ok = true;
	}
}

static void DrawBallPrediction() {
	if (!Cheat::ball_prediction) return;

	Cheat::BallPrediction pred;
	Cheat::GetBallPrediction(pred);
	if (pred.count < 2) return;

	Cheat::Vector3 camLoc;
	Cheat::Rotator camRot;
	float fov = 0.0f;
	if (!Cheat::GetCamera(camLoc, camRot, fov)) return;

	ImGuiIO& io = ImGui::GetIO();
	float sw = io.DisplaySize.x;
	float sh = io.DisplaySize.y;
	if (sw <= 0 || sh <= 0) return;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	ImVec2 prev; bool prev_ok = false;
	for (int i = 0; i < pred.count; i++) {
		float sx, sy;
		bool ok = Cheat::WorldToScreen(pred.points[i], camLoc, camRot, fov, sw, sh, sx, sy);
		if (!ok) { prev_ok = false; continue; }
		ImVec2 cur(sx, sy);
		if (prev_ok) {
			float t = (float)i / (float)pred.count;
			int a = (int)(230.f - t * 150.f);
			if (a < 50) a = 50;
			int r = (int)(80  + t * 175);
			int g = (int)(220 - t * 120);
			int b = (int)(255 - t * 80);
			dl->AddLine(prev, cur, IM_COL32(r, g, b, a), 2.6f);
		}
		prev = cur;
		prev_ok = true;
	}

	float ex, ey;
	if (Cheat::WorldToScreen(pred.points[pred.count - 1], camLoc, camRot, fov, sw, sh, ex, ey)) {
		dl->AddCircleFilled(ImVec2(ex, ey), 4.5f, IM_COL32(255, 90, 110, 230), 16);
		dl->AddCircle      (ImVec2(ex, ey), 9.0f, IM_COL32(255, 200, 60, 200), 18, 1.6f);
	}
}

static void DrawMoveGuide() {
	if (!Cheat::move_guide) return;

	Cheat::Vector3 playerPos, target;
	if (!Cheat::GetPlayerPosition(playerPos)) return;

	bool haveTarget = false;
	Cheat::BallPrediction pred;
	Cheat::GetBallPrediction(pred);
	if (pred.count >= 2) {
		target = pred.points[pred.count - 1];
		haveTarget = true;
	} else if (Cheat::GetBallPosition(target)) {
		haveTarget = true;
	}
	if (!haveTarget) return;

	Cheat::Vector3 camLoc;
	Cheat::Rotator camRot;
	float fov = 0.0f;
	if (!Cheat::GetCamera(camLoc, camRot, fov)) return;

	ImGuiIO& io = ImGui::GetIO();
	float sw = io.DisplaySize.x;
	float sh = io.DisplaySize.y;
	if (sw <= 0 || sh <= 0) return;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	const int SEG = 16;
	ImVec2 prev; bool prev_ok = false;
	for (int i = 0; i <= SEG; ++i) {
		double t = (double)i / (double)SEG;
		Cheat::Vector3 mid;
		mid.x = playerPos.x + (target.x - playerPos.x) * t;
		mid.y = playerPos.y + (target.y - playerPos.y) * t;
		mid.z = playerPos.z + (target.z - playerPos.z) * t;
		float sx, sy;
		bool ok = Cheat::WorldToScreen(mid, camLoc, camRot, fov, sw, sh, sx, sy);
		if (!ok) { prev_ok = false; continue; }
		ImVec2 cur(sx, sy);
		if (prev_ok) {
			dl->AddLine(prev, cur, IM_COL32(0,    0,   0,  140), 4.0f);
			dl->AddLine(prev, cur, IM_COL32(70,  255, 140, 230), 2.2f);
		}
		prev = cur;
		prev_ok = true;
	}

	float tx, ty;
	if (Cheat::WorldToScreen(target, camLoc, camRot, fov, sw, sh, tx, ty)) {
		double dx = target.x - playerPos.x;
		double dy = target.y - playerPos.y;
		double dz = target.z - playerPos.z;
		float dist = (float)sqrt(dx*dx + dy*dy + dz*dz);

		dl->AddCircleFilled(ImVec2(tx, ty), 5.0f,  IM_COL32(70,  255, 140, 230), 18);
		dl->AddCircle      (ImVec2(tx, ty), 11.0f, IM_COL32(255, 255, 255, 200), 22, 1.8f);

		char label[32];
		sprintf_s(label, "%.1fm", dist / 100.f);
		ImVec2 ts = ImGui::CalcTextSize(label);
		ImVec2 tp(tx - ts.x * 0.5f, ty + 12.f);
		dl->AddRectFilled(ImVec2(tp.x - 4, tp.y - 2), ImVec2(tp.x + ts.x + 4, tp.y + ts.y + 2),
			IM_COL32(0, 0, 0, 170), 3.f);
		dl->AddText(tp, IM_COL32(180, 255, 200, 255), label);
	}
}

static void DrawAimLines() {
	if (!Cheat::aim_view) return;

	Cheat::Vector3 camLoc;
	Cheat::Rotator camRot;
	float fov = 0.0f;
	if (!Cheat::GetCamera(camLoc, camRot, fov)) return;

	ImGuiIO& io = ImGui::GetIO();
	float sw = io.DisplaySize.x;
	float sh = io.DisplaySize.y;
	if (sw <= 0 || sh <= 0) return;

	static std::vector<Cheat::EspEntry> entries;
	bool saved_p = Cheat::esp_players;
	bool saved_b = Cheat::esp_ball;
	Cheat::esp_players = true;
	Cheat::esp_ball    = false;
	Cheat::GatherEsp(entries);
	Cheat::esp_players = saved_p;
	Cheat::esp_ball    = saved_b;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	const double DEG = 3.14159265358979323846 / 180.0;
	const double EYE_HEIGHT = 70.0;
	const double LEN        = (Cheat::aim_view_length < 100.f) ? 100.f : Cheat::aim_view_length;
	const float  MAX_PLAYER_DIST = 1800.0f;

	for (auto& e : entries) {
		if (e.team == -1 || e.team == 2) continue;
		if (e.distance > MAX_PLAYER_DIST) continue;

		double y = e.rotation.yaw   * DEG;
		double p = e.rotation.pitch * DEG;
		double cy = cos(y), sy = sin(y);
		double cp = cos(p), sp = sin(p);
		Cheat::Vector3 fwd = { cp * cy, cp * sy, sp };

		Cheat::Vector3 eye = { e.worldPos.x, e.worldPos.y, e.worldPos.z + EYE_HEIGHT };
		Cheat::Vector3 tip = { eye.x + fwd.x * LEN, eye.y + fwd.y * LEN, eye.z + fwd.z * LEN };

		float ex, ey, tx, ty;
		bool eok = Cheat::WorldToScreen(eye, camLoc, camRot, fov, sw, sh, ex, ey);
		bool tok = Cheat::WorldToScreen(tip, camLoc, camRot, fov, sw, sh, tx, ty);
		if (!eok || !tok) continue;

		ImU32 col = (e.team == 0) ? IM_COL32( 80, 180, 255, 200)
		          : (e.team == 1) ? IM_COL32(255,  80,  80, 200)
		                          : IM_COL32(220, 220, 220, 200);
		ImU32 colDim = (e.team == 0) ? IM_COL32( 80, 180, 255,  60)
		             : (e.team == 1) ? IM_COL32(255,  80,  80,  60)
		                             : IM_COL32(220, 220, 220,  60);

		dl->AddLine(ImVec2(ex, ey), ImVec2(tx, ty), IM_COL32(0,0,0,90), 2.2f);
		dl->AddLine(ImVec2(ex, ey), ImVec2(tx, ty), col, 1.2f);

		dl->AddCircleFilled(ImVec2(tx, ty), 2.5f, colDim, 10);
		dl->AddCircle      (ImVec2(tx, ty), 4.0f, col,    12, 1.0f);
	}
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (!init)
	{
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice)))
		{
			pDevice->GetImmediateContext(&pContext);
			DXGI_SWAP_CHAIN_DESC sd;
			pSwapChain->GetDesc(&sd);
			window = sd.OutputWindow;

			ID3D11Texture2D* pBackBuffer;
			if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer))) {
				pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
				pBackBuffer->Release();
				oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

				ImGui::CreateContext();
				ImGuiIO& io = ImGui::GetIO();
				io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
				io.IniFilename = nullptr;

				ImFontConfig fcfg;
				fcfg.OversampleH = 3;
				fcfg.OversampleV = 2;
				fcfg.PixelSnapH  = false;
				ImFont* mainFont = io.Fonts->AddFontFromFileTTF(
					"C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &fcfg,
					io.Fonts->GetGlyphRangesDefault());
				if (!mainFont) {
					mainFont = io.Fonts->AddFontFromFileTTF(
						"C:\\Windows\\Fonts\\tahoma.ttf", 15.0f, &fcfg,
						io.Fonts->GetGlyphRangesDefault());
				}
				if (!mainFont) {
					io.Fonts->AddFontDefault();
				}

				ImGui_ImplWin32_Init(window);
				ImGui_ImplDX11_Init(pDevice, pContext);
				ApplyTheme();

				Beep(1200, 300);
				init = true;
			}
		}
	}

	if (init) {
		Cheat::Tick();

		static bool insert_pressed = false;
		if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
			if (!insert_pressed) {
				show_menu = !show_menu;
				insert_pressed = true;
			}
		} else {
			insert_pressed = false;
		}

		bool any_draw = show_menu || Cheat::esp_players || Cheat::esp_ball ||
						Cheat::crosshair || Cheat::radar || Cheat::ball_trail ||
						Cheat::ball_prediction || Cheat::move_guide ||
						Cheat::aim_view;
		if (any_draw) {
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			DrawBallTrail();
			DrawBallPrediction();
			DrawMoveGuide();
			DrawAimLines();
			DrawEsp();
			DrawRadar();
			DrawCrosshair();
		}

		if (show_menu && any_draw) {
			const float dt    = ImGui::GetIO().DeltaTime;
			const float speed = 10.0f;
			for (int i = 0; i < 2; i++)
				tab_anim[i] += ((active_tab == i ? 1.f : 0.f) - tab_anim[i]) * speed * dt;

			ImGui::SetNextWindowSize(ImVec2(900, 560), ImGuiCond_FirstUseEver);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
			ImGui::Begin("##psocheat", &show_menu,
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar);
			ImGui::PopStyleVar();

			ImDrawList* draw = ImGui::GetWindowDrawList();
			ImVec2 wpos  = ImGui::GetWindowPos();
			ImVec2 wsize = ImGui::GetWindowSize();

			draw->AddRect(
				ImVec2(wpos.x - 0.5f, wpos.y - 0.5f),
				ImVec2(wpos.x + wsize.x + 0.5f, wpos.y + wsize.y + 0.5f),
				ColU32(0.55f, 0.25f, 0.85f, 0.45f), 14.f, 0, 1.f
			);

			const float header_h = 56.f;
			draw->AddRectFilled(ImVec2(wpos.x, wpos.y), ImVec2(wpos.x + wsize.x, wpos.y + header_h),
				ColU32(0.070f, 0.055f, 0.110f, 1.0f), 14.f, ImDrawFlags_RoundCornersTop);
			draw->AddRectFilledMultiColor(
				ImVec2(wpos.x, wpos.y + header_h - 1.f),
				ImVec2(wpos.x + wsize.x, wpos.y + header_h),
				ColU32(0.45f, 0.15f, 0.75f, 0.00f),
				ColU32(0.85f, 0.40f, 1.00f, 0.60f),
				ColU32(0.85f, 0.40f, 1.00f, 0.60f),
				ColU32(0.45f, 0.15f, 0.75f, 0.00f)
			);
			float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 3.0f);
			draw->AddCircleFilled(ImVec2(wpos.x + 22.f, wpos.y + header_h * 0.5f),
				5.f, ColU32(0.85f, 0.35f, 1.0f, 0.35f + 0.55f * pulse), 16);
			draw->AddCircleFilled(ImVec2(wpos.x + 22.f, wpos.y + header_h * 0.5f),
				2.5f, ColU32(1.0f, 0.80f, 1.0f, 1.0f), 12);
			draw->AddText(ImVec2(wpos.x + 38.f, wpos.y + 13.f),
				ColU32(0.97f, 0.90f, 1.0f, 1.0f), "discord : antalya.gov.tr");
			draw->AddText(ImVec2(wpos.x + 38.f, wpos.y + 30.f),
				ColU32(0.55f, 0.45f, 0.75f, 1.0f), "cheat v2.0");

			ImGui::SetCursorPos(ImVec2(wsize.x - 34.f, 17.f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.20f, 0.30f, 0.55f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.30f, 0.40f, 0.80f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.75f, 0.95f, 1.f));
			if (ImGui::Button("X", ImVec2(22.f, 22.f))) show_menu = false;
			ImGui::PopStyleColor(4);

			const char* tab_labels[] = { "Player", "ESP" };
			const char* tab_icons [] = { "",       "[]"  };
			ImGui::SetCursorPos(ImVec2(280.f, 6.f));
			for (int i = 0; i < 2; i++) {
				TopTab(i, tab_icons[i], tab_labels[i], active_tab);
			}
			ImGui::NewLine();

			const float footer_h = 28.f;
			const float body_y   = header_h + 10.f;
			const float body_h   = wsize.y - header_h - footer_h - 20.f;

			ImGui::SetCursorPos(ImVec2(12.f, body_y));
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.f);
			ImGui::BeginChild("##body", ImVec2(wsize.x - 24.f, body_h), false, ImGuiWindowFlags_NoScrollbar);

			float body_w = ImGui::GetContentRegionAvail().x;
			float col_w  = (body_w - 10.f) * 0.5f;
			float col_h  = ImGui::GetContentRegionAvail().y;

			ImGui::BeginChild("##colL", ImVec2(col_w, col_h), false, ImGuiWindowFlags_NoScrollbar);
			{
				if (active_tab == 0) {
					ModernCardBegin("Ball Control");
					ToggleRow("No Kick Cooldown",  &Cheat::no_kick_cooldown);
					ToggleRow("No Touch Slowdown", &Cheat::no_touch_slowdown);
					ModernCardEnd();

					ImGui::Dummy(ImVec2(0.f, 6.f));

					ModernCardBegin("Ball Aimbot");
					ToggleRow("Lock On Ball", &Cheat::aim_ball);
					ToggleRow("Vertical Aim", &Cheat::aim_ball_pitch);
					ImGui::TextColored(ImVec4(0.65f, 0.55f, 0.85f, 1.f),
						"Hold Mouse4 / Mouse5 to activate");
					if (!Cheat::aim_ball) ImGui::BeginDisabled();
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##aimsp", &Cheat::aim_speed, 0.05f, 1.0f, "Smoothness: %.2f");
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##aimsn", &Cheat::aim_sensitivity, 1.0f, 30.0f, "Sensitivity: %.1f px/deg");
					if (!Cheat::aim_ball) ImGui::EndDisabled();
					ModernCardEnd();

					ImGui::Dummy(ImVec2(0.f, 6.f));

					ModernCardBegin("Auto GK");
					ToggleRow("Auto Dive", &Cheat::auto_gk);
					if (!Cheat::auto_gk) ImGui::BeginDisabled();
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##gktt", &Cheat::gk_trigger_time, 0.05f, 0.5f, "Trigger: %.2fs early");
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##gkmh", &Cheat::gk_max_height, 100.f, 400.f, "Max Height: %.0f cm");
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##gkbt", &Cheat::gk_big_threshold, 30.f, 200.f, "Big Dive Threshold: %.0f cm");
					ImGui::TextColored(ImVec4(0.65f,0.55f,0.85f,1.f), "Keys (VK code)");
					ImGui::SetNextItemWidth(-1);
					ImGui::InputInt("##gkks", &Cheat::gk_key_short, 0, 0);
					ImGui::SameLine(); ImGui::TextDisabled("Short Dive Mod (Ctrl=162)");
					ImGui::SetNextItemWidth(-1);
					ImGui::InputInt("##gkkb", &Cheat::gk_key_big, 0, 0);
					ImGui::SameLine(); ImGui::TextDisabled("Big Dive Mod (W=87)");
					ImGui::SetNextItemWidth(-1);
					ImGui::InputInt("##gkl", &Cheat::gk_key_left, 0, 0);
					ImGui::SameLine(); ImGui::TextDisabled("Left (A=65)");
					ImGui::SetNextItemWidth(-1);
					ImGui::InputInt("##gkr", &Cheat::gk_key_right, 0, 0);
					ImGui::SameLine(); ImGui::TextDisabled("Right (D=68)");
					if (!Cheat::auto_gk) ImGui::EndDisabled();
					ModernCardEnd();
				}
				else if (active_tab == 1) {
					ModernCardBegin("General");
					ToggleRow("Players", &Cheat::esp_players);
					ToggleRow("Ball",    &Cheat::esp_ball);
					ModernCardEnd();

					ImGui::Dummy(ImVec2(0.f, 6.f));

					ModernCardBegin("Display");
					ToggleRow("Show Name",     &Cheat::esp_names);
					ToggleRow("Show Distance", &Cheat::esp_distance);
					ToggleRow("Tracer Lines",  &Cheat::esp_lines);
					ModernCardEnd();
				}
			}
			ImGui::EndChild();

			ImGui::SameLine(0.f, 10.f);

			ImGui::BeginChild("##colR", ImVec2(col_w, col_h), false, ImGuiWindowFlags_NoScrollbar);
			{
				if (active_tab == 0) {
					ModernCardBegin("Camera");
					ImGui::TextColored(ImVec4(0.80f, 0.70f, 0.95f, 1.f), "FOV");
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##fov", &Cheat::fov_value, 70.f, 150.f, "%.1f");
					ImGui::Dummy(ImVec2(0.f, 4.f));
					ToggleRow("Custom Camera Distance", &Cheat::custom_cam);
					{
						const double dmin = 100.0, dmax = 1500.0, hmin = -200.0, hmax = 500.0;
						if (!Cheat::custom_cam) ImGui::BeginDisabled();
						ImGui::SetNextItemWidth(-1);
						ImGui::SliderScalar("##cd", ImGuiDataType_Double, &Cheat::cam_dist_value, &dmin, &dmax, "Distance: %.0f");
						ImGui::SetNextItemWidth(-1);
						ImGui::SliderScalar("##ch", ImGuiDataType_Double, &Cheat::cam_height_value, &hmin, &hmax, "Height: %.0f");
						if (!Cheat::custom_cam) ImGui::EndDisabled();
					}
					ImGui::Dummy(ImVec2(0.f, 4.f));
					ToggleRow("Hold Zoom [C key]", &Cheat::zoom_hold);
					if (!Cheat::zoom_hold) ImGui::BeginDisabled();
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##zf", &Cheat::zoom_fov, 20.f, 80.f, "Zoom FOV: %.1f");
					if (!Cheat::zoom_hold) ImGui::EndDisabled();
					ModernCardEnd();
				}
				else if (active_tab == 1) {
					ModernCardBegin("Distance Limit");
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##md", &Cheat::esp_max_distance, 500.f, 15000.f, "Max: %.0f");
					ModernCardEnd();

					ImGui::Dummy(ImVec2(0.f, 6.f));

					ModernCardBegin("Crosshair & Ball Path");
					ToggleRow("Crosshair", &Cheat::crosshair);
					if (!Cheat::crosshair) ImGui::BeginDisabled();
					ImGui::TextColored(ImVec4(0.80f, 0.70f, 0.95f, 1.f), "Style");
					ImGui::SetNextItemWidth(-1);
					const char* styles[] = { "Dot", "Cross", "Gap Cross", "Circle" };
					ImGui::Combo("##cs", &Cheat::crosshair_style, styles, IM_ARRAYSIZE(styles));
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##csz", &Cheat::crosshair_size, 2.f, 30.f, "Size: %.0f");
					if (!Cheat::crosshair) ImGui::EndDisabled();
					ImGui::Dummy(ImVec2(0.f, 2.f));
					ToggleRow("Ball Trail",      &Cheat::ball_trail);
					ToggleRow("Ball Prediction", &Cheat::ball_prediction);
					if (!Cheat::ball_prediction) ImGui::BeginDisabled();
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##bpt", &Cheat::ball_pred_time, 0.3f, 3.0f, "Time: %.1fs");
					if (!Cheat::ball_prediction) ImGui::EndDisabled();
					ToggleRow("Move Guide", &Cheat::move_guide);
					ToggleRow("Aim Lines",  &Cheat::aim_view);
					if (!Cheat::aim_view) ImGui::BeginDisabled();
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##avl", &Cheat::aim_view_length, 200.f, 1500.f, "Length: %.0f");
					if (!Cheat::aim_view) ImGui::EndDisabled();
					ModernCardEnd();

					ImGui::Dummy(ImVec2(0.f, 6.f));

					ModernCardBegin("Radar");
					ToggleRow("Radar", &Cheat::radar);
					if (!Cheat::radar) ImGui::BeginDisabled();
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##rsz", &Cheat::radar_size, 100.f, 320.f, "Size: %.0f");
					ImGui::SetNextItemWidth(-1);
					ImGui::SliderFloat("##rrg", &Cheat::radar_range, 1000.f, 15000.f, "Range: %.0f");
					if (!Cheat::radar) ImGui::EndDisabled();
					ModernCardEnd();
				}
			}
			ImGui::EndChild();

			ImGui::EndChild();
			ImGui::PopStyleVar();

			ImVec2 fp(wpos.x, wpos.y + wsize.y - footer_h);
			draw->AddRectFilled(fp, ImVec2(fp.x + wsize.x, fp.y + footer_h), ColU32(0.045f, 0.035f, 0.080f, 1.0f));
			draw->AddRectFilledMultiColor(
				fp, ImVec2(fp.x + wsize.x, fp.y + 1.f),
				ColU32(0.45f, 0.20f, 0.70f, 0.00f),
				ColU32(0.80f, 0.35f, 1.00f, 0.60f),
				ColU32(0.80f, 0.35f, 1.00f, 0.60f),
				ColU32(0.45f, 0.20f, 0.70f, 0.00f)
			);

			int active_count =
				(int)Cheat::no_kick_cooldown + (int)Cheat::no_touch_slowdown +
				(int)Cheat::custom_cam + (int)Cheat::esp_players +
				(int)Cheat::esp_ball + (int)Cheat::crosshair +
				(int)Cheat::radar + (int)Cheat::ball_trail +
				(int)Cheat::zoom_hold +
				(int)Cheat::ball_prediction + (int)Cheat::move_guide +
				(int)Cheat::aim_view + (int)Cheat::auto_gk +
				(int)Cheat::aim_ball;

			char status[96];
			sprintf_s(status, "%d active feature%s", active_count, active_count == 1 ? "" : "s");
			draw->AddText(ImVec2(fp.x + 14.f, fp.y + 8.f), ColU32(0.75f, 0.60f, 0.90f, 1.0f), status);

			char fps_str[48];
			sprintf_s(fps_str, "%.0f FPS", ImGui::GetIO().Framerate);
			ImVec2 ts = ImGui::CalcTextSize(fps_str);
			draw->AddText(ImVec2(fp.x + wsize.x - ts.x - 14.f, fp.y + 8.f), ColU32(0.55f, 0.42f, 0.70f, 1.0f), fps_str);

			ImGui::End();
		}

		if (any_draw) {
			ImGui::Render();
			pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}
	}

	return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
	Sleep(5000);

	if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success) {
		kiero::bind(8, (void**)&oPresent, hkPresent);
		kiero::bind(13, (void**)&oResizeBuffers, hkResizeBuffers);
		Beep(100, 500);
	}

	Cheat::Init();

	return 0;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hMod);
		CreateThread(NULL, 0, MainThread, hMod, 0, NULL);
	}
	return TRUE;
}
