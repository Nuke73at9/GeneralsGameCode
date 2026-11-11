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

// FILE: StoredResourceUpdate.cpp /////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's stored resource (see StoredResourceUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/StoredResourceUpdate.h"

#include "Common/BitFlagsIO.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameLogic/AttributeModifier.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"

//-------------------------------------------------------------------------------------------------
StoredResourceUpdateModuleData::StoredResourceUpdateModuleData()
{
	m_capacity = 100;
	m_interval = 0;
	m_value = 0;
	m_conditionFull = MODELCONDITION_INVALID;
	m_showInUI = TRUE;
}

//-------------------------------------------------------------------------------------------------
// Level = <amount> [Upgrade:<object upgrade>] [Modifier:<attribute modifier>] [OCL:<list>] [Reset:Yes]
//-------------------------------------------------------------------------------------------------
void StoredResourceUpdateModuleData::parseLevel( INI *ini, void *instance, void * /*store*/, const void * /*userData*/ )
{
	StoredResourceUpdateModuleData *self = (StoredResourceUpdateModuleData *)instance;
	StoredResourceLevel level;
	level.m_amount = INI::scanInt( ini->getNextToken() );
	level.m_modifier = NAMEKEY_INVALID;
	level.m_reset = FALSE;
	for( const char *key = ini->getNextTokenOrNull( ini->getSepsColon() ); key != nullptr;
			key = ini->getNextTokenOrNull( ini->getSepsColon() ) )
	{
		const char *value = ini->getNextToken( ini->getSepsColon() );
		if( stricmp( key, "Upgrade" ) == 0 )
			level.m_upgradeName = value;
		else if( stricmp( key, "Modifier" ) == 0 )
			level.m_modifier = TheNameKeyGenerator->nameToKey( value );
		else if( stricmp( key, "OCL" ) == 0 )
			level.m_oclName = value;
		else if( stricmp( key, "Reset" ) == 0 )
			level.m_reset = INI::scanBool( value );
		else
		{
			DEBUG_CRASH(( "StoredResourceUpdate: unknown Level field %s", key ));
			throw INI_INVALID_DATA;
		}
	}
	// (a bit each in the reached mask)
	if( self->m_levels.size() < 32 )
		self->m_levels.push_back( level );
}

