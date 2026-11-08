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

// FILE: SpecialPower.cpp /////////////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2002
// Desc:   Special power templates and the system that holds them
// Edited: Kris Morness -- July 2002 (added BitFlag system)
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Object.h"
#include "Common/BitFlagsIO.h"


// GLOBAL /////////////////////////////////////////////////////////////////////////////////////////
SpecialPowerStore *TheSpecialPowerStore = nullptr;

#define DEFAULT_DEFECTION_DETECTION_PROTECTION_TIME_LIMIT (LOGICFRAMES_PER_SECOND * 10)

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// Externs ////////////////////////////////////////////////////////////////////////////////////////
template<>
const char* const SpecialPowerMaskType::s_bitNameList[] =
{
	"SPECIAL_INVALID",

	//Superweapons
	"SPECIAL_DAISY_CUTTER",
	"SPECIAL_PARADROP_AMERICA",
	"SPECIAL_CARPET_BOMB",
	"SPECIAL_CLUSTER_MINES",
	"SPECIAL_EMP_PULSE",
	"SPECIAL_NAPALM_STRIKE",
	"SPECIAL_CASH_HACK",
	"SPECIAL_NEUTRON_MISSILE",
	"SPECIAL_SPY_SATELLITE",
	"SPECIAL_DEFECTOR",
	"SPECIAL_TERROR_CELL",
	"SPECIAL_AMBUSH",
	"SPECIAL_BLACK_MARKET_NUKE",
	"SPECIAL_ANTHRAX_BOMB",
	"SPECIAL_SCUD_STORM",
#ifdef ALLOW_DEMORALIZE
	"SPECIAL_DEMORALIZE",
#else
	"SPECIAL_DEMORALIZE_OBSOLETE",
#endif
	"SPECIAL_CRATE_DROP",
	"SPECIAL_A10_THUNDERBOLT_STRIKE",
	"SPECIAL_DETONATE_DIRTY_NUKE",
	"SPECIAL_ARTILLERY_BARRAGE",

	//Special abilities
	"SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES",
	"SPECIAL_REMOTE_CHARGES",
	"SPECIAL_TIMED_CHARGES",
	"SPECIAL_HELIX_NAPALM_BOMB",
	"SPECIAL_HACKER_DISABLE_BUILDING",
	"SPECIAL_TANKHUNTER_TNT_ATTACK",
	"SPECIAL_BLACKLOTUS_CAPTURE_BUILDING",
	"SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK",
	"SPECIAL_BLACKLOTUS_STEAL_CASH_HACK",
	"SPECIAL_INFANTRY_CAPTURE_BUILDING",
	"SPECIAL_RADAR_VAN_SCAN",
	"SPECIAL_SPY_DRONE",
	"SPECIAL_DISGUISE_AS_VEHICLE",
	"SPECIAL_BOOBY_TRAP",
	"SPECIAL_REPAIR_VEHICLES",
	"SPECIAL_PARTICLE_UPLINK_CANNON",
	"SPECIAL_CASH_BOUNTY",
	"SPECIAL_CHANGE_BATTLE_PLANS",
	"SPECIAL_CIA_INTELLIGENCE",
	"SPECIAL_CLEANUP_AREA",
	"SPECIAL_LAUNCH_BAIKONUR_ROCKET",
  "SPECIAL_SPECTRE_GUNSHIP",
  "SPECIAL_GPS_SCRAMBLER",
	"SPECIAL_FRENZY",
	"SPECIAL_SNEAK_ATTACK",

	"SPECIAL_CHINA_CARPET_BOMB",
	"EARLY_SPECIAL_CHINA_CARPET_BOMB",
	"SPECIAL_LEAFLET_DROP",
	"EARLY_SPECIAL_LEAFLET_DROP",
	"EARLY_SPECIAL_FRENZY",
	"SPECIAL_COMMUNICATIONS_DOWNLOAD",
	"EARLY_SPECIAL_REPAIR_VEHICLES",
	"SPECIAL_TANK_PARADROP",
	"SUPW_SPECIAL_PARTICLE_UPLINK_CANNON",
	"AIRF_SPECIAL_DAISY_CUTTER",
	"NUKE_SPECIAL_CLUSTER_MINES",
	"NUKE_SPECIAL_NEUTRON_MISSILE",
	"AIRF_SPECIAL_A10_THUNDERBOLT_STRIKE",
	"AIRF_SPECIAL_SPECTRE_GUNSHIP",
	"INFA_SPECIAL_PARADROP_AMERICA",
	"SLTH_SPECIAL_GPS_SCRAMBLER",
	"AIRF_SPECIAL_CARPET_BOMB",
	"SUPR_SPECIAL_CRUISE_MISSILE",
	"LAZR_SPECIAL_PARTICLE_UPLINK_CANNON",
	"SUPW_SPECIAL_NEUTRON_MISSILE",

	"SPECIAL_BATTLESHIP_BOMBARDMENT",

	"SPECIAL_TOGGLE_DEPLOY",
	"SPECIAL_ABILITY_NO_TARGET",	// Tiberium Project
	"SPECIAL_ABILITY_AT_LOCATION",
	"SPECIAL_ABILITY_AT_OBJECT",
	"SPECIAL_PLAYER_POWER_01",
	"SPECIAL_PLAYER_POWER_02",
	"SPECIAL_PLAYER_POWER_03",
	"SPECIAL_PLAYER_POWER_04",
	"SPECIAL_PLAYER_POWER_05",
	"SPECIAL_PLAYER_POWER_06",
	"SPECIAL_PLAYER_POWER_07",
	"SPECIAL_PLAYER_POWER_08",
	"SPECIAL_PLAYER_POWER_09",
	"SPECIAL_PLAYER_POWER_10",
	"SPECIAL_PLAYER_POWER_11",
	"SPECIAL_PLAYER_POWER_12",
	"SPECIAL_PLAYER_POWER_13",
	"SPECIAL_PLAYER_POWER_14",
	"SPECIAL_PLAYER_POWER_15",
	"SPECIAL_PLAYER_POWER_16",
	"SPECIAL_PLAYER_POWER_17",
	"SPECIAL_PLAYER_POWER_18",
	"SPECIAL_PLAYER_POWER_19",
	"SPECIAL_PLAYER_POWER_20",
	"SPECIAL_PLAYER_POWER_21",
	"SPECIAL_PLAYER_POWER_22",
	"SPECIAL_PLAYER_POWER_23",
	"SPECIAL_PLAYER_POWER_24",
	"SPECIAL_PLAYER_POWER_25",
	"SPECIAL_PLAYER_POWER_26",
	"SPECIAL_PLAYER_POWER_27",
	"SPECIAL_PLAYER_POWER_28",
	"SPECIAL_PLAYER_POWER_29",
	"SPECIAL_PLAYER_POWER_30",
	"SPECIAL_PLAYER_POWER_31",
	"SPECIAL_PLAYER_POWER_32",
	"SPECIAL_PLAYER_POWER_33",
	"SPECIAL_PLAYER_POWER_34",
	"SPECIAL_PLAYER_POWER_35",
	"SPECIAL_PLAYER_POWER_36",
	"SPECIAL_PLAYER_POWER_37",
	"SPECIAL_PLAYER_POWER_38",
	"SPECIAL_PLAYER_POWER_39",
	"SPECIAL_PLAYER_POWER_40",
	"SPECIAL_PLAYER_POWER_41",
	"SPECIAL_PLAYER_POWER_42",
	"SPECIAL_PLAYER_POWER_43",
	"SPECIAL_PLAYER_POWER_44",
	"SPECIAL_PLAYER_POWER_45",
	"SPECIAL_PLAYER_POWER_46",
	"SPECIAL_PLAYER_POWER_47",
	"SPECIAL_PLAYER_POWER_48",
	"SPECIAL_PLAYER_POWER_49",
	"SPECIAL_PLAYER_POWER_50",
	"SPECIAL_PLAYER_POWER_51",
	"SPECIAL_PLAYER_POWER_52",
	"SPECIAL_PLAYER_POWER_53",
	"SPECIAL_PLAYER_POWER_54",
	"SPECIAL_PLAYER_POWER_55",
	"SPECIAL_PLAYER_POWER_56",
	"SPECIAL_PLAYER_POWER_57",
	"SPECIAL_PLAYER_POWER_58",
	"SPECIAL_PLAYER_POWER_59",
	"SPECIAL_PLAYER_POWER_60",
	"SPECIAL_PLAYER_POWER_61",
	"SPECIAL_PLAYER_POWER_62",
	"SPECIAL_PLAYER_POWER_63",
	"SPECIAL_PLAYER_POWER_64",
	"SPECIAL_PLAYER_POWER_65",
	"SPECIAL_PLAYER_POWER_66",
	"SPECIAL_PLAYER_POWER_67",
	"SPECIAL_PLAYER_POWER_68",
	"SPECIAL_PLAYER_POWER_69",
	"SPECIAL_PLAYER_POWER_70",
	"SPECIAL_PLAYER_POWER_71",
	"SPECIAL_PLAYER_POWER_72",
	"SPECIAL_PLAYER_POWER_73",
	"SPECIAL_PLAYER_POWER_74",
	"SPECIAL_PLAYER_POWER_75",
	"SPECIAL_PLAYER_POWER_76",
	"SPECIAL_PLAYER_POWER_77",
	"SPECIAL_PLAYER_POWER_78",
	"SPECIAL_PLAYER_POWER_79",
	"SPECIAL_PLAYER_POWER_80",
	"SPECIAL_PLAYER_POWER_81",
	"SPECIAL_PLAYER_POWER_82",
	"SPECIAL_PLAYER_POWER_83",
	"SPECIAL_PLAYER_POWER_84",
	"SPECIAL_PLAYER_POWER_85",
	"SPECIAL_PLAYER_POWER_86",
	"SPECIAL_PLAYER_POWER_87",
	"SPECIAL_PLAYER_POWER_88",
	"SPECIAL_PLAYER_POWER_89",
	"SPECIAL_PLAYER_POWER_90",
	"SPECIAL_PLAYER_POWER_91",
	"SPECIAL_PLAYER_POWER_92",
	"SPECIAL_PLAYER_POWER_93",
	"SPECIAL_PLAYER_POWER_94",
	"SPECIAL_PLAYER_POWER_95",
	"SPECIAL_PLAYER_POWER_96",

	nullptr
};
static_assert(ARRAY_SIZE(SpecialPowerMaskType::s_bitNameList) == SpecialPowerMaskType::NumBits + 1, "Incorrect array size");

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SpecialPowerStore::parseSpecialPowerDefinition( INI *ini )
{
	// read the name
	AsciiString name = ini->getNextToken();

	SpecialPowerTemplate* specialPower = TheSpecialPowerStore->findSpecialPowerTemplatePrivate( name );

	if ( ini->getLoadType() == INI_LOAD_CREATE_OVERRIDES )
	{
		if (specialPower)
		{
			SpecialPowerTemplate* child = (SpecialPowerTemplate*)specialPower->friend_getFinalOverride();
			specialPower = newInstance(SpecialPowerTemplate);
			*specialPower = *child;
			child->setNextOverride(specialPower);
			specialPower->markAsOverride();
			//TheSpecialPowerStore->m_specialPowerTemplates.push_back(specialPower); // nope, do NOT do this
		}
		else
		{
			specialPower = newInstance(SpecialPowerTemplate);
			const SpecialPowerTemplate *defaultTemplate = TheSpecialPowerStore->findSpecialPowerTemplate( "DefaultSpecialPower" );
			if( defaultTemplate )
				*specialPower = *defaultTemplate;
			specialPower->friend_setNameAndID(name, ++TheSpecialPowerStore->m_nextSpecialPowerID);
			specialPower->markAsOverride();
			TheSpecialPowerStore->m_specialPowerTemplates.push_back(specialPower);
		}
	}
	else
	{
		if (specialPower)
		{
			throw INI_INVALID_DATA;
		}
		else
		{
			specialPower = newInstance(SpecialPowerTemplate);
			const SpecialPowerTemplate *defaultTemplate = TheSpecialPowerStore->findSpecialPowerTemplate( "DefaultSpecialPower" );
			if( defaultTemplate )
				*specialPower = *defaultTemplate;
			specialPower->friend_setNameAndID(name, ++TheSpecialPowerStore->m_nextSpecialPowerID);
			TheSpecialPowerStore->m_specialPowerTemplates.push_back(specialPower);
		}
	}

	// parse the ini definition
	if (specialPower)
		ini->initFromINI( specialPower, specialPower->getFieldParse() );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/* static */ const FieldParse SpecialPowerTemplate::m_specialPowerFieldParse[] =
{

	{ "ReloadTime",								INI::parseDurationUnsignedInt,		nullptr,	offsetof( SpecialPowerTemplate, m_reloadTime ) },
	{ "RequiredScience",					INI::parseScience,								nullptr, offsetof( SpecialPowerTemplate, m_requiredScience ) },
	{ "InitiateSound",						INI::parseAudioEventRTS,					nullptr,	offsetof( SpecialPowerTemplate, m_initiateSound ) },
	{ "InitiateAtLocationSound",	INI::parseAudioEventRTS,					nullptr,	offsetof( SpecialPowerTemplate, m_initiateAtLocationSound ) },
	{ "PublicTimer",							INI::parseBool,										nullptr, offsetof( SpecialPowerTemplate, m_publicTimer ) },
	{ "Enum",											INI::parseIndexList,							SpecialPowerMaskType::getBitNames(), offsetof( SpecialPowerTemplate, m_type ) },
	{ "DetectionTime",						INI::parseDurationUnsignedInt,		nullptr,	offsetof( SpecialPowerTemplate, m_detectionTime ) },
	{ "SharedSyncedTimer",				INI::parseBool,										nullptr, offsetof( SpecialPowerTemplate, m_sharedNSync ) },
	{ "ViewObjectDuration",				INI::parseDurationUnsignedInt,		nullptr,	offsetof( SpecialPowerTemplate, m_viewObjectDuration ) },
	{ "ViewObjectRange",					INI::parseReal,										nullptr,	offsetof( SpecialPowerTemplate, m_viewObjectRange ) },
	{ "RadiusCursorRadius",				INI::parseReal,										nullptr,	offsetof( SpecialPowerTemplate, m_radiusCursorRadius ) },
	{ "ShortcutPower",						INI::parseBool,										nullptr, offsetof( SpecialPowerTemplate, m_shortcutPower ) },
	{ "AcademyClassify",					INI::parseIndexList,			TheAcademyClassificationTypeNames, offsetof( SpecialPowerTemplate, m_academyClassificationType ) },
	{ "Cost",											INI::parseUnsignedInt,						nullptr, offsetof( SpecialPowerTemplate, m_cost ) },	// Tiberium Project @feature
	{ "SupportPoints",						INI::parseUnsignedInt,						nullptr, offsetof( SpecialPowerTemplate, m_supportPoints ) },
	{ "ShroudedTargetOK",					INI::parseBool,										nullptr, offsetof( SpecialPowerTemplate, m_shroudedTargetOK ) },
	{ "AIUse",										SpecialPowerTemplate::parseAIUse,	nullptr, 0 },	// Tiberium Project @feature
	{ nullptr,	nullptr, nullptr,	0 }

};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpecialPowerTemplate::SpecialPowerTemplate()
{
	m_id = 0;
	m_type = SPECIAL_INVALID;
	m_reloadTime = 0;
	m_requiredScience = SCIENCE_INVALID;
	m_publicTimer = FALSE;
	m_detectionTime = DEFAULT_DEFECTION_DETECTION_PROTECTION_TIME_LIMIT;
	m_sharedNSync = FALSE;
	m_viewObjectDuration = 0;
	m_viewObjectRange = 0;
	m_radiusCursorRadius = 0;
	m_shortcutPower = FALSE;
	m_cost = 0;
	m_supportPoints = 0;
	m_shroudedTargetOK = FALSE;
	m_aiUse = AI_USE_NONE;
	m_aiKinds.clear();
	m_aiForbiddenKinds.clear();
	m_aiDistance = 0.0f;

}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature how the skirmish AI uses the power: "AIUse = Type [Kind ...] [-Kind ...] [Distance]"
//-------------------------------------------------------------------------------------------------
static const char *const TheSpecialPowerAIUseNames[] =
{
	"NONE",
	"ENEMY",
	"ENEMY_STRUCTURES",
	"SUPERWEAPON",
	"REINFORCE",
	"ALLY_BUFF",
	"HEAL",
	"STEALTH",
	"IN_COMBAT",
	nullptr
};
static_assert(ARRAY_SIZE(TheSpecialPowerAIUseNames) == AI_USE_COUNT + 1, "Incorrect array size");

void SpecialPowerTemplate::parseAIUse( INI *ini, void *instance, void * /*store*/, const void * /*userData*/ )
{
	SpecialPowerTemplate *self = (SpecialPowerTemplate *)instance;
	self->m_aiUse = (SpecialPowerAIUse)INI::scanIndexList( ini->getNextToken(), TheSpecialPowerAIUseNames );
	self->m_aiKinds.clear();
	self->m_aiForbiddenKinds.clear();
	self->m_aiDistance = 0.0f;
	for( const char *token = ini->getNextTokenOrNull(); token != nullptr; token = ini->getNextTokenOrNull() )
	{
		if( token[0] >= '0' && token[0] <= '9' )
			self->m_aiDistance = INI::scanReal( token );
		else if( token[0] == '-' )
		{
			if( !self->m_aiForbiddenKinds.setBitByName( token + 1 ) )
				DEBUG_CRASH( ( "SpecialPower %s: AIUse names an unknown kind %s", self->m_name.str(), token + 1 ) );
		}
		else if( !self->m_aiKinds.setBitByName( token ) )
			DEBUG_CRASH( ( "SpecialPower %s: AIUse names an unknown kind %s", self->m_name.str(), token ) );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpecialPowerTemplate::~SpecialPowerTemplate()
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpecialPowerStore::SpecialPowerStore()
{

	m_nextSpecialPowerID = 0;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpecialPowerStore::~SpecialPowerStore()
{

	// delete all templates
	for( size_t i = 0; i < m_specialPowerTemplates.size(); ++i )
		deleteInstance(m_specialPowerTemplates[ i ]);

	// erase the list
	m_specialPowerTemplates.clear();

	// set our count to zero
	m_nextSpecialPowerID = 0;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
SpecialPowerTemplate* SpecialPowerStore::findSpecialPowerTemplatePrivate( AsciiString name )
{

	// search the template list for matching name
	for( size_t i = 0; i < m_specialPowerTemplates.size(); ++i )
		if( m_specialPowerTemplates[ i ]->getName() == name )
			return m_specialPowerTemplates[ i ];

	return nullptr;  // not found

}

//-------------------------------------------------------------------------------------------------
/** Find a special power template given unique ID */
//-------------------------------------------------------------------------------------------------
const SpecialPowerTemplate *SpecialPowerStore::findSpecialPowerTemplateByID( UnsignedInt id )
{

	// search the template list for matching name
	for( size_t i = 0; i < m_specialPowerTemplates.size(); ++i )
		if( m_specialPowerTemplates[ i ]->getID() == id )
			return m_specialPowerTemplates[ i ];

	return nullptr;  // not found

}

//-------------------------------------------------------------------------------------------------
/** Find a special power template given index (WB) */
//-------------------------------------------------------------------------------------------------
const SpecialPowerTemplate *SpecialPowerStore::getSpecialPowerTemplateByIndex( UnsignedInt index )
{

	if (index >= 0 && index < m_specialPowerTemplates.size())
		return m_specialPowerTemplates[ index ];

	return nullptr;  // not found

}

//-------------------------------------------------------------------------------------------------
/** Return the size of the store (WB) */
//-------------------------------------------------------------------------------------------------
Int SpecialPowerStore::getNumSpecialPowers()
{

	return m_specialPowerTemplates.size();

}

//-------------------------------------------------------------------------------------------------
/** does the object (and therefore the player) meet all the requirements to use this power */
//-------------------------------------------------------------------------------------------------
Bool SpecialPowerStore::canUseSpecialPower( Object *obj, const SpecialPowerTemplate *specialPowerTemplate )
{

	// sanity
	if( obj == nullptr || specialPowerTemplate == nullptr )
		return FALSE;

	// as a first sanity check, the object must have a module capable of executing the power
	if( obj->getSpecialPowerModule( specialPowerTemplate ) == nullptr )
		return FALSE;

	//
	// in order to execute the special powers we have attached special power modules to the objects
	// that can use them.  However, just because an object has a module that is capable of
	// doing the power, does not mean the object and the player can actually execute the
	// power because some powers require a specialized science that the player must select and
	// they cannot have all of them.
	//

	// check for required science
	ScienceType requiredScience = specialPowerTemplate->getRequiredScience();
	if( requiredScience != SCIENCE_INVALID )
	{
		Player *player = obj->getControllingPlayer();

		if( player->hasScience( requiredScience ) == FALSE )
			return FALSE;

	}


	// I THINK THIS IS WHERE WE BAIL OUT IF A DIFFERENT CONYARD IS ALREADY CHARGING THIS SPECIAL RIGHT NOW //LORENZEN


	// all is well
	return TRUE;

}

//-------------------------------------------------------------------------------------------------
/** Reset */
//-------------------------------------------------------------------------------------------------
void SpecialPowerStore::reset()
{
	for (SpecialPowerTemplatePtrVector::iterator it = m_specialPowerTemplates.begin(); it != m_specialPowerTemplates.end(); /*++it*/)
	{
		SpecialPowerTemplate* si = *it;
		Overridable* temp = si->deleteOverrides();
		if (temp == nullptr)
		{
			it = m_specialPowerTemplates.erase(it);
		}
		else
		{
			++it;
		}
	}
}
