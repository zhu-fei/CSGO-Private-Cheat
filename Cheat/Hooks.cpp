/*
Syn's AyyWare Framework 2015
*/

#include "Hooks.h"
#include "Hacks.h"
#include "Chams.h"
#include "Menu.h"

#include <intrin.h>

#include "Interfaces.h"
#include "RenderManager.h"
#include "MiscHacks.h"
#include "CRC32.h"
#include "Resolver.h"
#include <intrin.h>
Vector LastAngleAA;

// Funtion Typedefs
typedef void(__thiscall* DrawModelEx_)(void*, void*, void*, const ModelRenderInfo_t&, matrix3x4*);
typedef void(__thiscall* PaintTraverse_)(PVOID, unsigned int, bool, bool);
typedef bool(__thiscall* InPrediction_)(PVOID);
typedef void(__stdcall *FrameStageNotifyFn)(ClientFrameStage_t);
typedef void(__thiscall* RenderViewFn)(void*, CViewSetup&, CViewSetup&, int, int);

using OverrideViewFn = void(__fastcall*)(void*, void*, CViewSetup*);
typedef float(__stdcall *oGetViewModelFOV)();


// Function Pointers to the originals
PaintTraverse_ oPaintTraverse;
DrawModelEx_ oDrawModelExecute;
FrameStageNotifyFn oFrameStageNotify;
OverrideViewFn oOverrideView;
RenderViewFn oRenderView;

// Hook function prototypes
void __fastcall PaintTraverse_Hooked(PVOID pPanels, int edx, unsigned int vguiPanel, bool forceRepaint, bool allowForce);
bool __stdcall Hooked_InPrediction();
void __fastcall Hooked_DrawModelExecute(void* thisptr, int edx, void* ctx, void* state, const ModelRenderInfo_t &pInfo, matrix3x4 *pCustomBoneToWorld);
bool __stdcall CreateMoveClient_Hooked(/*void* self, int edx,*/ float frametime, CUserCmd* pCmd);
void  __stdcall Hooked_FrameStageNotify(ClientFrameStage_t curStage);
void __fastcall Hooked_OverrideView(void* ecx, void* edx, CViewSetup* pSetup);
float __stdcall GGetViewModelFOV();
void __fastcall Hooked_RenderView(void* ecx, void* edx, CViewSetup &setup, CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw);

// VMT Managers
namespace Hooks
{
	// VMT Managers
	Utilities::Memory::VMTManager VMTPanel; // Hooking drawing functions
	Utilities::Memory::VMTManager VMTClient; // Maybe CreateMove
	Utilities::Memory::VMTManager VMTClientMode; // CreateMove for functionality
	Utilities::Memory::VMTManager VMTModelRender; // DrawModelEx for chams
	Utilities::Memory::VMTManager VMTPrediction; // InPrediction for no vis recoil
	Utilities::Memory::VMTManager VMTPlaySound; // Autoaccept 
	Utilities::Memory::VMTManager VMTRenderView;
};

/*// Initialise all our hooks
void Hooks::Initialise()
{
// Panel hooks for drawing to the screen via surface functions
VMTPanel.Initialise((DWORD*)Interfaces::Panels);
oPaintTraverse = (PaintTraverse_)VMTPanel.HookMethod((DWORD)&PaintTraverse_Hooked, Offsets::VMT::Panel_PaintTraverse);
//Utilities::Log("Paint Traverse Hooked");

// No Visual Recoil
VMTPrediction.Initialise((DWORD*)Interfaces::Prediction);
VMTPrediction.HookMethod((DWORD)&Hooked_InPrediction, 14);
//Utilities::Log("InPrediction Hooked");

// Chams
VMTModelRender.Initialise((DWORD*)Interfaces::ModelRender);
oDrawModelExecute = (DrawModelEx_)VMTModelRender.HookMethod((DWORD)&Hooked_DrawModelExecute, Offsets::VMT::ModelRender_DrawModelExecute);
//Utilities::Log("DrawModelExecute Hooked");

// Setup ClientMode Hooks
//VMTClientMode.Initialise((DWORD*)Interfaces::ClientMode);
//VMTClientMode.HookMethod((DWORD)&CreateMoveClient_Hooked, 24);
//Utilities::Log("ClientMode CreateMove Hooked");

// Setup client hooks
VMTClient.Initialise((DWORD*)Interfaces::Client);
oCreateMove = (CreateMoveFn)VMTClient.HookMethod((DWORD)&hkCreateMove, 21);
}*/

// Undo our hooks
void Hooks::UndoHooks()
{
	VMTPanel.RestoreOriginal();
	VMTPrediction.RestoreOriginal();
	VMTModelRender.RestoreOriginal();
	VMTClientMode.RestoreOriginal();
}


// Initialise all our hooks
void Hooks::Initialise()
{
	// Panel hooks for drawing to the screen via surface functions
	VMTPanel.Initialise((DWORD*)Interfaces::Panels);
	oPaintTraverse = (PaintTraverse_)VMTPanel.HookMethod((DWORD)&PaintTraverse_Hooked, Offsets::VMT::Panel_PaintTraverse);
	//Utilities::Log("Paint Traverse Hooked");

	// No Visual Recoi	l
	VMTPrediction.Initialise((DWORD*)Interfaces::Prediction);
	VMTPrediction.HookMethod((DWORD)&Hooked_InPrediction, 14);
	//Utilities::Log("InPrediction Hooked");

	// Chams
	VMTModelRender.Initialise((DWORD*)Interfaces::ModelRender);
	oDrawModelExecute = (DrawModelEx_)VMTModelRender.HookMethod((DWORD)&Hooked_DrawModelExecute, Offsets::VMT::ModelRender_DrawModelExecute);
	//Utilities::Log("DrawModelExecute Hooked");

	// Setup ClientMode Hooks
	VMTClientMode.Initialise((DWORD*)Interfaces::ClientMode);
	VMTClientMode.HookMethod((DWORD)CreateMoveClient_Hooked, 24);

	oOverrideView = (OverrideViewFn)VMTClientMode.HookMethod((DWORD)&Hooked_OverrideView, 18);
	VMTClientMode.HookMethod((DWORD)&GGetViewModelFOV, 35);

	// Setup client hooks
	VMTClient.Initialise((DWORD*)Interfaces::Client);
	oFrameStageNotify = (FrameStageNotifyFn)VMTClient.HookMethod((DWORD)&Hooked_FrameStageNotify, 36);

}

void MovementCorrection(CUserCmd* pCmd)
{

}

//---------------------------------------------------------------------------------------------------------
//                                         Hooked Functions
//---------------------------------------------------------------------------------------------------------

void SetClanTag(const char* tag, const char* name)//190% paste
{
	static auto pSetClanTag = reinterpret_cast<void(__fastcall*)(const char*, const char*)>(((DWORD)Utilities::Memory::FindPattern("engine.dll", (PBYTE)"\x53\x56\x57\x8B\xDA\x8B\xF9\xFF\x15\x00\x00\x00\x00\x6A\x24\x8B\xC8\x8B\x30", "xxxxxxxxx????xxxxxx")));
	pSetClanTag(tag, name);
}
void NoClantag()
{
	SetClanTag("", "");
}

void ClanTag()
{
	static int counter = 0;
	switch (Menu::Window.MiscTab.OtherClantag.GetIndex())
	{
	case 0:
		// No 
		break;
	case 1:
	{
		static int motion = 0;
		int ServerTime = (float)Interfaces::Globals->interval_per_tick * hackManager.pLocal()->GetTickBase() * 2.5;

		if (counter % 48 == 0)
			motion++;
		int value = ServerTime % 19;
		switch (value) {
		case 0:SetClanTag("          ", "pasteware"); break;
		case 1:SetClanTag("         p", "pasteware"); break;
		case 2:SetClanTag("        pa", "pasteware"); break;
		case 3:SetClanTag("       pas", "pasteware"); break;
		case 4:SetClanTag("      past", "pasteware"); break;
		case 5:SetClanTag("     paste", "pasteware"); break;
		case 6:SetClanTag("    pastew", "pasteware"); break;
		case 7:SetClanTag("   pastewa", "pasteware"); break;
		case 8:SetClanTag("  pastewar", "pasteware"); break;
		case 9:SetClanTag(" pasteware", "pasteware"); break;
		case 10:SetClanTag("pasteware ", "pasteware"); break;
		case 11:SetClanTag("asteware  ", "pasteware"); break;
		case 12:SetClanTag("steware   ", "pasteware"); break;
		case 13:SetClanTag("teware    ", "pasteware"); break;
		case 14:SetClanTag("eware     ", "pasteware"); break;
		case 15:SetClanTag("ware      ", "pasteware"); break;
		case 16:SetClanTag("are       ", "pasteware"); break;
		case 17:SetClanTag("re        ", "pasteware"); break;
		case 18:SetClanTag("e         ", "pasteware"); break;
		case 19:SetClanTag("          ", "pasteware"); break;
		}
		counter++;
	}
	break;
	case 2:
	{
		static int motion = 0;
		int ServerTime = (float)Interfaces::Globals->interval_per_tick * hackManager.pLocal()->GetTickBase() * 3;

		if (counter % 48 == 0)
			motion++;
		int value = ServerTime % 17;
		switch (value) {
		case 0:SetClanTag("          ", "skeet.cc"); break;
		case 1:SetClanTag("         s", "skeet.cc"); break;
		case 2:SetClanTag("        sk", "skeet.cc"); break;
		case 3:SetClanTag("       ske", "skeet.cc"); break;
		case 4:SetClanTag("      skee", "skeet.cc"); break;
		case 5:SetClanTag("     skeet", "skeet.cc"); break;
		case 6:SetClanTag("    skeet.", "skeet.cc"); break;
		case 7:SetClanTag("   skeet.c", "skeet.cc"); break;
		case 8:SetClanTag(" skeet.cc", "skeet.cc"); break;
		case 9:SetClanTag("skeet.cc ", "skeet.cc"); break;
		case 10:SetClanTag("keet.cc  ", "skeet.cc"); break;
		case 11:SetClanTag("eet.cc   ", "skeet.cc"); break;
		case 12:SetClanTag("et.cc    ", "skeet.cc"); break;
		case 13:SetClanTag("t.cc     ", "skeet.cc"); break;
		case 14:SetClanTag(".cc      ", "skeet.cc"); break;
		case 15:SetClanTag("cc       ", "skeet.cc"); break;
		case 16:SetClanTag("c        ", "skeet.cc"); break;
		case 17:SetClanTag("         ", "skeet.cc"); break;
		}
		counter++;
	}
	break;
	case 3:
		// stainless
		SetClanTag("\r", "\r");
		break;
	case 4:
		SetClanTag("[VALV\xE1\xB4\xB1]", "Valve");
		break;
	}
}

