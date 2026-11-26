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

// FILE: SquadUpdate.cpp //////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature a C&C3 squad keeping its soldiers in formation (see SquadUpdate.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/SquadUpdate.h"

#include "Common/ThingTemplate.h"
#include "Common/TiberiumTest.h"
#include "Common/Xfer.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIStateMachine.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SquadMemberUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"
#include "WWMath/wwmath.h"

namespace
{
	const UnsignedInt LOOK_EVERY = 5;		///< frames between the squad's looks at its soldiers (6 a second)

	NameKeyType squadKey()
	{
		static const NameKeyType key = NAMEKEY( "SquadUpdate" );
		return key;
	}

	NameKeyType memberKey()
	{
		static const NameKeyType key = NAMEKEY( "SquadMemberUpdate" );
		return key;
	}

	Real dist2D( const Coord3D &a, const Coord3D &b )
	{
		const Real dx = a.x - b.x, dy = a.y - b.y;
		return sqrtf( dx * dx + dy * dy );
	}

	/// a state the squad's soldiers fight in by themselves (the squad's own attack is theirs: SpawnBehavior)
	Bool isFightState( AIStateType t )
	{
		return t == AI_ATTACK_OBJECT || t == AI_FORCE_ATTACK_OBJECT || t == AI_ATTACK_AND_FOLLOW_OBJECT || t == AI_ATTACK_POSITION
			|| t == AI_ATTACK_AREA || t == AI_ATTACK_SQUAD || t == AI_HUNT || t == AI_GUARD_RETALIATE;
	}
}

//-------------------------------------------------------------------------------------------------
void SquadUpdateModuleData::parsePosition( INI *ini, void *instance, void * /*store*/, const void * /*userData*/ )
{
	Coord2D p;
	INI::parseCoord2D( ini, nullptr, &p, nullptr );
	( (SquadUpdateModuleData *)instance )->m_positions.push_back( p );
}

//-------------------------------------------------------------------------------------------------
SquadUpdate::SquadUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_lastGoal.zero();
	m_hadGoal = FALSE;
	m_selfMove = FALSE;
	m_selfGoal.zero();
	m_targetSquad = INVALID_ID;
	m_targetForced = FALSE;
	m_targetSource = CMD_FROM_AI;
	m_ownCommand = FALSE;
	// (the squads spread their looks over the frames by their IDs)
	setWakeFrame( getObject(), UPDATE_SLEEP( 1 + ( (UnsignedInt)getObject()->getID() % LOOK_EVERY ) ) );
}

//-------------------------------------------------------------------------------------------------
SquadUpdate::~SquadUpdate()
{
}

//-------------------------------------------------------------------------------------------------
SquadUpdate *SquadUpdate::getSquadUpdate( const Object *squad )
{
	return squad != nullptr ? (SquadUpdate *)squad->findUpdateModule( squadKey() ) : nullptr;
}

//-------------------------------------------------------------------------------------------------
Object *SquadUpdate::getSquadOf( const Object *member )
{
	if( member == nullptr || TheGameLogic == nullptr )
		return nullptr;
	const SquadMemberUpdate *m = (const SquadMemberUpdate *)member->findUpdateModule( memberKey() );
	if( m == nullptr || m->getSlaverID() == INVALID_ID )
		return nullptr;
	Object *squad = TheGameLogic->findObjectByID( m->getSlaverID() );
	return getSquadUpdate( squad ) != nullptr && !squad->isEffectivelyDead() ? squad : nullptr;
}

//-------------------------------------------------------------------------------------------------
Object *SquadUpdate::getSoldierToAttack( const Object *target, const Object *attacker )
{
	const SquadUpdate *squad = getSquadUpdate( target );
	if( squad == nullptr || attacker == nullptr )
		return nullptr;
	Object *best = nullptr;
	Real bestDist = 0.0f;
	for( size_t k = 0; k < squad->m_members.size(); ++k )
	{
		Object *m = squad->m_members[k] != INVALID_ID ? TheGameLogic->findObjectByID( squad->m_members[k] ) : nullptr;
		if( m == nullptr || m->isEffectivelyDead() || m->getContainedBy() != nullptr )
			continue;
		const Real d = dist2D( *m->getPosition(), *attacker->getPosition() );
		if( best == nullptr || d < bestDist )
		{
			best = m;
			bestDist = d;
		}
	}
	return best;
}

