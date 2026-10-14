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