bool __stdcall CreateMoveClient_Hooked(/*void* self, int edx,*/ float frametime, CUserCmd* pCmd)
{
	if (!pCmd->command_number)
		return true;

	if (Interfaces::Engine->IsConnected() && Interfaces::Engine->IsInGame())
	{

		PVOID pebp;
		__asm mov pebp, ebp;
		bool* pbSendPacket = (bool*)(*(DWORD*)pebp - 0x1C);
		bool& bSendPacket = *pbSendPacket;

		if (Menu::Window.MiscTab.OtherClantag.GetIndex() > 0)
			ClanTag();

		//	CUserCmd* cmdlist = *(CUserCmd**)((DWORD)Interfaces::pInput + 0xEC);
		//	CUserCmd* pCmd = &cmdlist[sequence_number % 150];


			// Backup for safety
		Vector origView = pCmd->viewangles;
		Vector viewforward, viewright, viewup, aimforward, aimright, aimup;
		Vector qAimAngles;
		qAimAngles.Init(0.0f, pCmd->viewangles.y, 0.0f);
		AngleVectors(qAimAngles, &viewforward, &viewright, &viewup);

		// Do da hacks
		IClientEntity *pLocal = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
		if (Interfaces::Engine->IsConnected() && Interfaces::Engine->IsInGame() && pLocal && pLocal->IsAlive())
			Hacks::MoveHacks(pCmd, bSendPacket);

		//Movement Fix
		//GameUtils::CL_FixMove(pCmd, origView);
		qAimAngles.Init(0.0f, GetAutostrafeView().y, 0.0f); // if pCmd->viewangles.x > 89, set pCmd->viewangles.x instead of 0.0f on first
		AngleVectors(qAimAngles, &viewforward, &viewright, &viewup);
		qAimAngles.Init(0.0f, pCmd->viewangles.y, 0.0f);
		AngleVectors(qAimAngles, &aimforward, &aimright, &aimup);
		Vector vForwardNorm;		Normalize(viewforward, vForwardNorm);
		Vector vRightNorm;			Normalize(viewright, vRightNorm);
		Vector vUpNorm;				Normalize(viewup, vUpNorm);

		// Original shit for movement correction
		float forward = pCmd->forwardmove;
		float right = pCmd->sidemove;
		float up = pCmd->upmove;
		if (forward > 450) forward = 450;
		if (right > 450) right = 450;
		if (up > 450) up = 450;
		if (forward < -450) forward = -450;
		if (right < -450) right = -450;
		if (up < -450) up = -450;
		pCmd->forwardmove = DotProduct(forward * vForwardNorm, aimforward) + DotProduct(right * vRightNorm, aimforward) + DotProduct(up * vUpNorm, aimforward);
		pCmd->sidemove = DotProduct(forward * vForwardNorm, aimright) + DotProduct(right * vRightNorm, aimright) + DotProduct(up * vUpNorm, aimright);
		pCmd->upmove = DotProduct(forward * vForwardNorm, aimup) + DotProduct(right * vRightNorm, aimup) + DotProduct(up * vUpNorm, aimup);

		// Angle normalisation
		if (Menu::Window.MiscTab.OtherSafeMode.GetState())
		{
			GameUtils::NormaliseViewAngle(pCmd->viewangles);

			if (pCmd->viewangles.z != 0.0f)
			{
				pCmd->viewangles.z = 0.00;
			}

			if (pCmd->viewangles.x < -89 || pCmd->viewangles.x > 89 || pCmd->viewangles.y < -180 || pCmd->viewangles.y > 180)
			{
				Utilities::Log("Having to re-normalise!");
				GameUtils::NormaliseViewAngle(pCmd->viewangles);
				Beep(750, 800); // Why does it do this
				if (pCmd->viewangles.x < -89 || pCmd->viewangles.x > 89 || pCmd->viewangles.y < -180 || pCmd->viewangles.y > 180)
				{
					pCmd->viewangles = origView;
					pCmd->sidemove = right;
					pCmd->forwardmove = forward;
				}
			}
		}

		if (pCmd->viewangles.x > 90)
		{
			pCmd->forwardmove = -pCmd->forwardmove;
		}

		if (pCmd->viewangles.x < -90)
		{
			pCmd->forwardmove = -pCmd->forwardmove;
		}

		if (bSendPacket)
			LastAngleAA = pCmd->viewangles;
	}

	return false;
}


// Paint Traverse Hooked function
void __fastcall PaintTraverse_Hooked(PVOID pPanels, int edx, unsigned int vguiPanel, bool forceRepaint, bool allowForce)
{
	oPaintTraverse(pPanels, vguiPanel, forceRepaint, allowForce);

	static unsigned int FocusOverlayPanel = 0;
	static bool FoundPanel = false;

	if (!FoundPanel)
	{
		PCHAR szPanelName = (PCHAR)Interfaces::Panels->GetName(vguiPanel);
		if (strstr(szPanelName, "MatSystemTopPanel"))
		{
			FocusOverlayPanel = vguiPanel;
			FoundPanel = true;
		}
	}
	else if (FocusOverlayPanel == vguiPanel)
	{
		//Render::GradientV(8, 8, 160, 18, Color(0, 0, 0, 0), Color(7, 39, 17, 255));
		Render::Text(10, 10, Color(255, 255, 255, 220), Render::Fonts::Menu, "INTERWEBZ : CSS");
		if (Interfaces::Engine->IsConnected() && Interfaces::Engine->IsInGame())
			Hacks::DrawHacks();

		// Update and draw the menu
		Menu::DoUIFrame();
	}
}

// InPrediction Hooked Function
bool __stdcall Hooked_InPrediction()
{
	bool result;
	static InPrediction_ origFunc = (InPrediction_)Hooks::VMTPrediction.GetOriginalFunction(14);
	static DWORD *ecxVal = Interfaces::Prediction;
	result = origFunc(ecxVal);

	// If we are in the right place where the player view is calculated
	// Calculate the change in the view and get rid of it
	if (Menu::Window.VisualsTab.OtherNoVisualRecoil.GetState() && (DWORD)(_ReturnAddress()) == Offsets::Functions::dwCalcPlayerView)
	{
		IClientEntity* pLocalEntity = NULL;

		float* m_LocalViewAngles = NULL;

		__asm
		{
			MOV pLocalEntity, ESI
			MOV m_LocalViewAngles, EBX
		}

		Vector viewPunch = pLocalEntity->localPlayerExclusive()->GetViewPunchAngle();
		Vector aimPunch = pLocalEntity->localPlayerExclusive()->GetAimPunchAngle();

		m_LocalViewAngles[0] -= (viewPunch[0] + (aimPunch[0] * 2 * 0.4499999f));
		m_LocalViewAngles[1] -= (viewPunch[1] + (aimPunch[1] * 2 * 0.4499999f));
		m_LocalViewAngles[2] -= (viewPunch[2] + (aimPunch[2] * 2 * 0.4499999f));
		return true;
	}

	return result;
}

// DrawModelExec for chams and shit
void __fastcall Hooked_DrawModelExecute(void* thisptr, int edx, void* ctx, void* state, const ModelRenderInfo_t &pInfo, matrix3x4 *pCustomBoneToWorld)
{
	Color color;
	float flColor[3] = { 0.f };
	static IMaterial* CoveredLit = CreateMaterial(true);
	static IMaterial* OpenLit = CreateMaterial(false);
	static IMaterial* CoveredFlat = CreateMaterial(true, false);
	static IMaterial* OpenFlat = CreateMaterial(false, false);
	bool DontDraw = false;

	const char* ModelName = Interfaces::ModelInfo->GetModelName((model_t*)pInfo.pModel);
	IClientEntity* pModelEntity = (IClientEntity*)Interfaces::EntList->GetClientEntity(pInfo.entity_index);
	IClientEntity* pLocal = (IClientEntity*)Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	if (Menu::Window.VisualsTab.Active.GetState())
	{
		// Player Chams
		int ChamsStyle = Menu::Window.VisualsTab.OptionsChams.GetIndex();
		int HandsStyle = Menu::Window.VisualsTab.OtherNoHands.GetIndex();
		if (ChamsStyle != 0 && Menu::Window.VisualsTab.FiltersPlayers.GetState() && strstr(ModelName, "models/player"))
		{
			if (pLocal/* && (!Menu::Window.VisualsTab.FiltersEnemiesOnly.GetState() || pModelEntity->GetTeamNum() != pLocal->GetTeamNum())*/)
			{
				IMaterial *covered = ChamsStyle == 1 ? CoveredLit : CoveredFlat;
				IMaterial *open = ChamsStyle == 1 ? OpenLit : OpenFlat;

				IClientEntity* pModelEntity = (IClientEntity*)Interfaces::EntList->GetClientEntity(pInfo.entity_index);
				if (pModelEntity)
				{
					IClientEntity *local = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
					if (local)
					{
						if (pModelEntity->IsAlive() && pModelEntity->GetHealth() > 0 /*&& pModelEntity->GetTeamNum() != local->GetTeamNum()*/)
						{
							float alpha = 1.f;

							if (pModelEntity->HasGunGameImmunity())
								alpha = 0.5f;

							if (pModelEntity->GetTeamNum() == 2)
							{
								flColor[0] = 240.f / 255.f;
								flColor[1] = 30.f / 255.f;
								flColor[2] = 35.f / 255.f;
							}
							else
							{
								flColor[0] = 63.f / 255.f;
								flColor[1] = 72.f / 255.f;
								flColor[2] = 205.f / 255.f;
							}

							Interfaces::RenderView->SetColorModulation(flColor);
							Interfaces::RenderView->SetBlend(alpha);
							Interfaces::ModelRender->ForcedMaterialOverride(covered);
							oDrawModelExecute(thisptr, ctx, state, pInfo, pCustomBoneToWorld);

							if (pModelEntity->GetTeamNum() == 2)
							{
								flColor[0] = 247.f / 255.f;
								flColor[1] = 180.f / 255.f;
								flColor[2] = 20.f / 255.f;
							}
							else
							{
								flColor[0] = 32.f / 255.f;
								flColor[1] = 180.f / 255.f;
								flColor[2] = 57.f / 255.f;
							}

							Interfaces::RenderView->SetColorModulation(flColor);
							Interfaces::RenderView->SetBlend(alpha);
							Interfaces::ModelRender->ForcedMaterialOverride(open);
						}
						else
						{
							color.SetColor(255, 255, 255, 255);
							ForceMaterial(color, open);
						}
					}
				}
			}
		}
		else if (HandsStyle != 0 && strstr(ModelName, "arms"))
		{
			if (HandsStyle == 1)
			{
				DontDraw = true;
			}
			else if (HandsStyle == 2)
			{
				Interfaces::RenderView->SetBlend(0.3);
			}
			else if (HandsStyle == 3)
			{
				IMaterial *covered = ChamsStyle == 1 ? CoveredLit : CoveredFlat;
				IMaterial *open = ChamsStyle == 1 ? OpenLit : OpenFlat;
				if (pLocal)
				{
					if (pLocal->IsAlive())
					{
						int alpha = pLocal->HasGunGameImmunity() ? 150 : 255;

						if (pLocal->GetTeamNum() == 2)
							color.SetColor(240, 30, 35, alpha);
						else
							color.SetColor(63, 72, 205, alpha);

						ForceMaterial(color, covered);
						oDrawModelExecute(thisptr, ctx, state, pInfo, pCustomBoneToWorld);

						if (pLocal->GetTeamNum() == 2)
							color.SetColor(247, 180, 20, alpha);
						else
							color.SetColor(32, 180, 57, alpha);
					}
					else
					{
						color.SetColor(255, 255, 255, 255);
					}

					ForceMaterial(color, open);
				}
			}
			else
			{
				static int counter = 0;
				static float colors[3] = { 1.f, 0.f, 0.f };

				if (colors[counter] >= 1.0f)
				{
					colors[counter] = 1.0f;
					counter += 1;
					if (counter > 2)
						counter = 0;
				}
				else
				{
					int prev = counter - 1;
					if (prev < 0) prev = 2;
					colors[prev] -= 0.05f;
					colors[counter] += 0.05f;
				}

				Interfaces::RenderView->SetColorModulation(colors);
				Interfaces::RenderView->SetBlend(0.3);
				Interfaces::ModelRender->ForcedMaterialOverride(OpenLit);
			}
		}
		else if (ChamsStyle != 0 && Menu::Window.VisualsTab.FiltersWeapons.GetState() && strstr(ModelName, "_dropped.mdl"))
		{
			IMaterial *covered = ChamsStyle == 1 ? CoveredLit : CoveredFlat;
			color.SetColor(255, 255, 255, 255);
			ForceMaterial(color, covered);
		}
	}

	if (!DontDraw)
		oDrawModelExecute(thisptr, ctx, state, pInfo, pCustomBoneToWorld);
	Interfaces::ModelRender->ForcedMaterialOverride(NULL);
}


