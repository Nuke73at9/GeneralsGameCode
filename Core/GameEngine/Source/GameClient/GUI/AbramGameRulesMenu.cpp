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
#include "GameClient/AbramGameRulesMenu.h"

#include "Common/AbramGameRules.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/GadgetCheckBox.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameNetwork/GameInfo.h"

namespace
{

struct RuleEntry
{
	AbramGameRule rule;
	const char *windowName;
	const wchar_t *label;
	const wchar_t *tooltip;
};

const RuleEntry Rules[] =
{
	{ ABRAM_RULE_LARGE_BUILD_QUEUE, "AbramGameRules:CheckBoxLargeBuildQueue", L"99 Unit Queue",
		L"Production buildings can queue up to 99 units, built in the order you place them." },
};

const Int RULE_COUNT = sizeof(Rules) / sizeof(Rules[0]);

GameWindow *checkBoxes[RULE_COUNT] = { nullptr };

// Layout of the original skirmish setup screen (800x600 design units): the "Limit Superweapons"
// check box and the free row under the player list where the rule boxes go.
const Real ANCHOR_DESIGN_X = 593.0f;
const Real ANCHOR_DESIGN_Y = 336.0f;
const Real ANCHOR_DESIGN_WIDTH = 152.0f;
const Real ROW_DESIGN_X = 49.0f;
const Real ROW_DESIGN_Y = 307.0f;
const Real BOX_DESIGN_WIDTH = 132.0f;

} // namespace

void AbramGameRulesMenu::init( GameWindow *anchorCheckBox )
{
	for (Int i = 0; i < RULE_COUNT; ++i)
		checkBoxes[i] = nullptr;

	if (anchorCheckBox == nullptr)
		return;

	GameWindow *parent = anchorCheckBox->winGetParent();
	if (parent == nullptr)
		return;

	Int anchorScreenX = 0, anchorScreenY = 0, anchorWidth = 0, anchorHeight = 0;
	anchorCheckBox->winGetScreenPosition( &anchorScreenX, &anchorScreenY );
	anchorCheckBox->winGetSize( &anchorWidth, &anchorHeight );
	Int parentScreenX = 0, parentScreenY = 0;
	parent->winGetScreenPosition( &parentScreenX, &parentScreenY );

	// The layout is scaled to the screen resolution, so measure the scale on the anchor.
	const Real scale = anchorWidth > 0 ? (Real)anchorWidth / ANCHOR_DESIGN_WIDTH : 1.0f;
	const Int rowX = anchorScreenX - parentScreenX + REAL_TO_INT( (ROW_DESIGN_X - ANCHOR_DESIGN_X) * scale );
	const Int rowY = anchorScreenY - parentScreenY + REAL_TO_INT( (ROW_DESIGN_Y - ANCHOR_DESIGN_Y) * scale );
	const Int boxWidth = REAL_TO_INT( BOX_DESIGN_WIDTH * scale );

	// Copy the look of the anchor check box (images, colors, font), but none of its identity.
	WinInstanceData *anchorData = anchorCheckBox->winGetInstanceData();

	for (Int i = 0; i < RULE_COUNT; ++i)
	{
		WinInstanceData instData;
		instData.init();
		instData.m_style = anchorData->m_style;
		for (Int d = 0; d < MAX_DRAW_DATA; ++d)
		{
			instData.m_enabledDrawData[d] = anchorData->m_enabledDrawData[d];
			instData.m_disabledDrawData[d] = anchorData->m_disabledDrawData[d];
			instData.m_hiliteDrawData[d] = anchorData->m_hiliteDrawData[d];
		}
		instData.m_enabledText = anchorData->m_enabledText;
		instData.m_disabledText = anchorData->m_disabledText;
		instData.m_hiliteText = anchorData->m_hiliteText;
		instData.m_imageOffset = anchorData->m_imageOffset;
		instData.m_font = anchorData->m_font;
		instData.m_id = TheNameKeyGenerator->nameToKey( Rules[i].windowName );
		instData.m_decoratedNameString = Rules[i].windowName;

		const UnsignedInt status = (anchorCheckBox->winGetStatus() & ~WIN_STATUS_HIDDEN) | WIN_STATUS_ENABLED;
		GameWindow *box = TheWindowManager->gogoGadgetCheckbox( parent, status,
			rowX + i * boxWidth, rowY, boxWidth, anchorHeight, &instData, anchorCheckBox->winGetFont(), FALSE );
		if (box == nullptr)
			continue;

		// Send clicks to the same window as the screen's own check boxes.
		box->winSetOwner( anchorCheckBox->winGetOwner() );
		GadgetCheckBoxSetText( box, UnicodeString( Rules[i].label ) );
		box->winSetTooltip( UnicodeString( Rules[i].tooltip ) );
		checkBoxes[i] = box;
	}
}

void AbramGameRulesMenu::refresh( const GameInfo *game )
{
	const UnsignedInt rules = game ? game->getAbramRules() : 0;
	for (Int i = 0; i < RULE_COUNT; ++i)
	{
		if (checkBoxes[i])
			GadgetCheckBoxSetChecked( checkBoxes[i], (rules & (UnsignedInt)Rules[i].rule) != 0 );
	}
}

Bool AbramGameRulesMenu::handleSelected( GameWindow *control, GameInfo *game )
{
	if (control == nullptr)
		return FALSE;

	for (Int i = 0; i < RULE_COUNT; ++i)
	{
		if (checkBoxes[i] == nullptr || control != checkBoxes[i])
			continue;

		if (game)
		{
			UnsignedInt rules = game->getAbramRules();
			if (GadgetCheckBoxIsChecked( control ))
				rules |= (UnsignedInt)Rules[i].rule;
			else
				rules &= ~(UnsignedInt)Rules[i].rule;
			game->setAbramRules( rules );
		}
		return TRUE;
	}
	return FALSE;
}

void AbramGameRulesMenu::enable( Bool enable )
{
	for (Int i = 0; i < RULE_COUNT; ++i)
	{
		if (checkBoxes[i])
			checkBoxes[i]->winEnable( enable );
	}
}
