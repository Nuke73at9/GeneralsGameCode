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

// FILE: AttributeModifierAuraUpdate.cpp //////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's attribute modifier auras (see AttributeModifierAuraUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/AttributeModifierAuraUpdate.h"

#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/ContainModule.h"

//-------------------------------------------------------------------------------------------------
AttributeModifierAuraUpdate::AttributeModifierAuraUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	setWakeFrame( getObject(), UPDATE_SLEEP_NONE );
}

//-------------------------------------------------------------------------------------------------
AttributeModifierAuraUpdate::~AttributeModifierAuraUpdate()
{
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime AttributeModifierAuraUpdate::update()
{
	const AttributeModifierAuraUpdateModuleData *data = getAttributeModifierAuraUpdateModuleData();
	Object *me = getObject();
	const UnsignedInt delay = data->m_refreshDelay > 0 ? data->m_refreshDelay : 1;
	const AttributeModifierTemplate *modifier = TheAttributeModifierStore->find( data->m_modifier );
	if( modifier == nullptr || me->isEffectivelyDead() )
		return UPDATE_SLEEP_FOREVER;

	if( me->isDisabled() || me->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) || me->testStatus( OBJECT_STATUS_SOLD ) )
		return UPDATE_SLEEP( delay );
	// Tiberium Project @feature only in the state given (its status bits, a toggle's own modifier on it)
	if( !me->getStatusBits().testForAll( data->m_requiredStatus ) || me->getStatusBits().testForAny( data->m_forbiddenStatus ) )
		return UPDATE_SLEEP( delay );
	if( data->m_requiredModifier != NAMEKEY_INVALID &&
			!me->hasAttributeModifier( TheAttributeModifierStore->find( data->m_requiredModifier ) ) )
		return UPDATE_SLEEP( delay );

	const UnsignedInt duration = modifier->getDuration() > 0 ? modifier->getDuration() : 2 * delay;

	// Tiberium Project @feature only its passengers (they are out of the partition while inside)
	if( data->m_affectContainedOnly )
	{
		ContainModuleInterface *contain = me->getContain();
		const ContainedItemsList *items = contain ? contain->getContainedItemsList() : nullptr;
		if( items != nullptr )
		{
			for( ContainedItemsList::const_iterator it = items->begin(); it != items->end(); ++it )
				affect( *it, modifier, duration );
		}
		return UPDATE_SLEEP( delay );
	}

	// Tiberium Project @feature no radius: only itself
	if( data->m_radius <= 0.0f )
	{
		affect( me, modifier, duration );
		return UPDATE_SLEEP( delay );
	}

	ObjectIterator *iter = ThePartitionManager->iterateObjectsInRange( me->getPosition(), data->m_radius, FROM_CENTER_2D );
	MemoryPoolObjectHolder hold( iter );
	for( Object *other = iter->first(); other; other = iter->next() )
		affect( other, modifier, duration );
	return UPDATE_SLEEP( delay );
}

//-------------------------------------------------------------------------------------------------
/** The modifier on one object, if Affects, KindOf and ForbiddenKindOf take it in */
void AttributeModifierAuraUpdate::affect( Object *other, const AttributeModifierTemplate *modifier, UnsignedInt duration )
{
	const AttributeModifierAuraUpdateModuleData *data = getAttributeModifierAuraUpdateModuleData();
	Object *me = getObject();
	if( other == nullptr || !attributeModifierReaches( me, other, data->m_affects ) )
		return;
	if( data->m_kindOf.any() && !other->isAnyKindOf( data->m_kindOf ) )
		return;
	if( other->isAnyKindOf( data->m_forbiddenKindOf ) )
		return;
	other->applyAttributeModifier( modifier, me->getID(), duration );
}

//-------------------------------------------------------------------------------------------------
void AttributeModifierAuraUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Xfer method
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void AttributeModifierAuraUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	UpdateModule::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void AttributeModifierAuraUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