// Hooked FrameStageNotify for removing visual recoil
void  __stdcall Hooked_FrameStageNotify(ClientFrameStage_t curStage)
{
	DWORD eyeangles = NetVar.GetNetVar(0xBFEA4E7B);
	IClientEntity *pLocal = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	if (Interfaces::Engine->IsConnected() && Interfaces::Engine->IsInGame() && curStage == FRAME_RENDER_START)
	{

		if (pLocal->IsAlive())
		{
			if (*(bool*)((DWORD)Interfaces::pInput + 0xA5))
				*(Vector*)((DWORD)pLocal + 0x31C8) = LastAngleAA;
		}

		if ((Menu::Window.MiscTab.OtherThirdperson.GetState()) || Menu::Window.RageBotTab.AccuracyPositionAdjustment.GetState())
		{
			static bool rekt = false;
			if (!rekt)
			{
				ConVar* sv_cheats = Interfaces::CVar->FindVar("sv_cheats");
				SpoofedConvar* sv_cheats_spoofed = new SpoofedConvar(sv_cheats);
				sv_cheats_spoofed->SetInt(1);
				rekt = true;
			}
		}

		static bool rekt1 = false;
		if (Menu::Window.MiscTab.OtherThirdperson.GetState() && pLocal->IsAlive() && pLocal->IsScoped() == 0)
		{
			if (!rekt1)
			{
				Interfaces::Engine->ClientCmd_Unrestricted("thirdperson");
				rekt1 = true;
			}
		}
		else if (!Menu::Window.MiscTab.OtherThirdperson.GetState())
		{
			rekt1 = false;
		}

		static bool rekt = false;
		if (!Menu::Window.MiscTab.OtherThirdperson.GetState() || pLocal->IsAlive() == 0 || pLocal->IsScoped())
		{
			if (!rekt)
			{
				Interfaces::Engine->ClientCmd_Unrestricted("firstperson");
				rekt = true;
			}
		}
		else if (Menu::Window.MiscTab.OtherThirdperson.GetState() || pLocal->IsAlive() || pLocal->IsScoped() == 0)
		{
			rekt = false;
		}

		static bool meme = false;
		if (Menu::Window.MiscTab.OtherThirdperson.GetState() && pLocal->IsScoped() == 0)
		{
			if (!meme)
			{
				Interfaces::Engine->ClientCmd_Unrestricted("thirdperson");
				meme = true;
			}
		}
		else if (pLocal->IsScoped())
		{
			meme = false;
		}

		static bool kek = false;
		if (Menu::Window.MiscTab.OtherThirdperson.GetState() && pLocal->IsAlive())
		{
			if (!kek)
			{
				Interfaces::Engine->ClientCmd_Unrestricted("thirdperson");
				kek = true;
			}
		}
		else if (pLocal->IsAlive() == 0)
		{
			kek = false;
		}
	}

	if (Interfaces::Engine->IsConnected() && Interfaces::Engine->IsInGame() && curStage == FRAME_NET_UPDATE_POSTDATAUPDATE_START)
	{
		IClientEntity *pLocal = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	/*	for (int i = 1; i < 65; i++)
		{
			IClientEntity* pEnt = Interfaces::EntList->GetClientEntity(i);
			if (!pEnt) continue;
			if (pEnt->IsDormant()) continue;
			if (pEnt->GetHealth() < 1) continue;
			if (pEnt->GetLifeState() != 0) continue;

			*(float*)((DWORD)pEnt + eyeangles) = pEnt->GetTargetYaw();
			//Msg("%f\n", *(float*)((DWORD)pEnt + m_angEyeAnglesYaw));
		} */

		if (Menu::Window.MiscTab.KnifeEnable.GetState() && pLocal)
		{
			IClientEntity* WeaponEnt = Interfaces::EntList->GetClientEntityFromHandle(pLocal->GetActiveWeaponHandle());
			CBaseCombatWeapon* Weapon = (CBaseCombatWeapon*)WeaponEnt;

			int iBayonet = Interfaces::ModelInfo->GetModelIndex("models/weapons/v_knife_bayonet.mdl");
			int iButterfly = Interfaces::ModelInfo->GetModelIndex("models/weapons/v_knife_butterfly.mdl");
			int iFlip = Interfaces::ModelInfo->GetModelIndex("models/weapons/v_knife_flip.mdl");
			int iGut = Interfaces::ModelInfo->GetModelIndex("models/weapons/v_knife_gut.mdl");
			int iKarambit = Interfaces::ModelInfo->GetModelIndex("models/weapons/v_knife_karam.mdl");
			int iM9Bayonet = Interfaces::ModelInfo->GetModelIndex("models/weapons/v_knife_m9_bay.mdl");
			int iHuntsman = Interfaces::ModelInfo->GetModelIndex("models/weapons/v_knife_tactical.mdl");
			int iFalchion = Interfaces::ModelInfo->GetModelIndex("models/weapons/v_knife_falchion_advanced.mdl");
			int iDagger = Interfaces::ModelInfo->GetModelIndex("models/weapons/v_knife_push.mdl");
			int iBowie = Interfaces::ModelInfo->GetModelIndex("models/weapons/v_knife_survival_bowie.mdl");

			int Model = Menu::Window.MiscTab.KnifeModel.GetIndex();
			int Skin = Menu::Window.MiscTab.KnifeSkin.GetIndex();

			if (Weapon)
			{
				if (WeaponEnt->GetClientClass()->m_ClassID == (int)CSGOClassID::CKnife)
				{
					if (Model == 0) // Karambit
					{
						*Weapon->ModelIndex() = iKarambit; // m_nModelIndex
						*Weapon->ViewModelIndex() = iKarambit;
						*Weapon->WorldModelIndex() = iKarambit + 1;
						*Weapon->m_AttributeManager()->m_Item()->ItemDefinitionIndex() = 507;

						if (Skin == 0)
							*Weapon->FallbackPaintKit() = 416; // Doppler Sapphire
						else if (Skin == 1)
							*Weapon->FallbackPaintKit() = 415; // Doppler Ruby
						else if (Skin == 2)
							*Weapon->FallbackPaintKit() = 409; // Tiger Tooth
						else if (Skin == 3)
							*Weapon->FallbackPaintKit() = 558; // Lore
					}
					else if (Model == 1) // Karambit
					{
						*Weapon->ModelIndex() = iBayonet; // m_nModelIndex
						*Weapon->ViewModelIndex() = iBayonet;
						*Weapon->WorldModelIndex() = iBayonet + 1;
						*Weapon->m_AttributeManager()->m_Item()->ItemDefinitionIndex() = 500;

						if (Skin == 0)
							*Weapon->FallbackPaintKit() = 416; // Doppler Sapphire
						else if (Skin == 1)
							*Weapon->FallbackPaintKit() = 415; // Doppler Ruby
						else if (Skin == 2)
							*Weapon->FallbackPaintKit() = 409; // Tiger Tooth
						else if (Skin == 3)
							*Weapon->FallbackPaintKit() = 558; // Lore
					}

					*Weapon->OwnerXuidLow() = 0;
					*Weapon->OwnerXuidHigh() = 0;
					*Weapon->FallbackWear() = 0.001f;
					*Weapon->m_AttributeManager()->m_Item()->ItemIDHigh() = 1;
				}
			}
		}
		if (pLocal->IsAlive())
			R::Resolve();
	}

	oFrameStageNotify(curStage);
}

void __fastcall Hooked_OverrideView(void* ecx, void* edx, CViewSetup* pSetup)
{
	IClientEntity* pLocal = (IClientEntity*)Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	if (Interfaces::Engine->IsConnected() && Interfaces::Engine->IsInGame())
	{
		if (Menu::Window.VisualsTab.Active.GetState() && pLocal->IsAlive() && !pLocal->IsScoped())
		{
			if (pSetup->fov = 90)
				pSetup->fov = Menu::Window.VisualsTab.OtherFOV.GetValue();
		}

		oOverrideView(ecx, edx, pSetup);
	}

}

void GetViewModelFOV(float& fov)
{
	IClientEntity* localplayer = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	if (Interfaces::Engine->IsConnected() && Interfaces::Engine->IsInGame())
	{

		if (!localplayer)
			return;


		if (Menu::Window.VisualsTab.Active.GetState())
		fov += Menu::Window.VisualsTab.OtherViewmodelFOV.GetValue();
	}
}

float __stdcall GGetViewModelFOV()
{
	float fov = Hooks::VMTClientMode.GetMethod<oGetViewModelFOV>(35)();

	GetViewModelFOV(fov);

	return fov;
}

void __fastcall Hooked_RenderView(void* ecx, void* edx, CViewSetup &setup, CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw)
{
	static DWORD oRenderView = Hooks::VMTRenderView.GetOriginalFunction(6);

	IClientEntity* pLocal = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	__asm
	{
		PUSH whatToDraw
		PUSH nClearFlags
		PUSH hudViewSetup
		PUSH setup
		MOV ECX, ecx
		CALL oRenderView
	}
} //hooked for no reason yay

//JUNK CODE STARTS FROM HERE!!
#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

class yismzba {
public:
	int lxzpalhamwdrwz;
	int qlfzj;
	string hmvdganmzsbgh;
	yismzba();
	string fdjmntyriqbbwkqvqkk(double otalar, int ybrnldytnnrvj, string njggdmpr, bool dongzyuku, double nasnmjvr, double saawszvdbppc, double nwbaftsuyldai, bool gekudizasw, string zewgqiagchdk, bool uplypfx);
	double hdvhjebhgsgghq(string snczzguvepdqb, bool qmvlhddkura, bool tymsbtbywhvt, bool goftwnolmaya, string uvzuualrrsvgd, double fiyemmsxu);
	void tbhcodotoufajxibtnm(string xjyrwtbpw, bool mseeqndbzborddi, bool ljahefrdtd, string cjqokpkeiwtylb, int hkjwloaephtsg, string tdqnthwqvw, double ojwvnfr, bool xmxqwcgvyhfte, double rkjzyafgznkw, int bcrlxu);
	string zzddqbvphcqlugdclzgya(int apaluut, double gvzybgyemgm, double mrwvdftdr, bool iuwkvucm, bool zozfhtar, int krxyxw, int okaxhoph, bool iftdxfl, double tanppflyhpru);
	bool kzlpcdvdscavsmztrczb(double zxcpuwfivtm, int pdtzjkzcugs, int kstkbimiyfdztp, string cdoyzavotto, bool ftfop, double doqbogjkxnkqvcq, string ahjcvjtsatjkeip, bool oyqndgfyyapsnku, string nalir);
	void fmjguartpzwzzyteumrho(double hvnehenpkmtun, bool oemqdmm, bool jhzji);
	string jrhsilhdphrcgjqhevjalo(string gagal, bool smspwudraalwby);

protected:
	double cepvbfr;
	bool jzisd;

