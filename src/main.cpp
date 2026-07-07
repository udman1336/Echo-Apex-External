#include "../include/overlay.h"
#include "../include/utils.h"
#include <string>

uint64_t GBase = 0;

// screen centre cached so aimbot doesn't hit SM every frame
static int ScreenW  = 0;
static int ScreenH  = 0;
static int ScreenCX = 0;
static int ScreenCY = 0;

inline void RefreshScreenMetrics()
{
	static auto LastRefresh = std::chrono::steady_clock::now();
	auto Now = std::chrono::steady_clock::now();
	if (ScreenW == 0 || std::chrono::duration_cast<std::chrono::seconds>(Now - LastRefresh).count() >= 1)
	{
		ScreenW  = GetSystemMetrics(SM_CXSCREEN);
		ScreenH  = GetSystemMetrics(SM_CYSCREEN);
		ScreenCX = ScreenW / 2;
		ScreenCY = ScreenH / 2;
		LastRefresh = Now;
	}
}

std::string GetLevelName()
{
	char Buf[64] = { 0 };
	for (int I = 0; I < 64; I++)
	{
		Buf[I] = Kernel.read<char>(Kernel.GetBaseAddress() + OFFSET_LEVELNAME + I);
		if (Buf[I] == '\0') break;
	}
	return std::string(Buf);
}

