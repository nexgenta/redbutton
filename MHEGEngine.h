/*
 * MHEGEngine.h
 */

#ifndef __MHEGENGINE_H__
#define __MHEGENGINE_H__

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#include "ISO13522-MHEG-5.h"
#include "MHEGDisplay.h"
#include "MHEGVideoOutput.h"
#include "MHEGBackend.h"
#include "MHEGApp.h"
#include "der_decode.h"
#include "listof.h"

/* default time to poll for missing content before generating a ContentRefError (seconds) */
#define MISSING_CONTENT_TIMEOUT		10

/* where to start searching for unused object numbers for clones */
#define FIRST_CLONED_OBJ_NUM		(1<<16)

/* engine ID strings, for GetEngineSupport "UKEngineProfile" feature and "WhoAmI" ResidentProgram */
#define MHEG_RECEIVER_ID	"GNULnx001"	/* receiver */
#define MHEG_ENGINE_ID		"MHGRBn001"	/* MHEG engine */
#define MHEG_DSMCC_ID		"DSMRBn001"	/* DSM-CC client */

/* EngineEvent EventTag values */
/* 0 and 1 are reserved */
#define EngineEvent_GroupIDRefError	2
#define EngineEvent_ContentRefError	3
#define EngineEvent_TextKeyFunction	4
/* 5 is reserved */
#define EngineEvent_VideoPrefChanged	6
#define EngineEvent_PauseResume		7
/* 8 is reserved */
#define EngineEvent_NetworkBootInfo	9
/* 10-15 are reserved */
#define EngineEvent_CancelKeyFunction	16
/* 17-99 are reserved */
#define EngineEvent_RedKeyFunction	100
#define EngineEvent_GreenKeyFunction	101
#define EngineEvent_YellowKeyFunction	102
#define EngineEvent_BlueKeyFunction	103
/* all other values are reserved */

/* EventTag key numbers for UserInput events */
#define MHEGKey_Up	1
#define MHEGKey_Down	2
#define MHEGKey_Left	3
#define MHEGKey_Right	4
#define MHEGKey_0	5
#define MHEGKey_1	6
#define MHEGKey_2	7
#define MHEGKey_3	8
#define MHEGKey_4	9
#define MHEGKey_5	10
#define MHEGKey_6	11
#define MHEGKey_7	12
#define MHEGKey_8	13
#define MHEGKey_9	14
#define MHEGKey_Select	15
#define MHEGKey_Cancel	16
#define MHEGKey_Red	100
#define MHEGKey_Green	101
#define MHEGKey_Yellow	102
#define MHEGKey_Blue	103
#define MHEGKey_Text	104

/* ContentHook values */
#define ContentHook_Bitmap_MPEG		2
#define ContentHook_Bitmap_PNG		4
#define ContentHook_Text		10
#define ContentHook_Stream_MPEG		10
#define ContentHook_Stream_File		11

/* cmd line options */
typedef struct
{
	bool remote;		/* or local rb-download backend */
	char *srg_loc;		/* service gateway location: directory for local; host[:port] for remote */
	int verbose;		/* -v flag */
	unsigned int timeout;	/* seconds to poll for missing content before generating a ContentRefError */
	bool fullscreen;	/* scale to fullscreen? */
	char *vo_method;	/* MHEGVideoOutputMethod name (NULL for default) */
	bool av_disabled;	/* true => audio and video output totally disabled */
	char *keymap;		/* keymap config file to use (NULL for default) */
} MHEGEngineOptions;

/* a list of files we are waiting for, and the objects that want them */
typedef struct
{
	RootClass *obj;
	OctetString file;
	time_t requested;	/* when we first asked for the file (used to timeout requests) */
} MissingContent;

DEFINE_LIST_OF(MissingContent);

/* takes a copy of the OctetString */
LIST_TYPE(MissingContent) *new_MissingContentListItem(RootClass *, OctetString *);
void free_MissingContentListItem(LIST_TYPE(MissingContent) *);

/* persistent storage */
typedef struct
{
	OctetString filename;
	LIST_OF(OriginalValue) *data;
} PersistentData;

DEFINE_LIST_OF(PersistentData);

LIST_TYPE(PersistentData) *new_PersistentDataListItem(OctetString *);
void free_PersistentDataListItem(LIST_TYPE(PersistentData) *);

/*
 * asynchronous events list
 * these are events generated by objects while processing an action
 * async events are processed when the action has finished
 * rather than at the time they are generated (like synchronous events)
 */
typedef struct
{
	ExternalReference src;
	EventType type;
	EventData *data;
} MHEGAsyncEvent;

DEFINE_LIST_OF(MHEGAsyncEvent);

LIST_TYPE(MHEGAsyncEvent) *new_MHEGAsyncEventListItem(ExternalReference *, EventType, EventData *);
void free_MHEGAsyncEventListItem(LIST_TYPE(MHEGAsyncEvent) *);

/* a list of actions that need performing */
typedef struct
{
	OctetString *group_id;		/* group identifier of the object that caused this action */
	ElementaryAction *action;	/* the action */
} MHEGAction;

DEFINE_LIST_OF(MHEGAction);

LIST_TYPE(MHEGAction) *new_MHEGActionListItem(OctetString *, ElementaryAction *);
void free_MHEGActionListItem(LIST_TYPE(MHEGAction) *);