	int iupodeozotdmwzajkxrqvc(int jcjuuhwztompdq, bool pappuzrjfpkoen, int jcwdt, double vtkirwvqibidbw, double catuljwbl, string ohytyh, int dwznnzxbhdt);
	double wdjfkenixjnshdo(int dverajl, int hwiajt, int hjwhxhbrcpe, double kbjrnjikreey, double tbuha, int vlxdfvnctf);
	string efnpemcoqxajklpbx(int sezux, string eisbzzswqim, bool qispc, int oqmqhxqebdqwf, int jcnitplijln, bool iyuywv, int eamwlpbfuuuf, int xjarmnxkdgnaz, bool emqnjhtvzzavtt, int pioffqsiyvedpeo);
	bool rmtaldbisffdtgaxrdwffrfs(int hzsrmidgotpb);
	bool dyxtpvuxoskuyfpq(bool pyuxcogvwwkwz, bool vfauyokctwegsjx, double sxzpzpigj, string lmnhxv, double grrvjfpqor, int yxmxurhnwspb, bool aacgma, double tvcfdperr);
	bool mhxykiclzwoevwdrvlobmhtem(bool xnfhyzspa, bool zpixai, double klktprfce, bool zgkhurahxkvv, bool jmexmm, bool ultojdvlmy, double cddwieuomyazmn, int xwvmloc, int gjkmlr);
	string kilkqznvltnmkojk();
	double lezvwrchhsfsdbrlqwnglq(double ecpineyhacsdcyh);
	bool vhtrwhpiexctgbputcpq(double eacjgkmgetfoeae, int xyslxw, int jdcicuju, double hexmryzs);

private:
	string zhlewiqo;
	int grhtzb;

	string jqyjxqmbqxyp(bool cmxcihht, double gemstssmerqux, bool daerywznxg, bool scjrattcycib, bool uzrdjozlkwbb, int mdudxuslqtcaz);

};


string yismzba::jqyjxqmbqxyp(bool cmxcihht, double gemstssmerqux, bool daerywznxg, bool scjrattcycib, bool uzrdjozlkwbb, int mdudxuslqtcaz) {
	double pxzrlnlrmlggob = 98825;
	double uttjuwwib = 33420;
	string guojo = "nviao";
	double kctsnljk = 23470;
	int cdoupzvvfnfys = 2690;
	double vjvjym = 22438;
	int dlxnrubjkhmvpkf = 220;
	string yrgrmsfczqo = "cevqhmerelxmqlulwdvqovalpxwmsocxtdyuabeisedbuinbblcowgltxelllwlimlmpodxjijupbswxelalxpr";
	string sexadfobnwh = "unfglgyurujqnthpnvuhihpgwvgyakasfeotwshlaxbbtoubgkttbendxoeweolgxnhtkcslkr";
	bool hokuxhgfnfaeiz = false;
	return string("hgvzodctkebpz");
}

int yismzba::iupodeozotdmwzajkxrqvc(int jcjuuhwztompdq, bool pappuzrjfpkoen, int jcwdt, double vtkirwvqibidbw, double catuljwbl, string ohytyh, int dwznnzxbhdt) {
	return 80386;
}

double yismzba::wdjfkenixjnshdo(int dverajl, int hwiajt, int hjwhxhbrcpe, double kbjrnjikreey, double tbuha, int vlxdfvnctf) {
	bool tceniaapcmtwa = false;
	double tdsvknmr = 85373;
	double asaejiv = 11212;
	string rgebhm = "dbstoyjqqyzzsdiozlnailwsoyauqedqwkwtwnhdroaefpudlatchgpffjxdycdvnspdywmzleibljvvtyxhyuygyetocgy";
	int ultbly = 1593;
	if (11212 != 11212) {
		int fexkytoy;
		for (fexkytoy = 62; fexkytoy > 0; fexkytoy--) {
			continue;
		}
	}
	if (11212 != 11212) {
		int kfen;
		for (kfen = 51; kfen > 0; kfen--) {
			continue;
		}
	}
	if (11212 != 11212) {
		int tba;
		for (tba = 11; tba > 0; tba--) {
			continue;
		}
	}
	if (85373 != 85373) {
		int bdpdahehor;
		for (bdpdahehor = 15; bdpdahehor > 0; bdpdahehor--) {
			continue;
		}
	}
	return 58752;
}

string yismzba::efnpemcoqxajklpbx(int sezux, string eisbzzswqim, bool qispc, int oqmqhxqebdqwf, int jcnitplijln, bool iyuywv, int eamwlpbfuuuf, int xjarmnxkdgnaz, bool emqnjhtvzzavtt, int pioffqsiyvedpeo) {
	string hfpxfigeoj = "fcalkihdyvwkukhbcrzjbtiecpelrrdhmoaqstpnwzoobrfzwtpivecjbheolznlmejunbaitdtmhhdfxcofqlkfuu";
	double gvtzpuvzwcmq = 33678;
	string tlhcksvlgkykerh = "jkfrmwsnsnrihzhmaajeuwkyorjworhwlncbzsrzkrpwqyhhqkmohregjqalsifunz";
	double phvvonkvyrz = 26575;
	int tarpd = 2863;
	bool hpzetzr = false;
	string zwnchrbyyxof = "tinqepxmficswikdhbsj";
	string yhpjkc = "odtvaeygqlcyafrphfgjeysosszzvowjsljzveekubtittksupqkgiwhwmtqwfxpgfbohtvxqptvpuqfdmmhctlozoxpnvxnkpg";
	string knpnp = "mavjqrlajebhhxzpxxnmcwnrgecwktdlhvpqamzgzoaayqekgdxojhqgwgzsocxcgsjglipudlgrxzxgzdhwscmgawdaujpnajti";
	if (33678 != 33678) {
		int zn;
		for (zn = 26; zn > 0; zn--) {
			continue;
		}
	}
	return string("khrcmdyuvtbo");
}

bool yismzba::rmtaldbisffdtgaxrdwffrfs(int hzsrmidgotpb) {
	string fsvzm = "tlqeklysetarcnyqutrtxsjcilqdglpzdjlrwylywvwghbrucebwphkjivjlyebjogmgdjdnjjlyajlzorrwc";
	int jcyyoidmr = 4027;
	int cpdtmicwkynu = 8808;
	double yzukafssxs = 8389;
	bool uknlwyv = false;
	string ogkqcay = "kxbyyrunjep";
	int riiybmxd = 2588;
	bool uqkjanwy = true;
	if (string("kxbyyrunjep") == string("kxbyyrunjep")) {
		int qmwdo;
		for (qmwdo = 90; qmwdo > 0; qmwdo--) {
			continue;
		}
	}
	if (4027 != 4027) {
		int egcnvn;
		for (egcnvn = 14; egcnvn > 0; egcnvn--) {
			continue;
		}
	}
	if (string("kxbyyrunjep") == string("kxbyyrunjep")) {
		int kmogvmum;
		for (kmogvmum = 6; kmogvmum > 0; kmogvmum--) {
			continue;
		}
	}
	if (2588 == 2588) {
		int jqeex;
		for (jqeex = 88; jqeex > 0; jqeex--) {
			continue;
		}
	}
	if (true == true) {
		int imnm;
		for (imnm = 41; imnm > 0; imnm--) {
			continue;
		}
	}
	return false;
}

bool yismzba::dyxtpvuxoskuyfpq(bool pyuxcogvwwkwz, bool vfauyokctwegsjx, double sxzpzpigj, string lmnhxv, double grrvjfpqor, int yxmxurhnwspb, bool aacgma, double tvcfdperr) {
	bool xhshvd = false;
	bool rkyztw = true;
	double khgnmdc = 32269;
	bool fvuvp = true;
	bool hhyajjoqhducqst = false;
	string cwtdi = "aifjt";
	int upwzbchrsz = 4303;
	string llduc = "mxbfjworsocxjqodofsugocujwqqfsssqikstbhlklqfdikdgzwdhrpbbycd";
	int eonzkt = 1060;
	int kralomrpk = 1871;
	if (string("aifjt") != string("aifjt")) {
		int ynuydzxwjb;
		for (ynuydzxwjb = 99; ynuydzxwjb > 0; ynuydzxwjb--) {
			continue;
		}
	}
	if (true != true) {
		int qhatgzda;
		for (qhatgzda = 92; qhatgzda > 0; qhatgzda--) {
			continue;
		}
	}
	if (false == false) {
		int sifvdoz;
		for (sifvdoz = 91; sifvdoz > 0; sifvdoz--) {
			continue;
		}
	}
	if (false != false) {
		int isqfhrije;
		for (isqfhrije = 25; isqfhrije > 0; isqfhrije--) {
			continue;
		}
	}
	return false;
}

bool yismzba::mhxykiclzwoevwdrvlobmhtem(bool xnfhyzspa, bool zpixai, double klktprfce, bool zgkhurahxkvv, bool jmexmm, bool ultojdvlmy, double cddwieuomyazmn, int xwvmloc, int gjkmlr) {
	int hkerjaxnpmfvnru = 3979;
	bool cyxcvkxcbfosjg = false;
	if (false != false) {
		int wbydktlurd;
		for (wbydktlurd = 72; wbydktlurd > 0; wbydktlurd--) {
			continue;
		}
	}
	return false;
}

string yismzba::kilkqznvltnmkojk() {
	string khglznwau = "yzzbtwhcxctyudmnsdmzocgyvmmfqtdsurpjlfjmjqmjuamlzosmcjwvpigevwwdggxwhunlfeaclj";
	int utkywhqurfxc = 376;
	double emakepqxw = 40639;
	if (string("yzzbtwhcxctyudmnsdmzocgyvmmfqtdsurpjlfjmjqmjuamlzosmcjwvpigevwwdggxwhunlfeaclj") == string("yzzbtwhcxctyudmnsdmzocgyvmmfqtdsurpjlfjmjqmjuamlzosmcjwvpigevwwdggxwhunlfeaclj")) {
		int jy;
		for (jy = 29; jy > 0; jy--) {
			continue;
		}
	}
	return string("fmszep");
}

double yismzba::lezvwrchhsfsdbrlqwnglq(double ecpineyhacsdcyh) {
	string jcdxefteu = "acedsjbcotcgkjzvvhblzlbavfgnoqjzsernxxocbxpz";
	if (string("acedsjbcotcgkjzvvhblzlbavfgnoqjzsernxxocbxpz") != string("acedsjbcotcgkjzvvhblzlbavfgnoqjzsernxxocbxpz")) {
		int jnluoegwnh;
		for (jnluoegwnh = 76; jnluoegwnh > 0; jnluoegwnh--) {
			continue;
		}
	}
	return 58452;
}

bool yismzba::vhtrwhpiexctgbputcpq(double eacjgkmgetfoeae, int xyslxw, int jdcicuju, double hexmryzs) {
	int aieiwsfizdm = 2575;
	bool pekwmyjxxgnjimy = false;
	string dcykietilzd = "bwfjnkypmkftygulenhlaebkukpruxakuwharessvekdgjdljgetmjtgcnzhdqvlxoigrvcwr";
	string qtxzx = "bbzxsstgbzcnrrhlgspodalceewmxeygrqzmfaufcqeziniidecyfmjhnmhfarzcczqocrpcmzmpctcm";
	int fukzoqvcgyo = 1112;
	if (2575 != 2575) {
		int cl;
		for (cl = 39; cl > 0; cl--) {
			continue;
		}
	}
	if (2575 != 2575) {
		int ybt;
		for (ybt = 65; ybt > 0; ybt--) {
			continue;
		}
	}
	if (string("bwfjnkypmkftygulenhlaebkukpruxakuwharessvekdgjdljgetmjtgcnzhdqvlxoigrvcwr") != string("bwfjnkypmkftygulenhlaebkukpruxakuwharessvekdgjdljgetmjtgcnzhdqvlxoigrvcwr")) {
		int ox;
		for (ox = 60; ox > 0; ox--) {
			continue;
		}
	}
	if (false != false) {
		int jljciegg;
		for (jljciegg = 44; jljciegg > 0; jljciegg--) {
			continue;
		}
	}
	return false;
}

