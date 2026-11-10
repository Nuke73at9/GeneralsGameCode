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

// FILE: EngineerContain.h ////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's engineer's way in (Kane's Wrath's EngineerContain, C&C4's CaptureContain):
// the riders it names (AllowedRiders) walk in, whoever's it is, one at a time, and are used up doing what it
// does as they come in:
//   - with ReplaceWith, it is raised anew for the rider's side and goes (a husk becomes the walker or the
//     vehicle again: Kane's Wrath's ReplaceWith, C&C4's OnCaptureOCL);
//   - else a friendly rider mends it whole (RepairAllies: Kane's Wrath's engineer repairs a building);
//   - and a hostile or neutral rider takes it for its side (CaptureEnemies: an engineer captures a building).
// The Allow*Inside fields say whose riders come in (SAGE's CanEnterFilter Relationship).
//
//   Behavior = EngineerContain ModuleTag_Husk
//     AllowedRiders       = KW_GDIEngineer KW_NODSaboteur
//     ReplaceWith         = KWOCL_GDIJuggernaughtHuskReplace
//     ReplaceFX           = KWFX_FX_Husk_Repaired
//     CaptureAndReplaceFX = KWFX_FX_Husk_Captured
//     HostileEnterVoice   = VoiceCaptureHusk
//   End
// FriendlyEnterVoice and HostileEnterVoice name the per-unit voice (UnitSpecificSounds) a rider says as it is
// ordered in (Kane's Wrath's NameOfVoiceToUseForFriendlyEnter and HostileEnter, C&C4's OnCaptureNameOfVoiceToUse).
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/OpenContain.h"

class FXList;
class ObjectCreationList;

//-------------------------------------------------------------------------------------------------
class EngineerContainModuleData : public OpenContainModuleData
{
public:
	std::vector<AsciiString>		m_allowedRiders;			///< who may come in, by name (none: anyone the kinds and sides let in)
	const ObjectCreationList*		m_replaceWith;				///< raised anew as this, for the rider's side
	Bool												m_destroyContainer;		///< and gone once raised (Yes)
	Bool												m_destroyRider;				///< the rider used up (Yes)
	Bool												m_captureEnemies;			///< a hostile or neutral rider takes it (Yes)
	Bool												m_repairAllies;				///< a friendly rider mends it whole (Yes)
	const FXList*								m_repairFX;
	const FXList*								m_captureFX;
	const FXList*								m_replaceFX;					///< raised by a friendly rider
	const FXList*								m_captureAndReplaceFX;	///< raised by a hostile or neutral rider
	AsciiString									m_friendlyEnterVoice;	///< the per-unit voice of a friend ordered in (VoiceEngineerRepair)
	AsciiString									m_hostileEnterVoice;	///< and of a foe or no one's (VoiceCaptureBuilding, VoiceCaptureHusk)

	EngineerContainModuleData();

	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class EngineerContain : public OpenContain
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( EngineerContain, "EngineerContain" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( EngineerContain, EngineerContainModuleData )

public:

	EngineerContain( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual Bool isEngineerContain() const override { return TRUE; }
	virtual const char *getEnterVoiceName( Bool hostile ) const override;
	virtual Bool isValidContainerFor( const Object* obj, Bool checkCapacity ) const override;
	virtual void onContaining( Object *rider, Bool wasSelected ) override;
	virtual Bool isKickOutOnCapture() override { return FALSE; }	///< the rider is used up as it takes it

protected:

	Bool isAllowedRider( const Object *rider ) const;

};
