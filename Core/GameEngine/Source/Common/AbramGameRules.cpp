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

#include "PreRTS.h"
#include "Common/AbramGameRules.h"

#include "GameNetwork/GameInfo.h"

Bool AbramGameRules::isEnabled( AbramGameRule rule )
{
	// TheGameInfo is set by GameLogic::startNewGame for skirmish, replays, network games and skirmish
	// save games, and is null in the campaign.
	return TheGameInfo != nullptr && (TheGameInfo->getAbramRules() & (UnsignedInt)rule) != 0;
}

namespace
{
	const UnsignedInt CashMultiplierChoices[] = { 1, 2, 3, 5, 10 };
	const UnsignedInt BuildSpeedChoices[] = { 1, 2, 4, 8 };

	const Int CASH_MULTIPLIER_CHOICE_COUNT = sizeof(CashMultiplierChoices) / sizeof(CashMultiplierChoices[0]);
	const Int BUILD_SPEED_CHOICE_COUNT = sizeof(BuildSpeedChoices) / sizeof(BuildSpeedChoices[0]);

	UnsignedInt currentRules()
	{
		return TheGameInfo != nullptr ? TheGameInfo->getAbramRules() : 0;
	}
}

Int AbramGameRules::getCashMultiplierChoiceCount() { return CASH_MULTIPLIER_CHOICE_COUNT; }
Int AbramGameRules::getBuildSpeedChoiceCount() { return BUILD_SPEED_CHOICE_COUNT; }

UnsignedInt AbramGameRules::getCashMultiplierChoice( Int index )
{
	return (index >= 0 && index < CASH_MULTIPLIER_CHOICE_COUNT) ? CashMultiplierChoices[index] : 1;
}

UnsignedInt AbramGameRules::getBuildSpeedChoice( Int index )
{
	return (index >= 0 && index < BUILD_SPEED_CHOICE_COUNT) ? BuildSpeedChoices[index] : 1;
}

Int AbramGameRules::getCashMultiplierIndex( UnsignedInt rules )
{
	const Int index = (Int)((rules & ABRAM_CASH_MULTIPLIER_MASK) >> ABRAM_CASH_MULTIPLIER_SHIFT);
	return index < CASH_MULTIPLIER_CHOICE_COUNT ? index : 0;
}

Int AbramGameRules::getBuildSpeedIndex( UnsignedInt rules )
{
	const Int index = (Int)((rules & ABRAM_BUILD_SPEED_MASK) >> ABRAM_BUILD_SPEED_SHIFT);
	return index < BUILD_SPEED_CHOICE_COUNT ? index : 0;
}

UnsignedInt AbramGameRules::setCashMultiplierIndex( UnsignedInt rules, Int index )
{
	if (index < 0 || index >= CASH_MULTIPLIER_CHOICE_COUNT)
		index = 0;
	return (rules & ~(UnsignedInt)ABRAM_CASH_MULTIPLIER_MASK) | ((UnsignedInt)index << ABRAM_CASH_MULTIPLIER_SHIFT);
}

UnsignedInt AbramGameRules::setBuildSpeedIndex( UnsignedInt rules, Int index )
{
	if (index < 0 || index >= BUILD_SPEED_CHOICE_COUNT)
		index = 0;
	return (rules & ~(UnsignedInt)ABRAM_BUILD_SPEED_MASK) | ((UnsignedInt)index << ABRAM_BUILD_SPEED_SHIFT);
}

UnsignedInt AbramGameRules::getCashMultiplier()
{
	return getCashMultiplierChoice( getCashMultiplierIndex( currentRules() ) );
}

UnsignedInt AbramGameRules::getBuildSpeed()
{
	return getBuildSpeedChoice( getBuildSpeedIndex( currentRules() ) );
}

UnsignedInt AbramGameRules::scaleIncome( UnsignedInt amount )
{
	return amount * getCashMultiplier();
}

Int AbramGameRules::scaleBuildTime( Int buildFrames )
{
	// Integer division keeps the result identical on every machine.
	const Int speed = (Int)getBuildSpeed();
	if (speed <= 1 || buildFrames <= 0)
		return buildFrames;
	const Int scaled = buildFrames / speed;
	return scaled > 0 ? scaled : 1;
}
