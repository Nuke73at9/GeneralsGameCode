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

// FILE: StrafeAreaUpdate.h ///////////////////////////////////////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's StrafeAreaUpdate and RunOffMapBehavior: an aircraft a power
// calls in (the Orcas of a bombing run, the Vertigos that drop mines or seed Tiberium) flies over the
// spot its object creation list sends it to (OrderAfterCreation = STRAFE), firing Weapon at points
// within Radius of it (TargetJitter more or less) from PreattackDistance before it until it is past,
// then flies on off the map and is gone.
//
//   Behavior = StrafeAreaUpdate ModuleTag_Strafe
//     Weapon            = KWW_GDIBombingRunOrcaWeapon
//     PreattackDistance = 500
//     Radius            = 50
//     TargetJitter      = 50
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"

class WeaponTemplate;

//-------------------------------------------------------------------------------------------------
class StrafeAreaUpdateModuleData : public UpdateModuleData
{
public:
	const WeaponTemplate *m_weapon;
	Real m_preattackDistance;			///< it fires from this far before the spot
	Real m_radius;								///< ... at points this far around it, and until this far past it
	Real m_targetJitter;					///< the points' spread beyond Radius

	StrafeAreaUpdateModuleData()
	{
		m_weapon = nullptr;
		m_preattackDistance = 200.0f;
		m_radius = 50.0f;
		m_targetJitter = 0.0f;
	}

	static void buildFieldParse( MultiIniFieldParse& p )
	{
		UpdateModuleData::buildFieldParse( p );
		static const FieldParse dataFieldParse[] =
		{
			{ "Weapon",							INI::parseWeaponTemplate,	nullptr, offsetof( StrafeAreaUpdateModuleData, m_weapon ) },
			{ "PreattackDistance",	INI::parseReal,						nullptr, offsetof( StrafeAreaUpdateModuleData, m_preattackDistance ) },
			{ "Radius",							INI::parseReal,						nullptr, offsetof( StrafeAreaUpdateModuleData, m_radius ) },
			{ "TargetJitter",				INI::parseReal,						nullptr, offsetof( StrafeAreaUpdateModuleData, m_targetJitter ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add( dataFieldParse );
	}
};

//-------------------------------------------------------------------------------------------------
class StrafeAreaUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( StrafeAreaUpdate, "StrafeAreaUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( StrafeAreaUpdate, StrafeAreaUpdateModuleData )

public:
	StrafeAreaUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

	/// fly over the spot, firing, and on off the map
	void strafe( const Coord3D *target );

	/// the object's strafe module, if it has one
	static StrafeAreaUpdate *find( Object *obj );

private:
	enum { STRAFE_IDLE = 0, STRAFE_RUN = 1, STRAFE_LEAVING = 2 };
	void leave();

	Int m_state;
	Coord3D m_target;
	Coord3D m_dir;								///< along its run (2D, unit)
	UnsignedInt m_nextShotFrame;
	UnsignedInt m_startFrame;
};
