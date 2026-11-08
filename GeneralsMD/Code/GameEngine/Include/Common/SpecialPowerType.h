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

// SpecialPowerType.h /////////////////////////////////////////////////////////////////////////////
// Part of header detangling
// JKMCD Aug 2002

#pragma once

// ------------------------------------------------------------------------------------------------
// don't forget to add new strings to SpecialPowerMaskType::s_bitNameList[]
// ------------------------------------------------------------------------------------------------
//
// Note: these values are saved in save files, so you MUST NOT REMOVE OR CHANGE
// existing values!
//
enum SpecialPowerType CPP_11(: Int)
{
	SPECIAL_INVALID,
	// don't forget to add new strings to SpecialPowerMaskType::s_bitNameList[]

	//Superweapons
	SPECIAL_DAISY_CUTTER,
	SPECIAL_PARADROP_AMERICA,
	SPECIAL_CARPET_BOMB,
	SPECIAL_CLUSTER_MINES,
	SPECIAL_EMP_PULSE,
	SPECIAL_NAPALM_STRIKE,
	SPECIAL_CASH_HACK,
	SPECIAL_NEUTRON_MISSILE,
	SPECIAL_SPY_SATELLITE,
	SPECIAL_DEFECTOR,
	SPECIAL_TERROR_CELL,
	SPECIAL_AMBUSH,
	SPECIAL_BLACK_MARKET_NUKE,
	SPECIAL_ANTHRAX_BOMB,
	SPECIAL_SCUD_STORM,
#ifdef ALLOW_DEMORALIZE
	SPECIAL_DEMORALIZE,
#else
	SPECIAL_DEMORALIZE_OBSOLETE,
#endif
	SPECIAL_CRATE_DROP,
	SPECIAL_A10_THUNDERBOLT_STRIKE,
	SPECIAL_DETONATE_DIRTY_NUKE,
	SPECIAL_ARTILLERY_BARRAGE,
	// don't forget to add new strings to SpecialPowerMaskType::s_bitNameList[]

	//Special abilities
	SPECIAL_MISSILE_DEFENDER_LASER_GUIDED_MISSILES,
	SPECIAL_REMOTE_CHARGES,
	SPECIAL_TIMED_CHARGES,
	SPECIAL_HELIX_NAPALM_BOMB,
	SPECIAL_HACKER_DISABLE_BUILDING,
	SPECIAL_TANKHUNTER_TNT_ATTACK,
	SPECIAL_BLACKLOTUS_CAPTURE_BUILDING,
	SPECIAL_BLACKLOTUS_DISABLE_VEHICLE_HACK,
	SPECIAL_BLACKLOTUS_STEAL_CASH_HACK,
	SPECIAL_INFANTRY_CAPTURE_BUILDING,
	SPECIAL_RADAR_VAN_SCAN,
	SPECIAL_SPY_DRONE,
	SPECIAL_DISGUISE_AS_VEHICLE,
	SPECIAL_BOOBY_TRAP,
	// don't forget to add new strings to SpecialPowerMaskType::s_bitNameList[]
	SPECIAL_REPAIR_VEHICLES,
	SPECIAL_PARTICLE_UPLINK_CANNON,
	SPECIAL_CASH_BOUNTY,
	SPECIAL_CHANGE_BATTLE_PLANS,
	SPECIAL_CIA_INTELLIGENCE,
	SPECIAL_CLEANUP_AREA,
	// don't forget to add new strings to SpecialPowerMaskType::s_bitNameList[]
	SPECIAL_LAUNCH_BAIKONUR_ROCKET,

  SPECIAL_SPECTRE_GUNSHIP,
  SPECIAL_GPS_SCRAMBLER,

	SPECIAL_FRENZY,
	SPECIAL_SNEAK_ATTACK,

