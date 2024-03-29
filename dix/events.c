/************************************************************

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

********************************************************/

/* The panoramix components contained the following notice */
/*****************************************************************

Copyright (c) 1991, 1997 Digital Equipment Corporation, Maynard, Massachusetts.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
DIGITAL EQUIPMENT CORPORATION BE LIABLE FOR ANY CLAIM, DAMAGES, INCLUDING,
BUT NOT LIMITED TO CONSEQUENTIAL OR INCIDENTAL DAMAGES, OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Digital Equipment Corporation
shall not be used in advertising or otherwise to promote the sale, use or other
dealings in this Software without prior written authorization from Digital
Equipment Corporation.

******************************************************************/

/*****************************************************************

Copyright 2003-2005 Sun Microsystems, Inc.

All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, and/or sell copies of the Software, and to permit persons
to whom the Software is furnished to do so, provided that the above
copyright notice(s) and this permission notice appear in all copies of
the Software and that both the above copyright notice(s) and this
permission notice appear in supporting documentation.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

Except as contained in this notice, the name of a copyright holder
shall not be used in advertising or otherwise to promote the sale, use
or other dealings in this Software without prior written authorization
of the copyright holder.

******************************************************************/

/** @file events.c
 * This file handles event delivery and a big part of the server-side protocol
 * handling (the parts for input devices).
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <X11/X.h>
#include "misc.h"
#include "resource.h"
#include <X11/Xproto.h>
#include "windowstr.h"
#include "inputstr.h"
#include "scrnintstr.h"
#include "cursorstr.h"

#include "dixstruct.h"
#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif
#include "globals.h"

#include <X11/extensions/XKBproto.h>
#include "xkbsrv.h"
#include "xace.h"

#ifdef XSERVER_DTRACE
#include <sys/types.h>
typedef const char *string;
#include "Xserver-dtrace.h"
#endif

#include <X11/extensions/XIproto.h>
#include <X11/extensions/XI2proto.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XI2.h>
#include "exglobals.h"
#include "exevents.h"
#include "exglobals.h"
#include "extnsionst.h"

#include "dixevents.h"
#include "dixgrabs.h"
#include "dispatch.h"

#include <X11/extensions/ge.h>
#include "geext.h"
#include "geint.h"

#include "eventstr.h"
#include "enterleave.h"
#include "eventconvert.h"

/* Extension events type numbering starts at EXTENSION_EVENT_BASE.  */
#define NoSuchEvent 0x80000000	/* so doesn't match NoEventMask */
#define StructureAndSubMask ( StructureNotifyMask | SubstructureNotifyMask )
#define AllButtonsMask ( \
	Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask )
#define MotionMask ( \
	PointerMotionMask | Button1MotionMask | \
	Button2MotionMask | Button3MotionMask | Button4MotionMask | \
	Button5MotionMask | ButtonMotionMask )
#define PropagateMask ( \
	KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | \
	MotionMask )
#define PointerGrabMask ( \
	ButtonPressMask | ButtonReleaseMask | \
	EnterWindowMask | LeaveWindowMask | \
	PointerMotionHintMask | KeymapStateMask | \
	MotionMask )
#define AllModifiersMask ( \
	ShiftMask | LockMask | ControlMask | Mod1Mask | Mod2Mask | \
	Mod3Mask | Mod4Mask | Mod5Mask )
#define LastEventMask OwnerGrabButtonMask
#define AllEventMasks (LastEventMask|(LastEventMask-1))


#define CORE_EVENT(event) \
    (!((event)->u.u.type & EXTENSION_EVENT_BASE) && \
      (event)->u.u.type != GenericEvent)
#define XI2_EVENT(event) \
    (((event)->u.u.type == GenericEvent) && \
    ((xGenericEvent*)(event))->extension == IReqCode)

/**
 * Used to indicate a implicit passive grab created by a ButtonPress event.
 * See DeliverEventsToWindow().
 */
#define ImplicitGrabMask (1 << 7)

#define WID(w) ((w) ? ((w)->drawable.id) : 0)

#define XE_KBPTR (xE->u.keyButtonPointer)


#define rClient(obj) (clients[CLIENT_ID((obj)->resource)])

CallbackListPtr EventCallback;
CallbackListPtr DeviceEventCallback;

#define DNPMCOUNT 8

Mask DontPropagateMasks[DNPMCOUNT];
static int DontPropagateRefCnts[DNPMCOUNT];

static void CheckVirtualMotion( DeviceIntPtr pDev, QdEventPtr qe, WindowPtr pWin);
static void CheckPhysLimits(DeviceIntPtr pDev,
                            CursorPtr cursor,
                            Bool generateEvents,
                            Bool confineToScreen,
                            ScreenPtr pScreen);
static Bool CheckPassiveGrabsOnWindow(WindowPtr pWin,
                                      DeviceIntPtr device,
                                      DeviceEvent *event,
                                      BOOL checkCore);

/** Key repeat hack. Do not use but in TryClientEvents */
extern BOOL EventIsKeyRepeat(xEvent *event);

/**
 * Main input device struct.
 *     inputInfo.pointer
 *     is the core pointer. Referred to as "virtual core pointer", "VCP",
 *     "core pointer" or inputInfo.pointer. The VCP is the first master
 *     pointer device and cannot be deleted.
 *
 *     inputInfo.keyboard
 *     is the core keyboard ("virtual core keyboard", "VCK", "core keyboard").
 *     See inputInfo.pointer.
 *
 *     inputInfo.devices
 *     linked list containing all devices including VCP and VCK.
 *
 *     inputInfo.off_devices
 *     Devices that have not been initialized and are thus turned off.
 *
 *     inputInfo.numDevices
 *     Total number of devices.
 *
 *     inputInfo.all_devices
 *     Virtual device used for XIAllDevices passive grabs. This device is
 *     not part of the inputInfo.devices list and mostly unset except for
 *     the deviceid. It exists because passivegrabs need a valid device
 *     reference.
 *
 *     inputInfo.all_master_devices
 *     Virtual device used for XIAllMasterDevices passive grabs. This device
 *     is not part of the inputInfo.devices list and mostly unset except for
 *     the deviceid. It exists because passivegrabs need a valid device
 *     reference.
 */
InputInfo inputInfo;

/**
 * syncEvents is the global structure for queued events.
 *
 * Devices can be frozen through GrabModeSync pointer grabs. If this is the
 * case, events from these devices are added to "pending" instead of being
 * processed normally. When the device is unfrozen, events in "pending" are
 * replayed and processed as if they would come from the device directly.
 */
static struct {
    QdEventPtr		pending, /**<  list of queued events */
                        *pendtail; /**< last event in list */
    /** The device to replay events for. Only set in AllowEvents(), in which
     * case it is set to the device specified in the request. */
    DeviceIntPtr	replayDev;	/* kludgy rock to put flag for */

    /**
     * The window the events are supposed to be replayed on.
     * This window may be set to the grab's window (but only when
     * Replay{Pointer|Keyboard} is given in the XAllowEvents()
     * request. */
    WindowPtr		replayWin;	/*   ComputeFreezes            */
    /**
     * Flag to indicate whether we're in the process of
     * replaying events. Only set in ComputeFreezes(). */
    Bool		playingEvents;
    TimeStamp		time;
} syncEvents;

/**
 * The root window the given device is currently on.
 */
#define RootWindow(dev) dev->spriteInfo->sprite->spriteTrace[0]

static xEvent* swapEvent = NULL;
static int swapEventLen = 0;

void
NotImplemented(xEvent *from, xEvent *to)
{
    FatalError("Not implemented");
}

/**
 * Convert the given event type from an XI event to a core event.
 * @param[in] The XI 1.x event type.
 * @return The matching core event type or 0 if there is none.
 */
int
XItoCoreType(int xitype)
{
    int coretype = 0;
    if (xitype == DeviceMotionNotify)
        coretype = MotionNotify;
    else if (xitype == DeviceButtonPress)
        coretype = ButtonPress;
    else if (xitype == DeviceButtonRelease)
        coretype = ButtonRelease;
    else if (xitype == DeviceKeyPress)
        coretype = KeyPress;
    else if (xitype == DeviceKeyRelease)
        coretype = KeyRelease;

    return coretype;
}

/**
 * @return true if the device owns a cursor, false if device shares a cursor
 * sprite with another device.
 */
Bool
DevHasCursor(DeviceIntPtr pDev)
{
    return pDev->spriteInfo->spriteOwner;
}

/*
 * @return true if a device is a pointer, check is the same as used by XI to
 * fill the 'use' field.
 */
Bool
IsPointerDevice(DeviceIntPtr dev)
{
    return (dev->type == MASTER_POINTER) ||
            (dev->valuator && dev->button) ||
            (dev->valuator && !dev->key);
}

/*
 * @return true if a device is a keyboard, check is the same as used by XI to
 * fill the 'use' field.
 *
 * Some pointer devices have keys as well (e.g. multimedia keys). Try to not
 * count them as keyboard devices.
 */
Bool
IsKeyboardDevice(DeviceIntPtr dev)
{
    return (dev->type == MASTER_KEYBOARD) ||
            ((dev->key && dev->kbdfeed) && !IsPointerDevice(dev));
}

Bool
IsMaster(DeviceIntPtr dev)
{
    return (dev->type == MASTER_POINTER || dev->type == MASTER_KEYBOARD);
}

static WindowPtr XYToWindow(
    DeviceIntPtr pDev,
    int x,
    int y
);

/**
 * Max event opcode.
 */
extern int lastEvent;

extern int DeviceMotionNotify;

#define CantBeFiltered NoEventMask
/**
 * Event masks for each event type.
 *
 * One set of filters for each device, but only the first layer
 * is initialized. The rest is memcpy'd in InitEvents.
 *
 * Filters are used whether a given event may be delivered to a client,
 * usually in the form of if (window-event-mask & filter); then deliver event.
 *
 * One notable filter is for PointerMotion/DevicePointerMotion events. Each
 * time a button is pressed, the filter is modified to also contain the
 * matching ButtonXMotion mask.
 */
static Mask filters[MAXDEVICES][128] = {
{
	NoSuchEvent,		       /* 0 */
	NoSuchEvent,		       /* 1 */
	KeyPressMask,		       /* KeyPress */
	KeyReleaseMask,		       /* KeyRelease */
	ButtonPressMask,	       /* ButtonPress */
	ButtonReleaseMask,	       /* ButtonRelease */
	PointerMotionMask,	       /* MotionNotify (initial state) */
	EnterWindowMask,	       /* EnterNotify */
	LeaveWindowMask,	       /* LeaveNotify */
	FocusChangeMask,	       /* FocusIn */
	FocusChangeMask,	       /* FocusOut */
	KeymapStateMask,	       /* KeymapNotify */
	ExposureMask,		       /* Expose */
	CantBeFiltered,		       /* GraphicsExpose */
	CantBeFiltered,		       /* NoExpose */
	VisibilityChangeMask,	       /* VisibilityNotify */
	SubstructureNotifyMask,	       /* CreateNotify */
	StructureAndSubMask,	       /* DestroyNotify */
	StructureAndSubMask,	       /* UnmapNotify */
	StructureAndSubMask,	       /* MapNotify */
	SubstructureRedirectMask,      /* MapRequest */
	StructureAndSubMask,	       /* ReparentNotify */
	StructureAndSubMask,	       /* ConfigureNotify */
	SubstructureRedirectMask,      /* ConfigureRequest */
	StructureAndSubMask,	       /* GravityNotify */
	ResizeRedirectMask,	       /* ResizeRequest */
	StructureAndSubMask,	       /* CirculateNotify */
	SubstructureRedirectMask,      /* CirculateRequest */
	PropertyChangeMask,	       /* PropertyNotify */
	CantBeFiltered,		       /* SelectionClear */
	CantBeFiltered,		       /* SelectionRequest */
	CantBeFiltered,		       /* SelectionNotify */
	ColormapChangeMask,	       /* ColormapNotify */
	CantBeFiltered,		       /* ClientMessage */
	CantBeFiltered		       /* MappingNotify */
}};

/**
 * For the given event, return the matching event filter. This filter may then
 * be AND'ed with the selected event mask.
 *
 * For XI2 events, the returned filter is simply the byte containing the event
 * mask we're interested in. E.g. for a mask of (1 << 13), this would be
 * byte[1].
 *
 * @param[in] dev The device the event belongs to, may be NULL.
 * @param[in] event The event to get the filter for. Only the type of the
 *                  event matters, or the extension + evtype for GenericEvents.
 * @return The filter mask for the given event.
 *
 * @see GetEventMask
 */
Mask
GetEventFilter(DeviceIntPtr dev, xEvent *event)
{
    if (event->u.u.type != GenericEvent)
        return filters[dev ? dev->id : 0][event->u.u.type];
    else if (XI2_EVENT(event))
        return (1 << (((xXIDeviceEvent*)event)->evtype % 8));
    ErrorF("[dix] Unknown device type %d. No filter\n", event->u.u.type);
    return 0;
}

/**
 * Return the windows complete XI2 mask for the given XI2 event type.
 */
Mask
GetWindowXI2Mask(DeviceIntPtr dev, WindowPtr win, xEvent* ev)
{
    OtherInputMasks *inputMasks = wOtherInputMasks(win);
    int filter;
    int evtype;

    if (!inputMasks || !XI2_EVENT(ev))
        return 0;

    evtype = ((xGenericEvent*)ev)->evtype;
    filter = GetEventFilter(dev, ev);

    return ((inputMasks->xi2mask[dev->id][evtype/8] & filter) ||
            inputMasks->xi2mask[XIAllDevices][evtype/8] ||
            (inputMasks->xi2mask[XIAllMasterDevices][evtype/8] && IsMaster(dev)));
}

static Mask
GetEventMask(DeviceIntPtr dev, xEvent *event, InputClients* other)
{
    /* XI2 filters are only ever 8 bit, so let's return a 8 bit mask */
    if (XI2_EVENT(event))
    {
        int byte = ((xGenericEvent*)event)->evtype / 8;
        return (other->xi2mask[dev->id][byte] |
                other->xi2mask[XIAllDevices][byte] |
                (IsMaster(dev)? other->xi2mask[XIAllMasterDevices][byte] : 0));
    } else if (CORE_EVENT(event))
        return other->mask[XIAllDevices];
    else
        return other->mask[dev->id];
}


static CARD8 criticalEvents[32] =
{
    0x7c, 0x30, 0x40			/* key, button, expose, and configure events */
};

static void
SyntheticMotion(DeviceIntPtr dev, int x, int y) {
    int screenno = 0;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
        screenno = dev->spriteInfo->sprite->screen->myNum;
#endif
    PostSyntheticMotion(dev, x, y, screenno,
            (syncEvents.playingEvents) ?  syncEvents.time.milliseconds : currentTime.milliseconds);

}

#ifdef PANORAMIX
static void PostNewCursor(DeviceIntPtr pDev);

static Bool
XineramaSetCursorPosition(
    DeviceIntPtr pDev,
    int x,
    int y,
    Bool generateEvent
){
    ScreenPtr pScreen;
    BoxRec box;
    int i;
    SpritePtr pSprite = pDev->spriteInfo->sprite;

    /* x,y are in Screen 0 coordinates.  We need to decide what Screen
       to send the message too and what the coordinates relative to
       that screen are. */

    pScreen = pSprite->screen;
    x += panoramiXdataPtr[0].x;
    y += panoramiXdataPtr[0].y;

    if(!POINT_IN_REGION(pScreen, &XineramaScreenRegions[pScreen->myNum],
								x, y, &box))
    {
	FOR_NSCREENS(i)
	{
	    if(i == pScreen->myNum)
		continue;
	    if(POINT_IN_REGION(pScreen, &XineramaScreenRegions[i], x, y, &box))
	    {
		pScreen = screenInfo.screens[i];
		break;
	    }
	}
    }

    pSprite->screen = pScreen;
    pSprite->hotPhys.x = x - panoramiXdataPtr[0].x;
    pSprite->hotPhys.y = y - panoramiXdataPtr[0].y;
    x -= panoramiXdataPtr[pScreen->myNum].x;
    y -= panoramiXdataPtr[pScreen->myNum].y;

    return (*pScreen->SetCursorPosition)(pDev, pScreen, x, y, generateEvent);
}


static void
XineramaConstrainCursor(DeviceIntPtr pDev)
{
    SpritePtr pSprite = pDev->spriteInfo->sprite;
    ScreenPtr pScreen;
    BoxRec newBox;

    pScreen = pSprite->screen;
    newBox = pSprite->physLimits;

    /* Translate the constraining box to the screen
       the sprite is actually on */
    newBox.x1 += panoramiXdataPtr[0].x - panoramiXdataPtr[pScreen->myNum].x;
    newBox.x2 += panoramiXdataPtr[0].x - panoramiXdataPtr[pScreen->myNum].x;
    newBox.y1 += panoramiXdataPtr[0].y - panoramiXdataPtr[pScreen->myNum].y;
    newBox.y2 += panoramiXdataPtr[0].y - panoramiXdataPtr[pScreen->myNum].y;

    (* pScreen->ConstrainCursor)(pDev, pScreen, &newBox);
}


static Bool
XineramaSetWindowPntrs(DeviceIntPtr pDev, WindowPtr pWin)
{
    SpritePtr pSprite = pDev->spriteInfo->sprite;

    if(pWin == WindowTable[0]) {
	    memcpy(pSprite->windows, WindowTable,
				PanoramiXNumScreens*sizeof(WindowPtr));
    } else {
	PanoramiXRes *win;
	int rc, i;

	rc = dixLookupResourceByType((pointer *)&win, pWin->drawable.id,
				     XRT_WINDOW, serverClient, DixReadAccess);
	if (rc != Success)
	    return FALSE;

	for(i = 0; i < PanoramiXNumScreens; i++) {
	    rc = dixLookupWindow(pSprite->windows + i, win->info[i].id,
				 serverClient, DixReadAccess);
	    if (rc != Success)  /* window is being unmapped */
		return FALSE;
	}
    }
    return TRUE;
}

static void
XineramaConfineCursorToWindow(DeviceIntPtr pDev,
                              WindowPtr pWin,
                              Bool generateEvents)
{
    SpritePtr pSprite = pDev->spriteInfo->sprite;

    int x, y, off_x, off_y, i;

    if(!XineramaSetWindowPntrs(pDev, pWin))
        return;

    i = PanoramiXNumScreens - 1;

    REGION_COPY(pSprite->screen, &pSprite->Reg1,
            &pSprite->windows[i]->borderSize);
    off_x = panoramiXdataPtr[i].x;
    off_y = panoramiXdataPtr[i].y;

    while(i--) {
        x = off_x - panoramiXdataPtr[i].x;
        y = off_y - panoramiXdataPtr[i].y;

        if(x || y)
            REGION_TRANSLATE(pSprite->screen, &pSprite->Reg1, x, y);

        REGION_UNION(pSprite->screen, &pSprite->Reg1, &pSprite->Reg1,
                &pSprite->windows[i]->borderSize);

        off_x = panoramiXdataPtr[i].x;
        off_y = panoramiXdataPtr[i].y;
    }

    pSprite->hotLimits = *REGION_EXTENTS(pSprite->screen, &pSprite->Reg1);

    if(REGION_NUM_RECTS(&pSprite->Reg1) > 1)
        pSprite->hotShape = &pSprite->Reg1;
    else
        pSprite->hotShape = NullRegion;

    pSprite->confined = FALSE;
    pSprite->confineWin = (pWin == WindowTable[0]) ? NullWindow : pWin;

    CheckPhysLimits(pDev, pSprite->current, generateEvents, FALSE, NULL);
}

#endif  /* PANORAMIX */

/**
 * Modifies the filter for the given protocol event type to the given masks.
 *
 * There's only two callers: UpdateDeviceState() and XI's SetMaskForExtEvent().
 * The latter initialises masks for the matching XI events, it's a once-off
 * setting.
 * UDS however changes the mask for MotionNotify and DeviceMotionNotify each
 * time a button is pressed to include the matching ButtonXMotion mask in the
 * filter.
 *
 * @param[in] deviceid The device to modify the filter for.
 * @param[in] mask The new filter mask.
 * @param[in] event Protocol event type.
 */
void
SetMaskForEvent(int deviceid, Mask mask, int event)
{
    if (deviceid < 0 || deviceid >= MAXDEVICES)
        FatalError("SetMaskForEvent: bogus device id");
    filters[deviceid][event] = mask;
}

void
SetCriticalEvent(int event)
{
    if (event >= 128)
	FatalError("SetCriticalEvent: bogus event number");
    criticalEvents[event >> 3] |= 1 << (event & 7);
}

void
ConfineToShape(DeviceIntPtr pDev, RegionPtr shape, int *px, int *py)
{
    BoxRec box;
    int x = *px, y = *py;
    int incx = 1, incy = 1;
    SpritePtr pSprite;

    pSprite = pDev->spriteInfo->sprite;
    if (POINT_IN_REGION(pSprite->hot.pScreen, shape, x, y, &box))
	return;
    box = *REGION_EXTENTS(pSprite->hot.pScreen, shape);
    /* this is rather crude */
    do {
	x += incx;
	if (x >= box.x2)
	{
	    incx = -1;
	    x = *px - 1;
	}
	else if (x < box.x1)
	{
	    incx = 1;
	    x = *px;
	    y += incy;
	    if (y >= box.y2)
	    {
		incy = -1;
		y = *py - 1;
	    }
	    else if (y < box.y1)
		return; /* should never get here! */
	}
    } while (!POINT_IN_REGION(pSprite->hot.pScreen, shape, x, y, &box));
    *px = x;
    *py = y;
}

static void
CheckPhysLimits(
    DeviceIntPtr pDev,
    CursorPtr cursor,
    Bool generateEvents,
    Bool confineToScreen, /* unused if PanoramiX on */
    ScreenPtr pScreen)    /* unused if PanoramiX on */
{
    HotSpot new;
    SpritePtr pSprite = pDev->spriteInfo->sprite;

    if (!cursor)
	return;
    new = pSprite->hotPhys;
#ifdef PANORAMIX
    if (!noPanoramiXExtension)
        /* I don't care what the DDX has to say about it */
        pSprite->physLimits = pSprite->hotLimits;
    else
#endif
    {
        if (pScreen)
            new.pScreen = pScreen;
        else
            pScreen = new.pScreen;
        (*pScreen->CursorLimits) (pDev, pScreen, cursor, &pSprite->hotLimits,
                &pSprite->physLimits);
        pSprite->confined = confineToScreen;
        (* pScreen->ConstrainCursor)(pDev, pScreen, &pSprite->physLimits);
    }

    /* constrain the pointer to those limits */
    if (new.x < pSprite->physLimits.x1)
	new.x = pSprite->physLimits.x1;
    else
	if (new.x >= pSprite->physLimits.x2)
	    new.x = pSprite->physLimits.x2 - 1;
    if (new.y < pSprite->physLimits.y1)
	new.y = pSprite->physLimits.y1;
    else
	if (new.y >= pSprite->physLimits.y2)
	    new.y = pSprite->physLimits.y2 - 1;
    if (pSprite->hotShape)
	ConfineToShape(pDev, pSprite->hotShape, &new.x, &new.y);
    if ((pScreen != pSprite->hotPhys.pScreen) ||
	(new.x != pSprite->hotPhys.x) || (new.y != pSprite->hotPhys.y))
    {
#ifdef PANORAMIX
        if (!noPanoramiXExtension)
            XineramaSetCursorPosition (pDev, new.x, new.y, generateEvents);
        else
#endif
        {
            if (pScreen != pSprite->hotPhys.pScreen)
                pSprite->hotPhys = new;
            (*pScreen->SetCursorPosition)
                (pDev, pScreen, new.x, new.y, generateEvents);
        }
        if (!generateEvents)
            SyntheticMotion(pDev, new.x, new.y);
    }

#ifdef PANORAMIX
    /* Tell DDX what the limits are */
    if (!noPanoramiXExtension)
        XineramaConstrainCursor(pDev);
#endif
}

