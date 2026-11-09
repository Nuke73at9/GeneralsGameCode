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


// FILE: UpgradeCrateCollide.cpp //////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's upgrade and veterancy crates (see the header)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/UpgradeCrateCollide.h"
#include "GameLogic/Object.h"

//-------------------------------------------------------------------------------------------------
UpgradeCrateCollideModuleData::UpgradeCrateCollideModuleData()
{
	m_levelsToGain = 1;
	m_healPerLevel = 0.0f;
	m_healAtMaxLevel = 0.0f;
	m_pickupFX = nullptr;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void UpgradeCrateCollideModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	CrateCollideModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "UpgradeToGrant",				INI::parseAsciiString,				nullptr, offsetof( UpgradeCrateCollideModuleData, m_upgradeName ) },
		{ "LevelsToGain",					INI::parseInt,								nullptr, offsetof( UpgradeCrateCollideModuleData, m_levelsToGain ) },
		{ "AllowedCollectors",		INI::parseAsciiStringVector,	nullptr, offsetof( UpgradeCrateCollideModuleData, m_allowedCollectors ) },
		{ "ForbiddenCollectors",	INI::parseAsciiStringVector,	nullptr, offsetof( UpgradeCrateCollideModuleData, m_forbiddenCollectors ) },
		{ "HealPerLevel",					INI::parsePercentToReal,			nullptr, offsetof( UpgradeCrateCollideModuleData, m_healPerLevel ) },
		{ "HealAtMaxLevel",				INI::parsePercentToReal,			nullptr, offsetof( UpgradeCrateCollideModuleData, m_healAtMaxLevel ) },
		{ "PickupFX",							INI::parseFXList,							nullptr, offsetof( UpgradeCrateCollideModuleData, m_pickupFX ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
UpgradeCrateCollide::UpgradeCrateCollide( Thing *thing, const ModuleData* moduleData ) : CrateCollide( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
UpgradeCrateCollide::~UpgradeCrateCollide()
{
}

//-------------------------------------------------------------------------------------------------
static Bool namedIn( const Object *obj, const std::vector<AsciiString>& names )
{
	const ThingTemplate *tmpl = obj->getTemplate();
	for( size_t i = 0; i < names.size(); ++i )
	{
		const ThingTemplate *named = TheThingFactory->findTemplate( names[ i ] );
		if( named != nullptr && tmpl->isEquivalentTo( named ) )
			return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool UpgradeCrateCollide::canGain( const Object *other ) const
{
	const UpgradeCrateCollideModuleData *d = getUpgradeCrateCollideModuleData();
	if( d->m_upgradeName.isNotEmpty() )
	{
		const UpgradeTemplate *up = TheUpgradeCenter->findUpgrade( d->m_upgradeName );
		return up != nullptr && up->getUpgradeType() == UPGRADE_TYPE_OBJECT && !other->hasUpgrade( up );
	}
	const ExperienceTracker *et = other->getExperienceTracker();
	return et != nullptr && et->isTrainable() && d->m_levelsToGain > 0 && et->canGainExpForLevel( d->m_levelsToGain );
}

//-------------------------------------------------------------------------------------------------
Bool UpgradeCrateCollide::isValidToExecute( const Object *other ) const
{
	const UpgradeCrateCollideModuleData *d = getUpgradeCrateCollideModuleData();
	if( !CrateCollide::isValidToExecute( other ) )
		return FALSE;
	if( other->isEffectivelyDead() || other->isSignificantlyAboveTerrain() )
		return FALSE;
	if( !d->m_allowedCollectors.empty() && !namedIn( other, d->m_allowedCollectors ) )
		return FALSE;
	if( !d->m_forbiddenCollectors.empty() && namedIn( other, d->m_forbiddenCollectors ) )
		return FALSE;
	// one that can gain nothing takes it only to heal
	return canGain( other ) || d->m_healAtMaxLevel > 0.0f;
}

//-------------------------------------------------------------------------------------------------
Bool UpgradeCrateCollide::executeCrateBehavior( Object *other )
{
	const UpgradeCrateCollideModuleData *d = getUpgradeCrateCollideModuleData();
	Real heal = d->m_healAtMaxLevel;
	if( canGain( other ) )
	{
		if( d->m_upgradeName.isNotEmpty() )
		{
			other->giveUpgrade( TheUpgradeCenter->findUpgrade( d->m_upgradeName ) );
			heal = d->m_healPerLevel;
		}
		else
		{
			other->getExperienceTracker()->gainExpForLevel( d->m_levelsToGain, TRUE );
			heal = d->m_healPerLevel * (Real)d->m_levelsToGain;
		}
	}
	BodyModuleInterface *body = other->getBodyModule();
	if( heal > 0.0f && body != nullptr )
		other->attemptHealing( body->getMaxHealth() * heal, getObject() );
	if( d->m_pickupFX != nullptr )
		FXList::doFXObj( d->m_pickupFX, other );
	return TRUE;
}

// ------------------------------------------------------------------------------------------------
void UpgradeCrateCollide::crc( Xfer *xfer )
{
	CrateCollide::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void UpgradeCrateCollide::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	CrateCollide::xfer( xfer );
}

// ------------------------------------------------------------------------------------------------
void UpgradeCrateCollide::loadPostProcess()
{
	CrateCollide::loadPostProcess();
}