// player slots live in the low range of cl_entitylist, no signifier needed
void SetupPlayers()
{
	constexpr int ScanMax = 128;

	while (true)
	{
		std::vector<DWORD_PTR> Temp;
		Temp.reserve(ScanMax);

		uintptr_t Base = Kernel.GetBaseAddress();

		for (int I = 0; I < ScanMax; I++)
		{
			DWORD_PTR Ent = Kernel.read<DWORD_PTR>(Base + OFFSET_ENTITYLIST + (I * 0x20));
			if (!Ent) continue;

			// alive=0, knocked=5 — props are never either
			int LifeState = Kernel.read<int>(Ent + OFFSET_LIFE_STATE);
			if (LifeState != 0 && LifeState != 5) continue;

			// team 0 = world entity, >50 = garbage
			int TeamId = Kernel.read<int>(Ent + OFFSET_TEAM);
			if (TeamId <= 0 || TeamId > 50) continue;

			// 100 base + 75 gold shield max, props read 0 or junk
			int Hp = Kernel.read<int>(Ent + OFFSET_HEALTH);
			if (Hp <= 0 || Hp > 175) continue;

			Temp.push_back(Ent);
		}

		{
			std::lock_guard<std::mutex> Lock(EntityListMutex);
			PlayerEntityList = Temp;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

void SetupLoot()
{
	while (true)
	{
		// nothing enabled, sleep and skip
		if (!Settings.ItemDist && !Settings.ItemName)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
			continue;
		}

		std::vector<DWORD_PTR> TempLoots;
		TempLoots.reserve(200);

		DWORD_PTR LocalPlayer = Kernel.read<DWORD_PTR>(Kernel.GetBaseAddress() + OFFSET_LOCAL_ENT);
		if (!LocalPlayer)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
			continue;
		}

		Vector LocalOrigin = Kernel.read<Vector>(LocalPlayer + OFFSET_ORIGIN);

		// cache signifiers for loot — reused addresses can lie after ~10s
		static std::unordered_map<uintptr_t, std::string> SigCache;
		static auto LastCacheClear = std::chrono::steady_clock::now();
		auto SigNow = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::seconds>(SigNow - LastCacheClear).count() > 10)
		{
			SigCache.clear();
			LastCacheClear = SigNow;
		}

		for (size_t I = 0; I < 15000; I++)
		{
			DWORD_PTR Ent = Kernel.read<DWORD_PTR>(Kernel.GetBaseAddress() + OFFSET_ENTITYLIST + (I * 0x20));
			if (!Ent) continue;

			std::string Sig;
			auto It = SigCache.find(Ent);
			if (It != SigCache.end())
				Sig = It->second;
			else
			{
				Sig = GetSignifier(Ent);
				SigCache[Ent] = Sig;
			}

			if (Sig == "prop_death_box" || Sig == "prop_survival")
			{
				Vector Origin = Kernel.read<Vector>(Ent + OFFSET_ORIGIN);
				if (LocalOrigin.DistTo(Origin) < 50.0f * 50.0f)
					TempLoots.push_back(Ent);
			}
		}

		{
			std::lock_guard<std::mutex> Lock(LootListMutex);
			EntityLoot = TempLoots;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	}
}

bool GetVisible(uint64_t LocalPtr, uint64_t EntPtr)
{
	float LastVisible  = Kernel.read<float>(EntPtr   + OFFSET_VISIBLE_TIME);
	float CurrentTime  = Kernel.read<float>(LocalPtr + m_currentFramePlayertimeBase);
	// visible if seen in the last 200ms
	return (LastVisible > 0.0f) && (LastVisible >= CurrentTime - 0.2f);
}

void Aimbot(uintptr_t LocalPlayer, Vector CamOrigin, Matrix ViewMatrix, Manager* Mgr, int LocalTeam)
{
	bool MouseDown      = (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
	bool CtrlDown       = Settings.ControllerEnabled && IsControllerAimButtonPressed();
	bool Ps5Down        = Settings.ControllerEnabled && IsPS5AimButtonPressed();

	if (!MouseDown && !CtrlDown && !Ps5Down) return;
	if (!Settings.Aimbot) return;

	WeaponXEntity Weap;
	Weap.update(LocalPlayer);
	float BulletSpeed = Weap.get_projectile_speed();
	float BulletGrav  = Weap.get_projectile_gravity();
	float ZoomFov     = Weap.get_zoom_fov();

	static int DbgTick = 0;
	if (DbgTick++ % 60 == 0)
		printf("[DBG] spd=%.2f grav=%.2f zoom=%.2f\n", BulletSpeed, BulletGrav, ZoomFov);

	DWORD_PTR BestTarget = 0;
	float     BestFov    = Settings.AimFOV;
	Vector    BestAngle  = { 0, 0, 0 };

	Vector CurrentAngles = Kernel.read<Vector>(LocalPlayer + OFFSET_VIEWANGLES);

	float MaxFov = Settings.AimFOV;
	if (ZoomFov != 0.f && ZoomFov != 1.f)
		MaxFov *= ZoomFov / 90.f;

	RefreshScreenMetrics();
	const int CX = ScreenCX;
	const int CY = ScreenCY;

	for (MainPlayer& P : Mgr->Players)
	{
		if (P.ptr == 0 || P.Health <= 0) continue;

		int Team = IntCache.Get(P.ptr + OFFSET_TEAM);
		if (Team == 0) Team = Kernel.read<int>(P.ptr + OFFSET_TEAM);
		if (Team == LocalTeam && !Settings.Team) continue;

		Vector Origin = Kernel.read<Vector>(P.ptr + OFFSET_ORIGIN);
		if (ToMeters(CamOrigin.DistTo(Origin)) > Settings.MaxDistance) continue;
		if (Settings.AimVisibleOnly && !GetVisible(LocalPlayer, P.ptr)) continue;

		uintptr_t  BonePtr = Kernel.read<uintptr_t>(P.ptr + OFFSET_BONES);
		Vector     HeadPos;
		if (BonePtr)
		{
			matrix3x4_t BoneMat = Kernel.read<matrix3x4_t>(BonePtr + 10 * 48);
			HeadPos = Vector(
				Origin.x + BoneMat.m_flMatVal[0][3],
				Origin.y + BoneMat.m_flMatVal[1][3],
				Origin.z + BoneMat.m_flMatVal[2][3]
			);
		}
		else
		{
			HeadPos = Vector(Origin.x, Origin.y, Origin.z + 64.f);
		}

		Vector W2S = _WorldToScreen(HeadPos, ViewMatrix);
		if (W2S.z <= 0.001f) continue;

		float Fov = sqrt(pow(W2S.x - CX, 2) + pow(W2S.y - CY, 2));
		if (Fov >= BestFov) continue;

		BestFov    = Fov;
		BestTarget = P.ptr;

		if (BulletSpeed > 1.f)
		{
			Vector TargetVel = Kernel.read<Vector>(P.ptr + OFFSET_ABS_VELOCITY);

			PredictCtx Ctx;
			Ctx.StartPos      = CamOrigin;
			Ctx.TargetPos     = HeadPos;
			Ctx.BulletSpeed   = BulletSpeed - (BulletSpeed * 0.08f);
			Ctx.BulletGravity = BulletGrav  + (BulletGrav  * 0.05f);
			Ctx.TargetVel     = TargetVel;

			if (BulletPredict(Ctx))
				BestAngle = { Ctx.AimAngles.y, Ctx.AimAngles.x, 0.f };
			else
				BestAngle = CalcAngle(CamOrigin, HeadPos);
		}
		else
		{
			BestAngle = CalcAngle(CamOrigin, HeadPos);
		}
	}

	if (!BestTarget) return;

	float DPitch = NormalizeAngle(BestAngle.x - CurrentAngles.x);
	float DYaw   = NormalizeAngle(BestAngle.y - CurrentAngles.y);

	Vector2 NewAngles =
	{
		NormalizeAngle(CurrentAngles.x + DPitch / Settings.AimSmooth),
		NormalizeAngle(CurrentAngles.y + DYaw   / Settings.AimSmooth)
	};

	if (CtrlDown)
	{
		Vector2 Stick = GetControllerRightStick();
		NewAngles.x = NormalizeAngle(NewAngles.x + (-Stick.y * Settings.ControllerSensitivity * 2.f));
		NewAngles.y = NormalizeAngle(NewAngles.y + ( Stick.x * Settings.ControllerSensitivity * 2.f));
	}

	if (Ps5Down)
	{
		Vector2 Stick = GetPS5RightStick();
		NewAngles.x = NormalizeAngle(NewAngles.x + (-Stick.y * Settings.ControllerSensitivity * 2.f));
		NewAngles.y = NormalizeAngle(NewAngles.y + ( Stick.x * Settings.ControllerSensitivity * 2.f));
	}

	Kernel.write<Vector2>(LocalPlayer + OFFSET_VIEWANGLES, NewAngles);
}

std::mutex ManagerMutex;
std::mutex EntityListMutex;
std::mutex LootListMutex;
Manager*   GlobalSharedManager = nullptr;

void UpdateManager()
{
	while (true)
	{
		Manager* Fresh = new Manager(1);
		{
			std::lock_guard<std::mutex> Lock(ManagerMutex);
			delete GlobalSharedManager;
			GlobalSharedManager = Fresh;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

ImU32 RgbToImU32(float R, float G, float B, float A = 1.f)
{
	return IM_COL32((int)(R * 255), (int)(G * 255), (int)(B * 255), (int)(A * 255));
}

void DrawESP(CheatOverlay* Overlay)
{
	// flush stale cache entries roughly once per second
	static auto LastClear = std::chrono::steady_clock::now();
	auto Now = std::chrono::steady_clock::now();
	if (std::chrono::duration_cast<std::chrono::milliseconds>(Now - LastClear).count() > 1000)
	{
		VectorCache.ClearExpired();
		IntCache.ClearExpired();
		UintptrCache.ClearExpired();
		LastClear = Now;
	}

	uint64_t ViewRenderer = Kernel.read<uint64_t>(Kernel.GetBaseAddress() + OFFSET_RENDER);
	if (!ViewRenderer) return;

	uint64_t ViewMatPtr = Kernel.read<uint64_t>(ViewRenderer + OFFSET_MATRIX);
	if (!ViewMatPtr) return;

	Matrix M = Kernel.read<Matrix>(ViewMatPtr);
	static Matrix LastGoodMatrix = M;

	bool MatValid = false;
	for (int I = 0; I < 16; I++)
		if (M.matrix[I] != 0.f) { MatValid = true; break; }

	if (MatValid) LastGoodMatrix = M;
	else          M = LastGoodMatrix;

	uintptr_t LocalPlayer = Kernel.read<uintptr_t>(Kernel.GetBaseAddress() + OFFSET_LOCAL_ENT);
	if (!LocalPlayer) return;

	int LocalTeam = IntCache.Get(LocalPlayer + OFFSET_TEAM);
	if (!LocalTeam)
	{
		LocalTeam = Kernel.read<int>(LocalPlayer + OFFSET_TEAM);
		IntCache.Set(LocalPlayer + OFFSET_TEAM, LocalTeam);
	}

	Vector LocalOrigin = Kernel.read<Vector>(LocalPlayer + OFFSET_ORIGIN);
	Vector CamOrigin   = Kernel.read<Vector>(LocalPlayer + OFFSET_CAMERAPOS);

	std::vector<MainPlayer> Players;
	std::vector<Loot>       Loots;
	{
		std::lock_guard<std::mutex> Lock(ManagerMutex);
		if (GlobalSharedManager)
		{
			Players = GlobalSharedManager->Players;
			Loots   = GlobalSharedManager->Loots;
		}
	}

	char TextBuf[32];

	Manager TempMgr(0);
	TempMgr.Players = Players;
	TempMgr.Loots   = Loots;
	Aimbot(LocalPlayer, CamOrigin, M, &TempMgr, LocalTeam);

	if (Settings.Aimbot)
	{
		RefreshScreenMetrics();
		float FovRadius = (Settings.AimFOV / 180.f) * ScreenCY;
		constexpr int Segs = 36;
		static float PrevFov = -1.f;
		static float CircX[36], CircY[36];
		if (Settings.AimFOV != PrevFov)
		{
			PrevFov = Settings.AimFOV;
			for (int I = 0; I < Segs; I++)
			{
				float A = (I / (float)Segs) * 2.f * (float)M_PI;
				CircX[I] = cosf(A) * FovRadius;
				CircY[I] = sinf(A) * FovRadius;
			}
		}
		ImU32 FovCol = RgbToImU32(0.2f, 0.8f, 1.f, 0.5f);
		for (int I = 0; I < Segs; I++)
		{
			int J = (I + 1) % Segs;
			Overlay->DrawLine(
				ScreenCX + CircX[I], ScreenCY + CircY[I],
				ScreenCX + CircX[J], ScreenCY + CircY[J],
				FovCol, 1.f);
		}
	}

	for (MainPlayer& P : TempMgr.Players)
	{
		if (P.ptr == 0 || P.Health <= 0) continue;

		int  TeamId = P.Team;
		bool IsNpc  = (TeamId == 0);
		if (!IsNpc && TeamId == LocalTeam && !Settings.Team) continue;

		if (Settings.Glow)
			EnableHighlight(P.ptr);

		Vector EntFeet    = GetEntityBonePosition(P.ptr, 8);
		Vector W2SFeet    = _WorldToScreen(EntFeet, M);
		if (W2SFeet.z <= 0.f) continue;

		int    YOff  = 40;
		Vector Org   = Kernel.read<Vector>(P.ptr + OFFSET_ORIGIN);
		float  Dist  = LocalOrigin.DistTo(Org);
		float  Meters = ToMeters(Dist);
		if (Meters > Settings.MaxDistance) continue;

		if (Settings.Team)
		{
			snprintf(TextBuf, sizeof(TextBuf), "#%d", TeamId);
			Overlay->DrawText(W2SFeet.x, W2SFeet.y - YOff, TextBuf, RgbToImU32(0.f, 1.f, 0.f));
			YOff += 20;
		}

		if (Settings.Name)
		{
			static std::unordered_map<uintptr_t, std::string> NameCache;
			static auto LastNameClear = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(Now - LastNameClear).count() > 5000)
			{
				NameCache.clear();
				LastNameClear = Now;
			}

			std::string Name;
			auto It = NameCache.find(P.ptr);
			if (It != NameCache.end()) Name = It->second;
			else { Name = GetName(P.ptr); NameCache[P.ptr] = Name; }

			Overlay->DrawText(W2SFeet.x, W2SFeet.y - YOff, Name.c_str(), RgbToImU32(0.2f, 0.6f, 1.f));
			YOff += 20;
		}

		Vector HeadPos = GetBonePositionByHitbox(P.ptr, (int)HitboxType::Head,     Org);
		Vector FeetPos = GetBonePositionByHitbox(P.ptr, (int)HitboxType::Rightleg, Org);
		Vector W2SHead = _WorldToScreen(HeadPos, M);
		Vector W2SFt   = _WorldToScreen(FeetPos, M);
		if (W2SHead.z <= 0.001f || W2SFt.z <= 0.001f) continue;

		// visibility is cached per 500ms so we're not hammering reads
		static std::unordered_map<uintptr_t, bool> VisCache;
		static auto LastVisClear = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(Now - LastVisClear).count() > 500)
		{
			VisCache.clear();
			LastVisClear = Now;
		}
		bool Visible;
		auto VIt = VisCache.find(P.ptr);
		if (VIt != VisCache.end()) Visible = VIt->second;
		else { Visible = GetVisible(LocalPlayer, P.ptr); VisCache[P.ptr] = Visible; }

		if (Settings.Box)
		{
			float Height = fabsf(W2SFt.y - W2SHead.y) * 1.1f;
			float Width  = Height / 2.5f;
			float Bx     = W2SHead.x - (Width / 2.f);
			float By     = W2SHead.y - (Height * 0.1f);

			ImU32 BoxCol = Visible ? RgbToImU32(0.f, 1.f, 0.f) : RgbToImU32(1.f, 0.f, 0.f);

			if (Settings.BoxType == BoxStyle::Full)
			{
				Overlay->DrawRect(Bx, By, Width, Height, BoxCol, 1.5f);
			}
			else if (Settings.BoxType == BoxStyle::Cornered)
			{
				float CW = Width  / 4.f;
				float CH = Height / 4.f;
				float T  = 1.5f;
				Overlay->DrawLine(Bx,         By,          Bx + CW,       By,          BoxCol, T);
				Overlay->DrawLine(Bx,         By,          Bx,            By + CH,     BoxCol, T);
				Overlay->DrawLine(Bx + Width,  By,          Bx + Width - CW, By,        BoxCol, T);
				Overlay->DrawLine(Bx + Width,  By,          Bx + Width,    By + CH,     BoxCol, T);
				Overlay->DrawLine(Bx,         By + Height, Bx + CW,       By + Height, BoxCol, T);
				Overlay->DrawLine(Bx,         By + Height, Bx,            By + Height - CH, BoxCol, T);
				Overlay->DrawLine(Bx + Width,  By + Height, Bx + Width - CW, By + Height, BoxCol, T);
				Overlay->DrawLine(Bx + Width,  By + Height, Bx + Width,    By + Height - CH, BoxCol, T);
			}
		}

		if (Settings.Skelton)
		{
			ImU32 SkelCol = Visible ? RgbToImU32(0.f, 1.f, 0.f) : RgbToImU32(1.f, 0.f, 0.f);

			static std::unordered_map<uintptr_t, std::unordered_map<int, Vector2D>> BoneCache;
			static auto LastBoneClear = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(Now - LastBoneClear).count() > 500)
			{
				BoneCache.clear();
				LastBoneClear = Now;
			}

			auto GetBone = [&](HitboxType Type, Vector2D& Out) -> bool
			{
				auto It = BoneCache.find(P.ptr);
				if (It != BoneCache.end())
				{
					auto BIt = It->second.find((int)Type);
					if (BIt != It->second.end()) { Out = BIt->second; return true; }
				}
				Vector World = GetBonePositionByHitbox(P.ptr, (int)Type, Org);
				Vector Scr   = _WorldToScreen(World, M);
				if (Scr.z < 0.001f) return false;
				Out = { Scr.x, Scr.y };
				BoneCache[P.ptr][(int)Type] = Out;
				return true;
			};

			Vector2D Head, Neck, UChest, LChest, Stomach, Hip;
			Vector2D LSh, LEl, LHa, RSh, REl, RHa;
			Vector2D LTh, LKn, LLe, RTh, RKn, RLe;

			if (!GetBone(HitboxType::Neck, Neck) || !GetBone(HitboxType::Hip, Hip)) continue;

			GetBone(HitboxType::Head,         Head);
			GetBone(HitboxType::UpperChest,   UChest);
			GetBone(HitboxType::LowerChest,   LChest);
			GetBone(HitboxType::Stomach,      Stomach);
			GetBone(HitboxType::Leftshoulder, LSh); GetBone(HitboxType::Leftelbow,  LEl); GetBone(HitboxType::Lefthand,  LHa);
			GetBone(HitboxType::Rightshoulder,RSh); GetBone(HitboxType::RightelbowBone, REl); GetBone(HitboxType::Righthand, RHa);
			GetBone(HitboxType::LeftThighs,   LTh); GetBone(HitboxType::Leftknees,  LKn); GetBone(HitboxType::Leftleg,   LLe);
			GetBone(HitboxType::RightThighs,  RTh); GetBone(HitboxType::Rightknees, RKn); GetBone(HitboxType::Rightleg,  RLe);

			auto Draw = [&](const Vector2D& S, const Vector2D& E)
			{
				if (S.x != 0 && E.x != 0)
					Overlay->DrawLine(S.x, S.y, E.x, E.y, SkelCol, 1.f);
			};

			Draw(Head, Neck); Draw(Neck, UChest); Draw(UChest, LChest); Draw(LChest, Stomach); Draw(Stomach, Hip);
			Draw(Neck, LSh);  Draw(LSh,  LEl);    Draw(LEl,   LHa);
			Draw(Neck, RSh);  Draw(RSh,  REl);    Draw(REl,   RHa);
			Draw(Hip,  LTh);  Draw(LTh,  LKn);    Draw(LKn,   LLe);
			Draw(Hip,  RTh);  Draw(RTh,  RKn);    Draw(RKn,   RLe);
		}
	}

	if (Settings.ItemDist || Settings.ItemName)
	{
		for (const Loot& L : TempMgr.Loots)
		{
			if (Settings.LootChams)
				EnableHighlight(L.ptr);

			auto Range = LootItems::itemLists.equal_range(L.nameid);
			for (auto It = Range.first; It != Range.second; ++It)
			{
				const auto& Item = It->second;
				Vector      W2S  = _WorldToScreen(L.origin, M);
				if (W2S.z <= 0.f) continue;

				ImU32 ItemCol = RgbToImU32(1.f, 1.f, 1.f);
				switch (Item.rarity)
				{
				case LootItems::RARE:      ItemCol = RgbToImU32(0.2f, 0.6f, 1.f); break;
				case LootItems::EPIC:      ItemCol = RgbToImU32(0.8f, 0.2f, 1.f); break;
				case LootItems::LEGENDARY: ItemCol = RgbToImU32(1.f,  0.9f, 0.f); break;
				case LootItems::HEIRLOOM:  ItemCol = RgbToImU32(1.f,  0.f,  0.f); break;
				default: break;
				}

				Overlay->DrawText(W2S.x, W2S.y, Item.itemName.c_str(), ItemCol);
				if (Settings.ItemDist)
				{
					std::string Dist = "[" + std::to_string((int)ToMeters(LocalOrigin.DistTo(L.origin))) + "M]";
					Overlay->DrawText(W2S.x, W2S.y + 15, Dist.c_str(), ItemCol);
				}
			}
		}
	}
}

// ============================================================
//  ECHO  —  Futuristic ImGui Menu
// ============================================================

static int   ActiveTab  = 0;
static float PulseTimer = 0.f;

namespace EchoTheme
{
	static const ImVec4 Accent       = { 0.04f, 0.85f, 0.98f, 1.00f };
	static const ImVec4 AccentDim    = { 0.04f, 0.85f, 0.98f, 0.25f };
	static const ImVec4 AccentMid    = { 0.04f, 0.85f, 0.98f, 0.55f };
	static const ImVec4 BgDeep       = { 0.04f, 0.04f, 0.08f, 0.97f };
	static const ImVec4 BgMid        = { 0.07f, 0.07f, 0.13f, 1.00f };
	static const ImVec4 BgPanel      = { 0.09f, 0.09f, 0.16f, 1.00f };
	static const ImVec4 BgHover      = { 0.12f, 0.12f, 0.22f, 1.00f };
	static const ImVec4 Border       = { 0.04f, 0.85f, 0.98f, 0.18f };
	static const ImVec4 TextPrimary  = { 0.92f, 0.95f, 1.00f, 1.00f };
	static const ImVec4 TextDim      = { 0.45f, 0.50f, 0.60f, 1.00f };
}

static void ApplyTheme()
{
	ImGuiStyle& S = ImGui::GetStyle();
	S.WindowRounding    = 10.f; S.ChildRounding  = 8.f;
	S.FrameRounding     =  6.f; S.PopupRounding  = 6.f;
	S.ScrollbarRounding =  6.f; S.GrabRounding   = 4.f;
	S.TabRounding       =  6.f;
	S.WindowBorderSize  =  1.f; S.ChildBorderSize = 1.f; S.FrameBorderSize = 1.f;
	S.WindowPadding     = { 14.f, 12.f };
	S.FramePadding      = {  8.f,  5.f };
	S.ItemSpacing       = {  8.f,  7.f };
	S.ItemInnerSpacing  = {  6.f,  4.f };
	S.ScrollbarSize     = 10.f;
	S.GrabMinSize       = 10.f;

	ImVec4* C = S.Colors;
	C[ImGuiCol_WindowBg]            = EchoTheme::BgDeep;
	C[ImGuiCol_ChildBg]             = EchoTheme::BgPanel;
	C[ImGuiCol_PopupBg]             = EchoTheme::BgMid;
	C[ImGuiCol_Border]              = EchoTheme::Border;
	C[ImGuiCol_BorderShadow]        = { 0,0,0,0 };
	C[ImGuiCol_FrameBg]             = EchoTheme::BgMid;
	C[ImGuiCol_FrameBgHovered]      = EchoTheme::BgHover;
	C[ImGuiCol_FrameBgActive]       = { 0.14f, 0.14f, 0.26f, 1.f };
	C[ImGuiCol_TitleBg]             = EchoTheme::BgDeep;
	C[ImGuiCol_TitleBgActive]       = EchoTheme::BgDeep;
	C[ImGuiCol_TitleBgCollapsed]    = EchoTheme::BgDeep;
	C[ImGuiCol_ScrollbarBg]         = EchoTheme::BgMid;
	C[ImGuiCol_ScrollbarGrab]       = EchoTheme::AccentDim;
	C[ImGuiCol_ScrollbarGrabHovered]= EchoTheme::AccentMid;
	C[ImGuiCol_ScrollbarGrabActive] = EchoTheme::Accent;
	C[ImGuiCol_CheckMark]           = EchoTheme::Accent;
	C[ImGuiCol_SliderGrab]          = EchoTheme::Accent;
	C[ImGuiCol_SliderGrabActive]    = { 0.20f, 0.95f, 1.f, 1.f };
	C[ImGuiCol_Button]              = EchoTheme::BgMid;
	C[ImGuiCol_ButtonHovered]       = EchoTheme::BgHover;
	C[ImGuiCol_ButtonActive]        = EchoTheme::AccentDim;
	C[ImGuiCol_Header]              = EchoTheme::AccentDim;
	C[ImGuiCol_HeaderHovered]       = EchoTheme::AccentMid;
	C[ImGuiCol_HeaderActive]        = EchoTheme::Accent;
	C[ImGuiCol_Separator]           = EchoTheme::Border;
	C[ImGuiCol_Tab]                 = EchoTheme::BgMid;
	C[ImGuiCol_TabHovered]          = EchoTheme::AccentMid;
	C[ImGuiCol_TabActive]           = EchoTheme::AccentDim;
	C[ImGuiCol_TabUnfocused]        = EchoTheme::BgMid;
	C[ImGuiCol_TabUnfocusedActive]  = EchoTheme::AccentDim;
	C[ImGuiCol_Text]                = EchoTheme::TextPrimary;
	C[ImGuiCol_TextDisabled]        = EchoTheme::TextDim;
	C[ImGuiCol_PlotHistogram]       = EchoTheme::Accent;
	C[ImGuiCol_PlotHistogramHovered]= { 0.20f, 0.95f, 1.f, 1.f };
}

static inline ImU32 LerpCol(ImVec4 A, ImVec4 B, float T)
{
	return IM_COL32(
		(int)((A.x + (B.x - A.x) * T) * 255),
		(int)((A.y + (B.y - A.y) * T) * 255),
		(int)((A.z + (B.z - A.z) * T) * 255),
		(int)((A.w + (B.w - A.w) * T) * 255)
	);
}

static void SectionHeader(const char* Label)
{
	ImDrawList* Dl = ImGui::GetWindowDrawList();
	ImVec2 P  = ImGui::GetCursorScreenPos();
	float  W  = ImGui::GetContentRegionAvail().x;

	Dl->AddRectFilled(ImVec2(P.x, P.y + 2), ImVec2(P.x + 3, P.y + 14),
		IM_COL32(4, 217, 250, 255), 2.f);
	Dl->AddRectFilledMultiColor(
		ImVec2(P.x + 6, P.y), ImVec2(P.x + W, P.y + 20),
		IM_COL32(4, 217, 250, 28), IM_COL32(4, 217, 250, 0),
		IM_COL32(4, 217, 250, 0), IM_COL32(4, 217, 250, 28));

	ImGui::SetCursorScreenPos(ImVec2(P.x + 10, P.y + 2));
	ImGui::PushStyleColor(ImGuiCol_Text, EchoTheme::Accent);
	ImGui::TextUnformatted(Label);
	ImGui::PopStyleColor();
	ImGui::Spacing();
}

static bool Toggle(const char* Label, bool* V)
{
	ImDrawList* Dl      = ImGui::GetWindowDrawList();
	ImVec2      Pos     = ImGui::GetCursorScreenPos();
	float H = 16.f, W = 30.f, R = H * 0.5f;

	ImGui::InvisibleButton(Label, ImVec2(W, H));
	bool Clicked = ImGui::IsItemClicked();
	if (Clicked) *V = !*V;

	static std::unordered_map<const char*, float> Anims;
	float& Anim = Anims[Label];
	Anim += ((*V ? 1.f : 0.f) - Anim) * 0.25f;

	ImU32 Track = LerpCol(EchoTheme::BgHover, EchoTheme::AccentDim, Anim);
	ImU32 Rim   = LerpCol(EchoTheme::Border,  EchoTheme::Accent,    Anim);
	ImU32 Knob  = LerpCol({ 0.45f, 0.50f, 0.60f, 1.f }, EchoTheme::Accent, Anim);
	float KnobX = Pos.x + R + Anim * (W - H);

	Dl->AddRectFilled(Pos, ImVec2(Pos.x + W, Pos.y + H), Track, R);
	Dl->AddRect(Pos, ImVec2(Pos.x + W, Pos.y + H), Rim, R, 0, 1.2f);
	Dl->AddCircleFilled(ImVec2(KnobX, Pos.y + H * 0.5f), H * 0.38f, Knob);
	if (*V)
		Dl->AddCircle(ImVec2(KnobX, Pos.y + H * 0.5f), H * 0.38f,
			IM_COL32(4, 217, 250, 180), 0, 1.f);

	ImGui::SameLine(0, 8);
	ImGui::PushStyleColor(ImGuiCol_Text, *V ? EchoTheme::TextPrimary : EchoTheme::TextDim);
	ImGui::TextUnformatted(Label);
	ImGui::PopStyleColor();
	return Clicked;
}

static void NeonSlider(const char* Label, float* V, float Mn, float Mx, const char* Fmt = "%.1f")
{
	ImGui::PushStyleColor(ImGuiCol_FrameBg,          EchoTheme::BgMid);
	ImGui::PushStyleColor(ImGuiCol_SliderGrab,       EchoTheme::Accent);
	ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, { 0.20f, 0.95f, 1.f, 1.f });
	ImGui::PushStyleColor(ImGuiCol_Text,             EchoTheme::TextDim);
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::SliderFloat(Label, V, Mn, Mx, Fmt);
	ImGui::PopStyleColor(4);
}

static void NeonCombo(const char* Label, int* Cur, const char* Items)
{
	ImGui::PushStyleColor(ImGuiCol_FrameBg,       EchoTheme::BgMid);
	ImGui::PushStyleColor(ImGuiCol_Header,        EchoTheme::AccentDim);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EchoTheme::AccentMid);
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::Combo(Label, Cur, Items);
	ImGui::PopStyleColor(3);
}

