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

// FILE: StealthDetectorUpgrade.h /////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's stealth detector that an upgrade turns on (its StealthDetectorUpdate's
// TriggeredBy): the object's StealthDetectorUpdate (InitiallyDisabled) starts detecting once the upgrade is
// given, and stops again if it is taken off (UndoWhenRemoved). Kane's Wrath's Avatar detects stealth after
// taking over an Attack Bike.
//
//   Behavior = StealthDetectorUpdate ModuleTag_Detector
//     DetectionRange    = 272
//     InitiallyDisabled = Yes
//   End
//   Behavior = StealthDetectorUpgrade ModuleTag_DetectorOn
//     TriggeredBy = KWUp_Upgrade_AvatarStealthDetect
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpgradeModule.h"

class Thing;

//-------------------------------------------------------------------------------------------------
class StealthDetectorUpgrade : public UpgradeModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( StealthDetectorUpgrade, "StealthDetectorUpgrade" )
	MAKE_STANDARD_MODULE_MACRO( StealthDetectorUpgrade );

public:

	StealthDetectorUpgrade( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype defined by MemoryPoolObject

protected:
	virtual void upgradeImplementation() override;			///< the detector on
	virtual void undoUpgradeImplementation() override;		///< and off again (UndoWhenRemoved)
	virtual Bool isSubObjectsUpgrade() override { return false; }

private:
	void setDetecting( Bool on );

};