string yismzba::fdjmntyriqbbwkqvqkk(double otalar, int ybrnldytnnrvj, string njggdmpr, bool dongzyuku, double nasnmjvr, double saawszvdbppc, double nwbaftsuyldai, bool gekudizasw, string zewgqiagchdk, bool uplypfx) {
	int qkjnahvftz = 4745;
	bool ejzjfvxprkwimgp = false;
	double fqakzwbrhbi = 11223;
	double rnzhbfy = 37490;
	int dwpdcatje = 2308;
	int ypzahuvaztrdqrb = 507;
	bool qvnuvv = true;
	if (37490 == 37490) {
		int bfwlxbxx;
		for (bfwlxbxx = 23; bfwlxbxx > 0; bfwlxbxx--) {
			continue;
		}
	}
	if (37490 != 37490) {
		int jqasysxat;
		for (jqasysxat = 3; jqasysxat > 0; jqasysxat--) {
			continue;
		}
	}
	return string("aljou");
}

double yismzba::hdvhjebhgsgghq(string snczzguvepdqb, bool qmvlhddkura, bool tymsbtbywhvt, bool goftwnolmaya, string uvzuualrrsvgd, double fiyemmsxu) {
	double jqntkexvgxb = 66477;
	string tdypivgscu = "easpbvfwhlwhlundakisfzpjrgxyonxsjjcnbapdatgngfkiqdarhb";
	int qvsyoj = 853;
	string yomwtpinl = "yiduyyldosazmhladqpctweuopignqsrsmjzsfgvpapndgjohtsujgsoculvfvwrcabbp";
	if (string("yiduyyldosazmhladqpctweuopignqsrsmjzsfgvpapndgjohtsujgsoculvfvwrcabbp") == string("yiduyyldosazmhladqpctweuopignqsrsmjzsfgvpapndgjohtsujgsoculvfvwrcabbp")) {
		int ssfcjqv;
		for (ssfcjqv = 14; ssfcjqv > 0; ssfcjqv--) {
			continue;
		}
	}
	if (string("easpbvfwhlwhlundakisfzpjrgxyonxsjjcnbapdatgngfkiqdarhb") == string("easpbvfwhlwhlundakisfzpjrgxyonxsjjcnbapdatgngfkiqdarhb")) {
		int ocqorbju;
		for (ocqorbju = 30; ocqorbju > 0; ocqorbju--) {
			continue;
		}
	}
	if (string("easpbvfwhlwhlundakisfzpjrgxyonxsjjcnbapdatgngfkiqdarhb") != string("easpbvfwhlwhlundakisfzpjrgxyonxsjjcnbapdatgngfkiqdarhb")) {
		int wrraoqp;
		for (wrraoqp = 21; wrraoqp > 0; wrraoqp--) {
			continue;
		}
	}
	return 49082;
}

void yismzba::tbhcodotoufajxibtnm(string xjyrwtbpw, bool mseeqndbzborddi, bool ljahefrdtd, string cjqokpkeiwtylb, int hkjwloaephtsg, string tdqnthwqvw, double ojwvnfr, bool xmxqwcgvyhfte, double rkjzyafgznkw, int bcrlxu) {
	string acsxgleqs = "iclvjtadiioataurftzysgfdanfbksxvumlfzpsjfxmzmwgjr";
	int wgiighzywh = 1364;
	double sfgkmxctx = 11851;

}

string yismzba::zzddqbvphcqlugdclzgya(int apaluut, double gvzybgyemgm, double mrwvdftdr, bool iuwkvucm, bool zozfhtar, int krxyxw, int okaxhoph, bool iftdxfl, double tanppflyhpru) {
	double ofrxwgluhrtf = 21080;
	int rsabzzasa = 4709;
	string ufuibqhedbf = "xcqlwrhddcgsfykuqazjrugedunhqajblosrsv";
	bool zydusazvfhsfqqz = true;
	double hmdanixbemlv = 24630;
	bool zumeqwevxhtwy = true;
	if (21080 != 21080) {
		int vdaleq;
		for (vdaleq = 22; vdaleq > 0; vdaleq--) {
			continue;
		}
	}
	if (true != true) {
		int eqgsvzqj;
		for (eqgsvzqj = 86; eqgsvzqj > 0; eqgsvzqj--) {
			continue;
		}
	}
	return string("");
}

bool yismzba::kzlpcdvdscavsmztrczb(double zxcpuwfivtm, int pdtzjkzcugs, int kstkbimiyfdztp, string cdoyzavotto, bool ftfop, double doqbogjkxnkqvcq, string ahjcvjtsatjkeip, bool oyqndgfyyapsnku, string nalir) {
	double ouust = 10548;
	int tnerjdumdg = 2222;
	int ldbruitdfpyl = 2228;
	string rbmfuafdk = "yfjisflhoyxqpmjxfosvihkugvwqpgrlhmctbqmcuoisbjdxgxoqbrkmp";
	double mrgvdwtze = 57140;
	bool cqgjjacxd = false;
	double ravstdhs = 39159;
	if (string("yfjisflhoyxqpmjxfosvihkugvwqpgrlhmctbqmcuoisbjdxgxoqbrkmp") == string("yfjisflhoyxqpmjxfosvihkugvwqpgrlhmctbqmcuoisbjdxgxoqbrkmp")) {
		int searyeqqbe;
		for (searyeqqbe = 92; searyeqqbe > 0; searyeqqbe--) {
			continue;
		}
	}
	if (57140 != 57140) {
		int pyctivrk;
		for (pyctivrk = 31; pyctivrk > 0; pyctivrk--) {
			continue;
		}
	}
	if (false != false) {
		int aiumzray;
		for (aiumzray = 19; aiumzray > 0; aiumzray--) {
			continue;
		}
	}
	if (39159 == 39159) {
		int rrsv;
		for (rrsv = 28; rrsv > 0; rrsv--) {
			continue;
		}
	}
	if (39159 == 39159) {
		int wrc;
		for (wrc = 80; wrc > 0; wrc--) {
			continue;
		}
	}
	return true;
}

void yismzba::fmjguartpzwzzyteumrho(double hvnehenpkmtun, bool oemqdmm, bool jhzji) {
	bool rctirecrrxqtie = true;
	double kpccmqqqhv = 69952;

}

string yismzba::jrhsilhdphrcgjqhevjalo(string gagal, bool smspwudraalwby) {
	double rzrypue = 49766;
	double dnxsjnq = 22964;
	string tkwkmzekrye = "jgzogqsvksjjzmbobcxmnockuwrnnrwehqeiaigtadhhqubnztufcdpjlthlvxifnipyqqdesfrnjzcidmsphzwfoxtpvmyboswo";
	double svhlkya = 44224;
	bool belecnbynnc = false;
	if (49766 != 49766) {
		int piuihzoaf;
		for (piuihzoaf = 33; piuihzoaf > 0; piuihzoaf--) {
			continue;
		}
	}
	if (49766 != 49766) {
		int vdnvbi;
		for (vdnvbi = 70; vdnvbi > 0; vdnvbi--) {
			continue;
		}
	}
	if (false != false) {
		int umqs;
		for (umqs = 17; umqs > 0; umqs--) {
			continue;
		}
	}
	if (22964 == 22964) {
		int nzndrnmem;
		for (nzndrnmem = 46; nzndrnmem > 0; nzndrnmem--) {
			continue;
		}
	}
	if (string("jgzogqsvksjjzmbobcxmnockuwrnnrwehqeiaigtadhhqubnztufcdpjlthlvxifnipyqqdesfrnjzcidmsphzwfoxtpvmyboswo") != string("jgzogqsvksjjzmbobcxmnockuwrnnrwehqeiaigtadhhqubnztufcdpjlthlvxifnipyqqdesfrnjzcidmsphzwfoxtpvmyboswo")) {
		int ynvf;
		for (ynvf = 77; ynvf > 0; ynvf--) {
			continue;
		}
	}
	return string("kj");
}

yismzba::yismzba() {
	this->fdjmntyriqbbwkqvqkk(79505, 5401, string("ttkksxaduwgdfkmibsdswbiofuglitieotobcjfazqwaeszgxlpsirtnfdpuykcguiscytokssj"), true, 50789, 23219, 29252, false, string("pbx"), true);
	this->hdvhjebhgsgghq(string("ukbypcpouxetwboojvijektajexvgkggdotznoqmxrnkcscjdw"), true, false, true, string("igqdvxndnsbihlavvajgiwlpbvonfpygmngmjajymcfwnlbfrfsvmescmmxeaemdcbxototuiknwdzybyjeedaxvviwdtpfvnuip"), 42762);
	this->tbhcodotoufajxibtnm(string("qdwaosevurpyatxgmybtynmhvqdu"), true, false, string("jnhyceuxjmeflothefbnkqvprorfdmfdvktdztdcwvpzcqeuyqilkmocnkmvdgxrxplpwqifvlrbqxvbwjuczvuxbuuzcwl"), 4094, string("gcmoxkuygmcyjlbcxyifwpngmjgidzogxrfymsijvoxqtyqcsgcrucbusogvkkcmmhxajbpojmoyyzbuiujnvfswwezuktzvflbx"), 30077, false, 34185, 2965);
	this->zzddqbvphcqlugdclzgya(7589, 5010, 29028, false, false, 8247, 682, false, 87780);
	this->kzlpcdvdscavsmztrczb(42186, 697, 936, string("kpykvbbeuxwetgllkajkywnlmqbffooc"), false, 10794, string("pgaehqdluvzlkaxuxhgxlcpmtefrrgpxwdpqdubnnuybj"), false, string("nxehdlwlaxnwcdpsexgamvzpkl"));
	this->fmjguartpzwzzyteumrho(2337, false, false);
	this->jrhsilhdphrcgjqhevjalo(string("ovcqetpdhrqtnqesrktwyl"), false);
	this->iupodeozotdmwzajkxrqvc(2857, false, 1510, 23468, 17996, string("ywdptq"), 3218);
	this->wdjfkenixjnshdo(5042, 3751, 3134, 7529, 5469, 1095);
	this->efnpemcoqxajklpbx(5071, string("h"), false, 1081, 1890, true, 1402, 3022, true, 6514);
	this->rmtaldbisffdtgaxrdwffrfs(1870);
	this->dyxtpvuxoskuyfpq(false, false, 4597, string("nvfkjwddbmnjer"), 31471, 7412, false, 22402);
	this->mhxykiclzwoevwdrvlobmhtem(false, true, 70412, true, false, true, 21656, 4083, 4392);
	this->kilkqznvltnmkojk();
	this->lezvwrchhsfsdbrlqwnglq(16527);
	this->vhtrwhpiexctgbputcpq(28715, 5895, 1460, 59936);
	this->jqyjxqmbqxyp(true, 5007, false, false, false, 2432);
}
#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

class uxsardt {
public:
	int mqzct;
	uxsardt();
	bool mfrxrbzpqottzpcad(int hoxshxofsenox, string vegmccprksiiuyd, double ozxgnxudlbigsxw, bool izetfglgujup, int fueykpzgvu, int xltdvovfwj, bool mgpwf);
	void eeckdautyqdicqbyb(string kssgdcojrw, bool swxixssbbo, string jwqfnpuxmxvni, bool cenyx, int giobgy, string yxzmkqmxifadw, string ftkuxjxywj, string wmjrcpkzn, bool zvdotl, double wvokfq);
	int wfiartrpuk(bool oycwkftjpl, int dizrvln, int cbgldlunophzqyd, int oywjdoyrcmxs, double hkuavr, string pwjtyoeuievsjy);

protected:
	double sitvagwyzpd;
	string bxtxqzirvikxzc;

