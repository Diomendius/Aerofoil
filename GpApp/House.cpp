//============================================================================
//----------------------------------------------------------------------------
//									House.c
//----------------------------------------------------------------------------
//============================================================================

#include "PLAppleEvents.h"
#include "PLKeyEncoding.h"
#include "PLNavigation.h"
#include "PLNumberFormatting.h"
#include "PLPasStr.h"
#include "PLResources.h"
#include "PLSound.h"
#include "DialogUtils.h"
#include "Externs.h"
#include "FileManager.h"
#include "House.h"
#include "RectUtils.h"


#define kGoToDialogID			1043


void UpdateGoToDialog (Dialog *);
Boolean GoToFilter (Dialog *, EventRecord *, short *);


houseHand	thisHouse;
linksPtr	linksList;
Str32		thisHouseName;
short		srcLocations[kMaxRoomObs];
short		destLocations[kMaxRoomObs];
short		wasFloor, wasSuite;
retroLink	retroLinkList[kMaxRoomObs];
Boolean		houseUnlocked;


extern	gameType	smallGame;
extern	short		numberRooms, mapLeftRoom, mapTopRoom, numStarsRemaining;
extern	Boolean		houseOpen, noRoomAtAll;
extern	Boolean		twoPlayerGame, wardBitSet, phoneBitSet;


//==============================================================  Functions
//--------------------------------------------------------------  CreateNewHouse
// Called to create a new house file.

#ifndef COMPILEDEMO
Boolean CreateNewHouse (void)
{
	AEKeyword			theKeyword;
	DescType			actualType;
	Size				actualSize;
	NavReplyRecord		theReply;
	NavDialogOptions	dialogOptions;
	VFileSpec			tempSpec;
	VFileSpec			theSpec;
	PLError_t			theErr;
	
	theErr = NavGetDefaultDialogOptions(&dialogOptions);
	theErr = NavPutFile(nil, &theReply, &dialogOptions, nil, 'gliH', 'ozm5', nil);
	if (theErr == PLErrors::kUserCancelled_TEMP)
		return false;
	if (!theReply.validRecord)
		return (false);
	
	theErr = AEGetNthPtr(&(theReply.selection), 1, typeFSS, &theKeyword, 
			&actualType, &theSpec, sizeof(VFileSpec), &actualSize);

	PortabilityLayer::FileManager *fm = PortabilityLayer::FileManager::GetInstance();

	if (theReply.replacing)
	{
		if (fm->FileExists(theSpec.m_dir, theSpec.m_name))
		{
			CheckFileError(PLErrors::kFileNotFound, theSpec.m_name);
			return (false);
		}

		if (!fm->DeleteFile(theSpec.m_dir, theSpec.m_name))
		{
			CheckFileError(PLErrors::kAccessDenied, theSpec.m_name);
			return (false);
		}
	}
	
	if (houseOpen)
	{
		if (!CloseHouse())
			return (false);
	}
	
	theErr = fm->CreateFileAtCurrentTime(theSpec.m_dir, theSpec.m_name, 'ozm5', 'gliH');
	if (!CheckFileError(theErr, PSTR("New House")))
		return (false);

	theErr = HCreateResFile(theSpec.m_dir, theSpec.m_name);
	if (theErr != PLErrors::kNone)
		YellowAlert(kYellowFailedResCreate, theErr);
	
	PasStringCopy(theSpec.m_name, thisHouseName);
	AddExtraHouse(theSpec);
	BuildHouseList();
	InitCursor();
	if (!OpenHouse())
		return (false);
	
	return (true);
}
#endif

//--------------------------------------------------------------  InitializeEmptyHouse

// Initializes all the structures for an empty (new) house.