static bool TabButton(const char* Label, int Idx, int& Active, float Width)
{
	bool IsActive = (Idx == Active);
	ImDrawList* Dl  = ImGui::GetWindowDrawList();
	ImVec2      Pos = ImGui::GetCursorScreenPos();
	float H = 32.f;

	ImGui::InvisibleButton(Label, ImVec2(Width, H));
	bool Clicked = ImGui::IsItemClicked();
	bool Hovered = ImGui::IsItemHovered();
	if (Clicked) Active = Idx;

	ImU32 BgCol = IsActive ? IM_COL32(4, 217, 250, 30)
		        : Hovered  ? IM_COL32(255, 255, 255, 8)
		        :             IM_COL32(0, 0, 0, 0);
	Dl->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + H), BgCol, 6.f);

	if (IsActive)
		Dl->AddRectFilled(
			ImVec2(Pos.x + 4, Pos.y + H - 2),
			ImVec2(Pos.x + Width - 4, Pos.y + H),
			IM_COL32(4, 217, 250, 220), 1.f);

	ImVec2 TxSz  = ImGui::CalcTextSize(Label);
	ImVec2 TxPos = { Pos.x + (Width - TxSz.x) * 0.5f, Pos.y + (H - TxSz.y) * 0.5f };
	ImU32  TxCol = IsActive ? IM_COL32(4, 217, 250, 255)
		         : Hovered  ? IM_COL32(200, 210, 220, 255)
		         :             IM_COL32(100, 110, 130, 255);
	Dl->AddText(TxPos, TxCol, Label);
	return Clicked;
}

