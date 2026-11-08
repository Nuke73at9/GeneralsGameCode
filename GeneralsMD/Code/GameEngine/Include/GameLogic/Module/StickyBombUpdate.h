/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: StickyBombUpdate.h ////////////////////////////////////////////////////////////////////////
// Author: Kris Morness, July 2002
// Desc:   Similar to ParachuteContain, this module is used essentially to attach a bomb to an object
//         moving around. The sticky bomb position simply updates to the specified bone until it explodes.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"

class WeaponTemplate;
class FXList;

//-------------------------------------------------------------------------------------------------
class StickyBombUpdateModuleData : public UpdateModuleData
{
public:
	AsciiString			m_attachToBone;
	Real						m_offsetZ;
	WeaponTemplate*	m_geometryBasedDamageWeaponTemplate;
	FXList*					m_geometryBasedDamageFX;
	Bool						m_keepProducer;		///< Tiberium Project: the one who made it stays its producer (not the target)
	Bool						m_dieWithTarget;	///< Tiberium Project: dies (its death modules run) when the target dies, not just vanishes

	StickyBombUpdateModuleData()
	{
		m_offsetZ = 10.0f;
		m_geometryBasedDamageWeaponTemplate = nullptr;
		m_geometryBasedDamageFX = nullptr;
		m_keepProducer = FALSE;
		m_dieWithTarget = FALSE;
	}

	static void buildFieldParse(MultiIniFieldParse& p)
	{
    UpdateModuleData::buildFieldParse(p);
		static const FieldParse dataFieldParse[] =
		{
			{ "AttachToTargetBone",				INI::parseAsciiString,		nullptr, offsetof( StickyBombUpdateModuleData, m_attachToBone ) },
			{ "OffsetZ",									INI::parseReal,						nullptr, offsetof( StickyBombUpdateModuleData, m_offsetZ ) },
			{ "GeometryBasedDamageWeapon",INI::parseWeaponTemplate, nullptr, offsetof( StickyBombUpdateModuleData, m_geometryBasedDamageWeaponTemplate ) },
			{ "GeometryBasedDamageFX",		INI::parseFXList,					nullptr, offsetof( StickyBombUpdateModuleData, m_geometryBasedDamageFX ) },
			// Tiberium Project @feature a charge stuck to its target by a weapon (C&C4's AttachToTarget): the
			// one who fired it stays its producer, so its damage spares him and not the target; and it dies
			// with the target (its death weapons and effects play) instead of vanishing
			{ "KeepProducer",							INI::parseBool,						nullptr, offsetof( StickyBombUpdateModuleData, m_keepProducer ) },
			{ "DieWithTarget",						INI::parseBool,						nullptr, offsetof( StickyBombUpdateModuleData, m_dieWithTarget ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
    p.add(dataFieldParse);
	}
};

//-------------------------------------------------------------------------------------------------
class StickyBombUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( StickyBombUpdate, "StickyBombUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( StickyBombUpdate, StickyBombUpdateModuleData )

public:

	StickyBombUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual void onObjectCreated() override;
#if !RETAIL_COMPATIBLE_CRC
	virtual void onDelete() override;
#endif

	virtual UpdateSleepTime update() override;							///< called once per frame

	void initStickyBomb( Object *object, const Object *bomber, const Coord3D *specificPos = nullptr );
	void detonate();
	Bool isTimedBomb() const { return m_dieFrame > 0; }
	UnsignedInt getDetonationFrame() const { return m_dieFrame; }
	Object* getTargetObject() const;
	void setTargetObject( Object *obj );

private:

	ObjectID			m_targetID;
	UnsignedInt		m_dieFrame;
	UnsignedInt   m_nextPingFrame;
};