static void
CheckVirtualMotion(
    DeviceIntPtr pDev,
    QdEventPtr qe,
    WindowPtr pWin)
{
    SpritePtr pSprite = pDev->spriteInfo->sprite;
    RegionPtr reg = NULL;
    DeviceEvent *ev = NULL;

    if (qe)
    {
        ev = &qe->event->device_event;
        switch(ev->type)
        {
            case ET_Motion:
            case ET_ButtonPress:
            case ET_ButtonRelease:
            case ET_KeyPress:
            case ET_KeyRelease:
            case ET_ProximityIn:
            case ET_ProximityOut:
                pSprite->hot.pScreen = qe->pScreen;
                pSprite->hot.x = ev->root_x;
                pSprite->hot.y = ev->root_y;
                pWin = pDev->deviceGrab.grab ? pDev->deviceGrab.grab->confineTo : NullWindow;
                break;
            default:
                break;
        }
    }
    if (pWin)
    {
	BoxRec lims;

#ifdef PANORAMIX
        if (!noPanoramiXExtension) {
            int x, y, off_x, off_y, i;

            if(!XineramaSetWindowPntrs(pDev, pWin))
                return;

            i = PanoramiXNumScreens - 1;

            REGION_COPY(pSprite->screen, &pSprite->Reg2,
                    &pSprite->windows[i]->borderSize);
            off_x = panoramiXdataPtr[i].x;
            off_y = panoramiXdataPtr[i].y;

            while(i--) {
                x = off_x - panoramiXdataPtr[i].x;
                y = off_y - panoramiXdataPtr[i].y;

                if(x || y)
                    REGION_TRANSLATE(pSprite->screen, &pSprite->Reg2, x, y);

                REGION_UNION(pSprite->screen, &pSprite->Reg2, &pSprite->Reg2,
                        &pSprite->windows[i]->borderSize);

                off_x = panoramiXdataPtr[i].x;
                off_y = panoramiXdataPtr[i].y;
            }
        } else
#endif
        {
            if (pSprite->hot.pScreen != pWin->drawable.pScreen)
            {
                pSprite->hot.pScreen = pWin->drawable.pScreen;
                pSprite->hot.x = pSprite->hot.y = 0;
            }
        }

	lims = *REGION_EXTENTS(pWin->drawable.pScreen, &pWin->borderSize);
	if (pSprite->hot.x < lims.x1)
	    pSprite->hot.x = lims.x1;
	else if (pSprite->hot.x >= lims.x2)
	    pSprite->hot.x = lims.x2 - 1;
	if (pSprite->hot.y < lims.y1)
	    pSprite->hot.y = lims.y1;
	else if (pSprite->hot.y >= lims.y2)
	    pSprite->hot.y = lims.y2 - 1;

#ifdef PANORAMIX
        if (!noPanoramiXExtension)
        {
            if (REGION_NUM_RECTS(&pSprite->Reg2) > 1)
                reg = &pSprite->Reg2;

        } else
#endif
        {
            if (wBoundingShape(pWin))
                reg = &pWin->borderSize;
        }

        if (reg)
            ConfineToShape(pDev, reg, &pSprite->hot.x, &pSprite->hot.y);

	if (qe && ev)
	{
	    qe->pScreen = pSprite->hot.pScreen;
	    ev->root_x = pSprite->hot.x;
	    ev->root_y = pSprite->hot.y;
	}
    }
#ifdef PANORAMIX
    if (noPanoramiXExtension) /* No typo. Only set the root win if disabled */
#endif
        RootWindow(pDev) = WindowTable[pSprite->hot.pScreen->myNum];
}

static void
ConfineCursorToWindow(DeviceIntPtr pDev, WindowPtr pWin, Bool generateEvents, Bool confineToScreen)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    SpritePtr pSprite = pDev->spriteInfo->sprite;

    if (syncEvents.playingEvents)
    {
	CheckVirtualMotion(pDev, (QdEventPtr)NULL, pWin);
	SyntheticMotion(pDev, pSprite->hot.x, pSprite->hot.y);
    }
    else
    {
#ifdef PANORAMIX
        if(!noPanoramiXExtension) {
            XineramaConfineCursorToWindow(pDev, pWin, generateEvents);
            return;
        }
#endif
	pSprite->hotLimits = *REGION_EXTENTS( pScreen, &pWin->borderSize);
	pSprite->hotShape = wBoundingShape(pWin) ? &pWin->borderSize
					       : NullRegion;
        CheckPhysLimits(pDev, pSprite->current, generateEvents,
                        confineToScreen, pScreen);
    }
}

Bool
PointerConfinedToScreen(DeviceIntPtr pDev)
{
    return pDev->spriteInfo->sprite->confined;
}

/**
 * Update the sprite cursor to the given cursor.
 *
 * ChangeToCursor() will display the new cursor and free the old cursor (if
 * applicable). If the provided cursor is already the updated cursor, nothing
 * happens.
 */
static void
ChangeToCursor(DeviceIntPtr pDev, CursorPtr cursor)
{
    SpritePtr pSprite = pDev->spriteInfo->sprite;
    ScreenPtr pScreen;

    if (cursor != pSprite->current)
    {
	if ((pSprite->current->bits->xhot != cursor->bits->xhot) ||
		(pSprite->current->bits->yhot != cursor->bits->yhot))
	    CheckPhysLimits(pDev, cursor, FALSE, pSprite->confined,
			    (ScreenPtr)NULL);
#ifdef PANORAMIX
        /* XXX: is this really necessary?? (whot) */
        if (!noPanoramiXExtension)
            pScreen = pSprite->screen;
        else
#endif
            pScreen = pSprite->hotPhys.pScreen;

        (*pScreen->DisplayCursor)(pDev, pScreen, cursor);
	FreeCursor(pSprite->current, (Cursor)0);
	pSprite->current = cursor;
	pSprite->current->refcnt++;
    }
}

/**
 * @returns true if b is a descendent of a
 */
Bool
IsParent(WindowPtr a, WindowPtr b)
{
    for (b = b->parent; b; b = b->parent)
	if (b == a) return TRUE;
    return FALSE;
}

/**
 * Update the cursor displayed on the screen.
 *
 * Called whenever a cursor may have changed shape or position.
 */
static void
PostNewCursor(DeviceIntPtr pDev)
{
    WindowPtr win;
    GrabPtr grab = pDev->deviceGrab.grab;
    SpritePtr   pSprite = pDev->spriteInfo->sprite;
    CursorPtr   pCursor;

    if (syncEvents.playingEvents)
	return;
    if (grab)
    {
	if (grab->cursor)
	{
	    ChangeToCursor(pDev, grab->cursor);
	    return;
	}
	if (IsParent(grab->window, pSprite->win))
	    win = pSprite->win;
	else
	    win = grab->window;
    }
    else
	win = pSprite->win;
    for (; win; win = win->parent)
    {
	if (win->optional)
        {
            pCursor = WindowGetDeviceCursor(win, pDev);
            if (!pCursor && win->optional->cursor != NullCursor)
                pCursor = win->optional->cursor;
            if (pCursor)
            {
                ChangeToCursor(pDev, pCursor);
                return;
            }
	}
    }
}


/**
 * @param dev device which you want to know its current root window
 * @return root window where dev's sprite is located
 */
WindowPtr
GetCurrentRootWindow(DeviceIntPtr dev)
{
    return RootWindow(dev);
}

/**
 * @return window underneath the cursor sprite.
 */
WindowPtr
GetSpriteWindow(DeviceIntPtr pDev)
{
    return pDev->spriteInfo->sprite->win;
}

/**
 * @return current sprite cursor.
 */
CursorPtr
GetSpriteCursor(DeviceIntPtr pDev)
{
    return pDev->spriteInfo->sprite->current;
}

/**
 * Set x/y current sprite position in screen coordinates.
 */
void
GetSpritePosition(DeviceIntPtr pDev, int *px, int *py)
{
    SpritePtr pSprite = pDev->spriteInfo->sprite;
    *px = pSprite->hotPhys.x;
    *py = pSprite->hotPhys.y;
}

#ifdef PANORAMIX
int
XineramaGetCursorScreen(DeviceIntPtr pDev)
{
    if(!noPanoramiXExtension) {
	return pDev->spriteInfo->sprite->screen->myNum;
    } else {
	return 0;
    }
}
#endif /* PANORAMIX */

#define TIMESLOP (5 * 60 * 1000) /* 5 minutes */

static void
MonthChangedOrBadTime(InternalEvent *ev)
{
    /* If the ddx/OS is careless about not processing timestamped events from
     * different sources in sorted order, then it's possible for time to go
     * backwards when it should not.  Here we ensure a decent time.
     */
    if ((currentTime.milliseconds - ev->any.time) > TIMESLOP)
	currentTime.months++;
    else
        ev->any.time = currentTime.milliseconds;
}

static void
NoticeTime(InternalEvent *ev)
{
    if (ev->any.time < currentTime.milliseconds)
        MonthChangedOrBadTime(ev);
    currentTime.milliseconds = ev->any.time;
    lastDeviceEventTime = currentTime;
}

void
NoticeEventTime(InternalEvent *ev)
{
    if (!syncEvents.playingEvents)
	NoticeTime(ev);
}

/**************************************************************************
 *            The following procedures deal with synchronous events       *
 **************************************************************************/

/**
 * EnqueueEvent is a device's processInputProc if a device is frozen.
 * Instead of delivering the events to the client, the event is tacked onto a
 * linked list for later delivery.
 */
void
EnqueueEvent(InternalEvent *ev, DeviceIntPtr device)
{
    QdEventPtr	tail = *syncEvents.pendtail;
    QdEventPtr	qe;
    SpritePtr	pSprite = device->spriteInfo->sprite;
    int		eventlen;
    DeviceEvent *event = &ev->device_event;

    NoticeTime((InternalEvent*)event);

    /* Fix for key repeating bug. */
    if (device->key != NULL && device->key->xkbInfo != NULL &&
        event->type == ET_KeyRelease)
	AccessXCancelRepeatKey(device->key->xkbInfo, event->detail.key);

#if 0
        /* FIXME: I'm broken now. Please fix me. */
    if (DeviceEventCallback)
    {
	DeviceEventInfoRec eventinfo;
	/*  The RECORD spec says that the root window field of motion events
	 *  must be valid.  At this point, it hasn't been filled in yet, so
	 *  we do it here.  The long expression below is necessary to get
	 *  the current root window; the apparently reasonable alternative
	 *  GetCurrentRootWindow()->drawable.id doesn't give you the right
	 *  answer on the first motion event after a screen change because
	 *  the data that GetCurrentRootWindow relies on hasn't been
	 *  updated yet.
	 */
	if (xE->u.u.type == DeviceMotionNotify)
	    XE_KBPTR.root =
		WindowTable[pSprite->hotPhys.pScreen->myNum]->drawable.id;
	eventinfo.events = xE;
	eventinfo.count = nevents;
	CallCallbacks(&DeviceEventCallback, (pointer)&eventinfo);
    }
#endif
    if (event->type == ET_Motion)
    {
#ifdef PANORAMIX
	if(!noPanoramiXExtension) {
            event->root_x += panoramiXdataPtr[pSprite->screen->myNum].x -
			      panoramiXdataPtr[0].x;
	    event->root_y += panoramiXdataPtr[pSprite->screen->myNum].y -
			      panoramiXdataPtr[0].y;
	}
#endif
	pSprite->hotPhys.x = event->root_x;
	pSprite->hotPhys.y = event->root_y;
	/* do motion compression, but not if from different devices */
	if (tail &&
	    (tail->event->any.type == ET_Motion) &&
            (tail->device == device) &&
	    (tail->pScreen == pSprite->hotPhys.pScreen))
	{
            DeviceEvent *tailev = &tail->event->device_event;
	    tailev->root_x = pSprite->hotPhys.x;
	    tailev->root_y = pSprite->hotPhys.y;
	    tailev->time = event->time;
	    tail->months = currentTime.months;
	    return;
	}
    }

    eventlen = event->length;

    qe = xalloc(sizeof(QdEventRec) + eventlen);
    if (!qe)
	return;
    qe->next = (QdEventPtr)NULL;
    qe->device = device;
    qe->pScreen = pSprite->hotPhys.pScreen;
    qe->months = currentTime.months;
    qe->event = (InternalEvent *)(qe + 1);
    memcpy(qe->event, event, eventlen);
    if (tail)
	syncEvents.pendtail = &tail->next;
    *syncEvents.pendtail = qe;
}

/**
 * Run through the list of events queued up in syncEvents.
 * For each event do:
 * If the device for this event is not frozen anymore, take it and process it
 * as usually.
 * After that, check if there's any devices in the list that are not frozen.
 * If there is none, we're done. If there is at least one device that is not
 * frozen, then re-run from the beginning of the event queue.
 */
static void
PlayReleasedEvents(void)
{
    QdEventPtr *prev, qe;
    DeviceIntPtr dev;
    DeviceIntPtr pDev;

    prev = &syncEvents.pending;
    while ( (qe = *prev) )
    {
	if (!qe->device->deviceGrab.sync.frozen)
	{
	    *prev = qe->next;
            pDev = qe->device;
	    if (*syncEvents.pendtail == *prev)
		syncEvents.pendtail = prev;
	    if (qe->event->any.type == ET_Motion)
		CheckVirtualMotion(pDev, qe, NullWindow);
	    syncEvents.time.months = qe->months;
            syncEvents.time.milliseconds = qe->event->any.time;
#ifdef PANORAMIX
	   /* Translate back to the sprite screen since processInputProc
	      will translate from sprite screen to screen 0 upon reentry
	      to the DIX layer */
	    if(!noPanoramiXExtension) {
                DeviceEvent *ev = &qe->event->device_event;
                switch(ev->type)
                {
                    case ET_Motion:
                    case ET_ButtonPress:
                    case ET_ButtonRelease:
                    case ET_KeyPress:
                    case ET_KeyRelease:
                    case ET_ProximityIn:
                    case ET_ProximityOut:
                        ev->root_x += panoramiXdataPtr[0].x -
                            panoramiXdataPtr[pDev->spriteInfo->sprite->screen->myNum].x;
                        ev->root_y += panoramiXdataPtr[0].y -
                            panoramiXdataPtr[pDev->spriteInfo->sprite->screen->myNum].y;
                        break;
                    default:
                        break;
                }

	    }
#endif
	    (*qe->device->public.processInputProc)(qe->event, qe->device);
	    xfree(qe);
	    for (dev = inputInfo.devices; dev && dev->deviceGrab.sync.frozen; dev = dev->next)
		;
	    if (!dev)
		break;
	    /* Playing the event may have unfrozen another device. */
	    /* So to play it safe, restart at the head of the queue */
	    prev = &syncEvents.pending;
	}
	else
	    prev = &qe->next;
    }
}

/**
 * Freeze or thaw the given devices. The device's processing proc is
 * switched to either the real processing proc (in case of thawing) or an
 * enqueuing processing proc (usually EnqueueEvent()).
 *
 * @param dev The device to freeze/thaw
 * @param frozen True to freeze or false to thaw.
 */
static void
FreezeThaw(DeviceIntPtr dev, Bool frozen)
{
    dev->deviceGrab.sync.frozen = frozen;
    if (frozen)
	dev->public.processInputProc = dev->public.enqueueInputProc;
    else
	dev->public.processInputProc = dev->public.realInputProc;
}

/**
 * Unfreeze devices and replay all events to the respective clients.
 *
 * ComputeFreezes takes the first event in the device's frozen event queue. It
 * runs up the sprite tree (spriteTrace) and searches for the window to replay
 * the events from. If it is found, it checks for passive grabs one down from
 * the window or delivers the events.
 */
static void
ComputeFreezes(void)
{
    DeviceIntPtr replayDev = syncEvents.replayDev;
    int i;
    WindowPtr w;
    GrabPtr grab;
    DeviceIntPtr dev;

    for (dev = inputInfo.devices; dev; dev = dev->next)
	FreezeThaw(dev, dev->deviceGrab.sync.other ||
                (dev->deviceGrab.sync.state >= FROZEN));
    if (syncEvents.playingEvents || (!replayDev && !syncEvents.pending))
	return;
    syncEvents.playingEvents = TRUE;
    if (replayDev)
    {
        DeviceEvent* event = replayDev->deviceGrab.sync.event;

	syncEvents.replayDev = (DeviceIntPtr)NULL;

        w = XYToWindow(replayDev, event->root_x, event->root_y);
	for (i = 0; i < replayDev->spriteInfo->sprite->spriteTraceGood; i++)
	{
	    if (syncEvents.replayWin ==
		replayDev->spriteInfo->sprite->spriteTrace[i])
	    {
		if (!CheckDeviceGrabs(replayDev, event, i+1)) {
		    if (replayDev->focus && !IsPointerEvent((InternalEvent*)event))
			DeliverFocusedEvent(replayDev, (InternalEvent*)event, w);
		    else
			DeliverDeviceEvents(w, (InternalEvent*)event, NullGrab,
                                            NullWindow, replayDev);
		}
		goto playmore;
	    }
	}
	/* must not still be in the same stack */
	if (replayDev->focus && !IsPointerEvent((InternalEvent*)event))
	    DeliverFocusedEvent(replayDev, (InternalEvent*)event, w);
	else
	    DeliverDeviceEvents(w, (InternalEvent*)event, NullGrab,
                                NullWindow, replayDev);
    }
playmore:
    for (dev = inputInfo.devices; dev; dev = dev->next)
    {
	if (!dev->deviceGrab.sync.frozen)
	{
	    PlayReleasedEvents();
	    break;
	}
    }
    syncEvents.playingEvents = FALSE;
    for (dev = inputInfo.devices; dev; dev = dev->next)
    {
        if (DevHasCursor(dev))
        {
            /* the following may have been skipped during replay,
              so do it now */
            if ((grab = dev->deviceGrab.grab) && grab->confineTo)
            {
                if (grab->confineTo->drawable.pScreen !=
                        dev->spriteInfo->sprite->hotPhys.pScreen)
                    dev->spriteInfo->sprite->hotPhys.x =
                        dev->spriteInfo->sprite->hotPhys.y = 0;
                ConfineCursorToWindow(dev, grab->confineTo, TRUE, TRUE);
            }
            else
                ConfineCursorToWindow(dev,
                        WindowTable[dev->spriteInfo->sprite->hotPhys.pScreen->myNum],
                        TRUE, FALSE);
            PostNewCursor(dev);
        }
    }
}

#ifdef RANDR
void
ScreenRestructured (ScreenPtr pScreen)
{
    GrabPtr grab;
    DeviceIntPtr pDev;

    for (pDev = inputInfo.devices; pDev; pDev = pDev->next)
    {
        if (!DevHasCursor(pDev))
            continue;

        /* GrabDevice doesn't have a confineTo field, so we don't need to
         * worry about it. */
        if ((grab = pDev->deviceGrab.grab) && grab->confineTo)
        {
            if (grab->confineTo->drawable.pScreen
                    != pDev->spriteInfo->sprite->hotPhys.pScreen)
                pDev->spriteInfo->sprite->hotPhys.x = pDev->spriteInfo->sprite->hotPhys.y = 0;
            ConfineCursorToWindow(pDev, grab->confineTo, TRUE, TRUE);
        }
        else
            ConfineCursorToWindow(pDev,
                    WindowTable[pDev->spriteInfo->sprite->hotPhys.pScreen->myNum],
                    TRUE, FALSE);
    }
}
#endif

static void
CheckGrabForSyncs(DeviceIntPtr thisDev, Bool thisMode, Bool otherMode)
{
    GrabPtr grab = thisDev->deviceGrab.grab;
    DeviceIntPtr dev;

    if (thisMode == GrabModeSync)
	thisDev->deviceGrab.sync.state = FROZEN_NO_EVENT;
    else
    {	/* free both if same client owns both */
	thisDev->deviceGrab.sync.state = THAWED;
	if (thisDev->deviceGrab.sync.other &&
	    (CLIENT_BITS(thisDev->deviceGrab.sync.other->resource) ==
	     CLIENT_BITS(grab->resource)))
	    thisDev->deviceGrab.sync.other = NullGrab;
    }

    /*
        XXX: Direct slave grab won't freeze the paired master device.
        The correct thing to do would be to freeze all SDs attached to the
        paired master device.
     */
    if (IsMaster(thisDev))
    {
        dev = GetPairedDevice(thisDev);
        if (otherMode == GrabModeSync)
            dev->deviceGrab.sync.other = grab;
        else
        {	/* free both if same client owns both */
            if (dev->deviceGrab.sync.other &&
                    (CLIENT_BITS(dev->deviceGrab.sync.other->resource) ==
                     CLIENT_BITS(grab->resource)))
                dev->deviceGrab.sync.other = NullGrab;
        }
    }
    ComputeFreezes();
}

/* Only ever used if a grab is called on an attached slave device. */
static int GrabPrivateKeyIndex;
static DevPrivateKey GrabPrivateKey = &GrabPrivateKeyIndex;

/**
 * Save the device's master device in the devPrivates. This needs to be done
 * if a client directly grabs a slave device that is attached to a master. For
 * the duration of the grab, the device is detached, ungrabbing re-attaches it
 * though.
 *
 * We store the ID of the master device only in case the master disappears
 * while the device has a grab.
 */
static void
DetachFromMaster(DeviceIntPtr dev)
{
    int id;
    if (!dev->u.master)
        return;

    id = dev->u.master->id;

    dixSetPrivate(&dev->devPrivates, GrabPrivateKey, (void *)id);
    AttachDevice(NULL, dev, NULL);
}

static void
ReattachToOldMaster(DeviceIntPtr dev)
{
    int id;
    void *p;
    DeviceIntPtr master = NULL;

    if (IsMaster(dev))
        return;


    p = dixLookupPrivate(&dev->devPrivates, GrabPrivateKey);
    id = (int)p; /* silence gcc warnings */
    dixLookupDevice(&master, id, serverClient, DixUseAccess);

    if (master)
    {
        AttachDevice(serverClient, dev, master);
        dixSetPrivate(&dev->devPrivates, GrabPrivateKey, NULL);
    }
}

/**
 * Activate a pointer grab on the given device. A pointer grab will cause all
 * core pointer events of this device to be delivered to the grabbing client only.
 * No other device will send core events to the grab client while the grab is
 * on, but core events will be sent to other clients.
 * Can cause the cursor to change if a grab cursor is set.
 *
 * Note that parameter autoGrab may be (True & ImplicitGrabMask) if the grab
 * is an implicit grab caused by a ButtonPress event.
 *
 * @param mouse The device to grab.
 * @param grab The grab structure, needs to be setup.
 * @param autoGrab True if the grab was caused by a button down event and not
 * explicitely by a client.
 */
void
ActivatePointerGrab(DeviceIntPtr mouse, GrabPtr grab,
                    TimeStamp time, Bool autoGrab)
{
    GrabInfoPtr grabinfo = &mouse->deviceGrab;
    WindowPtr oldWin = (grabinfo->grab) ?
                        grabinfo->grab->window
                        : mouse->spriteInfo->sprite->win;
    Bool isPassive = autoGrab & ~ImplicitGrabMask;

    /* slave devices need to float for the duration of the grab. */
    if (grab->grabtype == GRABTYPE_XI2 &&
        !(autoGrab & ImplicitGrabMask) && !IsMaster(mouse))
        DetachFromMaster(mouse);

    if (grab->confineTo)
    {
	if (grab->confineTo->drawable.pScreen
                != mouse->spriteInfo->sprite->hotPhys.pScreen)
	    mouse->spriteInfo->sprite->hotPhys.x =
                mouse->spriteInfo->sprite->hotPhys.y = 0;
	ConfineCursorToWindow(mouse, grab->confineTo, FALSE, TRUE);
    }
    DoEnterLeaveEvents(mouse, mouse->id, oldWin, grab->window, NotifyGrab);
    mouse->valuator->motionHintWindow = NullWindow;
    if (syncEvents.playingEvents)
        grabinfo->grabTime = syncEvents.time;
    else
	grabinfo->grabTime = time;
    if (grab->cursor)
	grab->cursor->refcnt++;
    grabinfo->activeGrab = *grab;
    grabinfo->grab = &grabinfo->activeGrab;
    grabinfo->fromPassiveGrab = isPassive;
    grabinfo->implicitGrab = autoGrab & ImplicitGrabMask;
    PostNewCursor(mouse);
    CheckGrabForSyncs(mouse,(Bool)grab->pointerMode, (Bool)grab->keyboardMode);
}