#ifndef COMPILEDEMO
Boolean InitializeEmptyHouse (void)
{
	houseType		*thisHousePtr;
	Str255			tempStr;
	
	if (thisHouse != nil)
		thisHouse.Dispose();
	
	thisHouse = NewHandle(sizeof(houseType)).StaticCast<houseType>();
	
	if (thisHouse == nil)
	{
		YellowAlert(kYellowUnaccounted, 1);
		return (false);
	}
	
	thisHousePtr = *thisHouse;
	
	thisHousePtr->version = kHouseVersion;
	thisHousePtr->firstRoom = -1;
	thisHousePtr->timeStamp = 0L;
	thisHousePtr->flags = 0L;
	thisHousePtr->initial.h = 32;
	thisHousePtr->initial.v = 32;
	ZeroHighScores();
	
	GetLocalizedString(11, tempStr);
	PasStringCopy(tempStr, thisHousePtr->banner);
	GetLocalizedString(12, tempStr);
	PasStringCopy(tempStr, thisHousePtr->trailer);
	thisHousePtr->hasGame = false;
	thisHousePtr->nRooms = 0;
	
	wardBitSet = false;
	phoneBitSet = false;
	
	numberRooms = 0;
	mapLeftRoom = 60;
	mapTopRoom = 50;
	thisRoomNumber = kRoomIsEmpty;
	previousRoom = -1;
	houseUnlocked = true;
	OpenMapWindow();
	UpdateMapWindow();
	noRoomAtAll = true;
	fileDirty = true;
	UpdateMenus(false);
	ReflectCurrentRoom(true);
	
	return (true);
}
#endif

//--------------------------------------------------------------  RealRoomNumberCount

// Returns the real number of rooms in a house (some rooms may still�
// be place-holders - they were deleted earlier and are flagged as�
// deleted but still occupy space in the file).

short RealRoomNumberCount (void)
{
	short		realRoomCount, i;
	char		wasState;
	
	realRoomCount = (*thisHouse)->nRooms;
	if (realRoomCount != 0)
	{
		for (i = 0; i < (*thisHouse)->nRooms; i++)
		{
			if ((*thisHouse)->rooms[i].suite == kRoomIsEmpty)
				realRoomCount--;
		}
	}
	
	return (realRoomCount);
}

//--------------------------------------------------------------  GetFirstRoomNumber

// Returns the room number (indicee into house file) of the room where�
// the player is to begin.

short GetFirstRoomNumber (void)
{
	short		firstRoom;
	char		wasState;
	
	if ((*thisHouse)->nRooms <= 0)
	{
		firstRoom = -1;
		noRoomAtAll = true;
	}
	else
	{
		firstRoom = (*thisHouse)->firstRoom;
		if ((firstRoom >= (*thisHouse)->nRooms) || (firstRoom < 0))
			firstRoom = 0;
	}
	
	return (firstRoom);
}

//--------------------------------------------------------------  WhereDoesGliderBegin

// Returns a rectangle indicating where in the first room the player's�
// glider is to appear.

void WhereDoesGliderBegin (Rect *theRect, short mode)
{
	Point		initialPt;
	char		wasState;
	
	if (mode == kResumeGameMode)
		initialPt = smallGame.where;
	else if (mode == kNewGameMode)
		initialPt = (*thisHouse)->initial;
	
	QSetRect(theRect, 0, 0, kGliderWide, kGliderHigh);
	QOffsetRect(theRect, initialPt.h, initialPt.v);
}

//--------------------------------------------------------------  HouseHasOriginalPicts

// Returns true is the current house has custom artwork imbedded.

Boolean HouseHasOriginalPicts (void)
{
	short		nPicts;
	
	nPicts = Count1Resources('PICT');
	return (nPicts > 0);
}

//--------------------------------------------------------------  CountHouseLinks

// Counts up the number of linked objects in a house.

