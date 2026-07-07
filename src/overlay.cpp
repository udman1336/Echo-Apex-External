#include "../include/overlay.h"
#include <comdef.h>
#include <corecrt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#pragma warning(disable : 4996)

static HWND          Win       = nullptr;
static HHOOK         MouseHook = nullptr;
static bool          MenuOpen  = false;

// catches mouse input over our window without stealing game focus
LRESULT CALLBACK MouseHookProc(int Code, WPARAM WParam, LPARAM LParam)
{
	if (Code >= 0 && MenuOpen)
	{
		MSLLHOOKSTRUCT* Mouse = (MSLLHOOKSTRUCT*)LParam;
		POINT Pt = Mouse->pt;
		RECT  Rect;
		GetWindowRect(Win, &Rect);

		if (PtInRect(&Rect, Pt))
		{
			ImGuiIO& Io = ImGui::GetIO();

			POINT ClientPt = Pt;
			ScreenToClient(Win, &ClientPt);
			Io.MousePos.x = (float)ClientPt.x;
			Io.MousePos.y = (float)ClientPt.y;

			if      (WParam == WM_LBUTTONDOWN) { Io.MouseDown[0] = true;  return 1; }
			else if (WParam == WM_LBUTTONUP)   { Io.MouseDown[0] = false; return 1; }
			else if (WParam == WM_RBUTTONDOWN) { Io.MouseDown[1] = true;  return 1; }
			else if (WParam == WM_RBUTTONUP)   { Io.MouseDown[1] = false; return 1; }
			else if (WParam == WM_MOUSEWHEEL)
			{
				Io.MouseWheel += ((float)(short)HIWORD(Mouse->mouseData)) / (float)WHEEL_DELTA;
				return 1;
			}
		}
	}
	return CallNextHookEx(MouseHook, Code, WParam, LParam);
}

// static member defs
ID3D11Device*           CheatOverlay::Device       = nullptr;
ID3D11DeviceContext*    CheatOverlay::DeviceCtx    = nullptr;
IDXGISwapChain*         CheatOverlay::SwapChain    = nullptr;
ID3D11RenderTargetView* CheatOverlay::RenderTarget  = nullptr;

auto CheatOverlay::SetWindowStyle() -> void
{
	int Style = (int)GetWindowLong(Win, -20);
	SetWindowLongPtr(Win, -20, (LONG_PTR)(Style | 0x20));
}

auto CheatOverlay::SetWindowTransparency() -> void
{
	MARGINS Margin = { -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(Win, &Margin);
	SetLayeredWindowAttributes(Win, 0x000000, 0xFF, 0x02);
}

auto CheatOverlay::GetWindow() -> HWND { return Win; }

auto CheatOverlay::Init() -> BOOL
{
	WNDCLASSEXA Wc = { 0 };
	Wc.cbSize        = sizeof(WNDCLASSEXA);
	Wc.lpfnWndProc   = DefWindowProcA;
	Wc.hInstance     = GetModuleHandle(NULL);
	Wc.lpszClassName = "EchoApexOverlay";
	RegisterClassExA(&Wc);

	Win = CreateWindowExA(
		WS_EX_LAYERED | WS_EX_TOOLWINDOW,
		"EchoApexOverlay", "Echo Apex",
		WS_POPUP,
		0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
		NULL, NULL, Wc.hInstance, NULL
	);

	if (!Win) return FALSE;

	// parent to apex so the overlay tracks with the game window
	HWND GameWnd = FindWindowA(NULL, "Apex Legends");
	if (GameWnd)
		SetWindowLongPtrA(Win, GWLP_HWNDPARENT, (LONG_PTR)GameWnd);

	SetLayeredWindowAttributes(Win, 0, 255, LWA_ALPHA);

	MARGINS Margin = { -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(Win, &Margin);

	ShowWindow(Win, SW_SHOW);
	UpdateWindow(Win);
	return TRUE;
}

auto CheatOverlay::ShutdownD3D() -> void
{
	if (RenderTarget) { RenderTarget->Release(); RenderTarget = nullptr; }
	if (SwapChain)    { SwapChain->Release();    SwapChain    = nullptr; }
	if (DeviceCtx)    { DeviceCtx->Release();    DeviceCtx    = nullptr; }
	if (Device)       { Device->Release();       Device       = nullptr; }
}

auto CheatOverlay::InitD3D() -> BOOL
{
	DXGI_SWAP_CHAIN_DESC Sd;
	ZeroMemory(&Sd, sizeof(Sd));
	Sd.BufferCount                        = 2;
	Sd.BufferDesc.Width                   = 0;
	Sd.BufferDesc.Height                  = 0;
	Sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
	Sd.BufferDesc.RefreshRate.Numerator   = 60;
	Sd.BufferDesc.RefreshRate.Denominator = 1;
	Sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Sd.OutputWindow                       = Win;
	Sd.SampleDesc.Count                   = 1;
	Sd.SampleDesc.Quality                 = 0;
	Sd.Windowed                           = TRUE;
	Sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

	D3D_FEATURE_LEVEL FeatureLevel;
	HRESULT Hr = D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
		D3D11_SDK_VERSION, &Sd, &SwapChain, &Device, &FeatureLevel, &DeviceCtx
	);

	if (FAILED(Hr)) return FALSE;

	CreateRenderTarget();
	return TRUE;
}

void CheatOverlay::CreateRenderTarget()
{
	if (RenderTarget) { RenderTarget->Release(); RenderTarget = nullptr; }

	ID3D11Texture2D* BackBuf = nullptr;
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&BackBuf);
	if (BackBuf)
	{
		Device->CreateRenderTargetView(BackBuf, nullptr, &RenderTarget);
		BackBuf->Release();
	}
}