void DrawMenu(CheatOverlay* Overlay)
{
	if (GetAsyncKeyState(VK_INSERT) & 1)
	{
		Settings.MenuOpen = !Settings.MenuOpen;
		Overlay->SetMenuState(Settings.MenuOpen);
	}
	if (!Settings.MenuOpen) return;

	PulseTimer += 0.016f;
	if (PulseTimer > 6.2832f) PulseTimer -= 6.2832f;
	float Pulse = 0.5f + 0.5f * sinf(PulseTimer * 1.4f);

	ApplyTheme();

	ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(120, 120), ImGuiCond_FirstUseEver);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(8, 7));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, EchoTheme::BgDeep);
	ImGui::PushStyleColor(ImGuiCol_Border,   { 0.04f, 0.85f, 0.98f, 0.25f + Pulse * 0.15f });

	ImGui::Begin("##EchoMenu", nullptr,
		ImGuiWindowFlags_NoTitleBar    |
		ImGuiWindowFlags_NoResize      |
		ImGuiWindowFlags_NoScrollbar   |
		ImGuiWindowFlags_NoScrollWithMouse);

	ImDrawList* Dl   = ImGui::GetWindowDrawList();
	ImVec2      WPos = ImGui::GetWindowPos();
	ImVec2      WSz  = ImGui::GetWindowSize();

	// header
	{
		float Bh = 58.f;
		Dl->AddRectFilledMultiColor(
			WPos, ImVec2(WPos.x + WSz.x, WPos.y + Bh),
			IM_COL32(6,6,15,255), IM_COL32(10,8,22,255),
			IM_COL32(12,10,26,255), IM_COL32(6,6,15,255));

		// scan line animation
		float ScanY = WPos.y + fmodf(PulseTimer * 18.f, Bh);
		Dl->AddLine(ImVec2(WPos.x, ScanY), ImVec2(WPos.x + WSz.x, ScanY),
			IM_COL32(4, 217, 250, (int)(30 * Pulse)), 1.f);

		// corner brackets
		float Ca = 12.f;
		ImU32 CCol = IM_COL32(4, 217, 250, (int)(180 + 60 * Pulse));
		Dl->AddLine(ImVec2(WPos.x + 1,       WPos.y + 1), ImVec2(WPos.x + Ca,      WPos.y + 1),  CCol, 1.5f);
		Dl->AddLine(ImVec2(WPos.x + 1,       WPos.y + 1), ImVec2(WPos.x + 1,       WPos.y + Ca), CCol, 1.5f);
		Dl->AddLine(ImVec2(WPos.x+WSz.x-Ca,  WPos.y + 1), ImVec2(WPos.x+WSz.x-1,  WPos.y + 1),  CCol, 1.5f);
		Dl->AddLine(ImVec2(WPos.x+WSz.x-1,   WPos.y + 1), ImVec2(WPos.x+WSz.x-1,  WPos.y + Ca), CCol, 1.5f);

		Dl->AddText(nullptr, 22.f, ImVec2(WPos.x + 14, WPos.y + 9),
			IM_COL32(4, 217, 250, 255), "ECHO");
		Dl->AddText(ImVec2(WPos.x + 15, WPos.y + 36),
			IM_COL32(100, 115, 145, 255), "FUTURISTIC MENU  v2.0");

		// status pill
		const char* StatusTxt = "ACTIVE";
		ImVec2 StSz = ImGui::CalcTextSize(StatusTxt);
		float  StX  = WPos.x + WSz.x - StSz.x - 24.f;
		float  StY  = WPos.y + (Bh - StSz.y) * 0.5f;
		Dl->AddRectFilled({ StX-8, StY-3 }, { StX+StSz.x+8, StY+StSz.y+3 },
			IM_COL32(4, 217, 250, (int)(30 + 20 * Pulse)), 6.f);
		Dl->AddRect({ StX-8, StY-3 }, { StX+StSz.x+8, StY+StSz.y+3 },
			IM_COL32(4, 217, 250, (int)(160 + 70 * Pulse)), 6.f, 0, 1.f);
		Dl->AddText({ StX, StY }, IM_COL32(4, 217, 250, 255), StatusTxt);

		Dl->AddLine(ImVec2(WPos.x, WPos.y + Bh), ImVec2(WPos.x + WSz.x, WPos.y + Bh),
			IM_COL32(4, 217, 250, 60), 1.f);

		ImGui::Dummy(ImVec2(WSz.x, Bh));
	}

	// tab bar
	{
		const char* Tabs[] = { "ESP", "AIMBOT", "VISUALS", "MISC" };
		int   NTabs  = 4;
		float TabW   = WSz.x / (float)NTabs;
		float TabBarH = 36.f;

		ImVec2 TbPos = ImGui::GetCursorScreenPos();
		Dl->AddRectFilled(TbPos, ImVec2(TbPos.x + WSz.x, TbPos.y + TabBarH),
			IM_COL32(7, 7, 14, 255));

		for (int I = 0; I < NTabs; I++)
		{
			ImGui::SetCursorScreenPos(ImVec2(TbPos.x + I * TabW, TbPos.y));
			TabButton(Tabs[I], I, ActiveTab, TabW);
			if (I < NTabs - 1)
				Dl->AddLine(
					ImVec2(TbPos.x + (I+1)*TabW, TbPos.y + 6),
					ImVec2(TbPos.x + (I+1)*TabW, TbPos.y + TabBarH - 6),
					IM_COL32(4, 217, 250, 20), 1.f);
		}

		Dl->AddLine(ImVec2(TbPos.x, TbPos.y + TabBarH),
			ImVec2(TbPos.x + WSz.x, TbPos.y + TabBarH),
			IM_COL32(4, 217, 250, 45), 1.f);

		ImGui::Dummy(ImVec2(WSz.x, TabBarH));
	}

	// content
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, EchoTheme::BgDeep);
	ImGui::BeginChild("##content", ImVec2(0, 380), false, ImGuiWindowFlags_NoScrollbar);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::Spacing();

	if (ActiveTab == 0)
	{
		SectionHeader("PLAYER ESP");
		ImGui::Columns(2, "EspCols", false);
		ImGui::SetColumnWidth(0, 230);
		Toggle("Box ESP",       &Settings.Box);
		Toggle("Skeleton",      &Settings.Skelton);
		Toggle("Health Bar",    &Settings.Health);
		Toggle("Shield Bar",    &Settings.Shield);
		ImGui::NextColumn();
		Toggle("Name ESP",      &Settings.Name);
		Toggle("Distance",      &Settings.Distance);
		Toggle("Team Number",   &Settings.Team);
		Toggle("Item Distance", &Settings.ItemDist);
		ImGui::Columns(1);
		ImGui::Spacing();

		ImGui::PushStyleColor(ImGuiCol_Text, EchoTheme::TextDim);
		ImGui::TextUnformatted("Box Style");
		ImGui::PopStyleColor();
		NeonCombo("##BoxType", (int*)&Settings.BoxType, "Full\0Cornered\0");
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, EchoTheme::TextDim);
		ImGui::TextUnformatted("Max Detection Range");
		ImGui::PopStyleColor();
		NeonSlider("##MaxDist", &Settings.MaxDistance, 10.f, 2000.f, "%.0f M");
		ImGui::Spacing();

		SectionHeader("LOOT ESP");
		Toggle("Item Names",    &Settings.ItemName);
		Toggle("Loot Distance", &Settings.ItemDist);
	}

	if (ActiveTab == 1)
	{
		SectionHeader("AIMBOT");
		ImGui::Columns(2, "AimCols", false);
		ImGui::SetColumnWidth(0, 230);
		Toggle("Enable Aimbot", &Settings.Aimbot);
		Toggle("Visible Only",  &Settings.AimVisibleOnly);
		ImGui::NextColumn();
		Toggle("Recoil Comp",   &Settings.RecoilComp);
		Toggle("Sway Comp",     &Settings.SwayComp);
		ImGui::Columns(1);
		ImGui::Spacing();

		// fov circle preview
		{
			ImVec2 PPos = ImGui::GetCursorScreenPos();
			float  PSz  = 80.f;
			float  Cx   = PPos.x + PSz * 0.5f;
			float  Cy   = PPos.y + PSz * 0.5f;
			float  Rad  = (Settings.AimFOV / 180.f) * (PSz * 0.45f);

			Dl->AddCircleFilled({ Cx, Cy }, PSz * 0.5f, IM_COL32(7,7,14,200), 48);
			Dl->AddCircle({ Cx, Cy }, PSz * 0.5f, IM_COL32(4,217,250,30), 48, 1.f);
			Dl->AddCircle({ Cx, Cy }, Rad, IM_COL32(4,217,250,(int)(180+60*Pulse)), 48, 1.5f);
			Dl->AddLine({ Cx-5, Cy }, { Cx+5, Cy }, IM_COL32(4,217,250,200), 1.2f);
			Dl->AddLine({ Cx, Cy-5 }, { Cx, Cy+5 }, IM_COL32(4,217,250,200), 1.2f);

			ImGui::Dummy(ImVec2(PSz, PSz));
			ImGui::SameLine(PSz + 14);
			ImGui::BeginGroup();
			ImGui::PushStyleColor(ImGuiCol_Text, EchoTheme::TextDim);
			ImGui::TextUnformatted("Aim FOV");
			ImGui::PopStyleColor();
			NeonSlider("##AimFov", &Settings.AimFOV, 1.f, 180.f, "%.0f deg");
			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, EchoTheme::TextDim);
			ImGui::TextUnformatted("Smoothness");
			ImGui::PopStyleColor();
			NeonSlider("##AimSmooth", &Settings.AimSmooth, 1.f, 20.f, "%.1f");
			ImGui::EndGroup();
			ImGui::Spacing();
		}
	}

	if (ActiveTab == 2)
	{
		SectionHeader("GLOW & CHAMS");
		ImGui::Columns(2, "VisCols", false);
		ImGui::SetColumnWidth(0, 230);
		Toggle("Player Glow", &Settings.Glow);
		ImGui::NextColumn();
		Toggle("Loot Chams",  &Settings.LootChams);
		ImGui::Columns(1);
		ImGui::Spacing();

		// decorative grid
		{
			ImVec2 GPos = ImGui::GetCursorScreenPos();
			float  Gw   = ImGui::GetContentRegionAvail().x;
			float  Gh   = 80.f;
			int    Cols = 12, Rows = 5;
			ImU32  GCol = IM_COL32(4, 217, 250, 12);
			ImU32  DCol = IM_COL32(4, 217, 250, (int)(40 + 30 * Pulse));

			for (int R = 0; R <= Rows; R++)
				Dl->AddLine({ GPos.x, GPos.y + R*(Gh/Rows) }, { GPos.x+Gw, GPos.y+R*(Gh/Rows) }, GCol, 0.5f);
			for (int C = 0; C <= Cols; C++)
				Dl->AddLine({ GPos.x + C*(Gw/Cols), GPos.y }, { GPos.x+C*(Gw/Cols), GPos.y+Gh }, GCol, 0.5f);

			float DotX = GPos.x + fmodf(PulseTimer * 28.f, Gw);
			Dl->AddCircleFilled({ DotX, GPos.y + Gh * 0.5f }, 3.f, DCol);
			ImGui::Dummy(ImVec2(Gw, Gh));
		}

		ImGui::Spacing();
		SectionHeader("INFO");
		ImGui::PushStyleColor(ImGuiCol_Text, EchoTheme::TextDim);
		ImGui::TextWrapped("Glow highlights players through walls. Loot Chams lights up nearby boxes. Use what you need.");
		ImGui::PopStyleColor();
	}

	if (ActiveTab == 3)
	{
		SectionHeader("CONTROLLER");
		Toggle("Controller Support", &Settings.ControllerEnabled);
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, EchoTheme::TextDim);
		ImGui::TextUnformatted("Controller Sensitivity");
		ImGui::PopStyleColor();
		NeonSlider("##CSens", &Settings.ControllerSensitivity, 0.1f, 5.f, "%.2f");
		ImGui::Spacing();

		SectionHeader("ABOUT");

		// about card
		{
			ImVec2 CardPos = ImGui::GetCursorScreenPos();
			float  Cw = ImGui::GetContentRegionAvail().x;
			float  Ch = 108.f;

			Dl->AddRectFilled(CardPos, { CardPos.x+Cw, CardPos.y+Ch }, IM_COL32(9,9,18,220), 8.f);
			Dl->AddRect(CardPos, { CardPos.x+Cw, CardPos.y+Ch },
				IM_COL32(4, 217, 250, (int)(40 + 25*Pulse)), 8.f, 0, 1.f);

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
			ImGui::BeginChild("##AboutCard", ImVec2(Cw, Ch), false, ImGuiWindowFlags_NoScrollbar);
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();

			ImGui::PushStyleColor(ImGuiCol_Text, EchoTheme::Accent);
			ImGui::TextUnformatted("ECHO  |  Futuristic Menu");
			ImGui::PopStyleColor();
			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, EchoTheme::TextDim);
			ImGui::TextUnformatted("Version  2.0  |  DX11 + ImGui");
			ImGui::Spacing();
			ImGui::TextUnformatted("Press  INSERT  to toggle");
			ImGui::PopStyleColor();

			ImGui::EndChild();
		}
	}

	ImGui::EndChild();

	// footer
	{
		ImVec2 FPos = ImGui::GetCursorScreenPos();
		float  Fh   = 28.f;
		Dl->AddRectFilled(FPos, { FPos.x+WSz.x, FPos.y+Fh }, IM_COL32(5,5,12,255));
		Dl->AddLine(FPos, { FPos.x+WSz.x, FPos.y }, IM_COL32(4,217,250,35), 1.f);

		float  Ca   = 12.f;
		ImU32  CCol = IM_COL32(4, 217, 250, (int)(100 + 50 * Pulse));
		Dl->AddLine({ FPos.x+1,        FPos.y+Fh-1 }, { FPos.x+Ca,        FPos.y+Fh-1 }, CCol, 1.5f);
		Dl->AddLine({ FPos.x+1,        FPos.y+Fh-Ca}, { FPos.x+1,         FPos.y+Fh-1 }, CCol, 1.5f);
		Dl->AddLine({ FPos.x+WSz.x-Ca, FPos.y+Fh-1 }, { FPos.x+WSz.x-1,   FPos.y+Fh-1 }, CCol, 1.5f);
		Dl->AddLine({ FPos.x+WSz.x-1,  FPos.y+Fh-Ca}, { FPos.x+WSz.x-1,   FPos.y+Fh-1 }, CCol, 1.5f);

		const char* FTxt = "INSERT  to toggle  |  ECHO v2.0";
		ImVec2 FTxtSz = ImGui::CalcTextSize(FTxt);
		Dl->AddText(
			{ FPos.x + (WSz.x - FTxtSz.x) * 0.5f, FPos.y + (Fh - FTxtSz.y) * 0.5f },
			IM_COL32(60, 70, 90, 255), FTxt);

		ImGui::Dummy(ImVec2(WSz.x, Fh));
	}

	ImGui::End();
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(2);

	// custom neon cursor while menu is up
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_None);
		ImVec2 Mp = ImGui::GetIO().MousePos;
		if (Mp.x >= 0 && Mp.y >= 0)
		{
			ImDrawList* Fdl = ImGui::GetForegroundDrawList();
			float OuterR = 7.f + 1.5f * Pulse;
			Fdl->AddCircle(Mp, OuterR, IM_COL32(4, 217, 250, (int)(160 + 80*Pulse)), 32, 1.2f);
			Fdl->AddCircleFilled(Mp, 2.5f, IM_COL32(4, 217, 250, (int)(220 + 35*Pulse)), 16);

			float Gap = 4.f, Arm = 7.f, Th = 1.5f;
			ImU32 ArmCol = IM_COL32(4, 217, 250, (int)(130 + 80*Pulse));
			Fdl->AddLine({ Mp.x, Mp.y-Gap },     { Mp.x, Mp.y-Gap-Arm },   ArmCol, Th);
			Fdl->AddLine({ Mp.x, Mp.y+Gap },     { Mp.x, Mp.y+Gap+Arm },   ArmCol, Th);
			Fdl->AddLine({ Mp.x-Gap, Mp.y },     { Mp.x-Gap-Arm, Mp.y },   ArmCol, Th);
			Fdl->AddLine({ Mp.x+Gap, Mp.y },     { Mp.x+Gap+Arm, Mp.y },   ArmCol, Th);

			float D = 5.f, Ds = 2.5f;
			ImU32 TkCol = IM_COL32(4, 217, 250, (int)(70 + 40*Pulse));
			Fdl->AddLine({ Mp.x+Ds, Mp.y-Ds }, { Mp.x+D, Mp.y-D }, TkCol, 1.f);
			Fdl->AddLine({ Mp.x-Ds, Mp.y-Ds }, { Mp.x-D, Mp.y-D }, TkCol, 1.f);
			Fdl->AddLine({ Mp.x+Ds, Mp.y+Ds }, { Mp.x+D, Mp.y+D }, TkCol, 1.f);
			Fdl->AddLine({ Mp.x-Ds, Mp.y+Ds }, { Mp.x-D, Mp.y+D }, TkCol, 1.f);
		}
	}
}

