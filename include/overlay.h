#pragma once
#include <windows.h>
#include <stdio.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include "HitboxType.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx11.h"

#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS

class CheatOverlay
{
public:
	auto SetWindowStyle()       -> void;
	auto SetWindowTransparency()-> void;
	auto GetWindow()            -> HWND;
	auto Init()                 -> BOOL;
	auto ShutdownD3D()          -> void;
	auto InitD3D()              -> BOOL;
	auto BeginScene()           -> void;
	auto EndScene()             -> void;
	auto ClearScene()           -> void;

	auto InitImGui()    -> BOOL;
	auto ShutdownImGui()-> void;
	auto NewFrame()     -> void;
	auto Render()       -> void;

	void DrawLine(float X1, float Y1, float X2, float Y2, ImU32 Color, float Thickness = 1.0f);
	void DrawRect(float X, float Y, float W, float H, ImU32 Color, float Thickness = 1.0f);
	void DrawFilledRect(float X, float Y, float W, float H, ImU32 Color);
	void DrawText(float X, float Y, const char* Text, ImU32 Color);

	void SetTransparency(bool Transparent);
	void SetMenuState(bool Open);

	ID3D11Device*        GetDevice()       { return Device; }
	ID3D11DeviceContext* GetDeviceCtx()    { return DeviceCtx; }
	IDXGISwapChain*      GetSwapChain()    { return SwapChain; }

private:
	static ID3D11Device*            Device;
	static ID3D11DeviceContext*     DeviceCtx;
	static IDXGISwapChain*          SwapChain;
	static ID3D11RenderTargetView*  RenderTarget;

	void CreateRenderTarget();
};