//-------------------------------------------------------------------------------------------------
Bool SquadUpdate::passOrder( const AICommandParms *parms )
{
	if( parms == nullptr || m_ownCommand )
		return FALSE;
	Object *me = getObject();
	// (an attack on a squad goes on until it has no soldier left, as C&C3's does; any other order ends it)
	const Bool atSquad = ( parms->m_cmd == AICMD_ATTACK_OBJECT || parms->m_cmd == AICMD_FORCE_ATTACK_OBJECT ) && parms->m_obj != nullptr
		&& parms->m_obj != me && getSquadUpdate( parms->m_obj ) != nullptr;
	m_targetSquad = atSquad ? parms->m_obj->getID() : INVALID_ID;
	m_targetForced = atSquad && parms->m_cmd == AICMD_FORCE_ATTACK_OBJECT;
	m_targetSource = atSquad ? (Int)parms->m_cmdSource : (Int)CMD_FROM_AI;
	// (getting in: its soldiers go in, the squad keeps with them, see update)
	const Bool enter = parms->m_cmd == AICMD_ENTER;
	switch( parms->m_cmd )
	{
		case AICMD_ATTACK_OBJECT:
		case AICMD_FORCE_ATTACK_OBJECT:
		case AICMD_ATTACK_TEAM:
		case AICMD_ATTACK_POSITION:
		case AICMD_ATTACK_AREA:
		case AICMD_HUNT:
		case AICMD_ENTER:
			break;
		default:
			return FALSE;
	}
	Int sent = 0;
	for( size_t k = 0; k < m_members.size(); ++k )
	{
		Object *m = m_members[k] != INVALID_ID ? TheGameLogic->findObjectByID( m_members[k] ) : nullptr;
		AIUpdateInterface *mai = m != nullptr && !m->isEffectivelyDead() && m->getContainedBy() == nullptr ? m->getAIUpdateInterface() : nullptr;
		if( mai == nullptr )
			continue;
		// (already on it: a player's order reaches its soldiers by the group, SpawnBehavior::orderSlavesToAttackTarget, as well)
		if( ( parms->m_cmd == AICMD_ATTACK_OBJECT || parms->m_cmd == AICMD_FORCE_ATTACK_OBJECT ) && parms->m_obj != nullptr
			&& mai->getGoalObject() == parms->m_obj && mai->isAttacking() )
			continue;
		if( parms->m_obj == me )
			continue;
		mai->aiDoCommand( parms );
		++sent;
	}
	if( TiberiumTest::isActive() && sent > 0 )
		TiberiumTest::log( "frame %u: squad %s #%u: order %d passed to %d soldiers", TheGameLogic->getFrame(), me->getTemplate()->getName().str(),
			(unsigned)me->getID(), (int)parms->m_cmd, (int)sent );
	return enter;
}

//-------------------------------------------------------------------------------------------------
void SquadUpdate::addMember( const Object *member )
{
	if( member == nullptr )
		return;
	for( size_t k = 0; k < m_members.size(); ++k )
		if( m_members[k] == member->getID() )
			return;
	for( size_t k = 0; k < m_members.size(); ++k )
	{
		const Object *was = TheGameLogic->findObjectByID( m_members[k] );
		if( was == nullptr || was->isEffectivelyDead() )
		{
			m_members[k] = member->getID();
			return;
		}
	}
	m_members.push_back( member->getID() );
}

//-------------------------------------------------------------------------------------------------
Int SquadUpdate::getMemberCount() const
{
	Int n = 0;
	for( size_t k = 0; k < m_members.size(); ++k )
	{
		const Object *m = TheGameLogic->findObjectByID( m_members[k] );
		if( m != nullptr && !m->isEffectivelyDead() )
			++n;
	}
	return n;
}

//-------------------------------------------------------------------------------------------------
void SquadUpdate::getSoldiers( std::vector<Object *> &out ) const
{
	for( size_t k = 0; k < m_members.size(); ++k )
	{
		Object *m = m_members[k] != INVALID_ID ? TheGameLogic->findObjectByID( m_members[k] ) : nullptr;
		if( m != nullptr && !m->isEffectivelyDead() && m->getContainedBy() == nullptr )
			out.push_back( m );
	}
}

