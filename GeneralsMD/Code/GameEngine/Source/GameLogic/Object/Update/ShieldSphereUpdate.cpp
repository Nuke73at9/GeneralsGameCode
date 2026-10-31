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

// FILE: ShieldSphereUpdate.cpp ///////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's shield spheres (see ShieldSphereUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/ShieldSphereUpdate.h"

#include "Common/Player.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/BodyModule.h"

static Bool s_reflecting = FALSE;	///< a reflected hit is not taken by another shield (no ping-pong)

std::vector<ShieldSphereUpdate *> ShieldSphereUpdate::s_shields;

static const UnsignedInt SHIELD_MEND_FRAMES = LOGICFRAMES_PER_SECOND / 2;

//-------------------------------------------------------------------------------------------------
ShieldSphereUpdateModuleData::ShieldSphereUpdateModuleData()
{
	m_initiallyActive = FALSE;
	m_radius = 0.0f;
	m_maxDamage = 0.0f;
	m_regenPerSecond = 0.0f;
	m_raiseAtPercent = 1.0f;
	m_damageTypes = DAMAGE_TYPE_FLAGS_ALL;
	m_absorbKillingBlow = FALSE;
	m_includeSelf = TRUE;
	m_reactChance = 1.0f;
	m_damageArc = 2.0f * PI;
	m_reflectDamage = FALSE;
	m_hitFX = nullptr;
	m_bonusMaxDamage = 0.0f;
}