/**
 * Delete grab on given device, update the sprite.
 *
 * Extension devices are set up for ActivateKeyboardGrab().
 */
void
DeactivatePointerGrab(DeviceIntPtr mouse)
{
    GrabPtr grab = mouse->deviceGrab.grab;
    DeviceIntPtr dev;
    Bool wasImplicit = (mouse->deviceGrab.fromPassiveGrab &&
                        mouse->deviceGrab.implicitGrab);

    mouse->valuator->motionHintWindow = NullWindow;
    mouse->deviceGrab.grab = NullGrab;
    mouse->deviceGrab.sync.state = NOT_GRABBED;
    mouse->deviceGrab.fromPassiveGrab = FALSE;

    for (dev = inputInfo.devices; dev; dev = dev->next)
    {
	if (dev->deviceGrab.sync.other == grab)
	    dev->deviceGrab.sync.other = NullGrab;
    }
    DoEnterLeaveEvents(mouse, mouse->id, grab->window,
                       mouse->spriteInfo->sprite->win, NotifyUngrab);
    if (grab->confineTo)
	ConfineCursorToWindow(mouse, RootWindow(mouse), FALSE, FALSE);
    PostNewCursor(mouse);
    if (grab->cursor)
	FreeCursor(grab->cursor, (Cursor)0);

    if (!wasImplicit && grab->grabtype == GRABTYPE_XI2)
        ReattachToOldMaster(mouse);

    ComputeFreezes();
}

/**
 * Activate a keyboard grab on the given device.
 *
 * Extension devices have ActivateKeyboardGrab() set as their grabbing proc.
 */
void
ActivateKeyboardGrab(DeviceIntPtr keybd, GrabPtr grab, TimeStamp time, Bool passive)
{
    GrabInfoPtr grabinfo = &keybd->deviceGrab;
    WindowPtr oldWin;

    /* slave devices need to float for the duration of the grab. */
    if (grab->grabtype == GRABTYPE_XI2 &&
        !(passive & ImplicitGrabMask) &&
        !IsMaster(keybd))
        DetachFromMaster(keybd);

    if (grabinfo->grab)
	oldWin = grabinfo->grab->window;
    else if (keybd->focus)
	oldWin = keybd->focus->win;
    else
	oldWin = keybd->spriteInfo->sprite->win;
    if (oldWin == FollowKeyboardWin)
	oldWin = keybd->focus->win;
    if (keybd->valuator)
	keybd->valuator->motionHintWindow = NullWindow;
    DoFocusEvents(keybd, oldWin, grab->window, NotifyGrab);
    if (syncEvents.playingEvents)
	grabinfo->grabTime = syncEvents.time;
    else
	grabinfo->grabTime = time;
    grabinfo->activeGrab = *grab;
    grabinfo->grab = &grabinfo->activeGrab;
    grabinfo->fromPassiveGrab = passive;
    grabinfo->implicitGrab = passive & ImplicitGrabMask;
    CheckGrabForSyncs(keybd, (Bool)grab->keyboardMode, (Bool)grab->pointerMode);
}

/**
 * Delete keyboard grab for the given device.
 */
void
DeactivateKeyboardGrab(DeviceIntPtr keybd)
{
    GrabPtr grab = keybd->deviceGrab.grab;
    DeviceIntPtr dev;
    WindowPtr focusWin = keybd->focus ? keybd->focus->win
                                           : keybd->spriteInfo->sprite->win;
    Bool wasImplicit = (keybd->deviceGrab.fromPassiveGrab &&
                        keybd->deviceGrab.implicitGrab);

    if (focusWin == FollowKeyboardWin)
	focusWin = inputInfo.keyboard->focus->win;
    if (keybd->valuator)
	keybd->valuator->motionHintWindow = NullWindow;
    keybd->deviceGrab.grab = NullGrab;
    keybd->deviceGrab.sync.state = NOT_GRABBED;
    keybd->deviceGrab.fromPassiveGrab = FALSE;

    for (dev = inputInfo.devices; dev; dev = dev->next)
    {
	if (dev->deviceGrab.sync.other == grab)
	    dev->deviceGrab.sync.other = NullGrab;
    }
    DoFocusEvents(keybd, grab->window, focusWin, NotifyUngrab);

    if (!wasImplicit && grab->grabtype == GRABTYPE_XI2)
        ReattachToOldMaster(keybd);

    ComputeFreezes();
}

void
AllowSome(ClientPtr client,
          TimeStamp time,
          DeviceIntPtr thisDev,
          int newState)
{
    Bool thisGrabbed, otherGrabbed, othersFrozen, thisSynced;
    TimeStamp grabTime;
    DeviceIntPtr dev;
    GrabInfoPtr devgrabinfo,
                grabinfo = &thisDev->deviceGrab;

    thisGrabbed = grabinfo->grab && SameClient(grabinfo->grab, client);
    thisSynced = FALSE;
    otherGrabbed = FALSE;
    othersFrozen = TRUE;
    grabTime = grabinfo->grabTime;
    for (dev = inputInfo.devices; dev; dev = dev->next)
    {
        devgrabinfo = &dev->deviceGrab;

	if (dev == thisDev)
	    continue;
	if (devgrabinfo->grab && SameClient(devgrabinfo->grab, client))
	{
	    if (!(thisGrabbed || otherGrabbed) ||
		(CompareTimeStamps(devgrabinfo->grabTime, grabTime) == LATER))
		grabTime = devgrabinfo->grabTime;
	    otherGrabbed = TRUE;
	    if (grabinfo->sync.other == devgrabinfo->grab)
		thisSynced = TRUE;
	    if (devgrabinfo->sync.state < FROZEN)
		othersFrozen = FALSE;
	}
	else if (!devgrabinfo->sync.other || !SameClient(devgrabinfo->sync.other, client))
	    othersFrozen = FALSE;
    }
    if (!((thisGrabbed && grabinfo->sync.state >= FROZEN) || thisSynced))
	return;
    if ((CompareTimeStamps(time, currentTime) == LATER) ||
	(CompareTimeStamps(time, grabTime) == EARLIER))
	return;
    switch (newState)
    {
	case THAWED:		       /* Async */
	    if (thisGrabbed)
		grabinfo->sync.state = THAWED;
	    if (thisSynced)
		grabinfo->sync.other = NullGrab;
	    ComputeFreezes();
	    break;
	case FREEZE_NEXT_EVENT:		/* Sync */
	    if (thisGrabbed)
	    {
		grabinfo->sync.state = FREEZE_NEXT_EVENT;
		if (thisSynced)
		    grabinfo->sync.other = NullGrab;
		ComputeFreezes();
	    }
	    break;
	case THAWED_BOTH:		/* AsyncBoth */
	    if (othersFrozen)
	    {
		for (dev = inputInfo.devices; dev; dev = dev->next)
		{
                    devgrabinfo = &dev->deviceGrab;
		    if (devgrabinfo->grab
                            && SameClient(devgrabinfo->grab, client))
			devgrabinfo->sync.state = THAWED;
		    if (devgrabinfo->sync.other &&
                            SameClient(devgrabinfo->sync.other, client))
			devgrabinfo->sync.other = NullGrab;
		}
		ComputeFreezes();
	    }
	    break;
	case FREEZE_BOTH_NEXT_EVENT:	/* SyncBoth */
	    if (othersFrozen)
	    {
		for (dev = inputInfo.devices; dev; dev = dev->next)
		{
                    devgrabinfo = &dev->deviceGrab;
		    if (devgrabinfo->grab
                            && SameClient(devgrabinfo->grab, client))
			devgrabinfo->sync.state = FREEZE_BOTH_NEXT_EVENT;
		    if (devgrabinfo->sync.other
                            && SameClient(devgrabinfo->sync.other, client))
			devgrabinfo->sync.other = NullGrab;
		}
		ComputeFreezes();
	    }
	    break;
	case NOT_GRABBED:		/* Replay */
	    if (thisGrabbed && grabinfo->sync.state == FROZEN_WITH_EVENT)
	    {
		if (thisSynced)
		    grabinfo->sync.other = NullGrab;
		syncEvents.replayDev = thisDev;
		syncEvents.replayWin = grabinfo->grab->window;
		(*grabinfo->DeactivateGrab)(thisDev);
		syncEvents.replayDev = (DeviceIntPtr)NULL;
	    }
	    break;
	case THAW_OTHERS:		/* AsyncOthers */
	    if (othersFrozen)
	    {
		for (dev = inputInfo.devices; dev; dev = dev->next)
		{
		    if (dev == thisDev)
			continue;
                    devgrabinfo = &dev->deviceGrab;
		    if (devgrabinfo->grab
                            && SameClient(devgrabinfo->grab, client))
			devgrabinfo->sync.state = THAWED;
		    if (devgrabinfo->sync.other
                            && SameClient(devgrabinfo->sync.other, client))
			devgrabinfo->sync.other = NullGrab;
		}
		ComputeFreezes();
	    }
	    break;
    }
}

/**
 * Server-side protocol handling for AllowEvents request.
 *
 * Release some events from a frozen device.
 */
int
ProcAllowEvents(ClientPtr client)
{
    TimeStamp		time;
    DeviceIntPtr	mouse = NULL;
    DeviceIntPtr	keybd = NULL;
    REQUEST(xAllowEventsReq);

    REQUEST_SIZE_MATCH(xAllowEventsReq);
    time = ClientTimeToServerTime(stuff->time);

    mouse = PickPointer(client);
    keybd = PickKeyboard(client);

    switch (stuff->mode)
    {
	case ReplayPointer:
	    AllowSome(client, time, mouse, NOT_GRABBED);
	    break;
	case SyncPointer:
	    AllowSome(client, time, mouse, FREEZE_NEXT_EVENT);
	    break;
	case AsyncPointer:
	    AllowSome(client, time, mouse, THAWED);
	    break;
	case ReplayKeyboard:
	    AllowSome(client, time, keybd, NOT_GRABBED);
	    break;
	case SyncKeyboard:
	    AllowSome(client, time, keybd, FREEZE_NEXT_EVENT);
	    break;
	case AsyncKeyboard:
	    AllowSome(client, time, keybd, THAWED);
	    break;
	case SyncBoth:
	    AllowSome(client, time, keybd, FREEZE_BOTH_NEXT_EVENT);
	    break;
	case AsyncBoth:
	    AllowSome(client, time, keybd, THAWED_BOTH);
	    break;
	default:
	    client->errorValue = stuff->mode;
	    return BadValue;
    }
    return Success;
}

/**
 * Deactivate grabs from any device that has been grabbed by the client.
 */
void
ReleaseActiveGrabs(ClientPtr client)
{
    DeviceIntPtr dev;
    Bool    done;

    /* XXX CloseDownClient should remove passive grabs before
     * releasing active grabs.
     */
    do {
	done = TRUE;
	for (dev = inputInfo.devices; dev; dev = dev->next)
	{
	    if (dev->deviceGrab.grab && SameClient(dev->deviceGrab.grab, client))
	    {
		(*dev->deviceGrab.DeactivateGrab)(dev);
		done = FALSE;
	    }
	}
    } while (!done);
}

/**************************************************************************
 *            The following procedures deal with delivering events        *
 **************************************************************************/

/**
 * Deliver the given events to the given client.
 *
 * More than one event may be delivered at a time. This is the case with
 * DeviceMotionNotifies which may be followed by DeviceValuator events.
 *
 * TryClientEvents() is the last station before actually writing the events to
 * the socket. Anything that is not filtered here, will get delivered to the
 * client.
 * An event is only delivered if
 *   - mask and filter match up.
 *   - no other client has a grab on the device that caused the event.
 *
 *
 * @param client The target client to deliver to.
 * @param dev The device the event came from. May be NULL.
 * @param pEvents The events to be delivered.
 * @param count Number of elements in pEvents.
 * @param mask Event mask as set by the window.
 * @param filter Mask based on event type.
 * @param grab Possible grab on the device that caused the event.
 *
 * @return 1 if event was delivered, 0 if not or -1 if grab was not set by the
 * client.
 */
int
TryClientEvents (ClientPtr client, DeviceIntPtr dev, xEvent *pEvents,
                 int count, Mask mask, Mask filter, GrabPtr grab)
{
    int i;
    int type;

#ifdef DEBUG_EVENTS
    ErrorF("[dix] Event([%d, %d], mask=0x%lx), client=%d%s",
           pEvents->u.u.type, pEvents->u.u.detail, mask,
           client ? client->index : -1,
           (client && client->clientGone) ? " (gone)" : "");
#endif

    if (!client || client == serverClient || client->clientGone) {
#ifdef DEBUG_EVENTS
        ErrorF(" not delivered to fake/dead client\n");
#endif
        return 0;
    }

    if (filter != CantBeFiltered && !(mask & filter))
    {
 #ifdef DEBUG_EVENTS
        ErrorF(" filtered\n");
 #endif
        return 0;
    }

    if (grab && !SameClient(grab, client))
    {
#ifdef DEBUG_EVENTS
        ErrorF(" not delivered due to grab\n");
#endif
        return -1; /* don't send, but notify caller */
    }

    type = pEvents->u.u.type;
    if (type == MotionNotify)
    {
        if (mask & PointerMotionHintMask)
        {
            if (WID(dev->valuator->motionHintWindow) ==
                    pEvents->u.keyButtonPointer.event)
            {
#ifdef DEBUG_EVENTS
                ErrorF("[dix] \n");
                ErrorF("[dix] motionHintWindow == keyButtonPointer.event\n");
#endif
                return 1; /* don't send, but pretend we did */
            }
            pEvents->u.u.detail = NotifyHint;
        }
        else
        {
            pEvents->u.u.detail = NotifyNormal;
        }
    }
    else if (type == DeviceMotionNotify)
    {
        if (MaybeSendDeviceMotionNotifyHint((deviceKeyButtonPointer*)pEvents,
                                            mask) != 0)
            return 1;
    } else if (type == KeyPress)
    {
        if (EventIsKeyRepeat(pEvents))
        {
            if (!_XkbWantsDetectableAutoRepeat(client))
            {
                xEvent release = *pEvents;
                release.u.u.type = KeyRelease;
                release.u.u.sequenceNumber = client->sequence;
                WriteEventsToClient(client, 1, &release);
#ifdef DEBUG_EVENTS
                ErrorF(" (plus fake core release for repeat)");
#endif
            } else
            {
#ifdef DEBUG_EVENTS
                ErrorF(" (detectable autorepeat for core)");
#endif
            }
        }

    } else if (type == DeviceKeyPress)
    {
        if (EventIsKeyRepeat(pEvents))
        {
            if (!_XkbWantsDetectableAutoRepeat(client))
            {
                deviceKeyButtonPointer release = *(deviceKeyButtonPointer *)pEvents;
                release.type = DeviceKeyRelease;
                release.sequenceNumber = client->sequence;
#ifdef DEBUG_EVENTS
                ErrorF(" (plus fake xi1 release for repeat)");
#endif
                WriteEventsToClient(client, 1, (xEvent *) &release);
            }
            else {
#ifdef DEBUG_EVENTS
                ErrorF(" (detectable autorepeat for core)");
#endif
            }
        }
    }

    type &= 0177;
    if (type != KeymapNotify)
    {
        /* all extension events must have a sequence number */
        for (i = 0; i < count; i++)
            pEvents[i].u.u.sequenceNumber = client->sequence;
    }

    if (BitIsOn(criticalEvents, type))
    {
        if (client->smart_priority < SMART_MAX_PRIORITY)
            client->smart_priority++;
        SetCriticalOutputPending();
    }

    WriteEventsToClient(client, count, pEvents);
#ifdef DEBUG_EVENTS
    ErrorF("[dix]  delivered\n");
#endif
    return 1;
}

/**
 * Deliver events to a window. At this point, we do not yet know if the event
 * actually needs to be delivered. May activate a grab if the event is a
 * button press.
 *
 * Core events are always delivered to the window owner. If the filter is
 * something other than CantBeFiltered, the event is also delivered to other
 * clients with the matching mask on the window.
 *
 * More than one event may be delivered at a time. This is the case with
 * DeviceMotionNotifies which may be followed by DeviceValuator events.
 *
 * @param pWin The window that would get the event.
 * @param pEvents The events to be delivered.
 * @param count Number of elements in pEvents.
 * @param filter Mask based on event type.
 * @param grab Possible grab on the device that caused the event.
 *
 * @return Number of events delivered to various clients.
 */
int
DeliverEventsToWindow(DeviceIntPtr pDev, WindowPtr pWin, xEvent
        *pEvents, int count, Mask filter, GrabPtr grab)
{
    int deliveries = 0, nondeliveries = 0;
    int attempt;
    InputClients *other;
    ClientPtr client = NullClient;
    Mask deliveryMask = 0; /* If a grab occurs due to a button press, then
		              this mask is the mask of the grab. */
    int type = pEvents->u.u.type;


    /* Deliver to window owner */
    if ((filter == CantBeFiltered) || CORE_EVENT(pEvents))
    {
	/* if nobody ever wants to see this event, skip some work */
	if (filter != CantBeFiltered &&
	    !((wOtherEventMasks(pWin)|pWin->eventMask) & filter))
	    return 0;

        if (IsInterferingGrab(wClient(pWin), pDev, pEvents))
                return 0;

	if (XaceHook(XACE_RECEIVE_ACCESS, wClient(pWin), pWin, pEvents, count))
	    /* do nothing */;
        else if ( (attempt = TryClientEvents(wClient(pWin), pDev, pEvents,
                                             count, pWin->eventMask,
                                             filter, grab)) )
	{
	    if (attempt > 0)
	    {
		deliveries++;
		client = wClient(pWin);
		deliveryMask = pWin->eventMask;
	    } else
		nondeliveries--;
	}
    }

    /* CantBeFiltered means only window owner gets the event */
    if (filter != CantBeFiltered)
    {
        if (CORE_EVENT(pEvents))
            other = (InputClients *)wOtherClients(pWin);
        else if (XI2_EVENT(pEvents))
        {
            OtherInputMasks *inputMasks = wOtherInputMasks(pWin);
            /* Has any client selected for the event? */
            if (!GetWindowXI2Mask(pDev, pWin, pEvents))
                return 0;
            other = inputMasks->inputClients;
        } else {
            OtherInputMasks *inputMasks = wOtherInputMasks(pWin);
            /* Has any client selected for the event? */
            if (!inputMasks ||
                !(inputMasks->inputEvents[pDev->id] & filter))
                return 0;

            other = inputMasks->inputClients;
        }

        for (; other; other = other->next)
        {
            Mask mask;
            if (IsInterferingGrab(rClient(other), pDev, pEvents))
                continue;

            mask = GetEventMask(pDev, pEvents, other);

            if (XaceHook(XACE_RECEIVE_ACCESS, rClient(other), pWin,
                        pEvents, count))
                /* do nothing */;
            else if ( (attempt = TryClientEvents(rClient(other), pDev,
                            pEvents, count,
                            mask, filter, grab)) )
            {
                if (attempt > 0)
                {
                    deliveries++;
                    client = rClient(other);
                    deliveryMask = mask;
                } else
                    nondeliveries--;
            }
        }
    }
    /*
     * Note that since core events are delivered first, an implicit grab may
     * be activated on a core grab, stopping the XI events.
     */
    if ((type == DeviceButtonPress || type == ButtonPress ||
        ((XI2_EVENT(pEvents) && ((xGenericEvent*)pEvents)->evtype == XI_ButtonPress)))
            && deliveries
            && (!grab))
    {
	GrabRec tempGrab;
        OtherInputMasks *inputMasks;

        memset(&tempGrab, 0, sizeof(GrabRec));
        tempGrab.next = NULL;
	tempGrab.device = pDev;
	tempGrab.resource = client->clientAsMask;
	tempGrab.window = pWin;
	tempGrab.ownerEvents = (deliveryMask & OwnerGrabButtonMask) ? TRUE : FALSE;
	tempGrab.eventMask = deliveryMask;
	tempGrab.keyboardMode = GrabModeAsync;
	tempGrab.pointerMode = GrabModeAsync;
	tempGrab.confineTo = NullWindow;
	tempGrab.cursor = NullCursor;
        tempGrab.type = type;
        if (type == ButtonPress)
            tempGrab.grabtype = GRABTYPE_CORE;
        else if (type == DeviceButtonPress)
            tempGrab.grabtype = GRABTYPE_XI;
        else
        {
            tempGrab.type = ((xGenericEvent*)pEvents)->evtype;
            tempGrab.grabtype = GRABTYPE_XI2;
        }

        /* get the XI and XI2 device mask */
        inputMasks = wOtherInputMasks(pWin);
        tempGrab.deviceMask = (inputMasks) ? inputMasks->inputEvents[pDev->id]: 0;

        if (inputMasks)
            memcpy(tempGrab.xi2mask, inputMasks->xi2mask,
                    sizeof(tempGrab.xi2mask));

	(*pDev->deviceGrab.ActivateGrab)(pDev, &tempGrab,
                                        currentTime, TRUE | ImplicitGrabMask);
    }
    else if ((type == MotionNotify) && deliveries)
	pDev->valuator->motionHintWindow = pWin;
    else
    {
	if ((type == DeviceMotionNotify || type == DeviceButtonPress) &&
	    deliveries)
	    CheckDeviceGrabAndHintWindow (pWin, type,
					  (deviceKeyButtonPointer*) pEvents,
					  grab, client, deliveryMask);
    }
    if (deliveries)
	return deliveries;
    return nondeliveries;
}

/* If the event goes to dontClient, don't send it and return 0.  if
   send works,  return 1 or if send didn't work, return 2.
   Only works for core events.
*/

#ifdef PANORAMIX
static int
XineramaTryClientEventsResult(
    ClientPtr client,
    GrabPtr grab,
    Mask mask,
    Mask filter
){
    if ((client) && (client != serverClient) && (!client->clientGone) &&
        ((filter == CantBeFiltered) || (mask & filter)))
    {
        if (grab && !SameClient(grab, client)) return -1;
	else return 1;
    }
    return 0;
}
#endif

/**
 * Try to deliver events to the interested parties.
 *
 * @param pWin The window that would get the event.
 * @param pEvents The events to be delivered.
 * @param count Number of elements in pEvents.
 * @param filter Mask based on event type.
 * @param dontClient Don't deliver to the dontClient.
 */
int
MaybeDeliverEventsToClient(WindowPtr pWin, xEvent *pEvents,
                           int count, Mask filter, ClientPtr dontClient)
{
    OtherClients *other;


    if (pWin->eventMask & filter)
    {
        if (wClient(pWin) == dontClient)
	    return 0;
#ifdef PANORAMIX
	if(!noPanoramiXExtension && pWin->drawable.pScreen->myNum)
	    return XineramaTryClientEventsResult(
			wClient(pWin), NullGrab, pWin->eventMask, filter);
#endif
	if (XaceHook(XACE_RECEIVE_ACCESS, wClient(pWin), pWin, pEvents, count))
	    return 1; /* don't send, but pretend we did */
	return TryClientEvents(wClient(pWin), NULL, pEvents, count,
			       pWin->eventMask, filter, NullGrab);
    }
    for (other = wOtherClients(pWin); other; other = other->next)
    {
	if (other->mask & filter)
	{
            if (SameClient(other, dontClient))
		return 0;
#ifdef PANORAMIX
	    if(!noPanoramiXExtension && pWin->drawable.pScreen->myNum)
	      return XineramaTryClientEventsResult(
			rClient(other), NullGrab, other->mask, filter);
#endif
	    if (XaceHook(XACE_RECEIVE_ACCESS, rClient(other), pWin, pEvents,
			 count))
		return 1; /* don't send, but pretend we did */
	    return TryClientEvents(rClient(other), NULL, pEvents, count,
				   other->mask, filter, NullGrab);
	}
    }
    return 2;
}

static Window FindChildForEvent(DeviceIntPtr dev, WindowPtr event)
{
    SpritePtr pSprite = dev->spriteInfo->sprite;
    WindowPtr w = pSprite->spriteTrace[pSprite->spriteTraceGood-1];
    Window child = None;

    /* If the search ends up past the root should the child field be
       set to none or should the value in the argument be passed
       through. It probably doesn't matter since everyone calls
       this function with child == None anyway. */
    while (w)
    {
        /* If the source window is same as event window, child should be
           none.  Don't bother going all all the way back to the root. */

        if (w == event)
        {
            child = None;
            break;
        }

        if (w->parent == event)
        {
            child = w->drawable.id;
            break;
        }
        w = w->parent;
    }
    return child;
}

