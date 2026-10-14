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

// Abram's Personal Test @feature The Mods screen. Opened from the main menu's Mods button.
// Built like the map select screen: a list on the left, a picture and description on the right.

#include "PreRTS.h"
#include "GameClient/ModSelectMenu.h"

#include "Common/AbramImageConvert.h"
#include "Common/FileSystem.h"
#include "Common/GameEngine.h"
#include "Common/GameSwitcher.h"
#include "Common/GlobalData.h"
#include "Common/ModManager.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/AnimateWindowManager.h"
#include "GameClient/Color.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Image.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"

#include <stdint.h>
#include <string.h>

namespace
{

// Size of the converted preview pictures. Powers of two suit every graphics card.
const Int PREVIEW_TEXTURE_WIDTH = 512;
const Int PREVIEW_TEXTURE_HEIGHT = 256;

// List rows store "mod index + 1" as item data, so row data 0 means "no mod".
const Int ROW_DATA_NO_MOD = 0;

Bool isShuttingDown = FALSE;
Bool buttonPushed = FALSE;

GameWindow *parentWindow = nullptr;
GameWindow *listboxMods = nullptr;
GameWindow *winModPreview = nullptr;
GameWindow *textTitle = nullptr;
GameWindow *textModName = nullptr;
GameWindow *textModInfo = nullptr;
GameWindow *textStatus = nullptr;
GameWindow *buttonActivate = nullptr;
GameWindow *buttonBack = nullptr;

NameKeyType listboxModsID = NAMEKEY_INVALID;
NameKeyType buttonActivateID = NAMEKEY_INVALID;
NameKeyType buttonBackID = NAMEKEY_INVALID;

// Mod index of the mod this game session was started with, -1 for none.
Int activeMod = -1;
// Mod index of the selected list row, -1 for none.
Int selectedMod = -1;

GameWindow *findWindow( const char *name )
{
	return TheWindowManager->winGetWindowFromId( nullptr, TheNameKeyGenerator->nameToKey( name ) );
}

void setText( GameWindow *window, const UnicodeString &text )
{
	if (window)
		GadgetStaticTextSetText( window, text );
}

UnicodeString toUnicode( const char *text )
{
	UnicodeString result;
	result.translate( AsciiString( text ) );
	return result;
}

// Builds a name that is safe to use as a file name and an image name.
AsciiString makeSafeName( const char *modName )
{
	AsciiString name = "AbramMod_";
	for (const char *c = modName; *c != '\0'; ++c)
	{
		const char ch = *c;
		const Bool safe = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
		name.concat( safe ? ch : '_' );
	}
	return name;
}

// Returns the mod's picture as an image the window can draw, or null if it has none.
// The picture is converted to a TGA in the user's MapPreviews folder, where the texture loader
// also looks for map previews.
const Image *getModPreviewImage( Int modIndex )
{
	if (modIndex < 0 || TheGlobalData == nullptr || TheMappedImageCollection == nullptr)
		return nullptr;

	AsciiString imageName = makeSafeName( ModManager::getModName( modIndex ) );

	const Image *existing = TheMappedImageCollection->findImageByName( imageName );
	if (existing)
		return existing;

	char sourcePath[260];
	if (!ModManager::findPreviewImage( modIndex, sourcePath, sizeof(sourcePath) ))
		return nullptr;

	AsciiString previewDir;
	previewDir.format( MAP_PREVIEW_DIR_PATH, TheGlobalData->getPath_UserData().str() );
	TheFileSystem->createDirectory( previewDir );

	AsciiString fileName = imageName;
	fileName.concat( ".tga" );

	AsciiString targetPath = previewDir;
	targetPath.concat( fileName );

	if (!AbramConvertImageToTga( sourcePath, targetPath.str(), PREVIEW_TEXTURE_WIDTH, PREVIEW_TEXTURE_HEIGHT ))
		return nullptr;

	Image *image = newInstance(Image);
	image->setName( imageName );
	image->setFilename( fileName );
	image->setStatus( IMAGE_STATUS_NONE );

	Region2D uv;
	uv.lo.x = 0.0f;
	uv.lo.y = 0.0f;
	uv.hi.x = 1.0f;
	uv.hi.y = 1.0f;
	image->setUV( &uv );
	image->setTextureWidth( PREVIEW_TEXTURE_WIDTH );
	image->setTextureHeight( PREVIEW_TEXTURE_HEIGHT );

	ICoord2D size;
	size.x = PREVIEW_TEXTURE_WIDTH;
	size.y = PREVIEW_TEXTURE_HEIGHT;
	image->setImageSize( &size );

	TheMappedImageCollection->addImage( image );
	return image;
}

void showPreview( Int modIndex )
{
	if (winModPreview == nullptr)
		return;

	const Image *image = getModPreviewImage( modIndex );
	if (image)
	{
		winModPreview->winSetStatus( WIN_STATUS_IMAGE );
		winModPreview->winSetEnabledImage( 0, image );
	}
	else
	{
		winModPreview->winClearStatus( WIN_STATUS_IMAGE );
	}
}

void showDetails( Int modIndex )
{
	if (modIndex < 0)
	{
		setText( textModName, UnicodeString( L"Original Game" ) );
		setText( textModInfo, UnicodeString( L"Play without mods.\n\nThis restarts the game without loading anything from the Mods folder." ) );
		return;
	}

	setText( textModName, toUnicode( ModManager::getModName( modIndex ) ) );

	char description[1024];
	UnicodeString info;
	if (ModManager::readDescription( modIndex, description, sizeof(description) ))
	{
		info = toUnicode( description );
	}
	else
	{
		info.format( L"%d data file(s).\n\nTip: add a description.txt and a preview.png to this mod's folder to show them here.",
			ModManager::countBigFiles( modIndex ) );
	}
	setText( textModInfo, info );
}

void updateButtons()
{
	if (buttonActivate == nullptr)
		return;

	if (selectedMod == activeMod)
	{
		GadgetButtonSetText( buttonActivate, UnicodeString( L"ACTIVE" ) );
		buttonActivate->winEnable( FALSE );
	}
	else
	{
		GadgetButtonSetText( buttonActivate, UnicodeString( selectedMod < 0 ? L"PLAY ORIGINAL" : L"ACTIVATE" ) );
		buttonActivate->winEnable( TRUE );
	}
}

void selectRow( Int row )
{
	selectedMod = -1;
	if (listboxMods && row >= 0)
	{
		const Int data = (Int)(intptr_t)GadgetListBoxGetItemData( listboxMods, row );
		selectedMod = data - 1;
	}

	showPreview( selectedMod );
	showDetails( selectedMod );
	updateButtons();
}

void populateList()
{
	if (listboxMods == nullptr)
		return;

	GadgetListBoxReset( listboxMods );

	const Color normal = GameMakeColor( 255, 255, 255, 255 );
	const Color highlighted = GameMakeColor( 186, 255, 12, 255 );

	Int activeRow = 0;

	Int row = GadgetListBoxAddEntryText( listboxMods, UnicodeString( L"Original Game" ),
		activeMod < 0 ? highlighted : normal, -1, 0 );
	GadgetListBoxSetItemData( listboxMods, (void *)(intptr_t)ROW_DATA_NO_MOD, row );

	for (Int i = 0; i < ModManager::getModCount(); ++i)
	{
		row = GadgetListBoxAddEntryText( listboxMods, toUnicode( ModManager::getModName( i ) ),
			i == activeMod ? highlighted : normal, -1, 0 );
		GadgetListBoxSetItemData( listboxMods, (void *)(intptr_t)(i + 1), row );
		if (i == activeMod)
			activeRow = row;
	}

	GadgetListBoxSetSelected( listboxMods, activeRow );
	selectRow( activeRow );
}

void updateStatusText()
{
	UnicodeString status;
	if (activeMod >= 0)
		status.format( L"Now playing: %ls", toUnicode( ModManager::getModName( activeMod ) ).str() );
	else if (ModManager::getModCount() == 0)
		status.format( L"No mods found. Put each mod in its own folder in: %ls", toUnicode( ModManager::getModsRoot() ).str() );
	else
		status = UnicodeString( L"Now playing: Original Game" );
	setText( textStatus, status );
}

void activateSelected()
{
	if (selectedMod == activeMod)
		return;

	const char *folder = (selectedMod >= 0) ? ModManager::getModFolder( selectedMod ) : "";
	if (GameSwitcher::requestRestartWithMod( folder ))
	{
		// Quit now; WinMain starts the game again with the chosen mod.
		buttonPushed = TRUE;
		TheGameEngine->setQuitting( TRUE );
	}
}

void shutdownComplete( WindowLayout *layout )
{
	isShuttingDown = FALSE;
	layout->hide( TRUE );
	TheShell->shutdownComplete( layout );
}

} // namespace