short CountHouseLinks (void)
{
	houseType	*thisHousePtr;
	short		numRooms, numLinks;
	short		r, i, what;
	char		wasState;
	
	numLinks = 0;
	
	thisHousePtr = *thisHouse;
	numRooms = thisHousePtr->nRooms;
	
	for (r = 0; r < numRooms; r++)
	{
		for (i = 0; i < kMaxRoomObs; i++)
		{
			what = thisHousePtr->rooms[r].objects[i].what;
			switch (what)
			{
				case kLightSwitch:
				case kMachineSwitch:
				case kThermostat:
				case kPowerSwitch:
				case kKnifeSwitch:
				case kInvisSwitch:
				case kTrigger:
				case kLgTrigger:
				if (thisHousePtr->rooms[r].objects[i].data.e.where != -1)
					numLinks++;
				break;
				
				case kMailboxLf:
				case kMailboxRt:
				case kFloorTrans:
				case kCeilingTrans:
				case kInvisTrans:
				case kDeluxeTrans:
				if (thisHousePtr->rooms[r].objects[i].data.d.where != -1)
					numLinks++;
				break;
			}
		}
	}
	
	return (numLinks);
}

//--------------------------------------------------------------  GenerateLinksList

// Generates a list of all objects that have links and what rooms�
// and objects they are linked to.  It is called in order to preserve�
// the links if the objects or rooms in a house are to be shuffled�
// around. 

#ifndef COMPILEDEMO
void GenerateLinksList (void)
{
	houseType	*thisHousePtr;
	objectType	thisObject;
	short		numLinks, numRooms, r, i, what;
	short		floor, suite, roomLinked, objectLinked;
	char		wasState;
	
	thisHousePtr = *thisHouse;
	numRooms = thisHousePtr->nRooms;
	
	numLinks = 0;
	
	for (r = 0; r < numRooms; r++)
	{
		for (i = 0; i < kMaxRoomObs; i++)
		{
			what = thisHousePtr->rooms[r].objects[i].what;
			switch (what)
			{
				case kLightSwitch:
				case kMachineSwitch:
				case kThermostat:
				case kPowerSwitch:
				case kKnifeSwitch:
				case kInvisSwitch:
				case kTrigger:
				case kLgTrigger:
				thisObject = thisHousePtr->rooms[r].objects[i];
				if (thisObject.data.e.where != -1)
				{
					ExtractFloorSuite(thisObject.data.e.where, &floor, &suite);
					roomLinked = GetRoomNumber(floor, suite);
					objectLinked = (short)thisObject.data.e.who;
					linksList[numLinks].srcRoom = r;
					linksList[numLinks].srcObj = i;
					linksList[numLinks].destRoom = roomLinked;
					linksList[numLinks].destObj = objectLinked;
					numLinks++;
				}
				break;
				
				case kMailboxLf:
				case kMailboxRt:
				case kFloorTrans:
				case kCeilingTrans:
				case kInvisTrans:
				case kDeluxeTrans:
				thisObject = thisHousePtr->rooms[r].objects[i];
				if (thisObject.data.d.where != -1)
				{
					ExtractFloorSuite(thisObject.data.d.where, &floor, &suite);
					roomLinked = GetRoomNumber(floor, suite);
					objectLinked = (short)thisObject.data.d.who;
					linksList[numLinks].srcRoom = r;
					linksList[numLinks].srcObj = i;
					linksList[numLinks].destRoom = roomLinked;
					linksList[numLinks].destObj = objectLinked;
					numLinks++;
				}
				break;
			}
		}
	}
}
#endif

//--------------------------------------------------------------  SortRoomObjects

// I'm a little fuzzy on what this does.

#ifndef COMPILEDEMO
void SortRoomsObjects (short which)
{
	short		probe, probe2, room, obj;
	Boolean		busy, looking;
	
	busy = true;
	probe = 0;
	
	do
	{
		if ((*thisHouse)->rooms[which].objects[probe].what == kObjectIsEmpty)
		{
			looking = true;
			probe2 = probe + 1;			// begin by looking at the next object
			do
			{
				if ((*thisHouse)->rooms[which].objects[probe2].what != kObjectIsEmpty)
				{
					(*thisHouse)->rooms[which].objects[probe] = 
							(*thisHouse)->rooms[which].objects[probe2];
					(*thisHouse)->rooms[which].objects[probe2].what = kObjectIsEmpty;
					if (srcLocations[probe2] != -1)
						linksList[srcLocations[probe2]].srcObj = probe;
					if (destLocations[probe2] != -1)
					{
						linksList[destLocations[probe2]].destObj = probe;
						room = linksList[destLocations[probe2]].srcRoom;
						obj = linksList[destLocations[probe2]].srcObj;
						(*thisHouse)->rooms[room].objects[obj].data.e.who = static_cast<Byte>(probe);
					}
					fileDirty = true;
					looking = false;
				}
				probe2++;
				if ((probe2 >= kMaxRoomObs) && (looking))
				{
					looking = false;
					busy = false;
				}
			}
			while (looking);
		}
		probe++;
		if (probe >= (kMaxRoomObs - 1))
			busy = false;
	}
	while (busy);
}
#endif