/**
 * Adjust event fields to comply with the window properties.
 *
 * @param xE Event to be modified in place
 * @param pWin The window to get the information from.
 * @param child Child window setting for event (if applicable)
 * @param calcChild If True, calculate the child window.
 */
void
FixUpEventFromWindow(
    DeviceIntPtr pDev,
    xEvent *xE,
    WindowPtr pWin,
    Window child,
    Bool calcChild)
{
    SpritePtr pSprite = pDev->spriteInfo->sprite;

    if (calcChild)
        child = FindChildForEvent(pDev, pWin);

    if (XI2_EVENT(xE))
    {
        xXIDeviceEvent* event = (xXIDeviceEvent*)xE;

        if (event->evtype == XI_RawKeyPress ||
            event->evtype == XI_RawKeyRelease ||
            event->evtype == XI_RawButtonPress ||
            event->evtype == XI_RawButtonRelease ||
            event->evtype == XI_RawMotion ||
            event->evtype == XI_DeviceChanged ||
            event->evtype == XI_HierarchyChanged ||
            event->evtype == XI_PropertyEvent)
            return;

        event->root = RootWindow(pDev)->drawable.id;
        event->event = pWin->drawable.id;
        if (pSprite->hot.pScreen == pWin->drawable.pScreen)
        {
            event->event_x = event->root_x - FP1616(pWin->drawable.x, 0);
            event->event_y = event->root_y - FP1616(pWin->drawable.y, 0);
            event->child = child;
        } else
        {
            event->event_x = 0;
            event->event_y = 0;
            event->child = None;
        }

        if (event->evtype == XI_Enter || event->evtype == XI_Leave ||
            event->evtype == XI_FocusIn || event->evtype == XI_FocusOut)
            ((xXIEnterEvent*)event)->same_screen =
                (pSprite->hot.pScreen == pWin->drawable.pScreen);

    } else
    {
        XE_KBPTR.root = RootWindow(pDev)->drawable.id;
        XE_KBPTR.event = pWin->drawable.id;
        if (pSprite->hot.pScreen == pWin->drawable.pScreen)
        {
            XE_KBPTR.sameScreen = xTrue;
            XE_KBPTR.child = child;
            XE_KBPTR.eventX =
                XE_KBPTR.rootX - pWin->drawable.x;
            XE_KBPTR.eventY =
                XE_KBPTR.rootY - pWin->drawable.y;
        }
        else
        {
            XE_KBPTR.sameScreen = xFalse;
            XE_KBPTR.child = None;
            XE_KBPTR.eventX = 0;
            XE_KBPTR.eventY = 0;
        }
    }
}

/**
 * Return masks for EventIsDeliverable.
 * @defgroup EventIsDeliverable return flags
 * @{
 */
#define XI_MASK                 (1 << 0) /**< XI mask set on window */
#define CORE_MASK               (1 << 1) /**< Core mask set on window */
#define DONT_PROPAGATE_MASK     (1 << 2) /**< DontPropagate mask set on window */
#define XI2_MASK                (1 << 3) /**< XI2 mask set on window */
/* @} */

/**
 * Check if a given event is deliverable at all on a given window.
 *
 * This function only checks if any client wants it, not for a specific
 * client.
 *
 * @param[in] dev The device this event is being sent for.
 * @param[in] event The event that is to be sent.
 * @param[in] win The current event window.
 *
 * @return Bitmask of ::XI2_MASK, ::XI_MASK, ::CORE_MASK, and
 * ::DONT_PROPAGATE_MASK.
 */
static int
EventIsDeliverable(DeviceIntPtr dev, InternalEvent* event, WindowPtr win)
{
    int rc = 0;
    int filter = 0;
    int type;
    OtherInputMasks *inputMasks = wOtherInputMasks(win);
    xEvent ev;

    /* XXX: this makes me gag */
    type = GetXI2Type(event);
    ev.u.u.type = GenericEvent; /* GetEventFilter only cares about type and evtype*/
    ((xGenericEvent*)&ev)->extension = IReqCode;
    ((xGenericEvent*)&ev)->evtype = type;
    filter = GetEventFilter(dev, &ev);
    if (type && inputMasks &&
        ((inputMasks->xi2mask[XIAllDevices][type/8] & filter) ||
         ((inputMasks->xi2mask[XIAllMasterDevices][type/8] & filter) && IsMaster(dev)) ||
         (inputMasks->xi2mask[dev->id][type/8] & filter)))
        rc |= XI2_MASK;

    type = GetXIType(event);
    ev.u.u.type = type;
    filter = GetEventFilter(dev, &ev);

    /* Check for XI mask */
    if (type && inputMasks &&
        (inputMasks->deliverableEvents[dev->id] & filter) &&
        (inputMasks->inputEvents[dev->id] & filter))
        rc |= XI_MASK;

    /* Check for XI DontPropagate mask */
    if (type && inputMasks &&
        (inputMasks->dontPropagateMask[dev->id] & filter))
        rc |= DONT_PROPAGATE_MASK;

    /* Check for core mask */
    type = GetCoreType(event);
    if (type && (win->deliverableEvents & filter) &&
        ((wOtherEventMasks(win) | win->eventMask) & filter))
        rc |= CORE_MASK;

    /* Check for core DontPropagate mask */
    if (type && (filter & wDontPropagateMask(win)))
        rc |= DONT_PROPAGATE_MASK;

    return rc;
}

/**
 * Deliver events caused by input devices.
 *
 * For events from a non-grabbed, non-focus device, DeliverDeviceEvents is
 * called directly from the processInputProc.
 * For grabbed devices, DeliverGrabbedEvent is called first, and _may_ call
 * DeliverDeviceEvents.
 * For focused events, DeliverFocusedEvent is called first, and _may_ call
 * DeliverDeviceEvents.
 *
 * @param pWin Window to deliver event to.
 * @param event The events to deliver, not yet in wire format.
 * @param grab Possible grab on a device.
 * @param stopAt Don't recurse up to the root window.
 * @param dev The device that is responsible for the event.
 *
 * @see DeliverGrabbedEvent
 * @see DeliverFocusedEvent
 */
int
DeliverDeviceEvents(WindowPtr pWin, InternalEvent *event, GrabPtr grab,
                    WindowPtr stopAt, DeviceIntPtr dev)
{
    Window child = None;
    Mask filter;
    int deliveries = 0;
    xEvent core;
    xEvent *xE = NULL;
    int rc, mask, count = 0;

    CHECKEVENT(event);

    while (pWin)
    {
        if ((mask = EventIsDeliverable(dev, event, pWin)))
        {
            /* XI2 events first */
            if (mask & XI2_MASK)
            {
                xEvent *xi2 = NULL;
                rc = EventToXI2(event, &xi2);
                if (rc == Success)
                {
                    /* XXX: XACE */
                    filter = GetEventFilter(dev, xi2);
                    FixUpEventFromWindow(dev, xi2, pWin, child, FALSE);
                    deliveries = DeliverEventsToWindow(dev, pWin, xi2, 1,
                                                       filter, grab);
                    xfree(xi2);
                    if (deliveries > 0)
                        goto unwind;
                } else if (rc != BadMatch)
                    ErrorF("[dix] %s: XI2 conversion failed in DDE (%d).\n",
                            dev->name, rc);
            }

            /* XI events */
            if (mask & XI_MASK)
            {
                rc = EventToXI(event, &xE, &count);
                if (rc == Success) {
                    if (XaceHook(XACE_SEND_ACCESS, NULL, dev, pWin, xE, count) == Success) {
                        filter = GetEventFilter(dev, xE);
                        FixUpEventFromWindow(dev, xE, pWin, child, FALSE);
                        deliveries = DeliverEventsToWindow(dev, pWin, xE, count,
                                                           filter, grab);
                        if (deliveries > 0)
                            goto unwind;
                    }
                } else if (rc != BadMatch)
                    ErrorF("[dix] %s: XI conversion failed in DDE (%d, %d). Skipping delivery.\n",
                            dev->name, event->any.type, rc);
            }

            /* Core event */
            if ((mask & CORE_MASK) && IsMaster(dev) && dev->coreEvents)
            {
                rc = EventToCore(event, &core);
                if (rc == Success) {
                    if (XaceHook(XACE_SEND_ACCESS, NULL, dev, pWin, &core, 1) == Success) {
                        filter = GetEventFilter(dev, &core);
                        FixUpEventFromWindow(dev, &core, pWin, child, FALSE);
                        deliveries = DeliverEventsToWindow(dev, pWin, &core, 1,
                                                           filter, grab);
                        if (deliveries > 0)
                            goto unwind;
                    }
                } else if (rc != BadMatch)
                        ErrorF("[dix] %s: Core conversion failed in DDE (%d, %d).\n",
                                dev->name, event->any.type, rc);
            }

            if ((deliveries < 0) || (pWin == stopAt) ||
                (mask & DONT_PROPAGATE_MASK))
            {
                deliveries = 0;
                goto unwind;
            }
        }

        child = pWin->drawable.id;
        pWin = pWin->parent;
    }

unwind:
    xfree(xE);
    return deliveries;
}

#undef XI_MASK
#undef CORE_MASK
#undef DONT_PROPAGATE_MASK

/**
 * Deliver event to a window and it's immediate parent. Used for most window
 * events (CreateNotify, ConfigureNotify, etc.). Not useful for events that
 * propagate up the tree or extension events
 *
 * In case of a ReparentNotify event, the event will be delivered to the
 * otherParent as well.
 *
 * @param pWin Window to deliver events to.
 * @param xE Events to deliver.
 * @param count number of events in xE.
 * @param otherParent Used for ReparentNotify events.
 */
int
DeliverEvents(WindowPtr pWin, xEvent *xE, int count,
              WindowPtr otherParent)
{
    Mask filter;
    int     deliveries;
    DeviceIntRec dummy;

#ifdef PANORAMIX
    if(!noPanoramiXExtension && pWin->drawable.pScreen->myNum)
	return count;
#endif

    if (!count)
	return 0;

    dummy.id = XIAllDevices;
    filter = GetEventFilter(&dummy, xE);
    if ((filter & SubstructureNotifyMask) && (xE->u.u.type != CreateNotify))
	xE->u.destroyNotify.event = pWin->drawable.id;
    if (filter != StructureAndSubMask)
	return DeliverEventsToWindow(&dummy, pWin, xE, count, filter, NullGrab);
    deliveries = DeliverEventsToWindow(&dummy, pWin, xE, count,
                                       StructureNotifyMask, NullGrab);
    if (pWin->parent)
    {
	xE->u.destroyNotify.event = pWin->parent->drawable.id;
	deliveries += DeliverEventsToWindow(&dummy, pWin->parent, xE, count,
					    SubstructureNotifyMask, NullGrab);
	if (xE->u.u.type == ReparentNotify)
	{
	    xE->u.destroyNotify.event = otherParent->drawable.id;
            deliveries += DeliverEventsToWindow(&dummy,
                    otherParent, xE, count, SubstructureNotifyMask,
						NullGrab);
	}
    }
    return deliveries;
}


static Bool
PointInBorderSize(WindowPtr pWin, int x, int y)
{
    BoxRec box;
    SpritePtr pSprite = inputInfo.pointer->spriteInfo->sprite;

    if(POINT_IN_REGION(pWin->drawable.pScreen, &pWin->borderSize, x, y, &box))
	return TRUE;

#ifdef PANORAMIX
    if(!noPanoramiXExtension &&
            XineramaSetWindowPntrs(inputInfo.pointer, pWin)) {
	int i;

	for(i = 1; i < PanoramiXNumScreens; i++) {
	   if(POINT_IN_REGION(pSprite->screen,
			&pSprite->windows[i]->borderSize,
			x + panoramiXdataPtr[0].x - panoramiXdataPtr[i].x,
			y + panoramiXdataPtr[0].y - panoramiXdataPtr[i].y,
			&box))
		return TRUE;
	}
    }
#endif
    return FALSE;
}

/**
 * Traversed from the root window to the window at the position x/y. While
 * traversing, it sets up the traversal history in the spriteTrace array.
 * After completing, the spriteTrace history is set in the following way:
 *   spriteTrace[0] ... root window
 *   spriteTrace[1] ... top level window that encloses x/y
 *       ...
 *   spriteTrace[spriteTraceGood - 1] ... window at x/y
 *
 * @returns the window at the given coordinates.
 */
static WindowPtr
XYToWindow(DeviceIntPtr pDev, int x, int y)
{
    WindowPtr  pWin;
    BoxRec		box;
    SpritePtr pSprite;

    pSprite = pDev->spriteInfo->sprite;
    pSprite->spriteTraceGood = 1;	/* root window still there */
    pWin = RootWindow(pDev)->firstChild;
    while (pWin)
    {
	if ((pWin->mapped) &&
	    (x >= pWin->drawable.x - wBorderWidth (pWin)) &&
	    (x < pWin->drawable.x + (int)pWin->drawable.width +
	     wBorderWidth(pWin)) &&
	    (y >= pWin->drawable.y - wBorderWidth (pWin)) &&
	    (y < pWin->drawable.y + (int)pWin->drawable.height +
	     wBorderWidth (pWin))
	    /* When a window is shaped, a further check
	     * is made to see if the point is inside
	     * borderSize
	     */
	    && (!wBoundingShape(pWin) || PointInBorderSize(pWin, x, y))
	    && (!wInputShape(pWin) ||
		POINT_IN_REGION(pWin->drawable.pScreen,
				wInputShape(pWin),
				x - pWin->drawable.x,
				y - pWin->drawable.y, &box))
#ifdef ROOTLESS
    /* In rootless mode windows may be offscreen, even when
     * they're in X's stack. (E.g. if the native window system
     * implements some form of virtual desktop system).
     */
		&& !pWin->rootlessUnhittable
#endif
	    )
	{
	    if (pSprite->spriteTraceGood >= pSprite->spriteTraceSize)
	    {
		pSprite->spriteTraceSize += 10;
		pSprite->spriteTrace = xrealloc(pSprite->spriteTrace,
		                    pSprite->spriteTraceSize*sizeof(WindowPtr));
	    }
	    pSprite->spriteTrace[pSprite->spriteTraceGood++] = pWin;
	    pWin = pWin->firstChild;
	}
	else
	    pWin = pWin->nextSib;
    }
    return pSprite->spriteTrace[pSprite->spriteTraceGood-1];
}

/**
 * Ungrab a currently FocusIn grabbed device and grab the device on the
 * given window. If the win given is the NoneWin, the device is ungrabbed if
 * applicable and FALSE is returned.
 *
 * @returns TRUE if the device has been grabbed, or FALSE otherwise.
 */
BOOL
ActivateFocusInGrab(DeviceIntPtr dev, WindowPtr old, WindowPtr win)
{
    BOOL rc = FALSE;
    DeviceEvent event;

    if (dev->deviceGrab.grab &&
        dev->deviceGrab.fromPassiveGrab &&
        dev->deviceGrab.grab->type == XI_Enter)
    {
        if (dev->deviceGrab.grab->window == win ||
            IsParent(dev->deviceGrab.grab->window, win))
            return FALSE;
        DoEnterLeaveEvents(dev, dev->id, old, win, XINotifyPassiveUngrab);
        (*dev->deviceGrab.DeactivateGrab)(dev);
    }

    if (win == NoneWin || win == PointerRootWin)
        return FALSE;

    memset(&event, 0, sizeof(DeviceEvent));
    event.header = ET_Internal;
    event.type = ET_FocusIn;
    event.length = sizeof(DeviceEvent);
    event.time = GetTimeInMillis();
    event.deviceid = dev->id;
    event.sourceid = dev->id;
    event.detail.button = 0;
    rc = CheckPassiveGrabsOnWindow(win, dev, &event, FALSE);
    if (rc)
        DoEnterLeaveEvents(dev, dev->id, old, win, XINotifyPassiveUngrab);
    return rc;
}

/**
 * Ungrab a currently Enter grabbed device and grab the device for the given
 * window.
 *
 * @returns TRUE if the device has been grabbed, or FALSE otherwise.
 */
static BOOL
ActivateEnterGrab(DeviceIntPtr dev, WindowPtr old, WindowPtr win)
{
    BOOL rc = FALSE;
    DeviceEvent event;

    if (dev->deviceGrab.grab &&
        dev->deviceGrab.fromPassiveGrab &&
        dev->deviceGrab.grab->type == XI_Enter)
    {
        if (dev->deviceGrab.grab->window == win ||
            IsParent(dev->deviceGrab.grab->window, win))
            return FALSE;
        DoEnterLeaveEvents(dev, dev->id, old, win, XINotifyPassiveUngrab);
        (*dev->deviceGrab.DeactivateGrab)(dev);
    }

    memset(&event, 0, sizeof(DeviceEvent));
    event.header = ET_Internal;
    event.type = ET_Enter;
    event.length = sizeof(DeviceEvent);
    event.time = GetTimeInMillis();
    event.deviceid = dev->id;
    event.sourceid = dev->id;
    event.detail.button = 0;
    rc = CheckPassiveGrabsOnWindow(win, dev, &event, FALSE);
    if (rc)
        DoEnterLeaveEvents(dev, dev->id, old, win, XINotifyPassiveGrab);

    return rc;
}

/**
 * Update the sprite coordinates based on the event. Update the cursor
 * position, then update the event with the new coordinates that may have been
 * changed. If the window underneath the sprite has changed, change to new
 * cursor and send enter/leave events.
 *
 * CheckMotion() will not do anything and return FALSE if the event is not a
 * pointer event.
 *
 * @return TRUE if the sprite has moved or FALSE otherwise.
 */
Bool
CheckMotion(DeviceEvent *ev, DeviceIntPtr pDev)
{
    WindowPtr prevSpriteWin, newSpriteWin;
    SpritePtr pSprite = pDev->spriteInfo->sprite;

    CHECKEVENT(ev);

    prevSpriteWin = pSprite->win;

    if (ev && !syncEvents.playingEvents)
    {
        /* GetPointerEvents() guarantees that pointer events have the correct
           rootX/Y set already. */
        switch (ev->type)
        {
            case ET_ButtonPress:
            case ET_ButtonRelease:
            case ET_Motion:
                break;
            default:
                /* all other events return FALSE */
                return FALSE;
        }


#ifdef PANORAMIX
        if (!noPanoramiXExtension)
        {
            /* Motion events entering DIX get translated to Screen 0
               coordinates.  Replayed events have already been
               translated since they've entered DIX before */
            ev->root_x += panoramiXdataPtr[pSprite->screen->myNum].x -
                                       panoramiXdataPtr[0].x;
            ev->root_y += panoramiXdataPtr[pSprite->screen->myNum].y -
                                       panoramiXdataPtr[0].y;
        } else
#endif
        {
            if (pSprite->hot.pScreen != pSprite->hotPhys.pScreen)
            {
                pSprite->hot.pScreen = pSprite->hotPhys.pScreen;
                RootWindow(pDev) = WindowTable[pSprite->hot.pScreen->myNum];
            }
        }

        pSprite->hot.x = ev->root_x;
        pSprite->hot.y = ev->root_y;
        if (pSprite->hot.x < pSprite->physLimits.x1)
            pSprite->hot.x = pSprite->physLimits.x1;
        else if (pSprite->hot.x >= pSprite->physLimits.x2)
            pSprite->hot.x = pSprite->physLimits.x2 - 1;
        if (pSprite->hot.y < pSprite->physLimits.y1)
            pSprite->hot.y = pSprite->physLimits.y1;
        else if (pSprite->hot.y >= pSprite->physLimits.y2)
            pSprite->hot.y = pSprite->physLimits.y2 - 1;
	if (pSprite->hotShape)
	    ConfineToShape(pDev, pSprite->hotShape, &pSprite->hot.x, &pSprite->hot.y);
	pSprite->hotPhys = pSprite->hot;

	if ((pSprite->hotPhys.x != ev->root_x) ||
	    (pSprite->hotPhys.y != ev->root_y))
	{
#ifdef PANORAMIX
            if (!noPanoramiXExtension)
            {
                XineramaSetCursorPosition(
                        pDev, pSprite->hotPhys.x, pSprite->hotPhys.y, FALSE);
            } else
#endif
            {
                (*pSprite->hotPhys.pScreen->SetCursorPosition)(
                        pDev, pSprite->hotPhys.pScreen,
                        pSprite->hotPhys.x, pSprite->hotPhys.y, FALSE);
            }
	}

	ev->root_x = pSprite->hot.x;
	ev->root_y = pSprite->hot.y;
    }

    newSpriteWin = XYToWindow(pDev, pSprite->hot.x, pSprite->hot.y);

    if (newSpriteWin != prevSpriteWin)
    {
        int sourceid;
        if (!ev) {
            UpdateCurrentTimeIf();
            sourceid = pDev->id; /* when from WindowsRestructured */
        } else
            sourceid = ev->sourceid;

	if (prevSpriteWin != NullWindow) {
            if (!ActivateEnterGrab(pDev, prevSpriteWin, newSpriteWin))
                DoEnterLeaveEvents(pDev, sourceid, prevSpriteWin,
                                   newSpriteWin, NotifyNormal);
        }
        /* set pSprite->win after ActivateEnterGrab, otherwise
           sprite window == grab_window and no enter/leave events are
           sent. */
        pSprite->win = newSpriteWin;
        PostNewCursor(pDev);
        return FALSE;
    }
    return TRUE;
}

/**
 * Windows have restructured, we need to update the sprite position and the
 * sprite's cursor.
 */
void
WindowsRestructured(void)
{
    DeviceIntPtr pDev = inputInfo.devices;
    while(pDev)
    {
        if (IsMaster(pDev) || !pDev->u.master)
            CheckMotion(NULL, pDev);
        pDev = pDev->next;
    }
}

#ifdef PANORAMIX
/* This was added to support reconfiguration under Xdmx.  The problem is
 * that if the 0th screen (i.e., WindowTable[0]) is moved to an origin
 * other than 0,0, the information in the private sprite structure must
 * be updated accordingly, or XYToWindow (and other routines) will not
 * compute correctly. */
void ReinitializeRootWindow(WindowPtr win, int xoff, int yoff)
{
    GrabPtr   grab;
    DeviceIntPtr pDev;
    SpritePtr pSprite;

    if (noPanoramiXExtension) return;

    pDev = inputInfo.devices;
    while(pDev)
    {
        if (DevHasCursor(pDev))
        {
            pSprite = pDev->spriteInfo->sprite;
            pSprite->hot.x        -= xoff;
            pSprite->hot.y        -= yoff;

            pSprite->hotPhys.x    -= xoff;
            pSprite->hotPhys.y    -= yoff;

            pSprite->hotLimits.x1 -= xoff;
            pSprite->hotLimits.y1 -= yoff;
            pSprite->hotLimits.x2 -= xoff;
            pSprite->hotLimits.y2 -= yoff;

            if (REGION_NOTEMPTY(pSprite->screen, &pSprite->Reg1))
                REGION_TRANSLATE(pSprite->screen, &pSprite->Reg1,    xoff, yoff);
            if (REGION_NOTEMPTY(pSprite->screen, &pSprite->Reg2))
                REGION_TRANSLATE(pSprite->screen, &pSprite->Reg2,    xoff, yoff);

            /* FIXME: if we call ConfineCursorToWindow, must we do anything else? */
            if ((grab = pDev->deviceGrab.grab) && grab->confineTo) {
                if (grab->confineTo->drawable.pScreen
                        != pSprite->hotPhys.pScreen)
                    pSprite->hotPhys.x = pSprite->hotPhys.y = 0;
                ConfineCursorToWindow(pDev, grab->confineTo, TRUE, TRUE);
            } else
                ConfineCursorToWindow(
                        pDev,
                        WindowTable[pSprite->hotPhys.pScreen->myNum],
                        TRUE, FALSE);

        }
        pDev = pDev->next;
    }
}
#endif