//-------------------------------------------------------------------------------------------------
Coord3D SquadUpdate::placeAt( Int k, const Coord3D &center, Real c, Real s ) const
{
	const std::vector<Coord2D> &places = getSquadUpdateModuleData()->m_positions;
	Coord3D at = center;
	if( k >= 0 && k < (Int)places.size() )
	{
		// (C&C3's places: X ahead of the squad, Y to its left)
		const Coord2D &p = places[k];
		at.x += p.x * c - p.y * s;
		at.y += p.x * s + p.y * c;
	}
	return at;
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime SquadUpdate::update()
{
	Object *me = getObject();
	AIUpdateInterface *ai = me->getAIUpdateInterface();
	if( ai == nullptr || me->isEffectivelyDead() )
		return UPDATE_SLEEP( LOOK_EVERY );
	const SquadUpdateModuleData *d = getSquadUpdateModuleData();
	const Coord3D pos = *me->getPosition();

	// its soldiers alive, by place
	std::vector<Object *> members( m_members.size(), nullptr );
	Int alive = 0;
	for( size_t k = 0; k < m_members.size(); ++k )
	{
		Object *m = TheGameLogic->findObjectByID( m_members[k] );
		if( m != nullptr && !m->isEffectivelyDead() && m->getAIUpdateInterface() != nullptr && m->getContainedBy() == nullptr )
		{
			members[k] = m;
			++alive;
		}
	}
	// its soldiers all inside a transport or a building: it keeps with them there, not to be selected or ordered, until
	// they come out
	Object *inside = nullptr;
	if( alive == 0 )
		for( size_t k = 0; k < m_members.size() && inside == nullptr; ++k )
		{
			Object *m = m_members[k] != INVALID_ID ? TheGameLogic->findObjectByID( m_members[k] ) : nullptr;
			if( m != nullptr && !m->isEffectivelyDead() && m->getContainedBy() != nullptr )
				inside = m->getContainedBy();
		}
	if( inside != nullptr )
	{
		if( me->isSelectable() )
		{
			TheGameLogic->deselectObject( me, PLAYERMASK_ALL, TRUE );
			me->setSelectable( FALSE );
			m_ownCommand = TRUE;
			ai->aiIdle( CMD_FROM_AI );
			m_ownCommand = FALSE;
		}
		me->setPosition( inside->getPosition() );
		return UPDATE_SLEEP( LOOK_EVERY );
	}
	if( alive > 0 && !me->isSelectable() && me->getTemplate()->isKindOf( KINDOF_SELECTABLE ) )
		me->setSelectable( TRUE );
	if( alive == 0 )
		return UPDATE_SLEEP( LOOK_EVERY );

	// its stance is theirs
	if( ai->hasStances() )
		for( size_t k = 0; k < members.size(); ++k )
		{
			AIUpdateInterface *mai = members[k] != nullptr ? members[k]->getAIUpdateInterface() : nullptr;
			if( mai != nullptr && mai->hasStances() && mai->getStance() != ai->getStance() )
				mai->setStance( ai->getStance() );
		}

	const AIStateType state = ai->getAIStateType();
	// (its soldiers take its orders as given to it: a player's order is theirs too, and so not undone by their own
	// retaliation; its own moves after them are the AI's)
	const CommandSourceType source = m_selfMove ? CMD_FROM_AI : ai->getLastCommandSource();
	const Bool attackMove = state == AI_ATTACK_MOVE_TO || state == AI_ATTACKFOLLOW_WAYPOINT_PATH_AS_TEAM
		|| state == AI_ATTACKFOLLOW_WAYPOINT_PATH_AS_INDIVIDUALS;
	const Bool moving = ( attackMove || state == AI_MOVE_TO || state == AI_FOLLOW_PATH || state == AI_FOLLOW_WAYPOINT_PATH_AS_TEAM
		|| state == AI_FOLLOW_WAYPOINT_PATH_AS_INDIVIDUALS || state == AI_FOLLOW_WAYPOINT_PATH_AS_TEAM_EXACT
		|| state == AI_FOLLOW_WAYPOINT_PATH_AS_INDIVIDUALS_EXACT || state == AI_FOLLOW_EXITPRODUCTION_PATH ) && ai->getGoalPosition() != nullptr;
	if( moving )
	{
		const Coord3D goal = *ai->getGoalPosition();
		if( !m_hadGoal || dist2D( goal, m_lastGoal ) > 1.0f )
		{
			m_lastGoal = goal;
			m_hadGoal = TRUE;
			// (its own move after its fighting soldiers: those still fighting go on)
			const Bool follow = m_selfMove && dist2D( goal, m_selfGoal ) <= 1.0f;
			if( !follow )
				m_selfMove = FALSE;
			// the formation turned towards the goal
			Real c = WWMath::CosTrig( me->getOrientation() ), s = WWMath::SinTrig( me->getOrientation() );
			const Real len = dist2D( goal, pos );
			if( len > 1.0f )
			{
				c = ( goal.x - pos.x ) / len;
				s = ( goal.y - pos.y ) / len;
			}
			for( size_t k = 0; k < members.size(); ++k )
			{
				AIUpdateInterface *mai = members[k] != nullptr ? members[k]->getAIUpdateInterface() : nullptr;
				if( mai == nullptr || ( follow && mai->isAttacking() ) )
					continue;
				Coord3D at = placeAt( (Int)k, goal, c, s );
				if( attackMove )
					mai->aiAttackMoveToPosition( &at, NO_MAX_SHOTS_LIMIT, follow ? CMD_FROM_AI : source );
				else
					mai->aiMoveToPosition( &at, follow ? CMD_FROM_AI : source );
			}
			if( TiberiumTest::isActive() && !follow )
				TiberiumTest::log( "frame %u: squad %s #%u: %d soldiers sent %s to %.0f %.0f", TheGameLogic->getFrame(),
					me->getTemplate()->getName().str(), (unsigned)me->getID(), (int)alive, attackMove ? "(attack-move)" : "", goal.x, goal.y );
		}
		return UPDATE_SLEEP( LOOK_EVERY );
	}
	m_hadGoal = FALSE;
	if( isFightState( state ) )
	{
		// the squad told to attack (by a script, its AI team or the player): its soldiers take on what it was told to, and the
		// squad itself stands (it follows them, below), so that it never walks itself into its target
		Object *victim = ai->getGoalObject();
		const Coord3D *spot = ai->getGoalPosition();
		const Bool forced = state == AI_FORCE_ATTACK_OBJECT;
		const Bool hasVictim = victim != nullptr && victim != me && !victim->isEffectivelyDead();
		const Bool atSpot = !hasVictim && state == AI_ATTACK_POSITION && spot != nullptr;
		for( size_t k = 0; k < members.size() && ( hasVictim || atSpot ); ++k )
		{
			AIUpdateInterface *mai = members[k] != nullptr ? members[k]->getAIUpdateInterface() : nullptr;
			if( mai == nullptr )
				continue;
			if( hasVictim )
			{
				if( mai->getGoalObject() != victim )
				{
					if( forced )
						mai->aiForceAttackObject( victim, NO_MAX_SHOTS_LIMIT, source );
					else
						mai->aiAttackObject( victim, NO_MAX_SHOTS_LIMIT, source );
				}
			}
			else
				mai->aiAttackPosition( spot, NO_MAX_SHOTS_LIMIT, source );
		}
		// (nothing to attack, as its retaliation for a soldier hit: they answer it themselves, together, below)
		if( TiberiumTest::isActive() && ( hasVictim || atSpot ) )
			TiberiumTest::log( "frame %u: squad %s #%u: %d soldiers attack %s", TheGameLogic->getFrame(), me->getTemplate()->getName().str(),
				(unsigned)me->getID(), (int)alive, victim != nullptr ? victim->getTemplate()->getName().str() : "a spot" );
		m_ownCommand = TRUE;
		ai->aiIdle( CMD_FROM_AI );
		m_ownCommand = FALSE;
		return UPDATE_SLEEP( LOOK_EVERY );
	}

	// the squad it was told to attack: an idle soldier takes on that one's soldier nearest it, until none is left
	if( m_targetSquad != INVALID_ID )
	{
		Object *target = TheGameLogic->findObjectByID( m_targetSquad );
		if( target == nullptr || target->isEffectivelyDead() || getSoldierToAttack( target, me ) == nullptr
			|| ( !m_targetForced && me->getRelationship( target ) != ENEMIES ) )
			m_targetSquad = INVALID_ID;
		else
			for( size_t k = 0; k < members.size(); ++k )
			{
				AIUpdateInterface *mai = members[k] != nullptr ? members[k]->getAIUpdateInterface() : nullptr;
				if( mai == nullptr || !mai->isIdle() )
					continue;
				if( m_targetForced )
					mai->aiForceAttackObject( target, NO_MAX_SHOTS_LIMIT, (CommandSourceType)m_targetSource );
				else
					mai->aiAttackObject( target, NO_MAX_SHOTS_LIMIT, (CommandSourceType)m_targetSource );
			}
	}

	// standing: it follows the middle of its soldiers that fight; an idle one away from its place goes back to it
	Coord3D middle;
	middle.zero();
	Int fighting = 0;
	for( size_t k = 0; k < members.size(); ++k )
	{
		AIUpdateInterface *mai = members[k] != nullptr ? members[k]->getAIUpdateInterface() : nullptr;
		if( mai != nullptr && mai->isAttacking() )
		{
			middle.x += members[k]->getPosition()->x;
			middle.y += members[k]->getPosition()->y;
			middle.z += members[k]->getPosition()->z;
			++fighting;
		}
	}
	if( fighting > 0 )
	{
		// (they fight together: an idle soldier takes on what the first fighting one does, as C&C3's squad engages as one)
		Object *shared = nullptr;
		for( size_t k = 0; k < members.size() && shared == nullptr; ++k )
		{
			AIUpdateInterface *mai = members[k] != nullptr ? members[k]->getAIUpdateInterface() : nullptr;
			Object *v = mai != nullptr && mai->isAttacking() ? mai->getGoalObject() : nullptr;
			if( v != nullptr && !v->isEffectivelyDead() )
				shared = v;
		}
		for( size_t k = 0; k < members.size() && shared != nullptr; ++k )
		{
			AIUpdateInterface *mai = members[k] != nullptr ? members[k]->getAIUpdateInterface() : nullptr;
			if( mai != nullptr && mai->isIdle() )
				mai->aiAttackObject( shared, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI );
		}
		middle.scale( 1.0f / (Real)fighting );
		if( dist2D( middle, pos ) > d->m_followRadius )
		{
			m_selfMove = TRUE;
			m_selfGoal = middle;
			m_ownCommand = TRUE;
			ai->aiMoveToPosition( &middle, CMD_FROM_AI );
			m_ownCommand = FALSE;
		}
		return UPDATE_SLEEP( LOOK_EVERY );
	}
	m_selfMove = FALSE;
	const Real c = WWMath::CosTrig( me->getOrientation() ), s = WWMath::SinTrig( me->getOrientation() );
	for( size_t k = 0; k < members.size(); ++k )
	{
		AIUpdateInterface *mai = members[k] != nullptr ? members[k]->getAIUpdateInterface() : nullptr;
		if( mai == nullptr || !mai->isIdle() )
			continue;
		Coord3D at = placeAt( (Int)k, pos, c, s );
		if( dist2D( at, *members[k]->getPosition() ) > d->m_slotTolerance )
			mai->aiMoveToPosition( &at, CMD_FROM_AI );
	}
	return UPDATE_SLEEP( LOOK_EVERY );
}

//-------------------------------------------------------------------------------------------------
void SquadUpdate::onCapture( Player * /*oldOwner*/, Player * /*newOwner*/ )
{
	Team *team = getObject()->getTeam();
	if( team == nullptr )
		return;
	for( size_t k = 0; k < m_members.size(); ++k )
	{
		Object *member = m_members[k] != INVALID_ID ? TheGameLogic->findObjectByID( m_members[k] ) : nullptr;
		if( member != nullptr && !member->isEffectivelyDead() && member->getTeam() != team )
			member->setTeam( team );
	}
}

//-------------------------------------------------------------------------------------------------
void SquadUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
// Version Info:
// 1: Initial version
//-------------------------------------------------------------------------------------------------
void SquadUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	UpdateModule::xfer( xfer );
	UnsignedShort count = (UnsignedShort)m_members.size();
	xfer->xferUnsignedShort( &count );
	if( xfer->getXferMode() == XFER_LOAD )
		m_members.assign( count, INVALID_ID );
	for( UnsignedShort k = 0; k < count; ++k )
		xfer->xferObjectID( &m_members[k] );
	xfer->xferCoord3D( &m_lastGoal );
	xfer->xferBool( &m_hadGoal );
	xfer->xferBool( &m_selfMove );
	xfer->xferCoord3D( &m_selfGoal );
	xfer->xferObjectID( &m_targetSquad );
	xfer->xferBool( &m_targetForced );
	xfer->xferInt( &m_targetSource );
}

//-------------------------------------------------------------------------------------------------
void SquadUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
