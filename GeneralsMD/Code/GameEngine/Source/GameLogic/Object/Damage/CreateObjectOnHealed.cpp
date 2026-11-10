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

// FILE: CreateObjectOnHealed.cpp /////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's CreateObjectOnHealed: see CreateObjectOnHealed.h
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CreateObjectOnHealed.h"

//-------------------------------------------------------------------------------------------------
CreateObjectOnHealedModuleData::CreateObjectOnHealedModuleData()
{
	m_ocl = nullptr;
	m_destroyParent = TRUE;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void CreateObjectOnHealedModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	DamageModuleData::buildFieldParse( p );

	static const FieldParse dataFieldParse[] =
	{
		{ "CreationList",			INI::parseObjectCreationList,				nullptr, offsetof( CreateObjectOnHealedModuleData, m_ocl ) },
		{ "DestroyParent",		INI::parseBool,											nullptr, offsetof( CreateObjectOnHealedModuleData, m_destroyParent ) },
		{ "UpgradeRequired",	INI::parseAsciiStringVectorAppend,	nullptr, offsetof( CreateObjectOnHealedModuleData, m_upgradeRequired ) },
		{ "UpgradeForbidden",	INI::parseAsciiStringVectorAppend,	nullptr, offsetof( CreateObjectOnHealedModuleData, m_upgradeForbidden ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
CreateObjectOnHealed::CreateObjectOnHealed( Thing *thing, const ModuleData* moduleData ) : DamageModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
CreateObjectOnHealed::~CreateObjectOnHealed()
{
}

//-------------------------------------------------------------------------------------------------
/** healed whole: what it makes, made (its player's), and it goes */
//-------------------------------------------------------------------------------------------------
void CreateObjectOnHealed::onHealing( DamageInfo * /*damageInfo*/ )
{
	Object *me = getObject();
	if( me->isDestroyed() || me->isEffectivelyDead() )
		return;

	const BodyModuleInterface *body = me->getBodyModule();
	if( body == nullptr || body->getHealth() < body->getMaxHealth() )
		return;

	const CreateObjectOnHealedModuleData *d = getCreateObjectOnHealedModuleData();
	for( std::vector<AsciiString>::const_iterator it = d->m_upgradeRequired.begin(); it != d->m_upgradeRequired.end(); ++it )
	{
		if( !me->hasUpgradeNamed( *it ) )
			return;
	}
	for( std::vector<AsciiString>::const_iterator it = d->m_upgradeForbidden.begin(); it != d->m_upgradeForbidden.end(); ++it )
	{
		if( me->hasUpgradeNamed( *it ) )
			return;
	}

	if( d->m_ocl != nullptr )
		ObjectCreationList::create( d->m_ocl, me, me );

	if( d->m_destroyParent )
		TheGameLogic->destroyObject( me );
}

//-------------------------------------------------------------------------------------------------
void CreateObjectOnHealed::crc( Xfer *xfer )
{
	DamageModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
//-------------------------------------------------------------------------------------------------
void CreateObjectOnHealed::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	DamageModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void CreateObjectOnHealed::loadPostProcess()
{
	DamageModule::loadPostProcess();
}
