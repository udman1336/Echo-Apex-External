#pragma once

#define OFFSET_ENTITYLIST		    0x641a428 //cl_entitylist
#define OFFSET_LOCAL_ENT			0x253a070 //LocalPlayer
#define OFFSET_LEVELNAME            0x1e56864 //
#define OFFSET_INATTACK				0x3f331a8 //+attack

#define OFFSET_NAME_INDEX			0x38
#define OFFSET_TEAM					0x0330 //m_iTeamNum
#define OFFSET_HEALTH				0x0320 //m_iHealth
#define OFFSET_SHIELD				0x0190  //m_shieldHealth
#define OFFSET_SHIELDMAX			0x194 ////m_shieldHealthMax
#define OFFSET_NAME					0x0481 //[RecvTable.DT_BaseEntity] m_iName
#define OFFSET_VISIBLE_TIME         0x1a84 //lastVisibleTime

#define OFFSET_NAMELIST				0x8e99ab0 //NameList

#define OFFSET_LIFE_STATE			0x0698 //m_lifeState
#define OFFSET_BLEED_OUT_STATE		0x27d8 //m_bleedoutState
#define OFFSET_studioHdr 0x1010 //studioHdr

#define OFFSET_CURRENT_WEAPON		0x1978 //m_latestPrimaryWeapons
#define OFFSET_CURRENT_WEAPONID		0x1890 //m_weaponNameIndex
#define OFFSET_WEAPON				0x1a70 //m_activeWeapon
#define OFFSET_BULLET_SPEED			0x28e0 //m_flProjectileSpeed
#define OFFSET_BULLET_SCALE			0x3b0 //m_flProjectileScale
#define OFFSET_ZOOM_FOV				0x1e68 //m_zoomFOV
#define OFFSET_AMMO					0x1620 //m_ammoInClip
#define OFFSET_ORIGIN				0x16c  //[DataMap.CBaseViewModel]m_vecAbsOrigin
#define OFFSET_BONES				0xe10 ////m_nForceBone
#define OFFSET_AIMPUNCH				0x2510 //m_currentFrameLocalPlayer.m_vecPunchWeapon_Angle
#define OFFSET_CAMERAPOS			0x1fbc //CPlayer!camera_origin
#define OFFSET_VIEWANGLES			0x25f8 //m_viewangle
#define OFFSET_SWAYANGLES			0x25f0 //m_swayangle

#define OFFSET_MATRIX				0x11a390  //ViewMatrix
#define OFFSET_RENDER				0x3f30868 //ViewRender
#define OFFSET_ITEMID				0x15f4  //m_customScriptInt
#define m_currentFramePlayertimeBase 0x2160 //m_currentFramePlayer.timeBase
#define OFFSET_SIGNIFIER			0x0478		// m_iSignifierName
#define OFFSET_ABS_VELOCITY			0x0374		// C_Player : m_vecAbsVelocity

// Highlight offsets (Apex uses highlight system, not traditional glow)
#define OFFSET_HIGHLIGHT_SETTINGS	0x6b87500
#define OFFSET_HIGHLIGHT_GENERIC_CONTEXTS 0x295
#define OFFSET_HIGHLIGHT_TEAM_BITS	0x1e8
#define OFFSET_HIGHLIGHT_TEAM_INDEX	0x1d8
#define OFFSET_HIGHLIGHT_FOCUSED	0x29d