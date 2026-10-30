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

// FILE: ShieldSphereUpdate.h /////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's shield spheres (C&C4's ShieldSphereUpdate and
// ReflectiveShieldSphereUpdate): a bubble around the object that takes the hits meant for it and
// its friends inside, from outside, until it has taken its fill; it mends itself and comes back up.
//
//   Behavior = ShieldSphereUpdate ModuleTag_Shield
//     Radius = 120.0                   ; the bubble (0: the object alone)
//     MaxDamage = 300                  ; what it takes before it goes down (0: never goes down)
//     RegenPerSecond = 5               ; what it mends a second
//     RaiseAtPercent = 90%             ; down, it comes back up mended this far
//     DamageTypes = LASER ARMOR_PIERCING SMALL_ARMS EXPLOSION   ; the hits it takes (default: all)
//     AbsorbKillingBlow = Yes          ; the hit that brings it down is taken whole
//     IncludeSelf = Yes                ; the object itself is inside (default Yes)
//     ReactChance = 25%                ; the share of hits it takes (default 100%)
//     DamageArc = 180                  ; only hits from within this arc of its front (default 360)
//     ReflectDamage = Yes              ; a hit it takes goes back to who fired it
//     HitFX = FX_ShieldHit             ; where a hit lands on it
//     BonusUpgrade = Upgrade_X         ; with this upgrade (the player's or the object's) ...
//     BonusMaxDamage = 300             ; ... it takes this much more
//     StartsActive = No                ; else up from the start
//     TriggeredBy = Upgrade_Y          ; (upgrade mux) up once this is had
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Damage.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"

#include <vector>

class FXList;

//-------------------------------------------------------------------------------------------------
class ShieldSphereUpdateModuleData : public UpdateModuleData
{
public:
	UpgradeMuxData m_upgradeMuxData;
	Bool m_initiallyActive;
	Real m_radius;
	Real m_maxDamage;
	Real m_regenPerSecond;
	Real m_raiseAtPercent;
	DamageTypeFlags m_damageTypes;
	Bool m_absorbKillingBlow;
	Bool m_includeSelf;
	Real m_reactChance;
	Real m_damageArc;					///< radians
	Bool m_reflectDamage;
	const FXList *m_hitFX;
	AsciiString m_bonusUpgrade;
	Real m_bonusMaxDamage;

	ShieldSphereUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class ShieldSphereUpdate : public UpdateModule, public UpgradeMux
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ShieldSphereUpdate, "ShieldSphereUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ShieldSphereUpdate, ShieldSphereUpdateModuleData )

public:

	ShieldSphereUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | MODULEINTERFACE_UPGRADE; }
	virtual UpgradeModuleInterface* getUpgrade() override { return this; }

	virtual UpdateSleepTime update() override;

	/** whether a shield takes this hit on the victim (and takes it if so): from ActiveBody */
	static Bool absorbDamage( Object *victim, Object *source, DamageType type, Real amount );

protected:

	virtual void upgradeImplementation() override;
	virtual void getUpgradeActivationMasks( UpgradeMaskType& activation, UpgradeMaskType& conflicting ) const override
	{
		getShieldSphereUpdateModuleData()->m_upgradeMuxData.getUpgradeActivationMasks( activation, conflicting );
	}
	virtual void performUpgradeFX() override
	{
		getShieldSphereUpdateModuleData()->m_upgradeMuxData.performUpgradeFX( getObject() );
	}
	virtual void processUpgradeRemoval() override
	{
		getShieldSphereUpdateModuleData()->m_upgradeMuxData.muxDataProcessUpgradeRemoval( getObject() );
	}
	virtual Bool requiresAllActivationUpgrades() const override
	{
		return getShieldSphereUpdateModuleData()->m_upgradeMuxData.m_requiresAllTriggers;
	}
	virtual Bool isSubObjectsUpgrade() override { return false; }

private:

	Real getMaxDamage() const;
	Bool takeHit( Object *victim, Object *source, DamageType type, Real amount );

	Real m_damage;			///< what it has taken and not yet mended
	Bool m_down;				///< brought down: back up once mended to RaiseAtPercent

	static std::vector<ShieldSphereUpdate *> s_shields;		///< every one there is, in the order made

};