/**
 * Initialize a sprite for the given device and set it to some sane values. If
 * the device already has a sprite alloc'd, don't realloc but just reset to
 * default values.
 * If a window is supplied, the sprite will be initialized with the window's
 * cursor and positioned in the center of the window's screen. The root window
 * is a good choice to pass in here.
 *
 * It's a good idea to call it only for pointer devices, unless you have a
 * really talented keyboard.
 *
 * @param pDev The device to initialize.
 * @param pWin The window where to generate the sprite in.
 *
 */
void
InitializeSprite(DeviceIntPtr pDev, WindowPtr pWin)
{
    SpritePtr pSprite;
    ScreenPtr pScreen;

    if (!pDev->spriteInfo->sprite)
    {
        DeviceIntPtr it;

        pDev->spriteInfo->sprite = (SpritePtr)xcalloc(1, sizeof(SpriteRec));
        if (!pDev->spriteInfo->sprite)
            FatalError("InitializeSprite: failed to allocate sprite struct");

        /* We may have paired another device with this device before our
         * device had a actual sprite. We need to check for this and reset the
         * sprite field for all paired devices.
         *
         * The VCK is always paired with the VCP before the VCP has a sprite.
         */
        for (it = inputInfo.devices; it; it = it->next)
        {
            if (it->spriteInfo->paired == pDev)
                it->spriteInfo->sprite = pDev->spriteInfo->sprite;
        }
        if (inputInfo.keyboard->spriteInfo->paired == pDev)
            inputInfo.keyboard->spriteInfo->sprite = pDev->spriteInfo->sprite;
    }

    pSprite = pDev->spriteInfo->sprite;
    pDev->spriteInfo->spriteOwner = TRUE;

    pScreen = (pWin) ? pWin->drawable.pScreen : (ScreenPtr)NULL;
    pSprite->hot.pScreen = pScreen;
    pSprite->hotPhys.pScreen = pScreen;
    if (pScreen)
    {
        pSprite->hotPhys.x = pScreen->width / 2;
        pSprite->hotPhys.y = pScreen->height / 2;
        pSprite->hotLimits.x2 = pScreen->width;
        pSprite->hotLimits.y2 = pScreen->height;
    }

    pSprite->hot = pSprite->hotPhys;
    pSprite->win = pWin;

    if (pWin)
    {
        pSprite->current = wCursor(pWin);
        pSprite->current->refcnt++;
	pSprite->spriteTrace = (WindowPtr *)xcalloc(1, 32*sizeof(WindowPtr));
	if (!pSprite->spriteTrace)
	    FatalError("Failed to allocate spriteTrace");
	pSprite->spriteTraceSize = 32;

	RootWindow(pDev) = pWin;
	pSprite->spriteTraceGood = 1;

	pSprite->pEnqueueScreen = pScreen;
	pSprite->pDequeueScreen = pSprite->pEnqueueScreen;

    } else {
        pSprite->current = NullCursor;
	pSprite->spriteTrace = NULL;
	pSprite->spriteTraceSize = 0;
	pSprite->spriteTraceGood = 0;
	pSprite->pEnqueueScreen = screenInfo.screens[0];
	pSprite->pDequeueScreen = pSprite->pEnqueueScreen;
    }

    if (pScreen)
    {
        (*pScreen->RealizeCursor) ( pDev, pScreen, pSprite->current);
        (*pScreen->CursorLimits) ( pDev, pScreen, pSprite->current,
                                   &pSprite->hotLimits, &pSprite->physLimits);
        pSprite->confined = FALSE;

        (*pScreen->ConstrainCursor) (pDev, pScreen,
                                     &pSprite->physLimits);
        (*pScreen->SetCursorPosition) (pDev, pScreen, pSprite->hot.x,
                                       pSprite->hot.y,
                                       FALSE);
        (*pScreen->DisplayCursor) (pDev, pScreen, pSprite->current);
    }
#ifdef PANORAMIX
    if(!noPanoramiXExtension) {
        pSprite->hotLimits.x1 = -panoramiXdataPtr[0].x;
        pSprite->hotLimits.y1 = -panoramiXdataPtr[0].y;
        pSprite->hotLimits.x2 = PanoramiXPixWidth  - panoramiXdataPtr[0].x;
        pSprite->hotLimits.y2 = PanoramiXPixHeight - panoramiXdataPtr[0].y;
        pSprite->physLimits = pSprite->hotLimits;
        pSprite->confineWin = NullWindow;
        pSprite->hotShape = NullRegion;
        pSprite->screen = pScreen;
        /* gotta UNINIT these someplace */
        REGION_NULL(pScreen, &pSprite->Reg1);
        REGION_NULL(pScreen, &pSprite->Reg2);
    }
#endif
}

/**
 * Update the mouse sprite info when the server switches from a pScreen to another.
 * Otherwise, the pScreen of the mouse sprite is never updated when we switch
 * from a pScreen to another. Never updating the pScreen of the mouse sprite
 * implies that windows that are in pScreen whose pScreen->myNum >0 will never
 * get pointer events. This is  because in CheckMotion(), sprite.hotPhys.pScreen
 * always points to the first pScreen it has been set by
 * DefineInitialRootWindow().
 *
 * Calling this function is useful for use cases where the server
 * has more than one pScreen.
 * This function is similar to DefineInitialRootWindow() but it does not
 * reset the mouse pointer position.
 * @param win must be the new pScreen we are switching to.
 */
void
UpdateSpriteForScreen(DeviceIntPtr pDev, ScreenPtr pScreen)
{
    SpritePtr pSprite = NULL;
    WindowPtr win = NULL;
    if (!pScreen)
        return ;

    if (!pDev->spriteInfo->sprite)
        return;

    pSprite = pDev->spriteInfo->sprite;

    win = WindowTable[pScreen->myNum];

    pSprite->hotPhys.pScreen = pScreen;
    pSprite->hot = pSprite->hotPhys;
    pSprite->hotLimits.x2 = pScreen->width;
    pSprite->hotLimits.y2 = pScreen->height;
    pSprite->win = win;
    pSprite->current = wCursor (win);
    pSprite->current->refcnt++;
    pSprite->spriteTraceGood = 1;
    pSprite->spriteTrace[0] = win;
    (*pScreen->CursorLimits) (pDev,
                              pScreen,
                              pSprite->current,
                              &pSprite->hotLimits,
                              &pSprite->physLimits);
    pSprite->confined = FALSE;
    (*pScreen->ConstrainCursor) (pDev, pScreen, &pSprite->physLimits);
    (*pScreen->DisplayCursor) (pDev, pScreen, pSprite->current);

#ifdef PANORAMIX
    if(!noPanoramiXExtension) {
        pSprite->hotLimits.x1 = -panoramiXdataPtr[0].x;
        pSprite->hotLimits.y1 = -panoramiXdataPtr[0].y;
        pSprite->hotLimits.x2 = PanoramiXPixWidth  - panoramiXdataPtr[0].x;
        pSprite->hotLimits.y2 = PanoramiXPixHeight - panoramiXdataPtr[0].y;
        pSprite->physLimits = pSprite->hotLimits;
        pSprite->screen = pScreen;
    }
#endif
}

/*
 * This does not take any shortcuts, and even ignores its argument, since
 * it does not happen very often, and one has to walk up the tree since
 * this might be a newly instantiated cursor for an intermediate window
 * between the one the pointer is in and the one that the last cursor was
 * instantiated from.
 */
void
WindowHasNewCursor(WindowPtr pWin)
{
    DeviceIntPtr pDev;

    for(pDev = inputInfo.devices; pDev; pDev = pDev->next)
        if (DevHasCursor(pDev))
            PostNewCursor(pDev);
}

void
NewCurrentScreen(DeviceIntPtr pDev, ScreenPtr newScreen, int x, int y)
{
    SpritePtr pSprite = pDev->spriteInfo->sprite;

    pSprite->hotPhys.x = x;
    pSprite->hotPhys.y = y;
#ifdef PANORAMIX
    if(!noPanoramiXExtension) {
	pSprite->hotPhys.x += panoramiXdataPtr[newScreen->myNum].x -
			    panoramiXdataPtr[0].x;
	pSprite->hotPhys.y += panoramiXdataPtr[newScreen->myNum].y -
			    panoramiXdataPtr[0].y;
	if (newScreen != pSprite->screen) {
	    pSprite->screen = newScreen;
	    /* Make sure we tell the DDX to update its copy of the screen */
	    if(pSprite->confineWin)
		XineramaConfineCursorToWindow(pDev,
                        pSprite->confineWin, TRUE);
	    else
		XineramaConfineCursorToWindow(pDev, WindowTable[0], TRUE);
	    /* if the pointer wasn't confined, the DDX won't get
	       told of the pointer warp so we reposition it here */
	    if(!syncEvents.playingEvents)
		(*pSprite->screen->SetCursorPosition)(
                                                      pDev,
                                                      pSprite->screen,
		    pSprite->hotPhys.x + panoramiXdataPtr[0].x -
			panoramiXdataPtr[pSprite->screen->myNum].x,
		    pSprite->hotPhys.y + panoramiXdataPtr[0].y -
			panoramiXdataPtr[pSprite->screen->myNum].y, FALSE);
	}
    } else
#endif
    if (newScreen != pSprite->hotPhys.pScreen)
	ConfineCursorToWindow(pDev, WindowTable[newScreen->myNum],
                TRUE, FALSE);
}

#ifdef PANORAMIX

static Bool
XineramaPointInWindowIsVisible(
    WindowPtr pWin,
    int x,
    int y
)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    BoxRec box;
    int i, xoff, yoff;

    if (!pWin->realized) return FALSE;

    if (POINT_IN_REGION(pScreen, &pWin->borderClip, x, y, &box))
        return TRUE;

    if(!XineramaSetWindowPntrs(inputInfo.pointer, pWin)) return FALSE;

    xoff = x + panoramiXdataPtr[0].x;
    yoff = y + panoramiXdataPtr[0].y;

    for(i = 1; i < PanoramiXNumScreens; i++) {
	pWin = inputInfo.pointer->spriteInfo->sprite->windows[i];
	pScreen = pWin->drawable.pScreen;
	x = xoff - panoramiXdataPtr[i].x;
	y = yoff - panoramiXdataPtr[i].y;

	if(POINT_IN_REGION(pScreen, &pWin->borderClip, x, y, &box)
	   && (!wInputShape(pWin) ||
	       POINT_IN_REGION(pWin->drawable.pScreen,
			       wInputShape(pWin),
			       x - pWin->drawable.x,
			       y - pWin->drawable.y, &box)))
            return TRUE;

    }

    return FALSE;
}

static int
XineramaWarpPointer(ClientPtr client)
{
    WindowPtr	dest = NULL;
    int		x, y, rc;
    SpritePtr   pSprite = PickPointer(client)->spriteInfo->sprite;

    REQUEST(xWarpPointerReq);


    if (stuff->dstWid != None) {
	rc = dixLookupWindow(&dest, stuff->dstWid, client, DixReadAccess);
	if (rc != Success)
	    return rc;
    }
    x = pSprite->hotPhys.x;
    y = pSprite->hotPhys.y;

    if (stuff->srcWid != None)
    {
	int     winX, winY;
	XID	winID = stuff->srcWid;
        WindowPtr source;

	rc = dixLookupWindow(&source, winID, client, DixReadAccess);
	if (rc != Success)
	    return rc;

	winX = source->drawable.x;
	winY = source->drawable.y;
	if(source == WindowTable[0]) {
	    winX -= panoramiXdataPtr[0].x;
	    winY -= panoramiXdataPtr[0].y;
	}
	if (x < winX + stuff->srcX ||
	    y < winY + stuff->srcY ||
	    (stuff->srcWidth != 0 &&
	     winX + stuff->srcX + (int)stuff->srcWidth < x) ||
	    (stuff->srcHeight != 0 &&
	     winY + stuff->srcY + (int)stuff->srcHeight < y) ||
	    !XineramaPointInWindowIsVisible(source, x, y))
	    return Success;
    }
    if (dest) {
	x = dest->drawable.x;
	y = dest->drawable.y;
	if(dest == WindowTable[0]) {
	    x -= panoramiXdataPtr[0].x;
	    y -= panoramiXdataPtr[0].y;
	}
    }

    x += stuff->dstX;
    y += stuff->dstY;

    if (x < pSprite->physLimits.x1)
	x = pSprite->physLimits.x1;
    else if (x >= pSprite->physLimits.x2)
	x = pSprite->physLimits.x2 - 1;
    if (y < pSprite->physLimits.y1)
	y = pSprite->physLimits.y1;
    else if (y >= pSprite->physLimits.y2)
	y = pSprite->physLimits.y2 - 1;
    if (pSprite->hotShape)
	ConfineToShape(PickPointer(client), pSprite->hotShape, &x, &y);

    XineramaSetCursorPosition(PickPointer(client), x, y, TRUE);

    return Success;
}

#endif


/**
 * Server-side protocol handling for WarpPointer request.
 * Warps the cursor position to the coordinates given in the request.
 */
int
ProcWarpPointer(ClientPtr client)
{
    WindowPtr	dest = NULL;
    int		x, y, rc;
    ScreenPtr	newScreen;
    DeviceIntPtr dev, tmp;
    SpritePtr   pSprite;

    REQUEST(xWarpPointerReq);
    REQUEST_SIZE_MATCH(xWarpPointerReq);

    dev = PickPointer(client);

    for (tmp = inputInfo.devices; tmp; tmp = tmp->next) {
        if ((tmp == dev) || (!IsMaster(tmp) && tmp->u.master == dev)) {
	    rc = XaceHook(XACE_DEVICE_ACCESS, client, dev, DixWriteAccess);
	    if (rc != Success)
		return rc;
	}
    }

    if (dev->u.lastSlave)
        dev = dev->u.lastSlave;
    pSprite = dev->spriteInfo->sprite;

#ifdef PANORAMIX
    if(!noPanoramiXExtension)
	return XineramaWarpPointer(client);
#endif

    if (stuff->dstWid != None) {
	rc = dixLookupWindow(&dest, stuff->dstWid, client, DixGetAttrAccess);
	if (rc != Success)
	    return rc;
    }
    x = pSprite->hotPhys.x;
    y = pSprite->hotPhys.y;

    if (stuff->srcWid != None)
    {
	int     winX, winY;
	XID	winID = stuff->srcWid;
        WindowPtr source;

	rc = dixLookupWindow(&source, winID, client, DixGetAttrAccess);
	if (rc != Success)
	    return rc;

	winX = source->drawable.x;
	winY = source->drawable.y;
	if (source->drawable.pScreen != pSprite->hotPhys.pScreen ||
	    x < winX + stuff->srcX ||
	    y < winY + stuff->srcY ||
	    (stuff->srcWidth != 0 &&
	     winX + stuff->srcX + (int)stuff->srcWidth < x) ||
	    (stuff->srcHeight != 0 &&
	     winY + stuff->srcY + (int)stuff->srcHeight < y) ||
	    !PointInWindowIsVisible(source, x, y))
	    return Success;
    }
    if (dest)
    {
	x = dest->drawable.x;
	y = dest->drawable.y;
	newScreen = dest->drawable.pScreen;
    } else
	newScreen = pSprite->hotPhys.pScreen;

    x += stuff->dstX;
    y += stuff->dstY;

    if (x < 0)
	x = 0;
    else if (x >= newScreen->width)
	x = newScreen->width - 1;
    if (y < 0)
	y = 0;
    else if (y >= newScreen->height)
	y = newScreen->height - 1;

    if (newScreen == pSprite->hotPhys.pScreen)
    {
	if (x < pSprite->physLimits.x1)
	    x = pSprite->physLimits.x1;
	else if (x >= pSprite->physLimits.x2)
	    x = pSprite->physLimits.x2 - 1;
	if (y < pSprite->physLimits.y1)
	    y = pSprite->physLimits.y1;
	else if (y >= pSprite->physLimits.y2)
	    y = pSprite->physLimits.y2 - 1;
	if (pSprite->hotShape)
	    ConfineToShape(dev, pSprite->hotShape, &x, &y);
        (*newScreen->SetCursorPosition)(dev, newScreen, x, y, TRUE);
    }
    else if (!PointerConfinedToScreen(dev))
    {
	NewCurrentScreen(dev, newScreen, x, y);
    }
    return Success;
}

static Bool
BorderSizeNotEmpty(DeviceIntPtr pDev, WindowPtr pWin)
{
     if(REGION_NOTEMPTY(pDev->spriteInfo->sprite->hotPhys.pScreen, &pWin->borderSize))
	return TRUE;

#ifdef PANORAMIX
     if(!noPanoramiXExtension && XineramaSetWindowPntrs(pDev, pWin)) {
	int i;

	for(i = 1; i < PanoramiXNumScreens; i++) {
	    if(REGION_NOTEMPTY(pDev->spriteInfo->sprite->screen,
                        &pDev->spriteInfo->sprite->windows[i]->borderSize))
		return TRUE;
	}
     }
#endif
     return FALSE;
}

/**
 * "CheckPassiveGrabsOnWindow" checks to see if the event passed in causes a
 * passive grab set on the window to be activated.
 * If a passive grab is activated, the event will be delivered to the client.
 *
 * @param pWin The window that may be subject to a passive grab.
 * @param device Device that caused the event.
 * @param event The current device event.
 * @param checkCore Check for core grabs too.
 */

static Bool
CheckPassiveGrabsOnWindow(
    WindowPtr pWin,
    DeviceIntPtr device,
    DeviceEvent *event,
    BOOL checkCore)
{
    GrabPtr grab = wPassiveGrabs(pWin);
    GrabRec tempGrab;
    GrabInfoPtr grabinfo;
#define CORE_MATCH      0x1
#define XI_MATCH        0x2
#define XI2_MATCH        0x4
    int match = 0;

    if (device->deviceGrab.grab)
        return FALSE;

    if (!grab)
	return FALSE;
    /* Fill out the grab details, but leave the type for later before
     * comparing */
    tempGrab.window = pWin;
    tempGrab.device = device;
    tempGrab.detail.exact = event->detail.key;
    tempGrab.detail.pMask = NULL;
    tempGrab.modifiersDetail.pMask = NULL;
    tempGrab.next = NULL;
    for (; grab; grab = grab->next)
    {
	DeviceIntPtr	gdev;
	XkbSrvInfoPtr	xkbi = NULL;
	Mask		mask = 0;

	gdev= grab->modifierDevice;
        if (grab->grabtype == GRABTYPE_CORE)
        {
            if (IsPointerDevice(device))
                gdev = GetPairedDevice(device);
            else
                gdev = device;
        } else if (grab->grabtype == GRABTYPE_XI2)
        {
            /* if the device is an attached slave device, gdev must be the
             * attached master keyboard. Since the slave may have been
             * reattached after the grab, the modifier device may not be the
             * same. */
            if (!IsMaster(grab->device) && device->u.master)
                gdev = GetMaster(device, MASTER_KEYBOARD);
        }


        if (gdev && gdev->key)
            xkbi= gdev->key->xkbInfo;
	tempGrab.modifierDevice = grab->modifierDevice;
        tempGrab.modifiersDetail.exact = xkbi ? xkbi->state.grab_mods : 0;

        /* Check for XI2 and XI grabs first */
        tempGrab.type = GetXI2Type((InternalEvent*)event);
        tempGrab.grabtype = GRABTYPE_XI2;
        if (GrabMatchesSecond(&tempGrab, grab, FALSE))
            match = XI2_MATCH;

        tempGrab.detail.exact = event->detail.key;
        if (!match)
        {
            tempGrab.type = GetXIType((InternalEvent*)event);
            tempGrab.grabtype = GRABTYPE_XI;
            if (GrabMatchesSecond(&tempGrab, grab, FALSE))
                match = XI_MATCH;
        }

        /* Check for a core grab (ignore the device when comparing) */
        if (!match && checkCore)
        {
            tempGrab.grabtype = GRABTYPE_CORE;
            if ((tempGrab.type = GetCoreType((InternalEvent*)event)) &&
                (GrabMatchesSecond(&tempGrab, grab, TRUE)))
                match = CORE_MATCH;
        }

        if (match && (!grab->confineTo ||
	     (grab->confineTo->realized &&
				BorderSizeNotEmpty(device, grab->confineTo))))
	{
            int rc, count = 0;
            xEvent *xE = NULL;
            xEvent core;

            event->corestate &= 0x1f00;
            event->corestate |= tempGrab.modifiersDetail.exact & (~0x1f00);
            grabinfo = &device->deviceGrab;
            /* In some cases a passive core grab may exist, but the client
             * already has a core grab on some other device. In this case we
             * must not get the grab, otherwise we may never ungrab the
             * device.
             */

            if (grab->grabtype == GRABTYPE_CORE)
            {
                DeviceIntPtr other;
                BOOL interfering = FALSE;

                /* A passive grab may have been created for a different device
                   than it is assigned to at this point in time.
                   Update the grab's device and modifier device to reflect the
                   current state.
                   Since XGrabDeviceButton requires to specify the
                   modifierDevice explicitly, we don't override this choice.
                   */
                if (tempGrab.type < GenericEvent)
                {
                    grab->device = device;
                    grab->modifierDevice = GetPairedDevice(device);
                }

                for (other = inputInfo.devices; other; other = other->next)
                {
                    GrabPtr othergrab = other->deviceGrab.grab;
                    if (othergrab && othergrab->grabtype == GRABTYPE_CORE &&
                        SameClient(grab, rClient(othergrab)) &&
                        ((IsPointerDevice(grab->device) &&
                         IsPointerDevice(othergrab->device)) ||
                         (IsKeyboardDevice(grab->device) &&
                          IsKeyboardDevice(othergrab->device))))
                    {
                        interfering = TRUE;
                        break;
                    }
                }
                if (interfering)
                    continue;
            }


            if (match & CORE_MATCH)
            {
                rc = EventToCore((InternalEvent*)event, &core);
                if (rc != Success)
                {
                    if (rc != BadMatch)
                        ErrorF("[dix] %s: core conversion failed in CPGFW "
                                "(%d, %d).\n", device->name, event->type, rc);
                    continue;
                }
                xE = &core;
                count = 1;
                mask = grab->eventMask;
            } else if (match & XI2_MATCH)
            {
                rc = EventToXI2((InternalEvent*)event, &xE);
                if (rc != Success)
                {
                    if (rc != BadMatch)
                        ErrorF("[dix] %s: XI2 conversion failed in CPGFW "
                                "(%d, %d).\n", device->name, event->type, rc);
                    continue;
                }
                count = 1;

                /* FIXME: EventToXI2 returns NULL for enter events, so
                 * dereferencing the event is bad. Internal event types are
                 * aligned with core events, so the else clause is valid.
                 * long-term we should use internal events for enter/focus
                 * as well */
                if (xE)
                    mask = grab->xi2mask[device->id][((xGenericEvent*)xE)->evtype/8];
                else if (event->type == XI_Enter || event->type == XI_FocusIn)
                    mask = grab->xi2mask[device->id][event->type/8];
            } else
            {
                rc = EventToXI((InternalEvent*)event, &xE, &count);
                if (rc != Success)
                {
                    if (rc != BadMatch)
                        ErrorF("[dix] %s: XI conversion failed in CPGFW "
                                "(%d, %d).\n", device->name, event->type, rc);
                    continue;
                }
                mask = grab->eventMask;
            }

	    (*grabinfo->ActivateGrab)(device, grab, currentTime, TRUE);

            if (xE)
            {
                FixUpEventFromWindow(device, xE, grab->window, None, TRUE);

                TryClientEvents(rClient(grab), device, xE, count, mask,
                                       GetEventFilter(device, xE), grab);
            }

	    if (grabinfo->sync.state == FROZEN_NO_EVENT)
	    {
                if (!grabinfo->sync.event)
                    grabinfo->sync.event = xcalloc(1, sizeof(InternalEvent));
                *grabinfo->sync.event = *event;
		grabinfo->sync.state = FROZEN_WITH_EVENT;
            }

            if (match & (XI_MATCH | XI2_MATCH))
                xfree(xE); /* on core match xE == &core */
	    return TRUE;
	}
    }
    return FALSE;
#undef CORE_MATCH
#undef XI_MATCH
#undef XI2_MATCH
}