	//Ack, this is ass. These enums fix a bug where new enums were missing for
	//shortcut powers... but the real clincher was that if you were say USA and
	//captured a Tank China command center, your US paradrop would be assigned
	//to the china tank drop and when you tried to fire it from the shortcut
	//it could pick the china one and not fire it because it didn't have
	//complete connection... ugh!!!
	SPECIAL_CHINA_CARPET_BOMB,
	EARLY_SPECIAL_CHINA_CARPET_BOMB,
	SPECIAL_LEAFLET_DROP,
	EARLY_SPECIAL_LEAFLET_DROP,
	EARLY_SPECIAL_FRENZY,
	SPECIAL_COMMUNICATIONS_DOWNLOAD,
	EARLY_SPECIAL_REPAIR_VEHICLES,
	SPECIAL_TANK_PARADROP,
	SUPW_SPECIAL_PARTICLE_UPLINK_CANNON,
	AIRF_SPECIAL_DAISY_CUTTER,
	NUKE_SPECIAL_CLUSTER_MINES,
	NUKE_SPECIAL_NEUTRON_MISSILE,
	AIRF_SPECIAL_A10_THUNDERBOLT_STRIKE,
	AIRF_SPECIAL_SPECTRE_GUNSHIP,
	INFA_SPECIAL_PARADROP_AMERICA,
	SLTH_SPECIAL_GPS_SCRAMBLER,
	AIRF_SPECIAL_CARPET_BOMB,
	SUPR_SPECIAL_CRUISE_MISSILE,
	LAZR_SPECIAL_PARTICLE_UPLINK_CANNON,
	SUPW_SPECIAL_NEUTRON_MISSILE,

	SPECIAL_BATTLESHIP_BOMBARDMENT,

	SPECIAL_TOGGLE_DEPLOY,	///< Tiberium Project: deploy/pack up a ManualDeploy unit (DeployToggleSpecialPower)

	// Tiberium Project: the converted C&C4 unit abilities, by how they are aimed
	SPECIAL_ABILITY_NO_TARGET,
	SPECIAL_ABILITY_AT_LOCATION,
	SPECIAL_ABILITY_AT_OBJECT,