static void InitializeEntryPoint(CheatOverlay* Overlay)
{
	HANDLE Con = GetStdHandle(STD_OUTPUT_HANDLE);

	SetColor(Con, CYAN);
	std::cout << skCrypt(" +------------------------------------------+") << std::endl;
	std::cout << skCrypt(" |          E C H O   A P E X               |") << std::endl;
	std::cout << skCrypt(" |          [Futuristic Framework]          |") << std::endl;
	std::cout << skCrypt(" +------------------------------------------+") << std::endl << std::endl;

	SetColor(Con, WHITE);
	std::cout << skCrypt("[>] Initializing low-level drivers... ") << std::flush;
	if (!Driver::SetupDriver())
	{
		SetColor(Con, RED);
		std::cout << skCrypt("FAILED") << std::endl;
		std::cerr << skCrypt("[-] Setup driver returned false.") << std::endl;
		Sleep(3000);
		return;
	}
	SetColor(Con, GREEN);
	std::cout << skCrypt("SUCCESS") << std::endl;

	SetColor(Con, WHITE);
	std::cout << skCrypt("[>] Searching for active process...   ") << std::flush;
	Driver::ProcessID = Driver::FindProcess("r5apex_dx12.exe");
	if (!Driver::ProcessID)
	{
		SetColor(Con, RED);
		std::cout << skCrypt("NOT FOUND") << std::endl;
		std::cerr << skCrypt("[-] Apex Legends (r5apex_dx12.exe) is not running.") << std::endl;
		Sleep(3000);
		return;
	}
	SetColor(Con, GREEN);
	std::cout << skCrypt("FOUND (PID: ") << std::dec << Driver::ProcessID << skCrypt(")") << std::endl;

	SetColor(Con, WHITE);
	std::cout << skCrypt("[>] Securing memory environment...     ") << std::flush;
	if (!Driver::BypassCR3())
	{
		SetColor(Con, YELLOW);
		std::cout << skCrypt("WARNING") << std::endl;
		std::cout << skCrypt("    CR3 bypass not loaded. Attempting default memory access.") << std::endl;
	}
	else
	{
		SetColor(Con, GREEN);
		std::cout << skCrypt("OK") << std::endl;
	}

	SetColor(Con, WHITE);
	std::cout << skCrypt("[>] Resolving virtual base module...  ") << std::flush;
	Driver::GetBase();
	if (!Kernel.GetBaseAddress())
	{
		SetColor(Con, RED);
		std::cout << skCrypt("FAILED") << std::endl;
		std::cerr << skCrypt("[-] Could not retrieve virtual module base address.") << std::endl;
		Sleep(5000);
		exit(1);
	}
	GBase = Kernel.GetBaseAddress();
	SetColor(Con, GREEN);
	std::cout << skCrypt("SUCCESS (0x") << std::hex << GBase << std::dec << skCrypt(")") << std::endl;

	NameList = Kernel.GetBaseAddress() + OFFSET_NAMELIST;

	SetColor(Con, WHITE);
	std::cout << skCrypt("[>] Detaching background routines...  ") << std::flush;
	std::thread(SetupLoot).detach();
	std::thread(SetupPlayers).detach();
	std::thread(UpdateManager).detach();
	SetColor(Con, GREEN);
	std::cout << skCrypt("OK") << std::endl;

	SetColor(Con, WHITE);
	std::cout << skCrypt("[>] Setting up render pipeline...     ") << std::flush;
	if (!Overlay->Init())
	{
		SetColor(Con, RED);
		std::cout << skCrypt("WINDOW FAILED") << std::endl;
		Sleep(2000);
		return;
	}
	if (!Overlay->InitD3D())
	{
		SetColor(Con, RED);
		std::cout << skCrypt("D3D FAILED") << std::endl;
		Sleep(2000);
		return;
	}
	if (!Overlay->InitImGui())
	{
		SetColor(Con, RED);
		std::cout << skCrypt("IMGUI FAILED") << std::endl;
		Sleep(2000);
		return;
	}
	SetColor(Con, GREEN);
	std::cout << skCrypt("SUCCESS") << std::endl;

	SetColor(Con, GREEN);
	std::cout << std::endl << skCrypt("[+] All systems running. Overlay active. Press INSERT to open menu.") << std::endl << std::endl;
	SetColor(Con, WHITE);

	while (true)
	{
		MSG Msg = { 0 };
		if (PeekMessage(&Msg, Overlay->GetWindow(), 0, 0, PM_REMOVE))
		{
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}

		Overlay->BeginScene();
		Overlay->ClearScene();

		Overlay->NewFrame();
		DrawMenu(Overlay);
		DrawESP(Overlay);
		Overlay->Render();

		Overlay->EndScene();

		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}

	Overlay->ShutdownImGui();
	Overlay->ShutdownD3D();
}

int main()
{
	CheatOverlay OverlayInstance;
	InitializeEntryPoint(&OverlayInstance);
}