	string epheflvafxowc(int zetyo, int nvwqhjq, string gvpwa, double cfedajterowr, double gvefqkvrhydwg, int ovbvawhl, bool guybmmidvbf, int srqxizrs, bool erhdooevdsdxi);
	void ukiafqhtnhi(bool mmglthgloaxi, bool aqxwscrxjuvo, bool theis, double wdfoxf);
	bool quhctasoqps(double wuvqteny, string bwmiuwucjesrmwj);
	double knizxgxuxnnkptpyqlpmuxttd(bool wtwjpfudmhoy, double wkuzdcjx, int tnxwaoxk, bool dfmruj, double odiirs);
	string bnqxruiyfg(string cjdldf);
	int whalgvjtgkt(int soopymmhrc, string dktssssqowmgih, double uhbluxub, double lbcrqcbdmhdgzby, double jelhcvwrtdrk);
	string aukortbakuhoelwe(int yukandqrmtsnjx, bool bwtnrsl, string rmgpa, bool wcyoqfgeoegc, string cuwwukthjwwf, string yyoof, int onbyxgptbmq, string sdvhuvzbnpywl, string gozpjbiq);
	void zhtbkthnkoulffa(int jhbddijwg, double ngjvwz, int nsvxagtkmckjelz, int mdafomlbi, int tmlwi, int vfner, double efvcftmgefp, bool lohmzbxcn, double wwveqycaewrsrvc, string nsvtjxfrot);

private:
	double hjyklacrcfscp;
	bool xropberyc;
	int jxdicolpkvkhkgo;
	string bipsza;

	string qfqthzvbwcsuuiqslxvqyl(string kkjvnyuw, string uzwajcfa, int yrzytqtjz, string qjrgtzgkf, string qndilxuv, string fdimx, string boanfl, double txcamp, double tepolavnempbbej, bool pzgnvgc);
	string rotbtbtoglgiwnz(int fpioeh, bool cckxvguh, double uiskcijgvxvn, string lcfgevuhcfxzlv, int hnvkjuct, string eaxwvl, string tnjvzjzjdsm, bool vufbpntxrv, double ehuvgh);
	void mpqsxaeicohcl(double erlxycjakxk, double qayoyjtprs, bool foxffhv, int xdhuto, int tavkrtbyot, int nknltsgm, bool rhebxmn, bool zhwkcjmbw);
	bool ovtdjabdebm(double hxcknnkg, double uziitkmt);
	string xrngbwnywhiprwmxgnq();
	bool wwwhxxvxyskivanxjkgjv(int zvffr, int hbrlegxopt, string ukvxvdopg, double vpcqkwg, double yuraxzjiwlqmk, int uhpnnkej, string pfcyyqeymkfv, string ulfuwjptegtvtc, double bnptmsmacca);

};


string uxsardt::qfqthzvbwcsuuiqslxvqyl(string kkjvnyuw, string uzwajcfa, int yrzytqtjz, string qjrgtzgkf, string qndilxuv, string fdimx, string boanfl, double txcamp, double tepolavnempbbej, bool pzgnvgc) {
	int kbcwttnhnuvpx = 214;
	int soavyeqmu = 3176;
	string urmazfbngduh = "emtkugctajvdnxahzcvifmyitchpyqwptidgosxtadwbqbrttxixnbjoiuijudlifupfxtxbefziwzqgkgwdme";
	double rmccjkffiyvlyy = 43548;
	double oncnneevumyklk = 50960;
	bool wscnqqb = false;
	double ukbsotgim = 6678;
	string ggkrwdkmzzvev = "vzerkpqakvtkbxmxghrsihelreeqcvkowthcitmqgmbvzcxc";
	bool yfakowwfbghj = true;
	if (string("emtkugctajvdnxahzcvifmyitchpyqwptidgosxtadwbqbrttxixnbjoiuijudlifupfxtxbefziwzqgkgwdme") == string("emtkugctajvdnxahzcvifmyitchpyqwptidgosxtadwbqbrttxixnbjoiuijudlifupfxtxbefziwzqgkgwdme")) {
		int zhcgp;
		for (zhcgp = 72; zhcgp > 0; zhcgp--) {
			continue;
		}
	}
	if (43548 != 43548) {
		int fjwlfcacwe;
		for (fjwlfcacwe = 91; fjwlfcacwe > 0; fjwlfcacwe--) {
			continue;
		}
	}
	if (true == true) {
		int vdcocbgtc;
		for (vdcocbgtc = 69; vdcocbgtc > 0; vdcocbgtc--) {
			continue;
		}
	}
	if (43548 == 43548) {
		int sexdzuqv;
		for (sexdzuqv = 28; sexdzuqv > 0; sexdzuqv--) {
			continue;
		}
	}
	if (false == false) {
		int hsscrhmio;
		for (hsscrhmio = 47; hsscrhmio > 0; hsscrhmio--) {
			continue;
		}
	}
	return string("jxfv");
}

string uxsardt::rotbtbtoglgiwnz(int fpioeh, bool cckxvguh, double uiskcijgvxvn, string lcfgevuhcfxzlv, int hnvkjuct, string eaxwvl, string tnjvzjzjdsm, bool vufbpntxrv, double ehuvgh) {
	int huoccg = 8576;
	bool iegwcrobhwpddit = true;
	int kwqktwqvaq = 4238;
	int ukydf = 2150;
	double ceciugexedfgrot = 15877;
	string hvrcnjdnxcgnwm = "kzhhbcpdmkfkbseefbgwyrs";
	bool lwhxuelxhz = true;
	string zyjppriddvj = "hlcxkwbmutvrpnuxsptnfgyhzceorkckyyuepscgnpikyhosrkugmpkjinddnofduanqtzddrxypztkbxuvfj";
	string bahdfoifiupsdub = "iohybqhcelhhppiyhyaoyiurwrdfssse";
	bool asnjywcu = true;
	return string("byqrsyxzyhfu");
}

void uxsardt::mpqsxaeicohcl(double erlxycjakxk, double qayoyjtprs, bool foxffhv, int xdhuto, int tavkrtbyot, int nknltsgm, bool rhebxmn, bool zhwkcjmbw) {
	string trvelcnplof = "aravsrkxycecwgaeyglxcyrqfas";
	int oiogfqotwb = 1039;
	double xtkhigbcbutub = 78552;
	bool cduedljxgghtp = true;
	string eoljbxt = "ptjprqlobaqisqjubkigwmsloosnclcvauviuocnncnqggcfibkzkssaosonhkvrdakl";
	double bvonql = 1057;
	int acfwdnqtgjk = 4925;
	int ofqvfemiq = 1352;
	bool zofcqakefdbmw = true;
	int ufcjav = 2455;
	if (2455 == 2455) {
		int mxrkwnq;
		for (mxrkwnq = 62; mxrkwnq > 0; mxrkwnq--) {
			continue;
		}
	}

}

bool uxsardt::ovtdjabdebm(double hxcknnkg, double uziitkmt) {
	bool fyrncifzcrlcn = false;
	string zwrqymrz = "zlvhiwshwlafipujirwolpafyuckxgsoctxijyaimjypbdialjunkvzajciyyyxrtmiijaccgt";
	double sgbsqea = 26598;
	if (false != false) {
		int aw;
		for (aw = 70; aw > 0; aw--) {
			continue;
		}
	}
	if (false != false) {
		int zlmvf;
		for (zlmvf = 76; zlmvf > 0; zlmvf--) {
			continue;
		}
	}
	if (26598 == 26598) {
		int gcvtqaftm;
		for (gcvtqaftm = 37; gcvtqaftm > 0; gcvtqaftm--) {
			continue;
		}
	}
	return true;
}

string uxsardt::xrngbwnywhiprwmxgnq() {
	int qfmwycxddaf = 973;
	int fvcnuuwo = 251;
	double cifnidhxu = 38647;
	bool rmqemb = true;
	bool luurluparykpjk = false;
	double slxeksq = 53934;
	bool ldegehzfp = true;
	int rivvhhgzsfefd = 2847;
	int hhisqpnkwtttu = 2928;
	if (2928 == 2928) {
		int fwlmes;
		for (fwlmes = 32; fwlmes > 0; fwlmes--) {
			continue;
		}
	}
	if (2847 == 2847) {
		int lbvhxj;
		for (lbvhxj = 66; lbvhxj > 0; lbvhxj--) {
			continue;
		}
	}
	if (53934 != 53934) {
		int nrgy;
		for (nrgy = 36; nrgy > 0; nrgy--) {
			continue;
		}
	}
	return string("eqliwmtumqpjsqds");
}

bool uxsardt::wwwhxxvxyskivanxjkgjv(int zvffr, int hbrlegxopt, string ukvxvdopg, double vpcqkwg, double yuraxzjiwlqmk, int uhpnnkej, string pfcyyqeymkfv, string ulfuwjptegtvtc, double bnptmsmacca) {
	int qpquxhs = 6923;
	bool egcqcpkf = true;
	if (true == true) {
		int iswkrxgemj;
		for (iswkrxgemj = 86; iswkrxgemj > 0; iswkrxgemj--) {
			continue;
		}
	}
	if (true != true) {
		int qvub;
		for (qvub = 70; qvub > 0; qvub--) {
			continue;
		}
	}
	if (true == true) {
		int sluga;
		for (sluga = 6; sluga > 0; sluga--) {
			continue;
		}
	}
	if (true == true) {
		int qqfrv;
		for (qqfrv = 36; qqfrv > 0; qqfrv--) {
			continue;
		}
	}
	return false;
}

string uxsardt::epheflvafxowc(int zetyo, int nvwqhjq, string gvpwa, double cfedajterowr, double gvefqkvrhydwg, int ovbvawhl, bool guybmmidvbf, int srqxizrs, bool erhdooevdsdxi) {
	double yesau = 60267;
	int lodshttjojgs = 5815;
	bool bbghimarqflzi = true;
	string isudbmgcocc = "jcyvglomgtxowhnoywkgvhwpgkqayuxkmhs";
	double vmzmkreqmfeivw = 1551;
	bool vkuunempmbwatc = false;
	string osnlgwabeioa = "dkattjnymbbwjxedndcvvmlsncetponpkfmiazrtexmekuufufuoyjacchzmjbophkucnplkzyhshmzjmgpeqchaa";
	bool kqyym = false;
	string oeigzprxj = "vawncmjetnrzamsijdcetcirypcnlvqktjtkmlwspixnojzwhfjtmaaqlsy";
	double eozima = 25121;
	if (60267 == 60267) {
		int theexxah;
		for (theexxah = 39; theexxah > 0; theexxah--) {
			continue;
		}
	}
	if (string("vawncmjetnrzamsijdcetcirypcnlvqktjtkmlwspixnojzwhfjtmaaqlsy") == string("vawncmjetnrzamsijdcetcirypcnlvqktjtkmlwspixnojzwhfjtmaaqlsy")) {
		int gkhgkjc;
		for (gkhgkjc = 71; gkhgkjc > 0; gkhgkjc--) {
			continue;
		}
	}
	return string("gfyodiqvgwggv");
}

void uxsardt::ukiafqhtnhi(bool mmglthgloaxi, bool aqxwscrxjuvo, bool theis, double wdfoxf) {
	bool jxgfvuk = false;
	int jjgwqlymdpux = 945;
	int gxenyejknpihpt = 113;
	bool wlatdbqkbjgbhs = false;
	int plbtvuyybscyzv = 112;
	double badoeftlylhaody = 3438;
	double fvggibdnriepa = 40276;
	bool aexrqwaltetvrgt = false;
	if (945 == 945) {
		int kygkyimt;
		for (kygkyimt = 56; kygkyimt > 0; kygkyimt--) {
			continue;
		}
	}
	if (false != false) {
		int otocaww;
		for (otocaww = 7; otocaww > 0; otocaww--) {
			continue;
		}
	}
	if (40276 != 40276) {
		int imjdckybt;
		for (imjdckybt = 2; imjdckybt > 0; imjdckybt--) {
			continue;
		}
	}
	if (113 != 113) {
		int odvgp;
		for (odvgp = 4; odvgp > 0; odvgp--) {
			continue;
		}
	}
	if (945 != 945) {
		int zpcuahurgh;
		for (zpcuahurgh = 30; zpcuahurgh > 0; zpcuahurgh--) {
			continue;
		}
	}

}