	// Tiberium Project @feature the converted player powers (Kane's Wrath's on their buildings, C&C4's on the
	// Support Crawler), one of these each: the shortcut bar finds a power's source by its type
	SPECIAL_PLAYER_POWER_01,
	SPECIAL_PLAYER_POWER_02,
	SPECIAL_PLAYER_POWER_03,
	SPECIAL_PLAYER_POWER_04,
	SPECIAL_PLAYER_POWER_05,
	SPECIAL_PLAYER_POWER_06,
	SPECIAL_PLAYER_POWER_07,
	SPECIAL_PLAYER_POWER_08,
	SPECIAL_PLAYER_POWER_09,
	SPECIAL_PLAYER_POWER_10,
	SPECIAL_PLAYER_POWER_11,
	SPECIAL_PLAYER_POWER_12,
	SPECIAL_PLAYER_POWER_13,
	SPECIAL_PLAYER_POWER_14,
	SPECIAL_PLAYER_POWER_15,
	SPECIAL_PLAYER_POWER_16,
	SPECIAL_PLAYER_POWER_17,
	SPECIAL_PLAYER_POWER_18,
	SPECIAL_PLAYER_POWER_19,
	SPECIAL_PLAYER_POWER_20,
	SPECIAL_PLAYER_POWER_21,
	SPECIAL_PLAYER_POWER_22,
	SPECIAL_PLAYER_POWER_23,
	SPECIAL_PLAYER_POWER_24,
	SPECIAL_PLAYER_POWER_25,
	SPECIAL_PLAYER_POWER_26,
	SPECIAL_PLAYER_POWER_27,
	SPECIAL_PLAYER_POWER_28,
	SPECIAL_PLAYER_POWER_29,
	SPECIAL_PLAYER_POWER_30,
	SPECIAL_PLAYER_POWER_31,
	SPECIAL_PLAYER_POWER_32,
	SPECIAL_PLAYER_POWER_33,
	SPECIAL_PLAYER_POWER_34,
	SPECIAL_PLAYER_POWER_35,
	SPECIAL_PLAYER_POWER_36,
	SPECIAL_PLAYER_POWER_37,
	SPECIAL_PLAYER_POWER_38,
	SPECIAL_PLAYER_POWER_39,
	SPECIAL_PLAYER_POWER_40,
	SPECIAL_PLAYER_POWER_41,
	SPECIAL_PLAYER_POWER_42,
	SPECIAL_PLAYER_POWER_43,
	SPECIAL_PLAYER_POWER_44,
	SPECIAL_PLAYER_POWER_45,
	SPECIAL_PLAYER_POWER_46,
	SPECIAL_PLAYER_POWER_47,
	SPECIAL_PLAYER_POWER_48,
	SPECIAL_PLAYER_POWER_49,
	SPECIAL_PLAYER_POWER_50,
	SPECIAL_PLAYER_POWER_51,
	SPECIAL_PLAYER_POWER_52,
	SPECIAL_PLAYER_POWER_53,
	SPECIAL_PLAYER_POWER_54,
	SPECIAL_PLAYER_POWER_55,
	SPECIAL_PLAYER_POWER_56,
	SPECIAL_PLAYER_POWER_57,
	SPECIAL_PLAYER_POWER_58,
	SPECIAL_PLAYER_POWER_59,
	SPECIAL_PLAYER_POWER_60,
	SPECIAL_PLAYER_POWER_61,
	SPECIAL_PLAYER_POWER_62,
	SPECIAL_PLAYER_POWER_63,
	SPECIAL_PLAYER_POWER_64,
	SPECIAL_PLAYER_POWER_65,
	SPECIAL_PLAYER_POWER_66,
	SPECIAL_PLAYER_POWER_67,
	SPECIAL_PLAYER_POWER_68,
	SPECIAL_PLAYER_POWER_69,
	SPECIAL_PLAYER_POWER_70,
	SPECIAL_PLAYER_POWER_71,
	SPECIAL_PLAYER_POWER_72,
	SPECIAL_PLAYER_POWER_73,
	SPECIAL_PLAYER_POWER_74,
	SPECIAL_PLAYER_POWER_75,
	SPECIAL_PLAYER_POWER_76,
	SPECIAL_PLAYER_POWER_77,
	SPECIAL_PLAYER_POWER_78,
	SPECIAL_PLAYER_POWER_79,
	SPECIAL_PLAYER_POWER_80,
	SPECIAL_PLAYER_POWER_81,
	SPECIAL_PLAYER_POWER_82,
	SPECIAL_PLAYER_POWER_83,
	SPECIAL_PLAYER_POWER_84,
	SPECIAL_PLAYER_POWER_85,
	SPECIAL_PLAYER_POWER_86,
	SPECIAL_PLAYER_POWER_87,
	SPECIAL_PLAYER_POWER_88,
	SPECIAL_PLAYER_POWER_89,
	SPECIAL_PLAYER_POWER_90,
	SPECIAL_PLAYER_POWER_91,
	SPECIAL_PLAYER_POWER_92,
	SPECIAL_PLAYER_POWER_93,
	SPECIAL_PLAYER_POWER_94,
	SPECIAL_PLAYER_POWER_95,
	SPECIAL_PLAYER_POWER_96,

	SPECIALPOWER_COUNT,
	// don't forget to add new strings to SpecialPowerMaskType::s_bitNameList[]
};

	// Tiberium Project @feature whether a power type is one of the converted player powers'
	inline bool isPlayerPowerType( SpecialPowerType t ) { return t >= SPECIAL_PLAYER_POWER_01 && t <= SPECIAL_PLAYER_POWER_96; }

	// Definition of these names is located in SpecialPower.cpp
