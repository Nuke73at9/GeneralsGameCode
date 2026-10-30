/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// FILE: ConditionSoundClientUpdate.h /////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's sounds by model state and by animation frame (C&C4's
// ModelConditionAudioLoopClientBehavior and AnimationSoundClientBehavior), on the client only:
//
//   ClientUpdate = ConditionSoundClientUpdate ModuleTag_Sounds
//     ; while the drawable has every condition named (none: always) and none after EXCLUDE; a looping
//     ; sound plays until then, any other plays once as the state begins
//     Loop = GDI_CrawlerADefense_ShieldLoop USER_3 EXCLUDE DYING
//     ; as an animation (Skeleton.Animation, as in the draw) passes the frames given
//     AnimationSound = GDI_CrawlerBVehicle_FootstepMS GUX_CRAWLERB_SKL.GUX_CRAWLERB_MOVA 17 22 38 45 EXCLUDE DYING
//   End
//
// Nothing here reaches the logic: the sounds follow the drawable's conditions and its draw's
// animation, which the logic never reads.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/AudioHandleSpecialValues.h"
#include "Common/ClientUpdateModule.h"
#include "Common/ModelState.h"

#include <vector>

//-------------------------------------------------------------------------------------------------
class ConditionSoundClientUpdateModuleData : public ClientUpdateModuleData
{
public:
	struct Loop
	{
		AsciiString sound;
		ModelConditionFlags required;
		ModelConditionFlags excluded;
	};
	struct AnimationSound
	{
		AsciiString sound;
		AsciiString animation;			///< upper case
		std::vector<Int> frames;
		ModelConditionFlags excluded;
	};
	std::vector<Loop> m_loops;
	std::vector<AnimationSound> m_animationSounds;

	static void buildFieldParse( MultiIniFieldParse& p );
	static void parseLoop( INI *ini, void *instance, void *store, const void *userData );
	static void parseAnimationSound( INI *ini, void *instance, void *store, const void *userData );
};

//-------------------------------------------------------------------------------------------------
class ConditionSoundClientUpdate : public ClientUpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ConditionSoundClientUpdate, "ConditionSoundClientUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ConditionSoundClientUpdate, ConditionSoundClientUpdateModuleData );

public:

	ConditionSoundClientUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual void clientUpdate() override;

protected:

	void stopAll();

	std::vector<AudioHandle> m_loopHandles;		///< per Loop: playing, or AHSV_NoSound
	std::vector<Bool> m_loopActive;
	AsciiString m_lastAnimation;
	Real m_lastFrame;

};