/**
 * CheckDeviceGrabs handles both keyboard and pointer events that may cause
 * a passive grab to be activated.
 *
 * If the event is a keyboard event, the ancestors of the focus window are
 * traced down and tried to see if they have any passive grabs to be
 * activated.  If the focus window itself is reached and it's descendants
 * contain the pointer, the ancestors of the window that the pointer is in
 * are then traced down starting at the focus window, otherwise no grabs are
 * activated.
 * If the event is a pointer event, the ancestors of the window that the
 * pointer is in are traced down starting at the root until CheckPassiveGrabs
 * causes a passive grab to activate or all the windows are
 * tried. PRH
 *
 * If a grab is activated, the event has been sent to the client already!
 *
 * The event we pass in must always be an XI event. From this, we then emulate
 * the core event and then check for grabs.
 *
 * @param device The device that caused the event.
 * @param xE The event to handle (Device{Button|Key}Press).
 * @param count Number of events in list.
 * @return TRUE if a grab has been activated or false otherwise.
*/

Bool
CheckDeviceGrabs(DeviceIntPtr device, DeviceEvent *event, int checkFirst)
{
    int i;
    WindowPtr pWin = NULL;
    FocusClassPtr focus = IsPointerEvent((InternalEvent*)event) ? NULL : device->focus;
    BOOL sendCore = (IsMaster(device) && device->coreEvents);

    if (event->type != ET_ButtonPress &&
        event->type != ET_KeyPress)
        return FALSE;

    if (event->type == ET_ButtonPress
        && (device->button->buttonsDown != 1))
	return FALSE;

    i = checkFirst;

    if (focus)
    {
	for (; i < focus->traceGood; i++)
	{
	    pWin = focus->trace[i];
	    if (pWin->optional &&
	        CheckPassiveGrabsOnWindow(pWin, device, event, sendCore))
		return TRUE;
	}

	if ((focus->win == NoneWin) ||
	    (i >= device->spriteInfo->sprite->spriteTraceGood) ||
	    ((i > checkFirst) &&
             (pWin != device->spriteInfo->sprite->spriteTrace[i-1])))
	    return FALSE;
    }

    for (; i < device->spriteInfo->sprite->spriteTraceGood; i++)
    {
	pWin = device->spriteInfo->sprite->spriteTrace[i];
	if (pWin->optional &&
	    CheckPassiveGrabsOnWindow(pWin, device, event, sendCore))
	    return TRUE;
    }

    return FALSE;
}

/**
 * Called for keyboard events to deliver event to whatever client owns the
 * focus.
 *
 * The event is delivered to the keyboard's focus window, the root window or
 * to the window owning the input focus.
 *
 * @param keybd The keyboard originating the event.
 * @param event The event, not yet in wire format.
 * @param window Window underneath the sprite.
 */
void
DeliverFocusedEvent(DeviceIntPtr keybd, InternalEvent *event, WindowPtr window)
{
    DeviceIntPtr ptr;
    WindowPtr focus = keybd->focus->win;
    BOOL sendCore = (IsMaster(keybd) && keybd->coreEvents);
    xEvent core;
    xEvent *xE = NULL, *xi2 = NULL;
    int count, rc;
    int deliveries = 0;

    if (focus == FollowKeyboardWin)
	focus = inputInfo.keyboard->focus->win;
    if (!focus)
	return;
    if (focus == PointerRootWin)
    {
	DeliverDeviceEvents(window, event, NullGrab, NullWindow, keybd);
	return;
    }
    if ((focus == window) || IsParent(focus, window))
    {
	if (DeliverDeviceEvents(window, event, NullGrab, focus, keybd))
	    return;
    }

    /* just deliver it to the focus window */
    ptr = GetPairedDevice(keybd);


    rc = EventToXI2(event, &xi2);
    if (rc == Success)
    {
        /* XXX: XACE */
        int filter = GetEventFilter(keybd, xi2);
        FixUpEventFromWindow(ptr, xi2, focus, None, FALSE);
        deliveries = DeliverEventsToWindow(keybd, focus, xi2, 1,
                                           filter, NullGrab);
        if (deliveries > 0)
            goto unwind;
    } else if (rc != BadMatch)
        ErrorF("[dix] %s: XI2 conversion failed in DFE (%d, %d). Skipping delivery.\n",
               keybd->name, event->any.type, rc);

    rc = EventToXI(event, &xE, &count);
    if (rc == Success &&
        XaceHook(XACE_SEND_ACCESS, NULL, keybd, focus, xE, count) == Success)
    {
        FixUpEventFromWindow(ptr, xE, focus, None, FALSE);
        deliveries = DeliverEventsToWindow(keybd, focus, xE, count,
                GetEventFilter(keybd, xE),
                NullGrab);

        if (deliveries > 0)
            goto unwind;
    } else if (rc != BadMatch)
        ErrorF("[dix] %s: XI conversion failed in DFE (%d, %d). Skipping delivery.\n",
               keybd->name, event->any.type, rc);

    if (sendCore)
    {
        rc = EventToCore(event, &core);
        if (rc == Success) {
            if (XaceHook(XACE_SEND_ACCESS, NULL, keybd, focus, &core, 1) == Success) {
                FixUpEventFromWindow(keybd, &core, focus, None, FALSE);
                deliveries = DeliverEventsToWindow(keybd, focus, &core, 1,
                                                   GetEventFilter(keybd, &core),
                                                   NullGrab);
            }
        } else if (rc != BadMatch)
            ErrorF("[dix] %s: core conversion failed DFE (%d, %d). Skipping delivery.\n",
                    keybd->name, event->any.type, rc);
    }

unwind:
    if (xE)
        xfree(xE);
    if (xi2)
        xfree(xi2);
    return;
}

/**
 * Deliver an event from a device that is currently grabbed. Uses
 * DeliverDeviceEvents() for further delivery if a ownerEvents is set on the
 * grab. If not, TryClientEvents() is used.
 *
 * @param deactivateGrab True if the device's grab should be deactivated.
 */
void
DeliverGrabbedEvent(InternalEvent *event, DeviceIntPtr thisDev,
                    Bool deactivateGrab)
{
    GrabPtr grab;
    GrabInfoPtr grabinfo;
    int deliveries = 0;
    DeviceIntPtr dev;
    SpritePtr pSprite = thisDev->spriteInfo->sprite;
    BOOL sendCore = FALSE;
    int rc, count = 0;
    xEvent *xi = NULL;
    xEvent *xi2 = NULL;

    grabinfo = &thisDev->deviceGrab;
    grab = grabinfo->grab;

    if (grab->ownerEvents)
    {
	WindowPtr focus;

        /* Hack: Some pointer device have a focus class. So we need to check
         * for the type of event, to see if we really want to deliver it to
         * the focus window. For pointer events, the answer is no.
         */
        if (IsPointerEvent(event))
            focus = PointerRootWin;
        else if (thisDev->focus)
	{
	    focus = thisDev->focus->win;
	    if (focus == FollowKeyboardWin)
		focus = inputInfo.keyboard->focus->win;
	}
	else
	    focus = PointerRootWin;
	if (focus == PointerRootWin)
	    deliveries = DeliverDeviceEvents(pSprite->win, event, grab,
                                             NullWindow, thisDev);
	else if (focus && (focus == pSprite->win ||
                    IsParent(focus, pSprite->win)))
	    deliveries = DeliverDeviceEvents(pSprite->win, event, grab, focus,
					     thisDev);
	else if (focus)
	    deliveries = DeliverDeviceEvents(focus, event, grab, focus,
					     thisDev);
    }
    if (!deliveries)
    {
        Mask mask;

        /* XXX: In theory, we could pass the internal events through to
         * everything and only convert just before hitting the wire. We can't
         * do that yet, so DGE is the last stop for internal events. From here
         * onwards, we deal with core/XI events.
         */

        mask = grab->eventMask;

        sendCore = (IsMaster(thisDev) && thisDev->coreEvents);
        /* try core event */
        if (sendCore && grab->grabtype == GRABTYPE_CORE)
        {
            xEvent core;

            rc = EventToCore(event, &core);
            if (rc == Success)
            {
                FixUpEventFromWindow(thisDev, &core, grab->window,
                        None, TRUE);
                if (XaceHook(XACE_SEND_ACCESS, 0, thisDev,
                            grab->window, &core, 1) ||
                        XaceHook(XACE_RECEIVE_ACCESS, rClient(grab),
                            grab->window, &core, 1))
                    deliveries = 1; /* don't send, but pretend we did */
                else if (!IsInterferingGrab(rClient(grab), thisDev, &core))
                {
                    deliveries = TryClientEvents(rClient(grab), thisDev,
                            &core, 1, mask,
                            GetEventFilter(thisDev, &core),
                            grab);
                }
            } else if (rc != BadMatch)
                ErrorF("[dix] DeliverGrabbedEvent. Core conversion failed.\n");
        }

        if (!deliveries)
        {
            rc = EventToXI2(event, &xi2);
            if (rc == Success)
            {
                int evtype = ((xGenericEvent*)xi2)->evtype;
                mask = grab->xi2mask[XIAllDevices][evtype/8] |
                    grab->xi2mask[XIAllMasterDevices][evtype/8] |
                    grab->xi2mask[thisDev->id][evtype/8];
                /* try XI2 event */
                FixUpEventFromWindow(thisDev, xi2, grab->window, None, TRUE);
                /* XXX: XACE */
                deliveries = TryClientEvents(rClient(grab), thisDev, xi2, 1, mask,
                        GetEventFilter(thisDev, xi2), grab);
            } else if (rc != BadMatch)
                ErrorF("[dix] %s: XI2 conversion failed in DGE (%d, %d). Skipping delivery.\n",
                        thisDev->name, event->any.type, rc);
        }

        if (!deliveries)
        {
            rc = EventToXI(event, &xi, &count);
            if (rc == Success)
            {
                /* try XI event */
                if (grabinfo->fromPassiveGrab  &&
                        grabinfo->implicitGrab)
                    mask = grab->deviceMask;
                else
                    mask = grab->eventMask;

                FixUpEventFromWindow(thisDev, xi, grab->window,
                        None, TRUE);

                if (XaceHook(XACE_SEND_ACCESS, 0, thisDev,
                            grab->window, xi, count) ||
                        XaceHook(XACE_RECEIVE_ACCESS, rClient(grab),
                            grab->window, xi, count))
                    deliveries = 1; /* don't send, but pretend we did */
                else
                {
                    deliveries =
                        TryClientEvents(rClient(grab), thisDev,
                                xi, count,
                                mask,
                                GetEventFilter(thisDev, xi),
                                grab);
                }
            } else if (rc != BadMatch)
                ErrorF("[dix] %s: XI conversion failed in DGE (%d, %d). Skipping delivery.\n",
                        thisDev->name, event->any.type, rc);
        }

        if (deliveries && (event->any.type == ET_Motion))
            thisDev->valuator->motionHintWindow = grab->window;
    }
    if (deliveries && !deactivateGrab && event->any.type != ET_Motion)
    {
	switch (grabinfo->sync.state)
	{
	case FREEZE_BOTH_NEXT_EVENT:
	    for (dev = inputInfo.devices; dev; dev = dev->next)
	    {
		if (dev == thisDev)
		    continue;
		FreezeThaw(dev, TRUE);
		if ((dev->deviceGrab.sync.state == FREEZE_BOTH_NEXT_EVENT) &&
		    (CLIENT_BITS(grab->resource) ==
		     CLIENT_BITS(dev->deviceGrab.sync.other->resource)))
		    dev->deviceGrab.sync.state = FROZEN_NO_EVENT;
		else
                    dev->deviceGrab.sync.other = grab;
	    }
	    /* fall through */
	case FREEZE_NEXT_EVENT:
	    grabinfo->sync.state = FROZEN_WITH_EVENT;
	    FreezeThaw(thisDev, TRUE);
	    if (!grabinfo->sync.event)
		grabinfo->sync.event = xcalloc(1, sizeof(InternalEvent));
	    *grabinfo->sync.event = event->device_event;
	    break;
	}
    }

    if (xi)
        xfree(xi);
    if (xi2)
        xfree(xi2);
}

/* This function is used to set the key pressed or key released state -
   this is only used when the pressing of keys does not cause
   the device's processInputProc to be called, as in for example Mouse Keys.
*/
void
FixKeyState (DeviceEvent *event, DeviceIntPtr keybd)
{
    int             key, bit;
    BYTE   *kptr;
    KeyClassPtr keyc = keybd->key;

    key = event->detail.key;
    kptr = &keyc->down[key >> 3];
    bit = 1 << (key & 7);

    if (event->type == ET_KeyPress) {
	DebugF("FixKeyState: Key %d %s\n",key,
               ((event->type == ET_KeyPress) ? "down" : "up"));
    }

    if (event->type == ET_KeyPress)
	    *kptr |= bit;
    else if (event->type == ET_KeyRelease)
	    *kptr &= ~bit;
    else
        FatalError("Impossible keyboard event");
}

#define AtMostOneClient \
	(SubstructureRedirectMask | ResizeRedirectMask | ButtonPressMask)
#define ManagerMask \
	(SubstructureRedirectMask | ResizeRedirectMask)

/**
 * Recalculate which events may be deliverable for the given window.
 * Recalculated mask is used for quicker determination which events may be
 * delivered to a window.
 *
 * The otherEventMasks on a WindowOptional is the combination of all event
 * masks set by all clients on the window.
 * deliverableEventMask is the combination of the eventMask and the
 * otherEventMask plus the events that may be propagated to the parent.
 *
 * Traverses to siblings and parents of the window.
 */
void
RecalculateDeliverableEvents(WindowPtr pWin)
{
    OtherClients *others;
    WindowPtr pChild;

    pChild = pWin;
    while (1)
    {
	if (pChild->optional)
	{
	    pChild->optional->otherEventMasks = 0;
	    for (others = wOtherClients(pChild); others; others = others->next)
	    {
		pChild->optional->otherEventMasks |= others->mask;
	    }
	}
	pChild->deliverableEvents = pChild->eventMask|
				    wOtherEventMasks(pChild);
	if (pChild->parent)
	    pChild->deliverableEvents |=
		(pChild->parent->deliverableEvents &
		 ~wDontPropagateMask(pChild) & PropagateMask);
	if (pChild->firstChild)
	{
	    pChild = pChild->firstChild;
	    continue;
	}
	while (!pChild->nextSib && (pChild != pWin))
	    pChild = pChild->parent;
	if (pChild == pWin)
	    break;
	pChild = pChild->nextSib;
    }
}

/**
 *
 *  \param value must conform to DeleteType
 */
int
OtherClientGone(pointer value, XID id)
{
    OtherClientsPtr other, prev;
    WindowPtr pWin = (WindowPtr)value;

    prev = 0;
    for (other = wOtherClients(pWin); other; other = other->next)
    {
	if (other->resource == id)
	{
	    if (prev)
		prev->next = other->next;
	    else
	    {
		if (!(pWin->optional->otherClients = other->next))
		    CheckWindowOptionalNeed (pWin);
	    }
	    xfree(other);
	    RecalculateDeliverableEvents(pWin);
	    return(Success);
	}
	prev = other;
    }
    FatalError("client not on event list");
    /*NOTREACHED*/
    return -1; /* make compiler happy */
}

int
EventSelectForWindow(WindowPtr pWin, ClientPtr client, Mask mask)
{
    Mask check;
    OtherClients * others;
    DeviceIntPtr dev;
    int rc;

    if (mask & ~AllEventMasks)
    {
	client->errorValue = mask;
	return BadValue;
    }
    check = (mask & ManagerMask);
    if (check) {
	rc = XaceHook(XACE_RESOURCE_ACCESS, client, pWin->drawable.id,
		      RT_WINDOW, pWin, RT_NONE, NULL, DixManageAccess);
	if (rc != Success)
	    return rc;
    }
    check = (mask & AtMostOneClient);
    if (check & (pWin->eventMask|wOtherEventMasks(pWin)))
    {				       /* It is illegal for two different
				          clients to select on any of the
				          events for AtMostOneClient. However,
				          it is OK, for some client to
				          continue selecting on one of those
				          events.  */
	if ((wClient(pWin) != client) && (check & pWin->eventMask))
	    return BadAccess;
	for (others = wOtherClients (pWin); others; others = others->next)
	{
	    if (!SameClient(others, client) && (check & others->mask))
		return BadAccess;
	}
    }
    if (wClient (pWin) == client)
    {
	check = pWin->eventMask;
	pWin->eventMask = mask;
    }
    else
    {
	for (others = wOtherClients (pWin); others; others = others->next)
	{
	    if (SameClient(others, client))
	    {
		check = others->mask;
		if (mask == 0)
		{
		    FreeResource(others->resource, RT_NONE);
		    return Success;
		}
		else
		    others->mask = mask;
		goto maskSet;
	    }
	}
	check = 0;
	if (!pWin->optional && !MakeWindowOptional (pWin))
	    return BadAlloc;
	others = xalloc(sizeof(OtherClients));
	if (!others)
	    return BadAlloc;
	others->mask = mask;
	others->resource = FakeClientID(client->index);
	others->next = pWin->optional->otherClients;
	pWin->optional->otherClients = others;
	if (!AddResource(others->resource, RT_OTHERCLIENT, (pointer)pWin))
	    return BadAlloc;
    }
maskSet:
    if ((mask & PointerMotionHintMask) && !(check & PointerMotionHintMask))
    {
        for (dev = inputInfo.devices; dev; dev = dev->next)
        {
            if (dev->valuator && dev->valuator->motionHintWindow == pWin)
                dev->valuator->motionHintWindow = NullWindow;
        }
    }
    RecalculateDeliverableEvents(pWin);
    return Success;
}

int
EventSuppressForWindow(WindowPtr pWin, ClientPtr client,
                       Mask mask, Bool *checkOptional)
{
    int i, free;

    if (mask & ~PropagateMask)
    {
	client->errorValue = mask;
	return BadValue;
    }
    if (pWin->dontPropagate)
	DontPropagateRefCnts[pWin->dontPropagate]--;
    if (!mask)
	i = 0;
    else
    {
	for (i = DNPMCOUNT, free = 0; --i > 0; )
	{
	    if (!DontPropagateRefCnts[i])
		free = i;
	    else if (mask == DontPropagateMasks[i])
		break;
	}
	if (!i && free)
	{
	    i = free;
	    DontPropagateMasks[i] = mask;
	}
    }
    if (i || !mask)
    {
	pWin->dontPropagate = i;
	if (i)
	    DontPropagateRefCnts[i]++;
	if (pWin->optional)
	{
	    pWin->optional->dontPropagateMask = mask;
	    *checkOptional = TRUE;
	}
    }
    else
    {
	if (!pWin->optional && !MakeWindowOptional (pWin))
	{
	    if (pWin->dontPropagate)
		DontPropagateRefCnts[pWin->dontPropagate]++;
	    return BadAlloc;
	}
	pWin->dontPropagate = 0;
        pWin->optional->dontPropagateMask = mask;
    }
    RecalculateDeliverableEvents(pWin);
    return Success;
}

/**
 * Assembles an EnterNotify or LeaveNotify and sends it event to the client.
 * Uses the paired keyboard to get some additional information.
 */
void
CoreEnterLeaveEvent(
    DeviceIntPtr mouse,
    int type,
    int mode,
    int detail,
    WindowPtr pWin,
    Window child)
{
    xEvent              event;
    WindowPtr		focus;
    DeviceIntPtr        keybd;
    GrabPtr	        grab = mouse->deviceGrab.grab;
    Mask		mask;

    keybd = GetPairedDevice(mouse);

    if ((pWin == mouse->valuator->motionHintWindow) &&
	(detail != NotifyInferior))
	mouse->valuator->motionHintWindow = NullWindow;
    if (grab)
    {
	mask = (pWin == grab->window) ? grab->eventMask : 0;
	if (grab->ownerEvents)
	    mask |= EventMaskForClient(pWin, rClient(grab));
    }
    else
    {
	mask = pWin->eventMask | wOtherEventMasks(pWin);
    }

    memset(&event, 0, sizeof(xEvent));
    event.u.u.type = type;
    event.u.u.detail = detail;
    event.u.enterLeave.time = currentTime.milliseconds;
    event.u.enterLeave.rootX = mouse->spriteInfo->sprite->hot.x;
    event.u.enterLeave.rootY = mouse->spriteInfo->sprite->hot.y;
    /* Counts on the same initial structure of crossing & button events! */
    FixUpEventFromWindow(mouse, &event, pWin, None, FALSE);
    /* Enter/Leave events always set child */
    event.u.enterLeave.child = child;
    event.u.enterLeave.flags = event.u.keyButtonPointer.sameScreen ?
        ELFlagSameScreen : 0;
    event.u.enterLeave.state = mouse->button ? (mouse->button->state & 0x1f00) : 0;
    if (keybd)
        event.u.enterLeave.state |=
                XkbGrabStateFromRec(&keybd->key->xkbInfo->state);
    event.u.enterLeave.mode = mode;
    focus = (keybd) ? keybd->focus->win : None;
    if ((focus != NoneWin) &&
            ((pWin == focus) || (focus == PointerRootWin) ||
             IsParent(focus, pWin)))
        event.u.enterLeave.flags |= ELFlagFocus;

    if ((mask & GetEventFilter(mouse, &event)))
    {
        if (grab)
            TryClientEvents(rClient(grab), mouse, &event, 1, mask,
                            GetEventFilter(mouse, &event), grab);
        else
            DeliverEventsToWindow(mouse, pWin, &event, 1,
                                  GetEventFilter(mouse, &event),
                                  NullGrab);
    }

    if ((type == EnterNotify) && (mask & KeymapStateMask))
    {
        xKeymapEvent ke;
        ClientPtr client = grab ? rClient(grab) : wClient(pWin);
        if (XaceHook(XACE_DEVICE_ACCESS, client, keybd, DixReadAccess))
            bzero((char *)&ke.map[0], 31);
        else
            memmove((char *)&ke.map[0], (char *)&keybd->key->down[1], 31);

        ke.type = KeymapNotify;
        if (grab)
            TryClientEvents(rClient(grab), keybd, (xEvent *)&ke, 1,
                            mask, KeymapStateMask, grab);
        else
            DeliverEventsToWindow(mouse, pWin, (xEvent *)&ke, 1,
                                  KeymapStateMask, NullGrab);
    }
}

void
DeviceEnterLeaveEvent(
    DeviceIntPtr mouse,
    int sourceid,
    int type,
    int mode,
    int detail,
    WindowPtr pWin,
    Window child)
{
    GrabPtr             grab = mouse->deviceGrab.grab;
    xXIEnterEvent       *event;
    int                 filter;
    int                 btlen, len, i;
    DeviceIntPtr        kbd;

    if ((mode == XINotifyPassiveGrab && type == XI_Leave) ||
        (mode == XINotifyPassiveUngrab && type == XI_Enter))
        return;

    btlen = (mouse->button) ? bits_to_bytes(mouse->button->numButtons) : 0;
    btlen = bytes_to_int32(btlen);
    len = sizeof(xXIEnterEvent) + btlen * 4;

    event = xcalloc(1, len);
    event->type         = GenericEvent;
    event->extension    = IReqCode;
    event->evtype       = type;
    event->length       = (len - sizeof(xEvent))/4;
    event->buttons_len  = btlen;
    event->detail       = detail;
    event->time         = currentTime.milliseconds;
    event->deviceid     = mouse->id;
    event->sourceid     = sourceid;
    event->mode         = mode;
    event->root_x       = FP1616(mouse->spriteInfo->sprite->hot.x, 0);
    event->root_y       = FP1616(mouse->spriteInfo->sprite->hot.y, 0);

    for (i = 0; mouse && mouse->button && i < mouse->button->numButtons; i++)
        if (BitIsOn(mouse->button->down, i))
            SetBit(&event[1], i);

    kbd = (IsMaster(mouse) || mouse->u.master) ? GetPairedDevice(mouse) : NULL;
    if (kbd && kbd->key)
    {
        event->mods.base_mods = kbd->key->xkbInfo->state.base_mods;
        event->mods.latched_mods = kbd->key->xkbInfo->state.latched_mods;
        event->mods.locked_mods = kbd->key->xkbInfo->state.locked_mods;

        event->group.base_group = kbd->key->xkbInfo->state.base_group;
        event->group.latched_group = kbd->key->xkbInfo->state.latched_group;
        event->group.locked_group = kbd->key->xkbInfo->state.locked_group;
    }

    FixUpEventFromWindow(mouse, (xEvent*)event, pWin, None, FALSE);

    filter = GetEventFilter(mouse, (xEvent*)event);

    if (grab)
    {
        Mask mask;
        mask = grab->xi2mask[XIAllDevices][type/8] |
               grab->xi2mask[XIAllMasterDevices][type/8] |
               grab->xi2mask[mouse->id][type/8];
        TryClientEvents(rClient(grab), mouse, (xEvent*)event, 1, mask,
                        filter, grab);
    } else {
        if (!GetWindowXI2Mask(mouse, pWin, (xEvent*)event))
            goto out;
        DeliverEventsToWindow(mouse, pWin, (xEvent*)event, 1, filter,
                              NullGrab);
    }

out:
    xfree(event);
}