//--------------------------------------------------------------  SortHouseObjects

// I'm a little fuzzy on what this does exactly either.

#ifndef COMPILEDEMO
void SortHouseObjects (void)
{
	houseType	*thisHousePtr;
	short		numLinks, numRooms, r, i, l;
	char		wasState;
	
	SpinCursor(3);
	
	CopyThisRoomToRoom();
	
	numLinks = CountHouseLinks();
	if (numLinks == 0)
		return;
	
	linksList = nil;
	linksList = (linksPtr)NewPtr(sizeof(linksType) * numLinks);
	if (linksList == nil)
		RedAlert(kErrNoMemory);
	
	GenerateLinksList();
	
	thisHousePtr = *thisHouse;
	numRooms = thisHousePtr->nRooms;
	
	for (r = 0; r < numRooms; r++)
	{
		for (i = 0; i < kMaxRoomObs; i++)	// initialize arrays
		{
			srcLocations[i] = -1;
			destLocations[i] = -1;
		}
		
		for (i = 0; i < kMaxRoomObs; i++)	// walk object list
		{
			for (l = 0; l < numLinks; l++)	// walk link list
			{
				if ((linksList[l].srcRoom == r) && (linksList[l].srcObj == i))
					srcLocations[i] = l;
				if ((linksList[l].destRoom == r) && (linksList[l].destObj == i))
					destLocations[i] = l;
			}
		}
		SortRoomsObjects(r);
		
		if ((r & 0x0007) == 0x0007)
			IncrementCursor();
	}
	
	SpinCursor(3);
	if (linksList != nil)
		DisposePtr((Ptr)linksList);
	ForceThisRoom(thisRoomNumber);
}
#endif

//--------------------------------------------------------------  CountRoomsVisited

// Goes through and counts the number of rooms a player has been to in�
// the current game.

short CountRoomsVisited (void)
{
	houseType	*thisHousePtr;
	short		numRooms, r, count;
	char		wasState;
	
	thisHousePtr = *thisHouse;
	numRooms = thisHousePtr->nRooms;
	count = 0;
	
	for (r = 0; r < numRooms; r++)
	{
		if (thisHousePtr->rooms[r].visited)
			count++;
	}
	
	return (count);
}

//--------------------------------------------------------------  GenerateRetroLinks

// Walk entire house looking for objects which are linked to objects�
// in the current room.