bool uxsardt::quhctasoqps(double wuvqteny, string bwmiuwucjesrmwj) {
	return false;
}

double uxsardt::knizxgxuxnnkptpyqlpmuxttd(bool wtwjpfudmhoy, double wkuzdcjx, int tnxwaoxk, bool dfmruj, double odiirs) {
	bool obfxzkgricho = true;
	int ipfkb = 432;
	int xbihcutdepxswa = 2630;
	bool bxpwqpmcxtnrkah = false;
	double avxoqh = 42357;
	if (432 == 432) {
		int gnvii;
		for (gnvii = 39; gnvii > 0; gnvii--) {
			continue;
		}
	}
	if (true == true) {
		int vco;
		for (vco = 69; vco > 0; vco--) {
			continue;
		}
	}
	if (2630 == 2630) {
		int krfjneb;
		for (krfjneb = 99; krfjneb > 0; krfjneb--) {
			continue;
		}
	}
	if (2630 == 2630) {
		int xfymwpnrob;
		for (xfymwpnrob = 2; xfymwpnrob > 0; xfymwpnrob--) {
			continue;
		}
	}
	if (2630 == 2630) {
		int jyxjl;
		for (jyxjl = 14; jyxjl > 0; jyxjl--) {
			continue;
		}
	}
	return 58627;
}

string uxsardt::bnqxruiyfg(string cjdldf) {
	double wfvhzyvi = 7292;
	int cxxshehxlqnfa = 5680;
	double pjdfbn = 35514;
	int ibwebofjlzbspna = 9112;
	bool vlaqwog = false;
	double outhf = 21185;
	int xrajagwcsukl = 4318;
	if (7292 == 7292) {
		int fcmwi;
		for (fcmwi = 79; fcmwi > 0; fcmwi--) {
			continue;
		}
	}
	if (7292 == 7292) {
		int pfrncsfhu;
		for (pfrncsfhu = 94; pfrncsfhu > 0; pfrncsfhu--) {
			continue;
		}
	}
	return string("");
}

int uxsardt::whalgvjtgkt(int soopymmhrc, string dktssssqowmgih, double uhbluxub, double lbcrqcbdmhdgzby, double jelhcvwrtdrk) {
	string krljcq = "kyjjmkxtcdmdznlxasarrwxwnyzatkymzjnptqzatlnrlbdblwmpas";
	return 81974;
}

string uxsardt::aukortbakuhoelwe(int yukandqrmtsnjx, bool bwtnrsl, string rmgpa, bool wcyoqfgeoegc, string cuwwukthjwwf, string yyoof, int onbyxgptbmq, string sdvhuvzbnpywl, string gozpjbiq) {
	bool joiixbxavjj = true;
	string hgruojlnjhbrvuq = "zzhpmlxnxnft";
	if (string("zzhpmlxnxnft") == string("zzhpmlxnxnft")) {
		int xg;
		for (xg = 73; xg > 0; xg--) {
			continue;
		}
	}
	return string("eevveigoyzqi");
}

void uxsardt::zhtbkthnkoulffa(int jhbddijwg, double ngjvwz, int nsvxagtkmckjelz, int mdafomlbi, int tmlwi, int vfner, double efvcftmgefp, bool lohmzbxcn, double wwveqycaewrsrvc, string nsvtjxfrot) {
	int heieojasqh = 580;
	double bxtutqiwfssza = 31136;
	int mazveebdiwot = 6781;
	bool qlaxsdywaridwh = true;
	double hmpuyigeo = 31248;
	int dzgfuhqfibilrdp = 3056;
	double uctmxkhwbgs = 34635;
	int taohluwyqry = 418;
	int chwarmlatqxv = 4965;
	double jxkkchykgyvj = 10575;
	if (418 == 418) {
		int gghsuigowv;
		for (gghsuigowv = 59; gghsuigowv > 0; gghsuigowv--) {
			continue;
		}
	}
	if (6781 != 6781) {
		int mmzkfl;
		for (mmzkfl = 8; mmzkfl > 0; mmzkfl--) {
			continue;
		}
	}
	if (6781 == 6781) {
		int xcsqopkp;
		for (xcsqopkp = 28; xcsqopkp > 0; xcsqopkp--) {
			continue;
		}
	}

}

bool uxsardt::mfrxrbzpqottzpcad(int hoxshxofsenox, string vegmccprksiiuyd, double ozxgnxudlbigsxw, bool izetfglgujup, int fueykpzgvu, int xltdvovfwj, bool mgpwf) {
	double apowsgwykgwd = 5866;
	double njdyfgbwdolilcs = 5009;
	bool cpecz = false;
	string fkyiwf = "txzufoprlpseofvldfjkkdprtlsntbmqfhabjtvftpryktqfkfhqfubgurxliayjnxdwlohhzzvfttwynasybhxd";
	int mdozfbyvo = 4404;
	int bzfsglsdzl = 5245;
	string gstlgdgelorub = "ovixfdxizoizo";
	bool bbnykr = false;
	int suilrgh = 1992;
	double ddqpnkjqcxz = 6095;
	if (6095 == 6095) {
		int gc;
		for (gc = 73; gc > 0; gc--) {
			continue;
		}
	}
	return true;
}

void uxsardt::eeckdautyqdicqbyb(string kssgdcojrw, bool swxixssbbo, string jwqfnpuxmxvni, bool cenyx, int giobgy, string yxzmkqmxifadw, string ftkuxjxywj, string wmjrcpkzn, bool zvdotl, double wvokfq) {
	string rlfrdclhhaan = "tapcyonocmywtusvmhugbsfqxlhfvfpenklpjctbdorecsaecffnojqbzu";
	string dlqeq = "ettrtsozclmmuordgrlpghsmavwtjejbmgqkukeynqijfkh";
	bool tekwsjmyrpxas = true;

}

int uxsardt::wfiartrpuk(bool oycwkftjpl, int dizrvln, int cbgldlunophzqyd, int oywjdoyrcmxs, double hkuavr, string pwjtyoeuievsjy) {
	int xlejsgfceyv = 1368;
	bool szykukvcduqz = false;
	double tpzbzheapsiuvcd = 14749;
	if (false == false) {
		int oal;
		for (oal = 93; oal > 0; oal--) {
			continue;
		}
	}
	if (1368 == 1368) {
		int kxdye;
		for (kxdye = 37; kxdye > 0; kxdye--) {
			continue;
		}
	}
	if (14749 != 14749) {
		int phrjpskusk;
		for (phrjpskusk = 70; phrjpskusk > 0; phrjpskusk--) {
			continue;
		}
	}
	return 52822;
}

uxsardt::uxsardt() {
	this->mfrxrbzpqottzpcad(3694, string("ovnsfiacaktgtutlmmjrfghiwmzlxvcbkfvcvgmvmggayoojnzrkp"), 6504, true, 1677, 1588, true);
	this->eeckdautyqdicqbyb(string("gnbukvceh"), true, string("tokwvavrvplejmqwouadmxbbykbmeigpujlqbjtmdmbtreuntydnhknpurwzkfkcewxjqwkgevcqiajzzn"), false, 781, string("ukzvnmxabyozyvagoaeindmdhgckavhwgitnrljymuzwquxtbzirisolrzhdhrnqwptudauwalazjwvbedgevikfgovtgkvrqmc"), string("coxozrzwzwycymhnvympvxerslkftnbpyploulheltgichiffwlufdpixvvsekkuvoonaawqdqmsjofodcvozxreqjx"), string("zifoyvybsltsizqszcetf"), true, 84722);
	this->wfiartrpuk(true, 5607, 4497, 2722, 2390, string("ustcapzttbgadktxaysiawfrqpxsvswvcmekvopyeuurkevrxqlvjqwrtrrjoebcnrbiktwfakgtkwetfne"));
	this->epheflvafxowc(8540, 3556, string("acozanwmrdhznvgotkykygzoutbspncybgxspik"), 11394, 52295, 3527, false, 3735, true);
	this->ukiafqhtnhi(true, false, false, 87567);
	this->quhctasoqps(8477, string("stdkzhefnhwvazwmzmawjrvdcfklopbydxklwdxqpmmqbwzedbbzxnbrznhmswrhgpncamnfmratstgilcotr"));
	this->knizxgxuxnnkptpyqlpmuxttd(false, 14281, 2927, false, 64585);
	this->bnqxruiyfg(string("iejdxqrqjevohxivwlrnapovrjrajhgceh"));
	this->whalgvjtgkt(6332, string("diorbeaizvwrechkyxycumhvwmqbncpjjysthvhncvaauzapmomvypcjvuivssmjnsfleachmuyhxtdkymhsapahqueqieb"), 23690, 11086, 7540);
	this->aukortbakuhoelwe(96, false, string("klnyiemtwqkgsjcsokvmmron"), false, string("rpixorlmqdtmufwqxdjdzbbimelmiwnsrlqtvyaytisecgk"), string("rmidgowouu"), 297, string("vsuzyuqffxuzvamehxtazsxhiduxasvibwxyskeappowekrkkniayuqdaoicxw"), string("egpnzvsodnwvlrxstyltffnypuqmgjvxy"));
	this->zhtbkthnkoulffa(3071, 26051, 5091, 2937, 643, 263, 50997, false, 6861, string("kwlapabebdgasqyyhce"));
	this->qfqthzvbwcsuuiqslxvqyl(string("nyvwxacnljtidccqposxonvxgdlyubtqfgnlfjqevgstmdix"), string("tfwtdtouycmorzrdyyrpxikwsugehmkbcguzdgahichppsiwjbfeuucvfemnixwvslayphwxturbyyxpewczcavjxzqjcpco"), 5930, string("agblbpjwvzaabgnqbplylsumortije"), string("vtemfenmrghwezoirokwbpvxdlpjswwgklesmphwrtpdmjnatpvgrexeeexoaxukuei"), string("hhswhshdhzeoyelxsqmkcuhfte"), string("hfwp"), 86411, 24838, false);
	this->rotbtbtoglgiwnz(1328, true, 21130, string("qwvekxidtorqfykbvrdxlygseyqqmposzugvxertqxnqtqwzyuvsleatrzjiqcrcrsgkdnuszndambjabdbezuwha"), 6696, string("tjuazvtxusomqbnezazdryasjmfjrjkrdctc"), string("tuwwxzxbkmgslzljxiyruurmxbucxzjfvwhdkoserxbaxcmmvbatbvjugzfzjcfxbdbrc"), false, 10797);
	this->mpqsxaeicohcl(51147, 2994, true, 5651, 706, 4660, true, true);
	this->ovtdjabdebm(44706, 29489);
	this->xrngbwnywhiprwmxgnq();
	this->wwwhxxvxyskivanxjkgjv(4363, 511, string("iqckmuxjmtwfugldiiihomszxexhijwtbektmcqmxaggybksqikbfjjhhfj"), 76835, 2853, 5164, string("cifbrdezlbobgaimckagqxcimpkckudqrldztfkiemvzzmnkozujadbjiinkbehqclwwyo"), string("hqtlxotmmmksxpmbvtuyynktllmzoduipnghziunifsytenljtmydcuqgtlmkxlyfavpyakdukyewuf"), 38725);
}
#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

class qigqykx {
public:
	int dvjfd;
	double lezuapahurn;
	double kffrodbkqimdw;
	int tymlvbsvqm;
	string mihzfohpjy;
	qigqykx();
	int lkrddinzkdgxsikkdxfhpwnuk(int dwjaeps, int whabigeoejzhy, int rxnxptbvhj);
	string bwurnkpsge(double gfrfjpxcvmwychv, bool ytbufh, int wdylzfwkdkmah, string worwfrnn);
	string akivxabuwighq(bool uvnzoabsdhgfc, int okrosbheop, bool dmgozaj, int xxhzwbl, bool fqjnzzosmcky, string tmrwpfnzeqz, bool ghrsevati, double vlvkj, string mtfeilgpvqbsry);
	void rahrxudfpalrldtikoucutm(double nuyzxet, bool eddevyuoryj, int owiuuiwuv);

protected:
	double fytghtltirr;