//-------------------------------------------------------------------------------------------------
void ModSelectMenuInit( WindowLayout *layout, void *userData )
{
	buttonPushed = FALSE;
	isShuttingDown = FALSE;

	TheShell->showShellMap( TRUE );
	layout->hide( FALSE );

	parentWindow = findWindow( "ModSelectMenu.wnd:ModSelectMenuParent" );
	listboxMods = findWindow( "ModSelectMenu.wnd:ListboxMods" );
	winModPreview = findWindow( "ModSelectMenu.wnd:WinModPreview" );
	textTitle = findWindow( "ModSelectMenu.wnd:StaticTextTitle" );
	textModName = findWindow( "ModSelectMenu.wnd:StaticTextModName" );
	textModInfo = findWindow( "ModSelectMenu.wnd:StaticTextModInfo" );
	textStatus = findWindow( "ModSelectMenu.wnd:StaticTextStatus" );
	buttonActivate = findWindow( "ModSelectMenu.wnd:ButtonActivate" );
	buttonBack = findWindow( "ModSelectMenu.wnd:ButtonBack" );

	listboxModsID = TheNameKeyGenerator->nameToKey( "ModSelectMenu.wnd:ListboxMods" );
	buttonActivateID = TheNameKeyGenerator->nameToKey( "ModSelectMenu.wnd:ButtonActivate" );
	buttonBackID = TheNameKeyGenerator->nameToKey( "ModSelectMenu.wnd:ButtonBack" );

	setText( textTitle, UnicodeString( L"MODS" ) );
	if (buttonBack)
		GadgetButtonSetText( buttonBack, UnicodeString( L"BACK" ) );

	ModManager::scan();
	activeMod = ModManager::findModByFolder( TheGlobalData ? TheGlobalData->m_modDir.str() : "" );

	populateList();
	updateStatusText();

	if (parentWindow)
		TheWindowManager->winSetFocus( parentWindow );

	if (buttonBack)
		TheShell->registerWithAnimateManager( buttonBack, WIN_ANIMATION_SLIDE_RIGHT, TRUE, 0 );
	if (buttonActivate)
		TheShell->registerWithAnimateManager( buttonActivate, WIN_ANIMATION_SLIDE_LEFT, TRUE, 0 );
}