void GenerateRetroLinks (void)
{
	houseType	*thisHousePtr;
	objectType	thisObject;
	short		i, r, numRooms, floor, suite;
	short		what, roomLinked, objectLinked;
	char		wasState;
	
	for (i = 0; i < kMaxRoomObs; i++)		// Initialize array.
		retroLinkList[i].room = -1;
	
	thisHousePtr = *thisHouse;
	numRooms = thisHousePtr->nRooms;
	
	for (r = 0; r < numRooms; r++)
	{
		for (i = 0; i < kMaxRoomObs; i++)
		{
			what = thisHousePtr->rooms[r].objects[i].what;
			switch (what)
			{
				case kLightSwitch:
				case kMachineSwitch:
				case kThermostat:
				case kPowerSwitch:
				case kKnifeSwitch:
				case kInvisSwitch:
				case kTrigger:
				case kLgTrigger:
				thisObject = thisHousePtr->rooms[r].objects[i];
				if (thisObject.data.e.where != -1)
				{
					ExtractFloorSuite(thisObject.data.e.where, &floor, &suite);
					roomLinked = GetRoomNumber(floor, suite);
					if (roomLinked == thisRoomNumber)
					{
						objectLinked = (short)thisObject.data.e.who;
						if (retroLinkList[objectLinked].room == -1)
						{
							retroLinkList[objectLinked].room = r;
							retroLinkList[objectLinked].object = i;
						}
					}
				}
				break;
				
				case kMailboxLf:
				case kMailboxRt:
				case kFloorTrans:
				case kCeilingTrans:
				case kInvisTrans:
				case kDeluxeTrans:
				thisObject = thisHousePtr->rooms[r].objects[i];
				if (thisObject.data.d.where != -1)
				{
					ExtractFloorSuite(thisObject.data.d.where, &floor, &suite);
					roomLinked = GetRoomNumber(floor, suite);
					if (roomLinked == thisRoomNumber)
					{
						objectLinked = (short)thisObject.data.d.who;
						if (retroLinkList[objectLinked].room == -1)
						{
							retroLinkList[objectLinked].room = r;
							retroLinkList[objectLinked].object = i;
						}
					}
				}
				break;
			}
		}
	}
}

//--------------------------------------------------------------  UpdateGoToDialog
// Redraws the "Go To Room..." dialog.

void UpdateGoToDialog (Dialog *theDialog)
{
	DrawDialog(theDialog);
	DrawDefaultButton(theDialog);
	FrameDialogItemC(theDialog, 10, kRedOrangeColor8);
}

//--------------------------------------------------------------  GoToFilter
// Dialog filter for the "Go To Room..." dialog.

Boolean GoToFilter (Dialog *dial, EventRecord *event, short *item)
{
	switch (event->what)
	{
		case keyDown:
		switch (event->message)
		{
			case PL_KEY_SPECIAL(kEnter):
			case PL_KEY_NUMPAD_SPECIAL(kEnter):
			FlashDialogButton(dial, kOkayButton);
			*item = kOkayButton;
			return(true);
			break;
			
			default:
			return(false);
		}
		break;
		
		case updateEvt:
		SetPortDialogPort(dial);
		UpdateGoToDialog(dial);
		EndUpdate(dial->GetWindow());
		event->what = nullEvent;
		return(false);
		break;
		
		default:
		return(false);
		break;
	}
}

//--------------------------------------------------------------  DoGoToDialog

// "Go To Room..." dialog.

 void DoGoToDialog (void)
 {
 	#define			kGoToFirstButt		2
 	#define			kGoToPrevButt		3
 	#define			kGoToFSButt			4
 	#define			kFloorEditText		5
 	#define			kSuiteEditText		6
	Dialog			*theDialog;
 	long			tempLong;
	short			item, roomToGoTo;
	Boolean			leaving, canceled;

	BringUpDialog(&theDialog, kGoToDialogID);
	
	if (GetFirstRoomNumber() == thisRoomNumber)
		MyDisableControl(theDialog, kGoToFirstButt);
	if ((!RoomNumExists(previousRoom)) || (previousRoom == thisRoomNumber))
		MyDisableControl(theDialog, kGoToPrevButt);
	
	SetDialogNumToStr(theDialog, kFloorEditText, (long)wasFloor);
	SetDialogNumToStr(theDialog, kSuiteEditText, (long)wasSuite);
	SelectDialogItemText(theDialog, kFloorEditText, 0, 1024);
	
	leaving = false;
	canceled = false;
	
	while (!leaving)
	{
		ModalDialog(GoToFilter, &item);
		
		if (item == kOkayButton)
		{
			roomToGoTo = -1;
			canceled = true;
			leaving = true;
		}
		else if (item == kGoToFirstButt)
		{
			roomToGoTo = GetFirstRoomNumber();
			leaving = true;
		}
		else if (item == kGoToPrevButt)
		{
			roomToGoTo = previousRoom;
			leaving = true;
		}
		else if (item == kGoToFSButt)
		{
			GetDialogNumFromStr(theDialog, kFloorEditText, &tempLong);
			wasFloor = (short)tempLong;
			GetDialogNumFromStr(theDialog, kSuiteEditText, &tempLong);
			wasSuite = (short)tempLong;
			roomToGoTo = GetRoomNumber(wasFloor, wasSuite);
			leaving = true;
		}
	}
	
	theDialog->Destroy();
	
	if (!canceled)
	{
		if (RoomNumExists(roomToGoTo))
		{
			DeselectObject();
			CopyRoomToThisRoom(roomToGoTo);
			ReflectCurrentRoom(false);
		}
		else
			SysBeep(1);
	}
}