//-------------------------------------------------------------------------------------------------
void ShieldSphereUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	static const FieldParse dataFieldParse[] =
	{
		{ "StartsActive",				INI::parseBool,							nullptr, offsetof( ShieldSphereUpdateModuleData, m_initiallyActive ) },
		{ "Radius",							INI::parseReal,							nullptr, offsetof( ShieldSphereUpdateModuleData, m_radius ) },
		{ "MaxDamage",					INI::parseReal,							nullptr, offsetof( ShieldSphereUpdateModuleData, m_maxDamage ) },
		{ "RegenPerSecond",			INI::parseReal,							nullptr, offsetof( ShieldSphereUpdateModuleData, m_regenPerSecond ) },
		{ "RaiseAtPercent",			INI::parsePercentToReal,		nullptr, offsetof( ShieldSphereUpdateModuleData, m_raiseAtPercent ) },
		{ "DamageTypes",				INI::parseDamageTypeFlags,	nullptr, offsetof( ShieldSphereUpdateModuleData, m_damageTypes ) },
		{ "AbsorbKillingBlow",	INI::parseBool,							nullptr, offsetof( ShieldSphereUpdateModuleData, m_absorbKillingBlow ) },
		{ "IncludeSelf",				INI::parseBool,							nullptr, offsetof( ShieldSphereUpdateModuleData, m_includeSelf ) },
		{ "ReactChance",				INI::parsePercentToReal,		nullptr, offsetof( ShieldSphereUpdateModuleData, m_reactChance ) },
		{ "DamageArc",					INI::parseAngleReal,				nullptr, offsetof( ShieldSphereUpdateModuleData, m_damageArc ) },
		{ "ReflectDamage",			INI::parseBool,							nullptr, offsetof( ShieldSphereUpdateModuleData, m_reflectDamage ) },
		{ "HitFX",							INI::parseFXList,						nullptr, offsetof( ShieldSphereUpdateModuleData, m_hitFX ) },
		{ "BonusUpgrade",				INI::parseAsciiString,			nullptr, offsetof( ShieldSphereUpdateModuleData, m_bonusUpgrade ) },
		{ "BonusMaxDamage",			INI::parseReal,							nullptr, offsetof( ShieldSphereUpdateModuleData, m_bonusMaxDamage ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse( p );
	p.add( dataFieldParse );
	p.add( UpgradeMuxData::getFieldParse(), offsetof( ShieldSphereUpdateModuleData, m_upgradeMuxData ) );
}

//-------------------------------------------------------------------------------------------------
ShieldSphereUpdate::ShieldSphereUpdate( Thing *thing, const ModuleData* moduleData ) :
	UpdateModule( thing, moduleData ),
	m_damage( 0.0f ),
	m_down( FALSE )
{
	s_shields.push_back( this );
	if( getShieldSphereUpdateModuleData()->m_initiallyActive )
		giveSelfUpgrade();
	setWakeFrame( getObject(), UPDATE_SLEEP( SHIELD_MEND_FRAMES ) );
}

//-------------------------------------------------------------------------------------------------
ShieldSphereUpdate::~ShieldSphereUpdate()
{
	for( std::vector<ShieldSphereUpdate *>::iterator it = s_shields.begin(); it != s_shields.end(); ++it )
	{
		if( *it == this )
		{
			s_shields.erase( it );
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void ShieldSphereUpdate::upgradeImplementation()
{
	m_damage = 0.0f;
	m_down = FALSE;
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature the interface's reading of an object's shield (TiberiumHUD's panel)
Bool ShieldSphereUpdate::getShieldOf( const Object *obj, Real *left, Real *most, Bool *up )
{
	if( obj == nullptr )
		return FALSE;
	static const NameKeyType key = NAMEKEY( "ShieldSphereUpdate" );
	const ShieldSphereUpdate *shield = static_cast<const ShieldSphereUpdate *>( const_cast<Object *>( obj )->findUpdateModule( key ) );
	if( shield == nullptr || !shield->isAlreadyUpgraded() )
		return FALSE;
	const Real full = shield->getMaxDamage();
	if( full <= 0.0f )
		return FALSE;
	*most = full;
	*left = shield->m_damage >= full ? 0.0f : full - shield->m_damage;
	*up = !shield->m_down;
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Real ShieldSphereUpdate::getMaxDamage() const
{
	const ShieldSphereUpdateModuleData *d = getShieldSphereUpdateModuleData();
	Real most = d->m_maxDamage;
	if( d->m_bonusMaxDamage != 0.0f && d->m_bonusUpgrade.isNotEmpty() && TheUpgradeCenter != nullptr )
	{
		const UpgradeTemplate *up = TheUpgradeCenter->findUpgrade( d->m_bonusUpgrade );
		const Object *me = getObject();
		if( up != nullptr )
		{
			const Player *player = me->getControllingPlayer();
			const Bool had = up->getUpgradeType() == UPGRADE_TYPE_PLAYER
				? ( player != nullptr && player->hasUpgradeComplete( up ) )
				: me->hasUpgrade( up );
			if( had )
				most += d->m_bonusMaxDamage;
		}
	}
	return most;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime ShieldSphereUpdate::update()
{
	const ShieldSphereUpdateModuleData *d = getShieldSphereUpdateModuleData();
	if( m_damage > 0.0f && d->m_regenPerSecond > 0.0f )
	{
		m_damage -= d->m_regenPerSecond * (Real)SHIELD_MEND_FRAMES / (Real)LOGICFRAMES_PER_SECOND;
		if( m_damage < 0.0f )
			m_damage = 0.0f;
	}
	const Real most = getMaxDamage();
	if( m_down && ( most <= 0.0f || m_damage <= most * ( 1.0f - d->m_raiseAtPercent ) ) )
		m_down = FALSE;
	return UPDATE_SLEEP( SHIELD_MEND_FRAMES );
}

//-------------------------------------------------------------------------------------------------
/** this shield takes the hit on the victim, if it is up, the victim inside and the hit from outside */
Bool ShieldSphereUpdate::takeHit( Object *victim, Object *source, DamageType type, Real amount )
{
	const ShieldSphereUpdateModuleData *d = getShieldSphereUpdateModuleData();
	Object *me = getObject();
	if( m_down || !isAlreadyUpgraded() || !getDamageTypeFlag( d->m_damageTypes, type ) )
		return FALSE;
	if( me->isEffectivelyDead() || me->isDisabled() || me->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		return FALSE;

	// the victim: itself, or a friend inside the bubble
	if( victim == me )
	{
		if( !d->m_includeSelf )
			return FALSE;
	}
	else
	{
		if( d->m_radius <= 0.0f || me->getRelationship( victim ) == ENEMIES )
			return FALSE;
		if( ThePartitionManager->getDistanceSquared( me, victim, FROM_CENTER_2D ) > sqr( d->m_radius ) )
			return FALSE;
	}

	// the hit: from outside the bubble, and from within its arc
	if( source != nullptr )
	{
		if( source == me )
			return FALSE;
		const Real r = max( d->m_radius, me->getGeometryInfo().getBoundingCircleRadius() );
		if( ThePartitionManager->getDistanceSquared( me, source, FROM_CENTER_2D ) <= sqr( r ) )
			return FALSE;
		if( d->m_damageArc < 2.0f * PI - 0.001f )
		{
			const Coord3D *from = source->getPosition();
			const Coord3D *at = me->getPosition();
			Real off = (Real)WWMath::Atan2Origin( from->y - at->y, from->x - at->x ) - me->getOrientation();
			while( off > PI ) off -= 2.0f * PI;
			while( off < -PI ) off += 2.0f * PI;
			if( fabs( off ) > d->m_damageArc * 0.5f )
				return FALSE;
		}
	}
	if( d->m_reactChance < 1.0f && GameLogicRandomValueReal( 0.0f, 1.0f ) > d->m_reactChance )
		return FALSE;

	// taken
	const Real most = getMaxDamage();
	if( most > 0.0f )
	{
		m_damage += amount;
		if( m_damage >= most )
		{
			m_down = TRUE;
			m_damage = most;
			if( !d->m_absorbKillingBlow )
				return FALSE;
		}
	}
	if( d->m_hitFX != nullptr )
		FXList::doFXPos( d->m_hitFX, victim->getPosition() );
	if( d->m_reflectDamage && source != nullptr && !source->isEffectivelyDead() )
	{
		DamageInfo back;
		back.in.m_damageType = type;
		back.in.m_deathType = DEATH_NORMAL;
		back.in.m_sourceID = me->getID();
		back.in.m_amount = amount;
		s_reflecting = TRUE;
		source->attemptDamage( &back );
		s_reflecting = FALSE;
	}
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool ShieldSphereUpdate::absorbDamage( Object *victim, Object *source, DamageType type, Real amount )
{
	if( s_shields.empty() || victim == nullptr || amount <= 0.0f || s_reflecting )
		return FALSE;
	for( size_t i = 0; i < s_shields.size(); ++i )
	{
		if( s_shields[i]->takeHit( victim, source, type, amount ) )
			return TRUE;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void ShieldSphereUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void ShieldSphereUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	UpgradeMux::upgradeMuxXfer( xfer );
	xfer->xferReal( &m_damage );
	xfer->xferBool( &m_down );
}

//-------------------------------------------------------------------------------------------------
void ShieldSphereUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	UpgradeMux::upgradeMuxLoadPostProcess();
}