void
CoreFocusEvent(DeviceIntPtr dev, int type, int mode, int detail, WindowPtr pWin)
{
    xEvent event;

    memset(&event, 0, sizeof(xEvent));
    event.u.focus.mode = mode;
    event.u.u.type = type;
    event.u.u.detail = detail;
    event.u.focus.window = pWin->drawable.id;

    DeliverEventsToWindow(dev, pWin, &event, 1,
                          GetEventFilter(dev, &event), NullGrab);
    if ((type == FocusIn) &&
            ((pWin->eventMask | wOtherEventMasks(pWin)) & KeymapStateMask))
    {
        xKeymapEvent ke;
        ClientPtr client = wClient(pWin);
        if (XaceHook(XACE_DEVICE_ACCESS, client, dev, DixReadAccess))
            bzero((char *)&ke.map[0], 31);
        else
            memmove((char *)&ke.map[0], (char *)&dev->key->down[1], 31);

        ke.type = KeymapNotify;
        DeliverEventsToWindow(dev, pWin, (xEvent *)&ke, 1,
                KeymapStateMask, NullGrab);
    }
}

/**
 * Set the input focus to the given window. Subsequent keyboard events will be
 * delivered to the given window.
 *
 * Usually called from ProcSetInputFocus as result of a client request. If so,
 * the device is the inputInfo.keyboard.
 * If called from ProcXSetInputFocus as result of a client xinput request, the
 * device is set to the device specified by the client.
 *
 * @param client Client that requested input focus change.
 * @param dev Focus device.
 * @param focusID The window to obtain the focus. Can be PointerRoot or None.
 * @param revertTo Specifies where the focus reverts to when window becomes
 * unviewable.
 * @param ctime Specifies the time.
 * @param followOK True if pointer is allowed to follow the keyboard.
 */
int
SetInputFocus(
    ClientPtr client,
    DeviceIntPtr dev,
    Window focusID,
    CARD8 revertTo,
    Time ctime,
    Bool followOK)
{
    FocusClassPtr focus;
    WindowPtr focusWin;
    int mode, rc;
    TimeStamp time;
    DeviceIntPtr keybd; /* used for FollowKeyboard or FollowKeyboardWin */


    UpdateCurrentTime();
    if ((revertTo != RevertToParent) &&
	(revertTo != RevertToPointerRoot) &&
	(revertTo != RevertToNone) &&
	((revertTo != RevertToFollowKeyboard) || !followOK))
    {
	client->errorValue = revertTo;
	return BadValue;
    }
    time = ClientTimeToServerTime(ctime);

    if (IsKeyboardDevice(dev))
        keybd = dev;
    else
        keybd = GetPairedDevice(dev);

    if ((focusID == None) || (focusID == PointerRoot))
	focusWin = (WindowPtr)(long)focusID;
    else if ((focusID == FollowKeyboard) && followOK)
    {
	focusWin = keybd->focus->win;
    }
    else {
	rc = dixLookupWindow(&focusWin, focusID, client, DixSetAttrAccess);
	if (rc != Success)
	    return rc;
	/* It is a match error to try to set the input focus to an
	unviewable window. */
	if(!focusWin->realized)
	    return(BadMatch);
    }
    rc = XaceHook(XACE_DEVICE_ACCESS, client, dev, DixSetFocusAccess);
    if (rc != Success)
	return Success;

    focus = dev->focus;
    if ((CompareTimeStamps(time, currentTime) == LATER) ||
	(CompareTimeStamps(time, focus->time) == EARLIER))
	return Success;
    mode = (dev->deviceGrab.grab) ? NotifyWhileGrabbed : NotifyNormal;
    if (focus->win == FollowKeyboardWin)
    {
        if (!ActivateFocusInGrab(dev, keybd->focus->win, focusWin))
            DoFocusEvents(dev, keybd->focus->win, focusWin, mode);
    } else
    {
        if (!ActivateFocusInGrab(dev, focus->win, focusWin))
            DoFocusEvents(dev, focus->win, focusWin, mode);
    }
    focus->time = time;
    focus->revert = revertTo;
    if (focusID == FollowKeyboard)
	focus->win = FollowKeyboardWin;
    else
	focus->win = focusWin;
    if ((focusWin == NoneWin) || (focusWin == PointerRootWin))
	focus->traceGood = 0;
    else
    {
        int depth = 0;
	WindowPtr pWin;

        for (pWin = focusWin; pWin; pWin = pWin->parent) depth++;
        if (depth > focus->traceSize)
        {
	    focus->traceSize = depth+1;
	    focus->trace = xrealloc(focus->trace,
				    focus->traceSize * sizeof(WindowPtr));
	}
	focus->traceGood = depth;
        for (pWin = focusWin, depth--; pWin; pWin = pWin->parent, depth--)
	    focus->trace[depth] = pWin;
    }
    return Success;
}

/**
 * Server-side protocol handling for SetInputFocus request.
 *
 * Sets the input focus for the virtual core keyboard.
 */
int
ProcSetInputFocus(ClientPtr client)
{
    DeviceIntPtr kbd = PickKeyboard(client);
    REQUEST(xSetInputFocusReq);

    REQUEST_SIZE_MATCH(xSetInputFocusReq);

    return SetInputFocus(client, kbd, stuff->focus,
			 stuff->revertTo, stuff->time, FALSE);
}

/**
 * Server-side protocol handling for GetInputFocus request.
 *
 * Sends the current input focus for the client's keyboard back to the
 * client.
 */
int
ProcGetInputFocus(ClientPtr client)
{
    DeviceIntPtr kbd = PickKeyboard(client);
    xGetInputFocusReply rep;
    FocusClassPtr focus = kbd->focus;
    int rc;
    /* REQUEST(xReq); */
    REQUEST_SIZE_MATCH(xReq);

    rc = XaceHook(XACE_DEVICE_ACCESS, client, kbd, DixGetFocusAccess);
    if (rc != Success)
	return rc;

    memset(&rep, 0, sizeof(xGetInputFocusReply));
    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    if (focus->win == NoneWin)
	rep.focus = None;
    else if (focus->win == PointerRootWin)
	rep.focus = PointerRoot;
    else rep.focus = focus->win->drawable.id;
    rep.revertTo = focus->revert;
    WriteReplyToClient(client, sizeof(xGetInputFocusReply), &rep);
    return Success;
}

/**
 * Server-side protocol handling for GrabPointer request.
 *
 * Sets an active grab on the client's ClientPointer and returns success
 * status to client.
 */
int
ProcGrabPointer(ClientPtr client)
{
    xGrabPointerReply rep;
    DeviceIntPtr device = PickPointer(client);
    GrabPtr grab;
    GrabMask mask;
    WindowPtr confineTo;
    CursorPtr oldCursor;
    REQUEST(xGrabPointerReq);
    TimeStamp time;
    int rc;

    REQUEST_SIZE_MATCH(xGrabPointerReq);
    UpdateCurrentTime();

    if (stuff->eventMask & ~PointerGrabMask)
    {
	client->errorValue = stuff->eventMask;
        return BadValue;
    }

    if (stuff->confineTo == None)
	confineTo = NullWindow;
    else
    {
	rc = dixLookupWindow(&confineTo, stuff->confineTo, client,
			     DixSetAttrAccess);
	if (rc != Success)
	    return rc;
    }

    memset(&rep, 0, sizeof(xGrabPointerReply));
    oldCursor = NullCursor;
    grab = device->deviceGrab.grab;

    if (grab)
    {
        if (grab->confineTo && !confineTo)
            ConfineCursorToWindow(device, RootWindow(device), FALSE, FALSE);
        oldCursor = grab->cursor;
    }

    mask.core = stuff->eventMask;

    rc = GrabDevice(client, device, stuff->pointerMode, stuff->keyboardMode,
                    stuff->grabWindow, stuff->ownerEvents, stuff->time,
                    &mask, GRABTYPE_CORE, stuff->cursor,
                    stuff->confineTo, &rep.status);
    if (rc != Success)
        return rc;

    if (oldCursor && rep.status == GrabSuccess)
        FreeCursor (oldCursor, (Cursor)0);

    time = ClientTimeToServerTime(stuff->time);
    rep.type = X_Reply;
    rep.sequenceNumber = client->sequence;
    rep.length = 0;
    WriteReplyToClient(client, sizeof(xGrabPointerReply), &rep);
    return Success;
}

/**
 * Server-side protocol handling for ChangeActivePointerGrab request.
 *
 * Changes properties of the grab hold by the client. If the client does not
 * hold an active grab on the device, nothing happens.
 */
int
ProcChangeActivePointerGrab(ClientPtr client)
{
    DeviceIntPtr device;
    GrabPtr      grab;
    CursorPtr newCursor, oldCursor;
    REQUEST(xChangeActivePointerGrabReq);
    TimeStamp time;

    REQUEST_SIZE_MATCH(xChangeActivePointerGrabReq);
    if (stuff->eventMask & ~PointerGrabMask)
    {
	client->errorValue = stuff->eventMask;
        return BadValue;
    }
    if (stuff->cursor == None)
	newCursor = NullCursor;
    else
    {
	int rc = dixLookupResourceByType((pointer *)&newCursor, stuff->cursor,
					 RT_CURSOR, client, DixUseAccess);
	if (rc != Success)
	{
	    client->errorValue = stuff->cursor;
	    return (rc == BadValue) ? BadCursor : rc;
	}
    }

    device = PickPointer(client);
    grab = device->deviceGrab.grab;

    if (!grab)
	return Success;
    if (!SameClient(grab, client))
	return Success;
    time = ClientTimeToServerTime(stuff->time);
    if ((CompareTimeStamps(time, currentTime) == LATER) ||
	     (CompareTimeStamps(time, device->deviceGrab.grabTime) == EARLIER))
	return Success;
    oldCursor = grab->cursor;
    grab->cursor = newCursor;
    if (newCursor)
	newCursor->refcnt++;
    PostNewCursor(device);
    if (oldCursor)
	FreeCursor(oldCursor, (Cursor)0);
    grab->eventMask = stuff->eventMask;
    return Success;
}

/**
 * Server-side protocol handling for UngrabPointer request.
 *
 * Deletes a pointer grab on a device the client has grabbed.
 */
int
ProcUngrabPointer(ClientPtr client)
{
    DeviceIntPtr device = PickPointer(client);
    GrabPtr grab;
    TimeStamp time;
    REQUEST(xResourceReq);

    REQUEST_SIZE_MATCH(xResourceReq);
    UpdateCurrentTime();
    grab = device->deviceGrab.grab;

    time = ClientTimeToServerTime(stuff->id);
    if ((CompareTimeStamps(time, currentTime) != LATER) &&
	    (CompareTimeStamps(time, device->deviceGrab.grabTime) != EARLIER) &&
	    (grab) && SameClient(grab, client))
	(*device->deviceGrab.DeactivateGrab)(device);
    return Success;
}

/**
 * Sets a grab on the given device.
 *
 * Called from ProcGrabKeyboard to work on the client's keyboard.
 * Called from ProcXGrabDevice to work on the device specified by the client.
 *
 * The parameters this_mode and other_mode represent the keyboard_mode and
 * pointer_mode parameters of XGrabKeyboard().
 * See man page for details on all the parameters
 *
 * @param client Client that owns the grab.
 * @param dev The device to grab.
 * @param this_mode GrabModeSync or GrabModeAsync
 * @param other_mode GrabModeSync or GrabModeAsync
 * @param status Return code to be returned to the caller.
 *
 * @returns Success or BadValue.
 */
int
GrabDevice(ClientPtr client, DeviceIntPtr dev,
           unsigned pointer_mode, unsigned keyboard_mode, Window grabWindow,
           unsigned ownerEvents, Time ctime, GrabMask *mask,
           int grabtype, Cursor curs, Window confineToWin, CARD8 *status)
{
    WindowPtr pWin, confineTo;
    GrabPtr grab;
    TimeStamp time;
    Mask access_mode = DixGrabAccess;
    int rc;
    GrabInfoPtr grabInfo = &dev->deviceGrab;
    CursorPtr cursor;

    UpdateCurrentTime();
    if ((keyboard_mode != GrabModeSync) && (keyboard_mode != GrabModeAsync))
    {
	client->errorValue = keyboard_mode;
        return BadValue;
    }
    if ((pointer_mode != GrabModeSync) && (pointer_mode != GrabModeAsync))
    {
	client->errorValue = pointer_mode;
        return BadValue;
    }
    if ((ownerEvents != xFalse) && (ownerEvents != xTrue))
    {
	client->errorValue = ownerEvents;
        return BadValue;
    }

    rc = dixLookupWindow(&pWin, grabWindow, client, DixSetAttrAccess);
    if (rc != Success)
	return rc;

    if (confineToWin == None)
	confineTo = NullWindow;
    else
    {
	rc = dixLookupWindow(&confineTo, confineToWin, client,
			     DixSetAttrAccess);
	if (rc != Success)
	    return rc;
    }

    if (curs == None)
	cursor = NullCursor;
    else
    {
	rc = dixLookupResourceByType((pointer *)&cursor, curs, RT_CURSOR,
			       client, DixUseAccess);
	if (rc != Success)
	{
	    client->errorValue = curs;
	    return (rc == BadValue) ? BadCursor : rc;
	}
	access_mode |= DixForceAccess;
    }

    if (keyboard_mode == GrabModeSync || pointer_mode == GrabModeSync)
	access_mode |= DixFreezeAccess;
    rc = XaceHook(XACE_DEVICE_ACCESS, client, dev, access_mode);
    if (rc != Success)
	return rc;

    time = ClientTimeToServerTime(ctime);
    grab = grabInfo->grab;
    if (grab && grab->grabtype != grabtype)
        *status = AlreadyGrabbed;
    if (grab && !SameClient(grab, client))
	*status = AlreadyGrabbed;
    else if ((!pWin->realized) ||
             (confineTo &&
                !(confineTo->realized
                    && BorderSizeNotEmpty(dev, confineTo))))
	*status = GrabNotViewable;
    else if ((CompareTimeStamps(time, currentTime) == LATER) ||
	     (CompareTimeStamps(time, grabInfo->grabTime) == EARLIER))
	*status = GrabInvalidTime;
    else if (grabInfo->sync.frozen &&
	     grabInfo->sync.other && !SameClient(grabInfo->sync.other, client))
	*status = GrabFrozen;
    else
    {
	GrabRec tempGrab;

        /* Otherwise segfaults happen on grabbed MPX devices */
        memset(&tempGrab, 0, sizeof(GrabRec));

        tempGrab.next = NULL;
	tempGrab.window = pWin;
	tempGrab.resource = client->clientAsMask;
	tempGrab.ownerEvents = ownerEvents;
	tempGrab.keyboardMode = keyboard_mode;
	tempGrab.pointerMode = pointer_mode;
	if (grabtype == GRABTYPE_CORE)
	    tempGrab.eventMask = mask->core;
	else if (grabtype == GRABTYPE_XI)
	    tempGrab.eventMask = mask->xi;
	else
	    memcpy(tempGrab.xi2mask, mask->xi2mask, sizeof(tempGrab.xi2mask));
	tempGrab.device = dev;
	tempGrab.cursor = cursor;
	tempGrab.confineTo = confineTo;
	tempGrab.grabtype = grabtype;
	(*grabInfo->ActivateGrab)(dev, &tempGrab, time, FALSE);
	*status = GrabSuccess;
    }
    return Success;
}

/**
 * Server-side protocol handling for GrabKeyboard request.
 *
 * Grabs the client's keyboard and returns success status to client.
 */
int
ProcGrabKeyboard(ClientPtr client)
{
    xGrabKeyboardReply rep;
    REQUEST(xGrabKeyboardReq);
    int result;
    DeviceIntPtr keyboard = PickKeyboard(client);
    GrabMask mask;

    REQUEST_SIZE_MATCH(xGrabKeyboardReq);

    memset(&rep, 0, sizeof(xGrabKeyboardReply));
    mask.core = KeyPressMask | KeyReleaseMask;

    result = GrabDevice(client, keyboard, stuff->pointerMode,
            stuff->keyboardMode, stuff->grabWindow, stuff->ownerEvents,
            stuff->time, &mask, GRABTYPE_CORE, None, None,
            &rep.status);

    if (result != Success)
	return result;
    rep.type = X_Reply;
    rep.sequenceNumber = client->sequence;
    rep.length = 0;
    WriteReplyToClient(client, sizeof(xGrabKeyboardReply), &rep);
    return Success;
}

/**
 * Server-side protocol handling for UngrabKeyboard request.
 *
 * Deletes a possible grab on the client's keyboard.
 */
int
ProcUngrabKeyboard(ClientPtr client)
{
    DeviceIntPtr device = PickKeyboard(client);
    GrabPtr grab;
    TimeStamp time;
    REQUEST(xResourceReq);

    REQUEST_SIZE_MATCH(xResourceReq);
    UpdateCurrentTime();

    grab = device->deviceGrab.grab;

    time = ClientTimeToServerTime(stuff->id);
    if ((CompareTimeStamps(time, currentTime) != LATER) &&
	(CompareTimeStamps(time, device->deviceGrab.grabTime) != EARLIER) &&
	(grab) && SameClient(grab, client) && grab->grabtype == GRABTYPE_CORE)
	(*device->deviceGrab.DeactivateGrab)(device);
    return Success;
}

/**
 * Server-side protocol handling for QueryPointer request.
 *
 * Returns the current state and position of the client's ClientPointer to the
 * client.
 */
int
ProcQueryPointer(ClientPtr client)
{
    xQueryPointerReply rep;
    WindowPtr pWin, t;
    DeviceIntPtr mouse = PickPointer(client);
    DeviceIntPtr keyboard;
    SpritePtr pSprite;
    int rc;
    REQUEST(xResourceReq);
    REQUEST_SIZE_MATCH(xResourceReq);

    rc = dixLookupWindow(&pWin, stuff->id, client, DixGetAttrAccess);
    if (rc != Success)
	return rc;
    rc = XaceHook(XACE_DEVICE_ACCESS, client, mouse, DixReadAccess);
    if (rc != Success && rc != BadAccess)
	return rc;

    keyboard = GetPairedDevice(mouse);

    pSprite = mouse->spriteInfo->sprite;
    if (mouse->valuator->motionHintWindow)
	MaybeStopHint(mouse, client);
    memset(&rep, 0, sizeof(xQueryPointerReply));
    rep.type = X_Reply;
    rep.sequenceNumber = client->sequence;
    rep.mask = mouse->button ? (mouse->button->state) : 0;
    rep.mask |= XkbStateFieldFromRec(&keyboard->key->xkbInfo->state);
    rep.length = 0;
    rep.root = (RootWindow(mouse))->drawable.id;
    rep.rootX = pSprite->hot.x;
    rep.rootY = pSprite->hot.y;
    rep.child = None;
    if (pSprite->hot.pScreen == pWin->drawable.pScreen)
    {
	rep.sameScreen = xTrue;
	rep.winX = pSprite->hot.x - pWin->drawable.x;
	rep.winY = pSprite->hot.y - pWin->drawable.y;
	for (t = pSprite->win; t; t = t->parent)
	    if (t->parent == pWin)
	    {
		rep.child = t->drawable.id;
		break;
	    }
    }
    else
    {
	rep.sameScreen = xFalse;
	rep.winX = 0;
	rep.winY = 0;
    }

#ifdef PANORAMIX
    if(!noPanoramiXExtension) {
	rep.rootX += panoramiXdataPtr[0].x;
	rep.rootY += panoramiXdataPtr[0].y;
	if(stuff->id == rep.root) {
	    rep.winX += panoramiXdataPtr[0].x;
	    rep.winY += panoramiXdataPtr[0].y;
	}
    }
#endif

    if (rc == BadAccess) {
	rep.mask = 0;
	rep.child = None;
	rep.rootX = 0;
	rep.rootY = 0;
	rep.winX = 0;
	rep.winY = 0;
    }

    WriteReplyToClient(client, sizeof(xQueryPointerReply), &rep);

    return(Success);
}

/**
 * Initializes the device list and the DIX sprite to sane values. Allocates
 * trace memory used for quick window traversal.
 */
void
InitEvents(void)
{
    int i;

    inputInfo.numDevices = 0;
    inputInfo.devices = (DeviceIntPtr)NULL;
    inputInfo.off_devices = (DeviceIntPtr)NULL;
    inputInfo.keyboard = (DeviceIntPtr)NULL;
    inputInfo.pointer = (DeviceIntPtr)NULL;
    /* The mask for pointer motion events may have changed in the last server
     * generation. See comment above definition of filters. */
    filters[0][PointerMotionMask] = MotionNotify;
    for (i = 1; i < MAXDEVICES; i++)
    {
        memcpy(&filters[i], filters[0], sizeof(filters[0]));
    }

    syncEvents.replayDev = (DeviceIntPtr)NULL;
    syncEvents.replayWin = NullWindow;
    while (syncEvents.pending)
    {
	QdEventPtr next = syncEvents.pending->next;
	xfree(syncEvents.pending);
	syncEvents.pending = next;
    }
    syncEvents.pendtail = &syncEvents.pending;
    syncEvents.playingEvents = FALSE;
    syncEvents.time.months = 0;
    syncEvents.time.milliseconds = 0;	/* hardly matters */
    currentTime.months = 0;
    currentTime.milliseconds = GetTimeInMillis();
    lastDeviceEventTime = currentTime;
    for (i = 0; i < DNPMCOUNT; i++)
    {
	DontPropagateMasks[i] = 0;
	DontPropagateRefCnts[i] = 0;
    }

    InputEventListLen = GetMaximumEventsNum();
    InputEventList = InitEventList(InputEventListLen);
    if (!InputEventList)
        FatalError("[dix] Failed to allocate input event list.\n");
}

void
CloseDownEvents(void)
{
    int len;
    EventListPtr list;

    len = GetEventList(&list);
    while(len--)
        xfree(list[len].event);
}

/**
 * Server-side protocol handling for SendEvent request.
 *
 * Locates the window to send the event to and forwards the event.
 */