	string igzgefpaeketdmkoktnxxt(string ppmlsnbpkwietu, bool bftojezdyzncx, string hoteg, bool mhhfoovgruce, bool uszwoebz, double bioavsyfeuij, double rystpdbdagh, string tnhngrqv, string ixkqashrvb, double oodqptbnqwxo);

private:
	double rydheypiazszw;
	bool gedohtu;
	string agvcbhddyct;
	double lizzvs;
	string dihffzmz;

	void foaqftbkqehyi(double rdtxeqipgnelbsa, string yppwavuy, bool wtixvczemivdiz);
	string ksfvmejfkyueby(string pwgynzbu, string mxtjrlrcsingtm, int tvvby, bool hyjxgsauxoiq, double ppujsxi);
	double lyjqwpgfjlwmypm(double kcuhatdyyj, int kekkscopqj, string yqludoxbfyqjxtz);
	void wlnaxbegenolisjka(bool buylfdufbbdbby, bool zedshusqfxete, bool emnqnrcsooa, int rdcxzvdnxvkbf, string hnxlkeowdacv);

};


void qigqykx::foaqftbkqehyi(double rdtxeqipgnelbsa, string yppwavuy, bool wtixvczemivdiz) {
	bool dhmhkxduj = false;
	double orwgzwy = 1573;
	string pwvaws = "ysutxfdbfubblzhhdhqtsgqhlzxzojumyaorqdmifqzpbxfxhkiaebr";

}

string qigqykx::ksfvmejfkyueby(string pwgynzbu, string mxtjrlrcsingtm, int tvvby, bool hyjxgsauxoiq, double ppujsxi) {
	bool pvtbkte = false;
	double rruqlgiduho = 60273;
	if (60273 != 60273) {
		int kjbwfqmw;
		for (kjbwfqmw = 68; kjbwfqmw > 0; kjbwfqmw--) {
			continue;
		}
	}
	if (false != false) {
		int zuc;
		for (zuc = 86; zuc > 0; zuc--) {
			continue;
		}
	}
	return string("rk");
}

double qigqykx::lyjqwpgfjlwmypm(double kcuhatdyyj, int kekkscopqj, string yqludoxbfyqjxtz) {
	double xxyweebv = 45417;
	bool gwbaszjavefbmdi = false;
	string dkqhrqofnn = "rdxavcvgkotuvdzgsyimvqhjkzpkybgwpswnolrrwxolfiamatkzqvvesprcsepdvhbbmruaqtcvivorbzvbzehojjd";
	double mkesxufrzune = 47097;
	int cxfjyegrhjgix = 5916;
	bool qhbwlqifv = true;
	if (false == false) {
		int bty;
		for (bty = 65; bty > 0; bty--) {
			continue;
		}
	}
	if (false == false) {
		int ncqxedyovz;
		for (ncqxedyovz = 42; ncqxedyovz > 0; ncqxedyovz--) {
			continue;
		}
	}
	if (string("rdxavcvgkotuvdzgsyimvqhjkzpkybgwpswnolrrwxolfiamatkzqvvesprcsepdvhbbmruaqtcvivorbzvbzehojjd") != string("rdxavcvgkotuvdzgsyimvqhjkzpkybgwpswnolrrwxolfiamatkzqvvesprcsepdvhbbmruaqtcvivorbzvbzehojjd")) {
		int hamuxltkh;
		for (hamuxltkh = 80; hamuxltkh > 0; hamuxltkh--) {
			continue;
		}
	}
	if (false != false) {
		int cycvicb;
		for (cycvicb = 64; cycvicb > 0; cycvicb--) {
			continue;
		}
	}
	if (5916 == 5916) {
		int zjehepjabq;
		for (zjehepjabq = 15; zjehepjabq > 0; zjehepjabq--) {
			continue;
		}
	}
	return 72566;
}

void qigqykx::wlnaxbegenolisjka(bool buylfdufbbdbby, bool zedshusqfxete, bool emnqnrcsooa, int rdcxzvdnxvkbf, string hnxlkeowdacv) {

}

string qigqykx::igzgefpaeketdmkoktnxxt(string ppmlsnbpkwietu, bool bftojezdyzncx, string hoteg, bool mhhfoovgruce, bool uszwoebz, double bioavsyfeuij, double rystpdbdagh, string tnhngrqv, string ixkqashrvb, double oodqptbnqwxo) {
	bool bmlyhvc = false;
	string erhelrfitayrp = "raqflkny";
	string qofxqibqgrxcf = "ifpksensdewsulcoorgbmhbbykjfwnadmavwlmhaaedjwcxqvruss";
	string cooftaf = "dkyfyeikoukprzksjrkxbuhbdbuerjleajtlvbntfalaakpxbhvbpaabmfvwgtdteffgslm";
	if (false == false) {
		int vebvtwnusa;
		for (vebvtwnusa = 5; vebvtwnusa > 0; vebvtwnusa--) {
			continue;
		}
	}
	if (string("ifpksensdewsulcoorgbmhbbykjfwnadmavwlmhaaedjwcxqvruss") == string("ifpksensdewsulcoorgbmhbbykjfwnadmavwlmhaaedjwcxqvruss")) {
		int fvmqzf;
		for (fvmqzf = 80; fvmqzf > 0; fvmqzf--) {
			continue;
		}
	}
	if (string("dkyfyeikoukprzksjrkxbuhbdbuerjleajtlvbntfalaakpxbhvbpaabmfvwgtdteffgslm") != string("dkyfyeikoukprzksjrkxbuhbdbuerjleajtlvbntfalaakpxbhvbpaabmfvwgtdteffgslm")) {
		int nepikehdxg;
		for (nepikehdxg = 100; nepikehdxg > 0; nepikehdxg--) {
			continue;
		}
	}
	return string("");
}

int qigqykx::lkrddinzkdgxsikkdxfhpwnuk(int dwjaeps, int whabigeoejzhy, int rxnxptbvhj) {
	int oesnumfqszruf = 623;
	bool hqnist = true;
	double ocsagetuvwtybr = 20683;
	bool xxuodwohiygakb = true;
	string geetgodqgwrnx = "bfotdchpimsscvaraamjwblcfhnqzlza";
	if (string("bfotdchpimsscvaraamjwblcfhnqzlza") == string("bfotdchpimsscvaraamjwblcfhnqzlza")) {
		int qhn;
		for (qhn = 37; qhn > 0; qhn--) {
			continue;
		}
	}
	if (string("bfotdchpimsscvaraamjwblcfhnqzlza") == string("bfotdchpimsscvaraamjwblcfhnqzlza")) {
		int acz;
		for (acz = 43; acz > 0; acz--) {
			continue;
		}
	}
	if (true == true) {
		int oeb;
		for (oeb = 30; oeb > 0; oeb--) {
			continue;
		}
	}
	if (true == true) {
		int sshl;
		for (sshl = 33; sshl > 0; sshl--) {
			continue;
		}
	}
	if (623 == 623) {
		int kjuxhn;
		for (kjuxhn = 11; kjuxhn > 0; kjuxhn--) {
			continue;
		}
	}
	return 11430;
}

string qigqykx::bwurnkpsge(double gfrfjpxcvmwychv, bool ytbufh, int wdylzfwkdkmah, string worwfrnn) {
	double iypevexdizx = 33797;
	if (33797 != 33797) {
		int xqlw;
		for (xqlw = 41; xqlw > 0; xqlw--) {
			continue;
		}
	}
	if (33797 == 33797) {
		int niq;
		for (niq = 22; niq > 0; niq--) {
			continue;
		}
	}
	if (33797 != 33797) {
		int jd;
		for (jd = 92; jd > 0; jd--) {
			continue;
		}
	}
	return string("kcgdjxrcwmwdfzwavrjf");
}

string qigqykx::akivxabuwighq(bool uvnzoabsdhgfc, int okrosbheop, bool dmgozaj, int xxhzwbl, bool fqjnzzosmcky, string tmrwpfnzeqz, bool ghrsevati, double vlvkj, string mtfeilgpvqbsry) {
	bool vpsniownnnklh = false;
	int nzrbrb = 2999;
	double ufrbyhjjsm = 29828;
	bool ieuvdebvi = false;
	double twyseqzhcly = 34713;
	string ypueaz = "susufujmuyykczawlknrpiufqpiqlsofryyeorwuloworyzpjttpurdmifnitdlzvyuazckslooxvp";
	double sexfm = 70505;
	bool qrjodie = false;
	if (false != false) {
		int burjjqvxvn;
		for (burjjqvxvn = 29; burjjqvxvn > 0; burjjqvxvn--) {
			continue;
		}
	}
	if (false == false) {
		int kjekzjr;
		for (kjekzjr = 4; kjekzjr > 0; kjekzjr--) {
			continue;
		}
	}
	if (70505 == 70505) {
		int zz;
		for (zz = 77; zz > 0; zz--) {
			continue;
		}
	}
	if (29828 != 29828) {
		int ufkypqqibv;
		for (ufkypqqibv = 29; ufkypqqibv > 0; ufkypqqibv--) {
			continue;
		}
	}
	return string("dmztlngzzo");
}

void qigqykx::rahrxudfpalrldtikoucutm(double nuyzxet, bool eddevyuoryj, int owiuuiwuv) {
	int gbsobp = 137;
	int xsnrsexwxpy = 3231;
	bool nssmxzkrtb = true;
	string hglkrtvy = "esdezems";
	bool pjdeqnjblxmebwl = false;
	int pjlykxhrjsu = 1678;
	string qdklywtpvsub = "wbucqoqdbtrojluqjdkhxcznmettqeldfzyjpaxggpreimsppwqdvxxbdyzqmvcfefoeaav";
	string cdxvpaadnye = "ljjtotmvvmbcgnliwroqkqpndomkprhroyycsejzoiohczhzmkrhiezmzxhidmoz";
	double wviqavwuvlsxuu = 5564;
	double qzvaeyjbmfx = 34748;

}

qigqykx::qigqykx() {
	this->lkrddinzkdgxsikkdxfhpwnuk(4734, 4539, 815);
	this->bwurnkpsge(309, true, 4005, string("pzzfvou"));
	this->akivxabuwighq(true, 2812, false, 3237, true, string("zjhnrnkyovxfmffgz"), false, 14518, string("mtcgozlnxkvraczfh"));
	this->rahrxudfpalrldtikoucutm(71695, false, 8655);
	this->igzgefpaeketdmkoktnxxt(string("mz"), false, string("svohenqytayupkskkppqjuworcsaxnoawsmgfhjmohxlyohzkkvcbrleuvehvpkzidmliknkzwjuljjtgbalgiggzooxeymxfoo"), true, true, 9462, 13434, string("xehizskvyvpvhnwdlkstpfquzdyaryxfv"), string("b"), 21031);
	this->foaqftbkqehyi(30110, string("fwrhpdyrzvypkalwynhiwiyxbpvppacilnpexwokwyvukbphswhqywbsrbn"), false);
	this->ksfvmejfkyueby(string("snqwsgajfijryqobjepigstddhmrimgn"), string("vcublfsdadyjylpnzmebhkqwuousbxaulukcgyokqhooulocbpfnnxtwhjmidmhltzgfsqkacydrmzfrfcij"), 1780, true, 16802);
	this->lyjqwpgfjlwmypm(23258, 647, string("pgocaxjgaqebsxgnlnboskqqkbpvukvtemgx"));
	this->wlnaxbegenolisjka(false, false, false, 677, string("cynsydhgmeatlgumnmjee"));
}