//-------------------------------------------------------------------------------------------------
void ModSelectMenuShutdown( WindowLayout *layout, void *userData )
{
	isShuttingDown = TRUE;

	// If we are shutting down for an immediate pop, skip the animations.
	Bool popImmediate = *(Bool *)userData;
	if (popImmediate)
	{
		shutdownComplete( layout );
		return;
	}

	TheShell->reverseAnimatewindow();
}

//-------------------------------------------------------------------------------------------------
void ModSelectMenuUpdate( WindowLayout *layout, void *userData )
{
	if (isShuttingDown && TheShell->isAnimFinished())
		shutdownComplete( layout );
}

//-------------------------------------------------------------------------------------------------
WindowMsgHandledType ModSelectMenuInput( GameWindow *window, UnsignedInt msg,
	WindowMsgData mData1, WindowMsgData mData2 )
{
	if (msg == GWM_CHAR)
	{
		UnsignedByte key = mData1;
		UnsignedByte state = mData2;
		if (buttonPushed)
			return MSG_IGNORED;

		if (key == KEY_ESC)
		{
			// Escape works like the Back button.
			if (BitIsSet( state, KEY_STATE_UP ) && buttonBack)
				TheWindowManager->winSendSystemMsg( window, GBM_SELECTED, (WindowMsgData)buttonBack, buttonBackID );
			return MSG_HANDLED;
		}
	}

	return MSG_IGNORED;
}

//-------------------------------------------------------------------------------------------------
WindowMsgHandledType ModSelectMenuSystem( GameWindow *window, UnsignedInt msg,
	WindowMsgData mData1, WindowMsgData mData2 )
{
	switch (msg)
	{
		case GWM_CREATE:
		case GWM_DESTROY:
			break;

		case GWM_INPUT_FOCUS:
		{
			// We want the keyboard focus, so Escape reaches us.
			if (mData1 == TRUE)
				*(Bool *)mData2 = TRUE;
			return MSG_HANDLED;
		}

		case GLM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			if (control && control->winGetWindowId() == listboxModsID)
				selectRow( (Int)mData2 );
			break;
		}

		case GLM_DOUBLE_CLICKED:
		{
			if (buttonPushed)
				break;
			GameWindow *control = (GameWindow *)mData1;
			if (control && control->winGetWindowId() == listboxModsID && (Int)mData2 >= 0)
			{
				selectRow( (Int)mData2 );
				activateSelected();
			}
			break;
		}

		case GBM_SELECTED:
		{
			if (buttonPushed)
				break;

			GameWindow *control = (GameWindow *)mData1;
			const Int controlID = control ? control->winGetWindowId() : NAMEKEY_INVALID;

			if (controlID == buttonBackID)
			{
				buttonPushed = TRUE;
				TheShell->pop();
			}
			else if (controlID == buttonActivateID)
			{
				activateSelected();
			}
			break;
		}

		default:
			return MSG_IGNORED;
	}

	return MSG_HANDLED;
}