int
ProcSendEvent(ClientPtr client)
{
    WindowPtr pWin;
    WindowPtr effectiveFocus = NullWindow; /* only set if dest==InputFocus */
    DeviceIntPtr dev = PickPointer(client);
    DeviceIntPtr keybd = GetPairedDevice(dev);
    SpritePtr pSprite = dev->spriteInfo->sprite;
    REQUEST(xSendEventReq);

    REQUEST_SIZE_MATCH(xSendEventReq);

    /* The client's event type must be a core event type or one defined by an
	extension. */

    if ( ! ((stuff->event.u.u.type > X_Reply &&
	     stuff->event.u.u.type < LASTEvent) ||
	    (stuff->event.u.u.type >= EXTENSION_EVENT_BASE &&
	     stuff->event.u.u.type < (unsigned)lastEvent)))
    {
	client->errorValue = stuff->event.u.u.type;
	return BadValue;
    }
    if (stuff->event.u.u.type == ClientMessage &&
	stuff->event.u.u.detail != 8 &&
	stuff->event.u.u.detail != 16 &&
	stuff->event.u.u.detail != 32)
    {
	client->errorValue = stuff->event.u.u.detail;
	return BadValue;
    }
    if (stuff->eventMask & ~AllEventMasks)
    {
	client->errorValue = stuff->eventMask;
	return BadValue;
    }

    if (stuff->destination == PointerWindow)
	pWin = pSprite->win;
    else if (stuff->destination == InputFocus)
    {
	WindowPtr inputFocus = (keybd) ? keybd->focus->win : NoneWin;

	if (inputFocus == NoneWin)
	    return Success;

	/* If the input focus is PointerRootWin, send the event to where
	the pointer is if possible, then perhaps propogate up to root. */
	if (inputFocus == PointerRootWin)
	    inputFocus = pSprite->spriteTrace[0]; /* Root window! */

	if (IsParent(inputFocus, pSprite->win))
	{
	    effectiveFocus = inputFocus;
	    pWin = pSprite->win;
	}
	else
	    effectiveFocus = pWin = inputFocus;
    }
    else
	dixLookupWindow(&pWin, stuff->destination, client, DixSendAccess);

    if (!pWin)
	return BadWindow;
    if ((stuff->propagate != xFalse) && (stuff->propagate != xTrue))
    {
	client->errorValue = stuff->propagate;
	return BadValue;
    }
    stuff->event.u.u.type |= 0x80;
    if (stuff->propagate)
    {
	for (;pWin; pWin = pWin->parent)
	{
	    if (XaceHook(XACE_SEND_ACCESS, client, NULL, pWin,
			 &stuff->event, 1))
		return Success;
            if (DeliverEventsToWindow(dev, pWin,
                        &stuff->event, 1, stuff->eventMask, NullGrab))
		return Success;
	    if (pWin == effectiveFocus)
		return Success;
	    stuff->eventMask &= ~wDontPropagateMask(pWin);
	    if (!stuff->eventMask)
		break;
	}
    }
    else if (!XaceHook(XACE_SEND_ACCESS, client, NULL, pWin, &stuff->event, 1))
        DeliverEventsToWindow(dev, pWin, &stuff->event,
                                    1, stuff->eventMask, NullGrab);
    return Success;
}

/**
 * Server-side protocol handling for UngrabKey request.
 *
 * Deletes a passive grab for the given key. Works on the
 * client's keyboard.
 */
int
ProcUngrabKey(ClientPtr client)
{
    REQUEST(xUngrabKeyReq);
    WindowPtr pWin;
    GrabRec tempGrab;
    DeviceIntPtr keybd = PickKeyboard(client);
    int rc;

    REQUEST_SIZE_MATCH(xUngrabKeyReq);
    rc = dixLookupWindow(&pWin, stuff->grabWindow, client, DixGetAttrAccess);
    if (rc != Success)
	return rc;

    if (((stuff->key > keybd->key->xkbInfo->desc->max_key_code) ||
	 (stuff->key < keybd->key->xkbInfo->desc->min_key_code))
	&& (stuff->key != AnyKey))
    {
	client->errorValue = stuff->key;
        return BadValue;
    }
    if ((stuff->modifiers != AnyModifier) &&
	(stuff->modifiers & ~AllModifiersMask))
    {
	client->errorValue = stuff->modifiers;
	return BadValue;
    }
    tempGrab.resource = client->clientAsMask;
    tempGrab.device = keybd;
    tempGrab.window = pWin;
    tempGrab.modifiersDetail.exact = stuff->modifiers;
    tempGrab.modifiersDetail.pMask = NULL;
    tempGrab.modifierDevice = GetPairedDevice(keybd);
    tempGrab.type = KeyPress;
    tempGrab.grabtype = GRABTYPE_CORE;
    tempGrab.detail.exact = stuff->key;
    tempGrab.detail.pMask = NULL;
    tempGrab.next = NULL;

    if (!DeletePassiveGrabFromList(&tempGrab))
	return(BadAlloc);
    return(Success);
}

/**
 * Server-side protocol handling for GrabKey request.
 *
 * Creates a grab for the client's keyboard and adds it to the list of passive
 * grabs.
 */
int
ProcGrabKey(ClientPtr client)
{
    WindowPtr pWin;
    REQUEST(xGrabKeyReq);
    GrabPtr grab;
    DeviceIntPtr keybd = PickKeyboard(client);
    int rc;
    GrabParameters param;
    GrabMask mask;

    REQUEST_SIZE_MATCH(xGrabKeyReq);

    memset(&param, 0, sizeof(param));
    param.grabtype = GRABTYPE_CORE;
    param.ownerEvents = stuff->ownerEvents;
    param.this_device_mode = stuff->keyboardMode;
    param.other_devices_mode = stuff->pointerMode;
    param.modifiers = stuff->modifiers;

    rc = CheckGrabValues(client, &param);
    if (rc != Success)
        return rc;

    if (((stuff->key > keybd->key->xkbInfo->desc->max_key_code) ||
	 (stuff->key < keybd->key->xkbInfo->desc->min_key_code))
	&& (stuff->key != AnyKey))
    {
	client->errorValue = stuff->key;
        return BadValue;
    }
    rc = dixLookupWindow(&pWin, stuff->grabWindow, client, DixSetAttrAccess);
    if (rc != Success)
	return rc;


    mask.core = (KeyPressMask | KeyReleaseMask);

    grab = CreateGrab(client->index, keybd, keybd, pWin, GRABTYPE_CORE, &mask,
                      &param, KeyPress, stuff->key, NullWindow, NullCursor);
    if (!grab)
	return BadAlloc;
    return AddPassiveGrabToList(client, grab);
}


/**
 * Server-side protocol handling for GrabButton request.
 *
 * Creates a grab for the client's ClientPointer and adds it as a passive grab
 * to the list.
 */
int
ProcGrabButton(ClientPtr client)
{
    WindowPtr pWin, confineTo;
    REQUEST(xGrabButtonReq);
    CursorPtr cursor;
    GrabPtr grab;
    DeviceIntPtr ptr, modifierDevice;
    Mask access_mode = DixGrabAccess;
    GrabMask mask;
    GrabParameters param;
    int rc;

    REQUEST_SIZE_MATCH(xGrabButtonReq);
    if ((stuff->pointerMode != GrabModeSync) &&
	(stuff->pointerMode != GrabModeAsync))
    {
	client->errorValue = stuff->pointerMode;
        return BadValue;
    }
    if ((stuff->keyboardMode != GrabModeSync) &&
	(stuff->keyboardMode != GrabModeAsync))
    {
	client->errorValue = stuff->keyboardMode;
        return BadValue;
    }
    if ((stuff->modifiers != AnyModifier) &&
	(stuff->modifiers & ~AllModifiersMask))
    {
	client->errorValue = stuff->modifiers;
	return BadValue;
    }
    if ((stuff->ownerEvents != xFalse) && (stuff->ownerEvents != xTrue))
    {
	client->errorValue = stuff->ownerEvents;
	return BadValue;
    }
    if (stuff->eventMask & ~PointerGrabMask)
    {
	client->errorValue = stuff->eventMask;
        return BadValue;
    }
    rc = dixLookupWindow(&pWin, stuff->grabWindow, client, DixSetAttrAccess);
    if (rc != Success)
	return rc;
    if (stuff->confineTo == None)
       confineTo = NullWindow;
    else {
	rc = dixLookupWindow(&confineTo, stuff->confineTo, client,
			     DixSetAttrAccess);
	if (rc != Success)
	    return rc;
    }
    if (stuff->cursor == None)
	cursor = NullCursor;
    else
    {
	rc = dixLookupResourceByType((pointer *)&cursor, stuff->cursor, RT_CURSOR,
			       client, DixUseAccess);
	if (rc != Success)
	if (!cursor)
	{
	    client->errorValue = stuff->cursor;
	    return (rc == BadValue) ? BadCursor : rc;
	}
	access_mode |= DixForceAccess;
    }

    ptr = PickPointer(client);
    modifierDevice = GetPairedDevice(ptr);
    if (stuff->pointerMode == GrabModeSync ||
	stuff->keyboardMode == GrabModeSync)
	access_mode |= DixFreezeAccess;
    rc = XaceHook(XACE_DEVICE_ACCESS, client, ptr, access_mode);
    if (rc != Success)
	return rc;

    memset(&param, 0, sizeof(param));
    param.grabtype = GRABTYPE_CORE;
    param.ownerEvents = stuff->ownerEvents;
    param.this_device_mode = stuff->keyboardMode;
    param.other_devices_mode = stuff->pointerMode;
    param.modifiers = stuff->modifiers;

    mask.core = stuff->eventMask;

    grab = CreateGrab(client->index, ptr, modifierDevice, pWin,
                      GRABTYPE_CORE, &mask, &param, ButtonPress,
                      stuff->button, confineTo, cursor);
    if (!grab)
	return BadAlloc;
    return AddPassiveGrabToList(client, grab);
}

/**
 * Server-side protocol handling for UngrabButton request.
 *
 * Deletes a passive grab on the client's ClientPointer from the list.
 */
int
ProcUngrabButton(ClientPtr client)
{
    REQUEST(xUngrabButtonReq);
    WindowPtr pWin;
    GrabRec tempGrab;
    int rc;
    DeviceIntPtr ptr;

    REQUEST_SIZE_MATCH(xUngrabButtonReq);
    if ((stuff->modifiers != AnyModifier) &&
	(stuff->modifiers & ~AllModifiersMask))
    {
	client->errorValue = stuff->modifiers;
	return BadValue;
    }
    rc = dixLookupWindow(&pWin, stuff->grabWindow, client, DixReadAccess);
    if (rc != Success)
	return rc;

    ptr = PickPointer(client);

    tempGrab.resource = client->clientAsMask;
    tempGrab.device = ptr;
    tempGrab.window = pWin;
    tempGrab.modifiersDetail.exact = stuff->modifiers;
    tempGrab.modifiersDetail.pMask = NULL;
    tempGrab.modifierDevice = GetPairedDevice(ptr);
    tempGrab.type = ButtonPress;
    tempGrab.detail.exact = stuff->button;
    tempGrab.grabtype = GRABTYPE_CORE;
    tempGrab.detail.pMask = NULL;
    tempGrab.next = NULL;

    if (!DeletePassiveGrabFromList(&tempGrab))
	return(BadAlloc);
    return(Success);
}

/**
 * Deactivate any grab that may be on the window, remove the focus.
 * Delete any XInput extension events from the window too. Does not change the
 * window mask. Use just before the window is deleted.
 *
 * If freeResources is set, passive grabs on the window are deleted.
 *
 * @param pWin The window to delete events from.
 * @param freeResources True if resources associated with the window should be
 * deleted.
 */
void
DeleteWindowFromAnyEvents(WindowPtr pWin, Bool freeResources)
{
    WindowPtr		parent;
    DeviceIntPtr	mouse = inputInfo.pointer;
    DeviceIntPtr	keybd = inputInfo.keyboard;
    FocusClassPtr	focus;
    OtherClientsPtr	oc;
    GrabPtr		passive;
    GrabPtr             grab;


    /* Deactivate any grabs performed on this window, before making any
	input focus changes. */
    grab = mouse->deviceGrab.grab;
    if (grab &&
	((grab->window == pWin) || (grab->confineTo == pWin)))
	(*mouse->deviceGrab.DeactivateGrab)(mouse);


    /* Deactivating a keyboard grab should cause focus events. */
    grab = keybd->deviceGrab.grab;
    if (grab && (grab->window == pWin))
	(*keybd->deviceGrab.DeactivateGrab)(keybd);

    /* And now the real devices */
    for (mouse = inputInfo.devices; mouse; mouse = mouse->next)
    {
        grab = mouse->deviceGrab.grab;
        if (grab && ((grab->window == pWin) || (grab->confineTo == pWin)))
            (*mouse->deviceGrab.DeactivateGrab)(mouse);
    }


    for (keybd = inputInfo.devices; keybd; keybd = keybd->next)
    {
        if (IsKeyboardDevice(keybd))
        {
            focus = keybd->focus;

            /* If the focus window is a root window (ie. has no parent) then don't
               delete the focus from it. */

            if ((pWin == focus->win) && (pWin->parent != NullWindow))
            {
                int focusEventMode = NotifyNormal;

                /* If a grab is in progress, then alter the mode of focus events. */

                if (keybd->deviceGrab.grab)
                    focusEventMode = NotifyWhileGrabbed;

                switch (focus->revert)
                {
                    case RevertToNone:
                        DoFocusEvents(keybd, pWin, NoneWin, focusEventMode);
                        focus->win = NoneWin;
                        focus->traceGood = 0;
                        break;
                    case RevertToParent:
                        parent = pWin;
                        do
                        {
                            parent = parent->parent;
                            focus->traceGood--;
                        } while (!parent->realized
                                /* This would be a good protocol change -- windows being reparented
                                   during SaveSet processing would cause the focus to revert to the
                                   nearest enclosing window which will survive the death of the exiting
                                   client, instead of ending up reverting to a dying window and thence
                                   to None
                                 */
#ifdef NOTDEF
				 || wClient(parent)->clientGone
#endif
                                );
                        if (!ActivateFocusInGrab(keybd, pWin, parent))
                            DoFocusEvents(keybd, pWin, parent, focusEventMode);
                        focus->win = parent;
                        focus->revert = RevertToNone;
                        break;
                    case RevertToPointerRoot:
                        if (!ActivateFocusInGrab(keybd, pWin, PointerRootWin))
                            DoFocusEvents(keybd, pWin, PointerRootWin, focusEventMode);
                        focus->win = PointerRootWin;
                        focus->traceGood = 0;
                        break;
                }
            }
        }

        if (IsPointerDevice(keybd))
        {
            if (keybd->valuator->motionHintWindow == pWin)
                keybd->valuator->motionHintWindow = NullWindow;
        }
    }

    if (freeResources)
    {
	if (pWin->dontPropagate)
	    DontPropagateRefCnts[pWin->dontPropagate]--;
	while ( (oc = wOtherClients(pWin)) )
	    FreeResource(oc->resource, RT_NONE);
	while ( (passive = wPassiveGrabs(pWin)) )
	    FreeResource(passive->resource, RT_NONE);
     }

    DeleteWindowFromAnyExtEvents(pWin, freeResources);
}

/**
 * Call this whenever some window at or below pWin has changed geometry. If
 * there is a grab on the window, the cursor will be re-confined into the
 * window.
 */
void
CheckCursorConfinement(WindowPtr pWin)
{
    GrabPtr grab;
    WindowPtr confineTo;
    DeviceIntPtr pDev;

#ifdef PANORAMIX
    if(!noPanoramiXExtension && pWin->drawable.pScreen->myNum) return;
#endif

    for (pDev = inputInfo.devices; pDev; pDev = pDev->next)
    {
        if (DevHasCursor(pDev))
        {
            grab = pDev->deviceGrab.grab;
            if (grab && (confineTo = grab->confineTo))
            {
                if (!BorderSizeNotEmpty(pDev, confineTo))
                    (*pDev->deviceGrab.DeactivateGrab)(pDev);
                else if ((pWin == confineTo) || IsParent(pWin, confineTo))
                    ConfineCursorToWindow(pDev, confineTo, TRUE, TRUE);
            }
        }
    }
}

Mask
EventMaskForClient(WindowPtr pWin, ClientPtr client)
{
    OtherClientsPtr	other;

    if (wClient (pWin) == client)
	return pWin->eventMask;
    for (other = wOtherClients(pWin); other; other = other->next)
    {
	if (SameClient(other, client))
	    return other->mask;
    }
    return 0;
}

/**
 * Server-side protocol handling for RecolorCursor request.
 */
int
ProcRecolorCursor(ClientPtr client)
{
    CursorPtr pCursor;
    int		rc, nscr;
    ScreenPtr	pscr;
    Bool	displayed;
    SpritePtr   pSprite = PickPointer(client)->spriteInfo->sprite;
    REQUEST(xRecolorCursorReq);

    REQUEST_SIZE_MATCH(xRecolorCursorReq);
    rc = dixLookupResourceByType((pointer *)&pCursor, stuff->cursor, RT_CURSOR,
			   client, DixWriteAccess);
    if (rc != Success)
    {
	client->errorValue = stuff->cursor;
	return (rc == BadValue) ? BadCursor : rc;
    }

    pCursor->foreRed = stuff->foreRed;
    pCursor->foreGreen = stuff->foreGreen;
    pCursor->foreBlue = stuff->foreBlue;

    pCursor->backRed = stuff->backRed;
    pCursor->backGreen = stuff->backGreen;
    pCursor->backBlue = stuff->backBlue;

    for (nscr = 0; nscr < screenInfo.numScreens; nscr++)
    {
	pscr = screenInfo.screens[nscr];
#ifdef PANORAMIX
	if(!noPanoramiXExtension)
	    displayed = (pscr == pSprite->screen);
	else
#endif
	    displayed = (pscr == pSprite->hotPhys.pScreen);
	( *pscr->RecolorCursor)(PickPointer(client), pscr, pCursor,
				(pCursor == pSprite->current) && displayed);
    }
    return (Success);
}

/**
 * Write the given events to a client, swapping the byte order if necessary.
 * To swap the byte ordering, a callback is called that has to be set up for
 * the given event type.
 *
 * In the case of DeviceMotionNotify trailed by DeviceValuators, the events
 * can be more than one. Usually it's just one event.
 *
 * Do not modify the event structure passed in. See comment below.
 *
 * @param pClient Client to send events to.
 * @param count Number of events.
 * @param events The event list.
 */
void
WriteEventsToClient(ClientPtr pClient, int count, xEvent *events)
{
#ifdef PANORAMIX
    xEvent    eventCopy;
#endif
    xEvent    *eventTo, *eventFrom;
    int       i,
              eventlength = sizeof(xEvent);

    /* Let XKB rewrite the state, as it depends on client preferences. */
    XkbFilterEvents(pClient, count, events);

#ifdef PANORAMIX
    if(!noPanoramiXExtension &&
       (panoramiXdataPtr[0].x || panoramiXdataPtr[0].y))
    {
	switch(events->u.u.type) {
	case MotionNotify:
	case ButtonPress:
	case ButtonRelease:
	case KeyPress:
	case KeyRelease:
	case EnterNotify:
	case LeaveNotify:
	/*
	   When multiple clients want the same event DeliverEventsToWindow
	   passes the same event structure multiple times so we can't
	   modify the one passed to us
        */
	    count = 1;  /* should always be 1 */
	    memcpy(&eventCopy, events, sizeof(xEvent));
	    eventCopy.u.keyButtonPointer.rootX += panoramiXdataPtr[0].x;
	    eventCopy.u.keyButtonPointer.rootY += panoramiXdataPtr[0].y;
	    if(eventCopy.u.keyButtonPointer.event ==
	       eventCopy.u.keyButtonPointer.root)
	    {
		eventCopy.u.keyButtonPointer.eventX += panoramiXdataPtr[0].x;
		eventCopy.u.keyButtonPointer.eventY += panoramiXdataPtr[0].y;
	    }
	    events = &eventCopy;
	    break;
	default: break;
	}
    }
#endif

    if (EventCallback)
    {
	EventInfoRec eventinfo;
	eventinfo.client = pClient;
	eventinfo.events = events;
	eventinfo.count = count;
	CallCallbacks(&EventCallback, (pointer)&eventinfo);
    }
#ifdef XSERVER_DTRACE
    if (XSERVER_SEND_EVENT_ENABLED()) {
	for (i = 0; i < count; i++)
	{
	    XSERVER_SEND_EVENT(pClient->index, events[i].u.u.type, &events[i]);
	}
    }
#endif
    /* Just a safety check to make sure we only have one GenericEvent, it just
     * makes things easier for me right now. (whot) */
    for (i = 1; i < count; i++)
    {
        if (events[i].u.u.type == GenericEvent)
        {
            ErrorF("[dix] TryClientEvents: Only one GenericEvent at a time.\n");
            return;
        }
    }

    if (events->u.u.type == GenericEvent)
    {
        eventlength += ((xGenericEvent*)events)->length * 4;
    }

    if(pClient->swapped)
    {
        if (eventlength > swapEventLen)
        {
            swapEventLen = eventlength;
            swapEvent = Xrealloc(swapEvent, swapEventLen);
            if (!swapEvent)
            {
                FatalError("WriteEventsToClient: Out of memory.\n");
                return;
            }
        }

	for(i = 0; i < count; i++)
	{
	    eventFrom = &events[i];
            eventTo = swapEvent;

	    /* Remember to strip off the leading bit of type in case
	       this event was sent with "SendEvent." */
	    (*EventSwapVector[eventFrom->u.u.type & 0177])
		(eventFrom, eventTo);

	    WriteToClient(pClient, eventlength, (char *)eventTo);
	}
    }
    else
    {
        /* only one GenericEvent, remember? that means either count is 1 and
         * eventlength is arbitrary or eventlength is 32 and count doesn't
         * matter. And we're all set. Woohoo. */
	WriteToClient(pClient, count * eventlength, (char *) events);
    }
}

/*
 * Set the client pointer for the given client.
 *
 * A client can have exactly one ClientPointer. Each time a
 * request/reply/event is processed and the choice of devices is ambiguous
 * (e.g. QueryPointer request), the server will pick the ClientPointer (see
 * PickPointer()).
 * If a keyboard is needed, the first keyboard paired with the CP is used.
 */
int
SetClientPointer(ClientPtr client, DeviceIntPtr device)
{
    int rc = XaceHook(XACE_DEVICE_ACCESS, client, device, DixUseAccess);
    if (rc != Success)
	return rc;

    if (!IsMaster(device))
    {
        ErrorF("[dix] Need master device for ClientPointer. This is a bug.\n");
        return BadDevice;
    } else if (!device->spriteInfo->spriteOwner)
    {
        ErrorF("[dix] Device %d does not have a sprite. "
                "Cannot be ClientPointer\n", device->id);
        return BadDevice;
    }
    client->clientPtr = device;
    return Success;
}

/* PickPointer will pick an appropriate pointer for the given client.
 *
 * An "appropriate device" is (in order of priority):
 *  1) A device the given client has a core grab on.
 *  2) A device set as ClientPointer for the given client.
 *  3) The first master device.
 */
DeviceIntPtr
PickPointer(ClientPtr client)
{
    DeviceIntPtr it = inputInfo.devices;

    /* First, check if the client currently has a grab on a device. Even
     * keyboards count. */
    for(it = inputInfo.devices; it; it = it->next)
    {
        GrabPtr grab = it->deviceGrab.grab;
        if (grab && grab->grabtype == GRABTYPE_CORE && SameClient(grab, client))
        {
            it = GetMaster(it, MASTER_POINTER);
            return it; /* Always return a core grabbed device */
        }
    }

    if (!client->clientPtr)
    {
        DeviceIntPtr it = inputInfo.devices;
        while (it)
        {
            if (IsMaster(it) && it->spriteInfo->spriteOwner)
            {
                client->clientPtr = it;
                break;
            }
            it = it->next;
        }
    }
    return client->clientPtr;
}

/* PickKeyboard will pick an appropriate keyboard for the given client by
 * searching the list of devices for the keyboard device that is paired with
 * the client's pointer.
 */
DeviceIntPtr
PickKeyboard(ClientPtr client)
{
    DeviceIntPtr ptr = PickPointer(client);
    DeviceIntPtr kbd = GetMaster(ptr, MASTER_KEYBOARD);

    if (!kbd)
    {
        ErrorF("[dix] ClientPointer not paired with a keyboard. This "
                "is a bug.\n");
    }

    return kbd;
}

/* A client that has one or more core grabs does not get core events from
 * devices it does not have a grab on. Legacy applications behave bad
 * otherwise because they are not used to it and the events interfere.
 * Only applies for core events.
 *
 * Return true if a core event from the device would interfere and should not
 * be delivered.
 */
Bool
IsInterferingGrab(ClientPtr client, DeviceIntPtr dev, xEvent* event)
{
    DeviceIntPtr it = inputInfo.devices;

    switch(event->u.u.type)
    {
        case KeyPress:
        case KeyRelease:
        case ButtonPress:
        case ButtonRelease:
        case MotionNotify:
        case EnterNotify:
        case LeaveNotify:
            break;
        default:
            return FALSE;
    }

    if (dev->deviceGrab.grab && SameClient(dev->deviceGrab.grab, client))
        return FALSE;

    while(it)
    {
        if (it != dev)
        {
            if (it->deviceGrab.grab && SameClient(it->deviceGrab.grab, client)
                        && !it->deviceGrab.fromPassiveGrab)
            {
                if ((IsPointerDevice(it) && IsPointerDevice(dev)) ||
                        (IsKeyboardDevice(it) && IsKeyboardDevice(dev)))
                    return TRUE;
            }
        }
        it = it->next;
    }

    return FALSE;
}