//-------------------------------------------------------------------------------------------------
void StoredResourceUpdateModuleData::buildFieldParse( MultiIniFieldParse& p )
{
	UpdateModuleData::buildFieldParse( p );
	static const FieldParse dataFieldParse[] =
	{
		{ "Capacity",				INI::parseInt,								nullptr, offsetof( StoredResourceUpdateModuleData, m_capacity ) },
		{ "UpdateInterval",		INI::parseDurationUnsignedInt,			nullptr, offsetof( StoredResourceUpdateModuleData, m_interval ) },
		{ "UpdateValue",			INI::parseInt,								nullptr, offsetof( StoredResourceUpdateModuleData, m_value ) },
		{ "RequiredStatus",		ObjectStatusMaskType::parseFromINI,		nullptr, offsetof( StoredResourceUpdateModuleData, m_requiredStatus ) },
		{ "ForbiddenStatus",		ObjectStatusMaskType::parseFromINI,		nullptr, offsetof( StoredResourceUpdateModuleData, m_forbiddenStatus ) },
		{ "ConditionFull",		ModelConditionFlags::parseSingleBitFromINI,	nullptr, offsetof( StoredResourceUpdateModuleData, m_conditionFull ) },
		{ "ShowInUI",				INI::parseBool,							nullptr, offsetof( StoredResourceUpdateModuleData, m_showInUI ) },
		{ "Level",					parseLevel,									nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add( dataFieldParse );
}

//-------------------------------------------------------------------------------------------------
StoredResourceUpdate::StoredResourceUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_amount = 0;
	m_reachedMask = 0;
	m_full = FALSE;
	const StoredResourceUpdateModuleData *data = getStoredResourceUpdateModuleData();
	setWakeFrame( getObject(), data->m_interval > 0 && data->m_value != 0 ? UPDATE_SLEEP( data->m_interval ) : UPDATE_SLEEP_FOREVER );
}

//-------------------------------------------------------------------------------------------------
StoredResourceUpdate::~StoredResourceUpdate()
{
}

//-------------------------------------------------------------------------------------------------
StoredResourceUpdate *StoredResourceUpdate::find( const Object *obj )
{
	static const NameKeyType key = NAMEKEY( "StoredResourceUpdate" );
	return obj != nullptr ? (StoredResourceUpdate *)obj->findUpdateModule( key ) : nullptr;
}

//-------------------------------------------------------------------------------------------------
Real StoredResourceUpdate::getFillRatio() const
{
	const Int capacity = getStoredResourceUpdateModuleData()->m_capacity;
	if( capacity <= 0 )
		return 0.0f;
	return clamp( 0.0f, (Real)m_amount / (Real)capacity, 1.0f );
}

//-------------------------------------------------------------------------------------------------
// every UpdateInterval: the gain, while the object is whole and has the statuses it needs
//-------------------------------------------------------------------------------------------------
UpdateSleepTime StoredResourceUpdate::update()
{
	const StoredResourceUpdateModuleData *data = getStoredResourceUpdateModuleData();
	if( data->m_interval == 0 || data->m_value == 0 )
		return UPDATE_SLEEP_FOREVER;
	Object *me = getObject();
	const ObjectStatusMaskType &status = me->getStatusBits();
	const Bool whole = !me->isEffectivelyDead() && !me->isDisabled()
		&& !status.test( OBJECT_STATUS_UNDER_CONSTRUCTION ) && !status.test( OBJECT_STATUS_SOLD );
	if( whole && status.testSetAndClear( data->m_requiredStatus, data->m_forbiddenStatus ) )
		addAmount( data->m_value );
	return UPDATE_SLEEP( data->m_interval );
}

//-------------------------------------------------------------------------------------------------
void StoredResourceUpdate::addAmount( Int delta )
{
	const Int capacity = getStoredResourceUpdateModuleData()->m_capacity;
	Int amount = m_amount + delta;
	if( amount > capacity )
		amount = capacity;
	if( amount < 0 )
		amount = 0;
	if( amount == m_amount )
		return;
	m_amount = amount;
	applyLevels();
}

//-------------------------------------------------------------------------------------------------
// the levels the store holds now: those reached since given their upgrade and modifier (and their
// OCL made), those left behind their upgrade and modifier taken off; then the full condition
//-------------------------------------------------------------------------------------------------
void StoredResourceUpdate::applyLevels()
{
	const StoredResourceUpdateModuleData *data = getStoredResourceUpdateModuleData();
	Object *me = getObject();
	// (a level with Reset empties the store: the levels are gone over again then, once more at most for each)
	for( size_t pass = 0; pass <= data->m_levels.size(); ++pass )
	{
		Bool reset = FALSE;
		for( size_t i = 0; i < data->m_levels.size(); ++i )
		{
			const StoredResourceLevel &level = data->m_levels[i];
			const UnsignedInt bit = 1u << i;
			const Bool reached = m_amount >= level.m_amount;
			const Bool had = ( m_reachedMask & bit ) != 0;
			if( reached == had )
				continue;
			const UpgradeTemplate *upgrade = level.m_upgradeName.isEmpty() || TheUpgradeCenter == nullptr ? nullptr :
				TheUpgradeCenter->findUpgrade( level.m_upgradeName );
			const AttributeModifierTemplate *modifier = level.m_modifier == NAMEKEY_INVALID || TheAttributeModifierStore == nullptr ? nullptr :
				TheAttributeModifierStore->find( level.m_modifier );
			if( reached )
			{
				m_reachedMask |= bit;
				if( upgrade != nullptr && !me->hasUpgrade( upgrade ) )
					me->giveUpgrade( upgrade );
				if( modifier != nullptr )
					me->applyAttributeModifier( modifier, me->getID() );
				if( level.m_oclName.isNotEmpty() )
				{
					const ObjectCreationList *ocl = TheObjectCreationListStore->findObjectCreationList( level.m_oclName.str() );
					if( ocl != nullptr )
						ObjectCreationList::create( ocl, me, nullptr );
				}
				if( level.m_reset )
					reset = TRUE;
			}
			else
			{
				m_reachedMask &= ~bit;
				if( upgrade != nullptr && me->hasUpgrade( upgrade ) )
					me->removeUpgrade( upgrade );
				if( modifier != nullptr )
					me->removeAttributeModifier( modifier, me->getID() );
			}
		}
		if( !reset )
			break;
		m_amount = 0;
	}

	const Bool full = data->m_capacity > 0 && m_amount >= data->m_capacity;
	if( full != m_full && data->m_conditionFull != MODELCONDITION_INVALID )
	{
		if( full )
			me->setModelConditionState( data->m_conditionFull );
		else
			me->clearModelConditionState( data->m_conditionFull );
	}
	m_full = full;
}

//-------------------------------------------------------------------------------------------------
void StoredResourceUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
	xfer->xferInt( &m_amount );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void StoredResourceUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	UpdateModule::xfer( xfer );
	xfer->xferInt( &m_amount );
	xfer->xferUnsignedInt( &m_reachedMask );
	xfer->xferBool( &m_full );
}

//-------------------------------------------------------------------------------------------------
void StoredResourceUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