//--------------------------------------------------------------  ConvertHouseVer1To2

// This function goes through an old version 1 house and converts it�
// to version 2.

void ConvertHouseVer1To2 (void)
{
	Str255		roomStr, message;
	short		wasRoom, floor, suite;
	short		i, h, numRooms;
	char		wasState;
	
	CopyThisRoomToRoom();
	wasRoom = thisRoomNumber;
	GetLocalizedString(13, message);
	OpenMessageWindow(message);
	
	SpinCursor(3);
	
	numRooms = (*thisHouse)->nRooms;
	for (i = 0; i < numRooms; i++)
	{
		if ((*thisHouse)->rooms[i].suite != kRoomIsEmpty)
		{
			NumToString((long)i, roomStr);
			GetLocalizedString(14, message);
			PasStringConcat(message, roomStr);
			SetMessageWindowMessage(message);
			SpinCursor(1);
			
			ForceThisRoom(i);
			for (h = 0; h < kMaxRoomObs; h++)
			{
				switch (thisRoom->objects[h].what)
				{
					case kMailboxLf:
					case kMailboxRt:
					case kFloorTrans:
					case kCeilingTrans:
					case kInvisTrans:
					case kDeluxeTrans:
					if (thisRoom->objects[h].data.d.where != -1)
					{
						ExtractFloorSuite(thisRoom->objects[h].data.d.where, &floor, &suite);
						floor += kNumUndergroundFloors;
						thisRoom->objects[h].data.d.where = MergeFloorSuite(floor, suite);
					}
					break;
					
					case kLightSwitch:
					case kMachineSwitch:
					case kThermostat:
					case kPowerSwitch:
					case kKnifeSwitch:
					case kInvisSwitch:
					case kTrigger:
					case kLgTrigger:
					if (thisRoom->objects[h].data.e.where != -1)
					{
						ExtractFloorSuite(thisRoom->objects[h].data.e.where, &floor, &suite);
						floor += kNumUndergroundFloors;
						thisRoom->objects[h].data.e.where = MergeFloorSuite(floor, suite);
					}
					break;
				}
			}
			CopyThisRoomToRoom();
		}
	}
	
	(*thisHouse)->version = kHouseVersion;
	
	InitCursor();
	CloseMessageWindow();
	ForceThisRoom(wasRoom);
}

//--------------------------------------------------------------  ShiftWholeHouse

void ShiftWholeHouse (short howFar)
{
	short		wasRoom;
	short		i, h, numRooms;
	char		wasState;
	
	OpenMessageWindow(PSTR("Shifting Whole House�"));
	SpinCursor(3);
	
	CopyThisRoomToRoom();
	wasRoom = thisRoomNumber;
	numRooms = (*thisHouse)->nRooms;
	
	for (i = 0; i < numRooms; i++)
	{
		if ((*thisHouse)->rooms[i].suite != kRoomIsEmpty)
		{
			SpinCursor(1);
			
			ForceThisRoom(i);
			for (h = 0; h < kMaxRoomObs; h++)
			{
			}
			CopyThisRoomToRoom();
		}
	}
	
	ForceThisRoom(wasRoom);
	
	InitCursor();
	CloseMessageWindow();
}