/* a list of active links */
typedef LinkClass *LinkClassPtr;

DEFINE_LIST_OF(LinkClassPtr);

/* reasons for stopping the current app */
typedef enum
{
	QuitReason_DontQuit,	/* don't need to quit yet */
	QuitReason_GUIQuit,	/* GUI wants us to exit the programme */
	QuitReason_Quit,	/* Quit called in the current app */
	QuitReason_Launch,	/* Launch called, new app is in engine.quit_data */
	QuitReason_Spawn,	/* Spawn called, new app is in engine.quit_data */
	QuitReason_Retune	/* SI_TuneIndex programme called, channel URL is in engine.quit_data */
} QuitReason;

/* global engine state */
typedef struct
{
	int verbose;					/* -v cmd line flag */
	unsigned int timeout;				/* how long to poll for missing content before generating an error */
	MHEGDisplay display;				/* make porting easier */
	MHEGVideoOutputMethod *vo_method;		/* video output method (resolved from name given in MHEGEngineOptions) */
	bool av_disabled;				/* true => video and audio output totally disabled */
	MHEGBackend backend;				/* local or remote access to DSMCC carousel and MPEG streams */
	MHEGApp active_app;				/* application we are currently running */
	QuitReason quit_reason;				/* do we need to stop the current app */
	OctetString quit_data;				/* new app to Launch or Spawn, or channel to Retune to */
	OctetString *der_object;			/* DER object we are currently decoding */
	LIST_OF(RootClassPtr) *objects;			/* all currently loaded MHEG objects */
	LIST_OF(MissingContent) *missing_content;	/* files we are waiting for */
	LIST_OF(LinkClassPtr) *active_links;		/* currently active LinkClass objects */
	LIST_OF(MHEGAsyncEvent) *async_eventq;		/* asynchronous events that need processing */
	LIST_OF(MHEGAction) *main_actionq;		/* UK MHEG Profile event processing method */
	LIST_OF(MHEGAction) *temp_actionq;		/* UK MHEG Profile event processing method */
	LIST_OF(PersistentData) *persistent;		/* persistent files */
} MHEGEngine;

/* prototypes */
void MHEGEngine_init(MHEGEngineOptions *);
int MHEGEngine_run(void);
void MHEGEngine_fini(void);

MHEGDisplay *MHEGEngine_getDisplay(void);
MHEGVideoOutputMethod *MHEGEngine_getVideoOutputMethod(void);
bool MHEGEngine_avDisabled(void);

void MHEGEngine_TransitionTo(TransitionTo *, OctetString *);

void MHEGEngine_quit(QuitReason, OctetString *);

ApplicationClass *MHEGEngine_getActiveApplication(void);
SceneClass *MHEGEngine_getActiveScene(void);

void MHEGEngine_addVisibleObject(RootClass *);
void MHEGEngine_removeVisibleObject(RootClass *);
void MHEGEngine_bringToFront(RootClass *);
void MHEGEngine_sendToBack(RootClass *);
void MHEGEngine_putBefore(RootClass *, RootClass *);
void MHEGEngine_putBehind(RootClass *, RootClass *);
void MHEGEngine_redrawArea(XYPosition *, OriginalBoxSize *);

void MHEGEngine_addActiveLink(LinkClass *);
void MHEGEngine_removeActiveLink(LinkClass *);

void MHEGEngine_keyPressed(unsigned int);

PersistentData *MHEGEngine_findPersistentData(OctetString *, bool);

void MHEGEngine_generateEvent(ExternalReference *, EventType, EventData *);
void MHEGEngine_generateAsyncEvent(ExternalReference *, EventType, EventData *);

void MHEGEngine_processMHEGEvents(void);
void MHEGEngine_processNextAsyncEvent(void);
void MHEGEngine_addToTempActionQ(ActionClass *, OctetString *);

void MHEGEngine_setDERObject(OctetString *);
void MHEGEngine_resolveDERObjectReference(ObjectReference *, ExternalReference *);

void MHEGEngine_addObjectReference(RootClass *);
void MHEGEngine_removeObjectReference(RootClass *);
RootClass *MHEGEngine_findObjectReference(ObjectReference *, OctetString *);

RootClass *MHEGEngine_findGroupObject(OctetString *);
unsigned int MHEGEngine_getUnusedObjectNumber(RootClass *);

void MHEGEngine_addMissingContent(RootClass *, OctetString *);
void MHEGEngine_removeMissingContent(RootClass *);
void MHEGEngine_pollMissingContent(void);

bool MHEGEngine_checkContentRef(ContentReference *);
bool MHEGEngine_loadFile(OctetString *, OctetString *);
FILE *MHEGEngine_openFile(OctetString *);
MHEGStream *MHEGEngine_openStream(int, bool, int *, int *, bool, int *, int *);
void MHEGEngine_closeStream(MHEGStream *);
void MHEGEngine_retune(OctetString *);

char *MHEGEngine_absoluteFilename(OctetString *);

/* convert PNG to internal format */
MHEGBitmap *MHEGEngine_newBitmap(OctetString *, bool, int);
void MHEGEngine_freeBitmap(MHEGBitmap *);

void verbose(char *, ...);

#endif	/* __MHEGENGINE_H__ */

