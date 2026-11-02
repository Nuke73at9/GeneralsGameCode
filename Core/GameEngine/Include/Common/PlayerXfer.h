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

// Tiberium Project @feature Saving and loading what a game holds for each player, now that Zero Hour holds
// 32 players (GameCommon.h): a save made before kept 16, and a player mask of 16 bits. The callers raised
// their xfer versions with this; an older version reads the old layout ('legacy').

#pragma once

#include "Common/GameCommon.h"
#include "Common/Xfer.h"

#include <string.h>

/// A player mask: all of its bits, or the 16 a save made before kept.
inline void xferPlayerMask( Xfer *xfer, PlayerMaskType *mask, Bool legacy )
{
	if( !legacy )
	{
		xfer->xferUser( mask, sizeof( PlayerMaskType ) );
		return;
	}
	UnsignedShort old = (UnsignedShort)( *mask & 0xffff );
	xfer->xferUser( &old, sizeof( old ) );
	*mask = old;
}

/// One value for each player (an array of MAX_PLAYER_COUNT): all of them, or the first 16 a save made
/// before kept (loading, the rest are cleared: those players did not exist then).
inline void xferPerPlayer( Xfer *xfer, void *values, size_t valueSize, Bool legacy )
{
	if( !legacy )
	{
		xfer->xferUser( values, (Int)( valueSize * MAX_PLAYER_COUNT ) );
		return;
	}
	xfer->xferUser( values, (Int)( valueSize * MAX_PLAYER_COUNT_LEGACY ) );
	if( xfer->getXferMode() == XFER_LOAD && MAX_PLAYER_COUNT > MAX_PLAYER_COUNT_LEGACY )
		memset( (char *)values + valueSize * MAX_PLAYER_COUNT_LEGACY, 0, valueSize * ( MAX_PLAYER_COUNT - MAX_PLAYER_COUNT_LEGACY ) );
}