auto CheatOverlay::BeginScene() -> void
{
	float Clear[4] = { 0.f, 0.f, 0.f, 0.f };
	DeviceCtx->OMSetRenderTargets(1, &RenderTarget, nullptr);
	DeviceCtx->ClearRenderTargetView(RenderTarget, Clear);
}

auto CheatOverlay::EndScene() -> void
{
	SwapChain->Present(1, 0);
}

auto CheatOverlay::ClearScene() -> void
{
	float Clear[4] = { 0.f, 0.f, 0.f, 0.f };
	DeviceCtx->ClearRenderTargetView(RenderTarget, Clear);
}

auto CheatOverlay::InitImGui() -> BOOL
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();

	if (!ImGui_ImplWin32_Init(Win))                return FALSE;
	if (!ImGui_ImplDX11_Init(Device, DeviceCtx))   return FALSE;

	return TRUE;
}

auto CheatOverlay::ShutdownImGui() -> void
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

auto CheatOverlay::NewFrame() -> void
{
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();
}

auto CheatOverlay::Render() -> void
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void CheatOverlay::DrawLine(float X1, float Y1, float X2, float Y2, ImU32 Color, float Thickness)
{
	ImGui::GetBackgroundDrawList()->AddLine(ImVec2(X1, Y1), ImVec2(X2, Y2), Color, Thickness);
}

void CheatOverlay::DrawRect(float X, float Y, float W, float H, ImU32 Color, float Thickness)
{
	ImGui::GetBackgroundDrawList()->AddRect(ImVec2(X, Y), ImVec2(X + W, Y + H), Color, 0.f, 0, Thickness);
}

void CheatOverlay::DrawFilledRect(float X, float Y, float W, float H, ImU32 Color)
{
	ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(X, Y), ImVec2(X + W, Y + H), Color);
}

void CheatOverlay::DrawText(float X, float Y, const char* Text, ImU32 Color)
{
	ImGui::GetBackgroundDrawList()->AddText(ImVec2(X, Y), Color, Text);
}

void CheatOverlay::SetTransparency(bool Transparent)
{
	LONG_PTR Style = GetWindowLongPtr(Win, GWL_EXSTYLE);
	if (Transparent)
		SetWindowLongPtr(Win, GWL_EXSTYLE, Style | WS_EX_TRANSPARENT);
	else
		SetWindowLongPtr(Win, GWL_EXSTYLE, Style & ~WS_EX_TRANSPARENT);
}

void CheatOverlay::SetMenuState(bool Open)
{
	MenuOpen = Open;

	if (Open)
	{
		SetTransparency(false);
		if (!MouseHook)
			MouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, GetModuleHandle(NULL), 0);
	}
	else
	{
		SetTransparency(true);
		if (MouseHook)
		{
			UnhookWindowsHookEx(MouseHook);
			MouseHook = nullptr;
		}
	}
}