/*****************************************************************************/
/**       Copyright 1988 by Evans & Sutherland Computer Corporation,        **/
/**                          Salt Lake City, Utah                           **/
/**  Portions Copyright 1989 by the Massachusetts Institute of Technology   **/
/**                        Cambridge, Massachusetts                         **/
/**                                                                         **/
/**                           All Rights Reserved                           **/
/**                                                                         **/
/**    Permission to use, copy, modify, and distribute this software and    **/
/**    its documentation  for  any  purpose  and  without  fee is hereby    **/
/**    granted, provided that the above copyright notice appear  in  all    **/
/**    copies and that both  that  copyright  notice  and  this  permis-    **/
/**    sion  notice appear in supporting  documentation,  and  that  the    **/
/**    names of Evans & Sutherland and M.I.T. not be used in advertising    **/
/**    in publicity pertaining to distribution of the  software  without    **/
/**    specific, written prior permission.                                  **/
/**                                                                         **/
/**    EVANS & SUTHERLAND AND M.I.T. DISCLAIM ALL WARRANTIES WITH REGARD    **/
/**    TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES  OF  MERCHANT-    **/
/**    ABILITY  AND  FITNESS,  IN  NO  EVENT SHALL EVANS & SUTHERLAND OR    **/
/**    M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL  DAM-    **/
/**    AGES OR  ANY DAMAGES WHATSOEVER  RESULTING FROM LOSS OF USE, DATA    **/
/**    OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER    **/
/**    TORTIOUS ACTION, ARISING OUT OF OR IN  CONNECTION  WITH  THE  USE    **/
/**    OR PERFORMANCE OF THIS SOFTWARE.                                     **/
/*****************************************************************************/

/***********************************************************************
 *
 * $XConsortium: menus.c,v 1.186 91/07/17 13:58:00 dave Exp $
 *
 * twm menu code
 *
 * 17-Nov-87 Thomas E. LaStrange		File created
 *
 ***********************************************************************/

#include <stdio.h>
#include <signal.h>
#include <X11/Xos.h>
#include <ctype.h> /* DSE */
#include "twm.h"
#include "gc.h"
#include "menus.h"
#include "resize.h"
#include "events.h"
#include "util.h"
#include "parse.h"
#include "gram.h"
#include "screen.h"
#include "desktop.h"
#include <X11/Xmu/CharSet.h>
#include <X11/bitmaps/menu12>
#include "version.h"

extern XEvent Event;

int RootFunction = NULL;
MenuRoot *ActiveMenu = NULL;		/* the active menu */
MenuItem *ActiveItem = NULL;		/* the active menu item */
int MoveFunction;			/* either F_MOVE or F_FORCEMOVE */
int WindowMoved = FALSE;
int menuFromFrameOrWindowOrTitlebar = FALSE;

int ConstMove = FALSE;		/* constrained move variables */
int ConstMoveDir;
int ConstMoveX;
int ConstMoveY;
int ConstMoveXL;
int ConstMoveXR;
int ConstMoveYT;
int ConstMoveYB;

/* Globals used to keep track of whether the mouse has moved during
   a resize function. */
int ResizeOrigX;
int ResizeOrigY;

int MenuDepth = 0;		/* number of menus up */
static struct {
	int x;
	int y;
} MenuOrigins[MAXMENUDEPTH];
static Cursor LastCursor;

void WarpAlongRing(), WarpToWindow();

extern char *Action;
extern int Context;
extern TwmWindow *ButtonWindow, *Tmp_win;
extern XEvent Event, ButtonEvent;
extern char *InitFile;
static void Identify();

#define MAX(x,y) ((x)>(y)?(x):(y))
/* DSE */

#define SHADOWWIDTH 5			/* in pixels */

#define EDGE_OFFSET 5 /* DSE */
#define PULLDOWNMENU_OFFSET ((Scr->RightHandSidePulldownMenus)?\
	(ActiveMenu->width - EDGE_OFFSET * 2 - Scr->pullW):\
	(ActiveMenu->width >> 1)) /* DSE */



/***********************************************************************
 *
 *  Procedure:
 *	InitMenus - initialize menu roots
 *
 ***********************************************************************
 */

void
InitMenus()
{
    int i, j, k;
    FuncKey *key, *tmp;

    for (i = 0; i < MAX_BUTTONS+1; i++)
	for (j = 0; j < NUM_CONTEXTS; j++)
	    for (k = 0; k < MOD_SIZE; k++)
	    {
		Scr->Mouse[i][j][k].func = NULL;
		Scr->Mouse[i][j][k].item = NULL;
	    }

    Scr->DefaultFunction.func = NULL;
    Scr->WindowFunction.func = NULL;

    if (FirstScreen)
    {
	for (key = Scr->FuncKeyRoot.next; key != NULL;)
	{
	    free(key->name);
	    tmp = key;
	    key = key->next;
	    free((char *) tmp);
	}
	Scr->FuncKeyRoot.next = NULL;
    }

}



/***********************************************************************
 *
 *  Procedure:
 *	AddFuncKey - add a function key to the list
 *
 *  Inputs:
 *	name	- the name of the key
 *	cont	- the context to look for the key press in
 *	mods	- modifier keys that need to be pressed
 *	func	- the function to perform
 *	win_name- the window name (if any)
 *	action	- the action string associated with the function (if any)
 *
 ***********************************************************************
 */

Bool AddFuncKey (name, cont, mods, func, win_name, action)
    char *name;
    int cont, mods, func;
    char *win_name;
    char *action;
{
    FuncKey *tmp;
    KeySym keysym;
    KeyCode keycode;

    /*
     * Don't let a 0 keycode go through, since that means AnyKey to the
     * XGrabKey call in GrabKeys().
     */
    if ((keysym = XStringToKeysym(name)) == NoSymbol ||
	(keycode = XKeysymToKeycode(dpy, keysym)) == 0)
    {
	return False;
    }

    /* see if there already is a key defined for this context */
    for (tmp = Scr->FuncKeyRoot.next; tmp != NULL; tmp = tmp->next)
    {
	if (tmp->keysym == keysym &&
	    tmp->cont == cont &&
	    tmp->mods == mods)
	    break;
    }

    if (tmp == NULL)
    {
	tmp = (FuncKey *) malloc(sizeof(FuncKey));
	tmp->next = Scr->FuncKeyRoot.next;
	Scr->FuncKeyRoot.next = tmp;
    }

    tmp->name = name;
    tmp->keysym = keysym;
    tmp->keycode = keycode;
    tmp->cont = cont;
    tmp->mods = mods;
    tmp->func = func;
    tmp->win_name = win_name;
    tmp->action = action;

    return True;
}



int CreateTitleButton (name, func, action, menuroot, rightside, append)
    char *name;
    int func;
    char *action;
    MenuRoot *menuroot;
    Bool rightside;
    Bool append;
{
    TitleButton *tb = (TitleButton *) malloc (sizeof(TitleButton));

    if (!tb) {
	fprintf (stderr,
		 "%s:  unable to allocate %d bytes for title button\n",
		 ProgramName, sizeof(TitleButton));
	return 0;
    }

    tb->next = NULL;
    tb->name = name;			/* note that we are not copying */
    tb->bitmap = None;			/* WARNING, values not set yet */
    tb->width = 0;			/* see InitTitlebarButtons */
    tb->height = 0;			/* ditto */
    tb->func = func;
    tb->action = action;
    tb->menuroot = menuroot;
    tb->rightside = rightside;
    if (rightside) {
	Scr->TBInfo.nright++;
    } else {
	Scr->TBInfo.nleft++;
    }

    /*
     * Cases for list:
     *
     *     1.  empty list, prepend left       put at head of list
     *     2.  append left, prepend right     put in between left and right
     *     3.  append right                   put at tail of list
     *
     * Do not refer to widths and heights yet since buttons not created
     * (since fonts not loaded and heights not known).
     */
    if ((!Scr->TBInfo.head) || ((!append) && (!rightside))) {	/* 1 */
	tb->next = Scr->TBInfo.head;
	Scr->TBInfo.head = tb;
    } else if (append && rightside) {	/* 3 */
	register TitleButton *t;
	for /* SUPPRESS 530 */
	  (t = Scr->TBInfo.head; t->next; t = t->next);
	t->next = tb;
	tb->next = NULL;
    } else {				/* 2 */
	register TitleButton *t, *prev = NULL;
	for (t = Scr->TBInfo.head; t && !t->rightside; t = t->next) {
	    prev = t;
	}
	if (prev) {
	    tb->next = prev->next;
	    prev->next = tb;
	} else {
	    tb->next = Scr->TBInfo.head;
	    Scr->TBInfo.head = tb;
	}
    }

    return 1;
}



/*
 * InitTitlebarButtons - Do all the necessary stuff to load in a titlebar
 * button.  If we can't find the button, then put in a question; if we can't
 * find the question mark, something is wrong and we are probably going to be
 * in trouble later on.
 */
void InitTitlebarButtons ()
{
    TitleButton *tb;
    int h;

    /*
     * initialize dimensions
     */
    Scr->TBInfo.width = (Scr->TitleHeight -
			 2 * (Scr->FramePadding + Scr->ButtonIndent));
    Scr->TBInfo.pad = ((Scr->TitlePadding > 1)
		       ? ((Scr->TitlePadding + 1) / 2) : 1);
    h = Scr->TBInfo.width - 2 * Scr->TBInfo.border;

    /*
     * add in some useful buttons and bindings so that novices can still
     * use the system. -- modified by DSE 
     */

    if (!Scr->NoDefaultTitleButtons) /* DSE */
    	{
		/* insert extra buttons */
		if (!CreateTitleButton (TBPM_ICONIFY, F_ICONIFY, "", (MenuRoot *) NULL,
				False, False))
			{
			fprintf(stderr,"%s:  unable to add iconify button\n",ProgramName);
			}
		if (!CreateTitleButton (TBPM_RESIZE, F_RESIZE, "", (MenuRoot *) NULL,
				True, True))
			{
			fprintf(stderr,"%s:  unable to add resize button\n",ProgramName);
			}
		}
	if (!Scr->NoDefaultMouseOrKeyboardBindings) /* DSE */
		{
		AddDefaultBindings ();
		}

    ComputeCommonTitleOffsets ();

    /*
     * load in images and do appropriate centering
     */

    for (tb = Scr->TBInfo.head; tb; tb = tb->next) {
	tb->bitmap = FindBitmap (tb->name, &tb->width, &tb->height);
	if (!tb->bitmap) {
	    tb->bitmap = FindBitmap (TBPM_QUESTION, &tb->width, &tb->height);
	    if (!tb->bitmap) {		/* cannot happen (see util.c) */
		fprintf (stderr,
			 "%s:  unable to add titlebar button \"%s\"\n",
			 ProgramName, tb->name);
	    }
	}

	tb->dstx = (h - tb->width + 1) / 2;
	if (tb->dstx < 0) {		/* clip to minimize copying */
	    tb->srcx = -(tb->dstx);
	    tb->width = h;
	    tb->dstx = 0;
	} else {
	    tb->srcx = 0;
	}
	tb->dsty = (h - tb->height + 1) / 2;
	if (tb->dsty < 0) {
	    tb->srcy = -(tb->dsty);
	    tb->height = h;
	    tb->dsty = 0;
	} else {
	    tb->srcy = 0;
	}
    }
}



PaintEntry(mr, mi, exposure)
MenuRoot *mr;
MenuItem *mi;
int exposure;
{
    int y_offset;
    int text_y;
    GC gc;
    MyFont *font; /* DSE */

#ifdef DEBUG_MENUS
    fprintf(stderr, "Paint entry\n");
#endif

    y_offset = mi->item_num * Scr->EntryHeight;

    if (mi->func != F_TITLE)
    {
	int x, y;
	
	font = &(Scr->MenuFont); /* DSE */
	text_y = y_offset + font->y; /* DSE */

	if (mi->state)
	{
	    XSetForeground(dpy, Scr->NormalGC, mi->hi_back);

	    XFillRectangle(dpy, mr->w, Scr->NormalGC, 0, y_offset,
		mr->width, Scr->EntryHeight);

		FBF(mi->hi_fore, mi->hi_back, font->font->fid); /* DSE */

	    XDrawString(dpy, mr->w, Scr->NormalGC, mi->x,
		text_y, mi->item, mi->strlen);

	    gc = Scr->NormalGC;
	}
	else
	{
	    if (mi->user_colors || !exposure)
	    {
		XSetForeground(dpy, Scr->NormalGC, mi->back);

		XFillRectangle(dpy, mr->w, Scr->NormalGC, 0, y_offset,
		    mr->width, Scr->EntryHeight);

		FBF(mi->fore, mi->back, font->font->fid); /* DSE */
		
		gc = Scr->NormalGC;
		}
	    else
		gc = Scr->MenuGC;

	    XDrawString(dpy, mr->w, gc, mi->x, text_y, mi->item, mi->strlen);
	}

	if (mi->func == F_MENU)
	{
	    /* create the pull right pixmap if needed */
	    if (Scr->pullPm == None)
	    {
		Scr->pullPm = CreateMenuIcon (Scr->MenuFont.height,
			&Scr->pullW,&Scr->pullH);
	    }
	    x = mr->width - Scr->pullW - EDGE_OFFSET; /* DSE */
	    y = y_offset + ((font->height - Scr->pullH) / 2); /* DSE */
	    XCopyPlane(dpy, Scr->pullPm, mr->w, gc, 0, 0,
		Scr->pullW, Scr->pullH, x, y, 1);
	}
    }
    else
    {
	int y;

	if ( Scr->MenuTitleFont.name != NULL ) /* DSE */
	    font = &(Scr->MenuTitleFont);
	else
	    font = &(Scr->MenuFont);
	
	text_y = y_offset + font->y; /* DSE */

	XSetForeground(dpy, Scr->NormalGC, mi->back);

	/* fill the rectangle with the title background color */
	XFillRectangle(dpy, mr->w, Scr->NormalGC, 0, y_offset,
	    mr->width, Scr->EntryHeight);

	{
	    XSetForeground(dpy, Scr->NormalGC, mi->fore);
	    /* now draw the dividing lines */
	    if (y_offset)
	      XDrawLine (dpy, mr->w, Scr->NormalGC, 0, y_offset,
			 mr->width, y_offset);
	    y = ((mi->item_num+1) * Scr->EntryHeight)-1;
	    XDrawLine(dpy, mr->w, Scr->NormalGC, 0, y, mr->width, y);
	}

	FBF(mi->fore, mi->back, font->font->fid); /* DSE */
	/* finally render the title */
	XDrawString(dpy, mr->w, Scr->NormalGC, mi->x,
	    text_y, mi->item, mi->strlen);
    }
}



PaintMenu(mr, e)
MenuRoot *mr;
XEvent *e;
{
    MenuItem *mi;

    for (mi = mr->first; mi != NULL; mi = mi->next)
    {
	int y_offset = mi->item_num * Scr->EntryHeight;

	/* be smart about handling the expose, redraw only the entries
	 * that we need to
	 */
	if (e->xexpose.y < (y_offset + Scr->EntryHeight) &&
	    (e->xexpose.y + e->xexpose.height) > y_offset)
	{
	    PaintEntry(mr, mi, True);
	}
    }
    XSync(dpy, 0);
}



static Bool fromMenu;
extern int GlobalFirstTime; /* for StayUpMenus -- PF */

UpdateMenu()
{
	MenuItem *mi;
    int i, x, y, x_root, y_root, entry;
	int done;
	MenuItem *badItem = NULL;
	static int firstTime = True;

	fromMenu = TRUE;

	while (TRUE)
	{	/* block until there is an event */
#if 0
		if (!menuFromFrameOrWindowOrTitlebar
		&& ! Scr->StayUpMenus) {
	  XMaskEvent(dpy,
			 ButtonPressMask | ButtonReleaseMask |
			 EnterWindowMask | ExposureMask |
			 VisibilityChangeMask | LeaveWindowMask |
			 ButtonMotionMask, &Event);
	}
	if (Event.type == MotionNotify) {
		/* discard any extra motion events before a release */
		while(XCheckMaskEvent(dpy,
		ButtonMotionMask | ButtonReleaseMask, &Event))
		if (Event.type == ButtonRelease)
			break;
	}
#else
	while (XCheckMaskEvent(dpy, ButtonPressMask | ButtonReleaseMask |
		EnterWindowMask | ExposureMask, &Event))
	{	/* taken from tvtwm */
#endif
	if (!DispatchEvent ()) continue;

	if (Event.type == ButtonRelease )
	{	if (Scr->StayUpMenus)
		{
			if (firstTime == True)
			{	/* it was the first release of the button */
				firstTime = False;
			}
			else
			{	/* thats the second we need to return now */
				firstTime = True;
				menuFromFrameOrWindowOrTitlebar = FALSE;
				fromMenu = FALSE;
				return;
			}
		}
		else
		{	/* not stay-up */
			menuFromFrameOrWindowOrTitlebar = FALSE;
			fromMenu = FALSE;
			return;
		}
	}
	if ( Cancel) return;

	}
	/* if (Event.type != MotionNotify) */
		/* continue; */
	/* if we haven't received the enter notify yet, wait */
	if (!ActiveMenu || !ActiveMenu->entered)
		continue;

	done = FALSE;
	XQueryPointer( dpy, ActiveMenu->w, &JunkRoot, &JunkChild,
		&x_root, &y_root, &x, &y, &JunkMask);

#if 0
	/* if we haven't recieved the enter notify yet, wait */
	if (ActiveMenu && !ActiveMenu->entered)
		continue;
#endif

	XFindContext(dpy, ActiveMenu->w, ScreenContext, (caddr_t *)&Scr);

	if (x < 0 || y < 0 ||
		x >= ActiveMenu->width || y >= ActiveMenu->height)
	{
		if (ActiveItem && ActiveItem->func != F_TITLE)
		{
		ActiveItem->state = 0;
		PaintEntry(ActiveMenu, ActiveItem, False);
		}
		ActiveItem = NULL;
		continue;
	}

	/* look for the entry that the mouse is in */
	entry = y / Scr->EntryHeight;
	for (i = 0, mi = ActiveMenu->first; mi != NULL; i++, mi=mi->next)
	{
		if (i == entry)
		break;
	}

	/* if there is an active item, we might have to turn it off */
	if (ActiveItem)
	{
		/* is the active item the one we are on ? */
		if (ActiveItem->item_num == entry && ActiveItem->state)
		done = TRUE;

		/* if we weren't on the active entry, let's turn the old
		 * active one off
		 */
		if (!done && ActiveItem->func != F_TITLE)
		{
		ActiveItem->state = 0;
		PaintEntry(ActiveMenu, ActiveItem, False);
		}
	}

	/* if we weren't on the active item, change the active item and turn
	 * it on
	 */
	if (!done)
	{
		ActiveItem = mi;
		if (ActiveItem->func != F_TITLE && !ActiveItem->state)
		{
		ActiveItem->state = 1;
		PaintEntry(ActiveMenu, ActiveItem, False);

		if (Scr->StayUpOptionalMenus)            /* PF */
			GlobalFirstTime = firstTime = False; /* PF */
		
		}
	}

	/* now check to see if we were over the arrow of a pull right entry */
	if (ActiveItem->func == F_MENU &&
/*		((ActiveMenu->width - x) < (ActiveMenu->width >> 1))) */
		( x > PULLDOWNMENU_OFFSET )) /* DSE */
	{
		MenuRoot *save = ActiveMenu;
		int savex = MenuOrigins[MenuDepth - 1].x;
		int savey = MenuOrigins[MenuDepth - 1].y;

		if (MenuDepth < MAXMENUDEPTH) {
		PopUpMenu (ActiveItem->sub,
			   (savex + PULLDOWNMENU_OFFSET), /* DSE */
			   (savey + ActiveItem->item_num * Scr->EntryHeight)
			   /*(savey + ActiveItem->item_num * Scr->EntryHeight +
				(Scr->EntryHeight >> 1))*/, False);
		} else if (!badItem) {
		XBell (dpy, 0);
		badItem = ActiveItem;
		}

		/* if the menu did get popped up, unhighlight the active item */
		if (save != ActiveMenu && ActiveItem->state)
		{
		ActiveItem->state = 0;
		PaintEntry(save, ActiveItem, False);
		ActiveItem = NULL;
		}
	}
	if (badItem != ActiveItem) badItem = NULL;
	XFlush(dpy);
	}

}



/***********************************************************************
 *
 *	Procedure:
 *	NewMenuRoot - create a new menu root
 *
 *	Returned Value:
 *	(MenuRoot *)
 *
 *	Inputs:
 *	name	- the name of the menu root
 *
 ***********************************************************************
 */

MenuRoot *
NewMenuRoot(name)
	char *name;
{
	MenuRoot *tmp;

#define UNUSED_PIXEL ((unsigned long) (~0))	/* more than 24 bits */

	tmp = (MenuRoot *) malloc(sizeof(MenuRoot));
	tmp->hi_fore = UNUSED_PIXEL;
	tmp->hi_back = UNUSED_PIXEL;
	tmp->name = name;
	tmp->prev = NULL;
	tmp->first = NULL;
	tmp->last = NULL;
	tmp->items = 0;
	tmp->width = 0;
	tmp->mapped = NEVER_MAPPED;
	tmp->pull = FALSE;
	tmp->w = None;
	tmp->shadow = None;
	tmp->real_menu = FALSE;

	if (Scr->MenuList == NULL)
	{
	Scr->MenuList = tmp;
	Scr->MenuList->next = NULL;
	}

	if (Scr->LastMenu == NULL)
	{
	Scr->LastMenu = tmp;
	Scr->LastMenu->next = NULL;
	}
	else
	{
	Scr->LastMenu->next = tmp;
	Scr->LastMenu = tmp;
	Scr->LastMenu->next = NULL;
	}

	if (strcmp(name, TWM_WINDOWS) == 0)
	Scr->Windows = tmp;

	return (tmp);
}



/***********************************************************************
 *
 *	Procedure:
 *	AddToMenu - add an item to a root menu
 *
 *	Returned Value:
 *	(MenuItem *)
 *
 *	Inputs:
 *	menu	- pointer to the root menu to add the item
 *	item	- the text to appear in the menu
 *	action	- the string to possibly execute
 *	sub	- the menu root if it is a pull-right entry
 *	func	- the numeric function
 *	fore	- foreground color string
 *	back	- background color string
 *
 ***********************************************************************
 */

MenuItem *
AddToMenu(menu, item, action, sub, func, fore, back)
	MenuRoot *menu;
	char *item, *action;
	MenuRoot *sub;
	int func;
	char *fore, *back;
{
	MenuItem *tmp;
	int width;
	MyFont *font; /* DSE */

#ifdef DEBUG_MENUS
	fprintf(stderr, "adding menu item=\"%s\", action=%s, sub=%d, f=%d\n",
	item, action, sub, func);
#endif

	tmp = (MenuItem *) malloc(sizeof(MenuItem));
	tmp->root = menu;

	if (menu->first == NULL)
	{
	menu->first = tmp;
	tmp->prev = NULL;
	}
	else
	{
	menu->last->next = tmp;
	tmp->prev = menu->last;
	}
	menu->last = tmp;

	tmp->item = item;
	tmp->strlen = strlen(item);
	tmp->action = action;
	tmp->next = NULL;
	tmp->sub = NULL;
	tmp->state = 0;
	tmp->func = func;

    if ( func == F_TITLE && (Scr->MenuTitleFont.name != NULL) ) /* DSE */
		font= &(Scr->MenuTitleFont);
    else
		font= &(Scr->MenuFont);

	if (!Scr->HaveFonts) CreateFonts();
	width = XTextWidth(font->font, item, tmp->strlen);
	if (width <= 0)
	width = 1;
	if (width > menu->width)
	menu->width = width;

	tmp->user_colors = FALSE;
	if (Scr->Monochrome == COLOR && fore != NULL)
	{
	int save;

	save = Scr->FirstTime;
	Scr->FirstTime = TRUE;
	GetColor(COLOR, &tmp->fore, fore);
	GetColor(COLOR, &tmp->back, back);
	Scr->FirstTime = save;
	tmp->user_colors = TRUE;
	}
	if (sub != NULL)
	{
	tmp->sub = sub;
	menu->pull = TRUE;
	}
	tmp->item_num = menu->items++;

	return (tmp);
}



MakeMenus()
{
	MenuRoot *mr;

	for (mr = Scr->MenuList; mr != NULL; mr = mr->next)
	{
	if (mr->real_menu == FALSE)
		continue;

	MakeMenu(mr);
	}
}



MakeMenu(mr)
MenuRoot *mr;
{
	MenuItem *start, *end, *cur, *tmp;
	XColor f1, f2, f3;
	XColor b1, b2, b3;
	XColor save_fore, save_back;
	int num, i;
	int fred, fgreen, fblue;
	int bred, bgreen, bblue;
	int width;
	unsigned long valuemask;
	XSetWindowAttributes attributes;
	Colormap cmap = Scr->TwmRoot.cmaps.cwins[0]->colormap->c;
	MyFont *titleFont;
	
	if ( Scr->MenuTitleFont.name != NULL ) /* DSE */
		{
		Scr->EntryHeight = MAX(Scr->MenuFont.height,
		                       Scr->MenuTitleFont.height) + 4;
		titleFont = &(Scr->MenuTitleFont);
		}
	else
		{
		Scr->EntryHeight = Scr->MenuFont.height + 4;
		titleFont= &(Scr->MenuFont);
		}

	/* lets first size the window accordingly */
	if (mr->mapped == NEVER_MAPPED)
	{
	if (mr->pull == TRUE)
	{
		mr->width += 16 + 2 * EDGE_OFFSET; /* DSE */
	}

	width = mr->width + 2 * EDGE_OFFSET; /* DSE */

	for (cur = mr->first; cur != NULL; cur = cur->next)
	{
		if (cur->func != F_TITLE)
		cur->x = EDGE_OFFSET; /* DSE */
		else
		{
		cur->x = width - XTextWidth(titleFont->font, cur->item,
			cur->strlen);
		cur->x /= 2;
		}
	}
	mr->height = mr->items * Scr->EntryHeight;
	mr->width += 10;

	if (Scr->Shadow)
	{
		/*
		 * Make sure that you don't draw into the shadow window or else
		 * the background bits there will get saved
		 */
		valuemask = (CWBackPixel | CWBorderPixel);
		attributes.background_pixel = Scr->MenuShadowColor;
		attributes.border_pixel = Scr->MenuShadowColor;
		if (Scr->SaveUnder) {
		valuemask |= CWSaveUnder;
		attributes.save_under = True;
		}
		mr->shadow = XCreateWindow (dpy, Scr->Root, 0, 0,
					(unsigned int) mr->width,
					(unsigned int) mr->height,
					(unsigned int)0,
					CopyFromParent,
					(unsigned int) CopyFromParent,
					(Visual *) CopyFromParent,
					valuemask, &attributes);
	}

	valuemask = (CWBackPixel | CWBorderPixel | CWEventMask);
	attributes.background_pixel = Scr->MenuC.back;
	attributes.border_pixel = Scr->MenuC.fore;
	attributes.event_mask = (ExposureMask | EnterWindowMask);
	if (Scr->SaveUnder) {
		valuemask |= CWSaveUnder;
		attributes.save_under = True;
	}
	if (Scr->BackingStore) {
		valuemask |= CWBackingStore;
		attributes.backing_store = Always;
	}
	mr->w = XCreateWindow (dpy, Scr->Root, 0, 0, (unsigned int) mr->width,
				   (unsigned int) mr->height, (unsigned int) 1,
				   CopyFromParent, (unsigned int) CopyFromParent,
				   (Visual *) CopyFromParent,
				   valuemask, &attributes);


	XSaveContext(dpy, mr->w, MenuContext, (caddr_t)mr);
	XSaveContext(dpy, mr->w, ScreenContext, (caddr_t)Scr);

	mr->mapped = UNMAPPED;
	}

	/* get the default colors into the menus */
	for (tmp = mr->first; tmp != NULL; tmp = tmp->next)
	{
	if (!tmp->user_colors) {
		if (tmp->func != F_TITLE) {
		tmp->fore = Scr->MenuC.fore;
		tmp->back = Scr->MenuC.back;
		} else {
		tmp->fore = Scr->MenuTitleC.fore;
		tmp->back = Scr->MenuTitleC.back;
		}
	}

	if (mr->hi_fore != UNUSED_PIXEL)
	{
		tmp->hi_fore = mr->hi_fore;
		tmp->hi_back = mr->hi_back;
	}
	else
	{
		tmp->hi_fore = tmp->back;
		tmp->hi_back = tmp->fore;
	}
	}

	if (Scr->Monochrome == MONOCHROME || !Scr->InterpolateMenuColors)
	return;

	start = mr->first;
	while (TRUE)
	{
	for (; start != NULL; start = start->next)
	{
		if (start->user_colors)
		break;
	}
	if (start == NULL)
		break;

	for (end = start->next; end != NULL; end = end->next)
	{
		if (end->user_colors)
		break;
	}
	if (end == NULL)
		break;

	/* we have a start and end to interpolate between */
	num = end->item_num - start->item_num;

	f1.pixel = start->fore;
	XQueryColor(dpy, cmap, &f1);
	f2.pixel = end->fore;
	XQueryColor(dpy, cmap, &f2);

	b1.pixel = start->back;
	XQueryColor(dpy, cmap, &b1);
	b2.pixel = end->back;
	XQueryColor(dpy, cmap, &b2);

	fred = ((int)f2.red - (int)f1.red) / num;
	fgreen = ((int)f2.green - (int)f1.green) / num;
	fblue = ((int)f2.blue - (int)f1.blue) / num;

	bred = ((int)b2.red - (int)b1.red) / num;
	bgreen = ((int)b2.green - (int)b1.green) / num;
	bblue = ((int)b2.blue - (int)b1.blue) / num;

	f3 = f1;
	f3.flags = DoRed | DoGreen | DoBlue;

	b3 = b1;
	b3.flags = DoRed | DoGreen | DoBlue;

	num -= 1;
	for (i = 0, cur = start->next; i < num; i++, cur = cur->next)
	{
		f3.red += fred;
		f3.green += fgreen;
		f3.blue += fblue;
		save_fore = f3;

		b3.red += bred;
		b3.green += bgreen;
		b3.blue += bblue;
		save_back = b3;
		
		if (Scr->DontInterpolateTitles && (cur->func == F_TITLE))
			continue; /* DSE -- from tvtwm */
		
		XAllocColor(dpy, cmap, &f3);
		XAllocColor(dpy, cmap, &b3);
		cur->hi_back = cur->fore = f3.pixel;
		cur->hi_fore = cur->back = b3.pixel;
		cur->user_colors = True;

		f3 = save_fore;
		b3 = save_back;
	}
	start = end;
	}
}



/***********************************************************************
 *
 *	Procedure:
 *	PopUpMenu - pop up a pull down menu
 *
 *	Inputs:
 *	menu	- the root pointer of the menu to pop up
 *	x, y	- location of upper left of menu
 *		center	- whether or not to center horizontally over position
 *
 ***********************************************************************
 */

Bool PopUpMenu (menu, x, y, center)
	MenuRoot *menu;
	int x, y;
	Bool center;
{
	int WindowNameOffset, WindowNameCount;
	TwmWindow **WindowNames;
	TwmWindow *tmp_win2,*tmp_win3;
	int i;
	int (*compar)() =
	  (Scr->CaseSensitive ? strcmp : XmuCompareISOLatin1);

	if (!menu) return False;

	InstallRootColormap();

	if (menu == Scr->Windows)
	{
	TwmWindow *tmp_win;

	/* this is the twm windows menu,  let's go ahead and build it */

	DestroyMenu (menu);

	menu->first = NULL;
	menu->last = NULL;
	menu->items = 0;
	menu->width = 0;
	menu->mapped = NEVER_MAPPED;
  	AddToMenu(menu, "TWM Windows", NULLSTR, NULL, F_TITLE,NULLSTR,NULLSTR);

		WindowNameOffset=(char *)Scr->TwmRoot.next->name -
							   (char *)Scr->TwmRoot.next;
		for(tmp_win = Scr->TwmRoot.next , WindowNameCount=0;
			tmp_win != NULL;
			tmp_win = tmp_win->next)
		  WindowNameCount++;
		WindowNames =
		  (TwmWindow **)malloc(sizeof(TwmWindow *)*WindowNameCount);
		WindowNames[0] = Scr->TwmRoot.next;
		for(tmp_win = Scr->TwmRoot.next->next , WindowNameCount=1;
			tmp_win != NULL;
			tmp_win = tmp_win->next,WindowNameCount++)
		{
			tmp_win2 = tmp_win;
			for (i=0;i<WindowNameCount;i++)
			{
				if ((*compar)(tmp_win2->name,WindowNames[i]->name) < 0)
				{
					tmp_win3 = tmp_win2;
					tmp_win2 = WindowNames[i];
					WindowNames[i] = tmp_win3;
				}
			}
			WindowNames[WindowNameCount] = tmp_win2;
		}
		for (i=0; i<WindowNameCount; i++)
		{
			AddToMenu(menu, WindowNames[i]->name, (char *)WindowNames[i],
					  NULL, F_POPUP,NULL,NULL);
			if (!Scr->OldFashionedTwmWindowsMenu
			&& Scr->Monochrome == COLOR)/*RFBCOLOR*/
			{/*RFBCOLOR*/
				menu->last->user_colors = TRUE;/*RFBCOLOR*/
				menu->last->fore =
					WindowNames[i]->virtual.fore;/*RFBCOLOR*/
				menu->last->back =
					WindowNames[i]->virtual.back;/*RFBCOLOR*/
/**********************************************************/
/*														  */
/*	Okay, okay, it's a bit of a kludge.					  */
/*														  */
/*	On the other hand, it's nice to have the TWM-Windows  */
/*	menu come up with "the right colors". And the colors  */
/*	from the panner are not a bad choice...				  */
/*														  */
/**********************************************************/
			}/*RFBCOLOR*/
		}
		free(WindowNames);

	MakeMenu(menu);
	}

	if (menu->w == None || menu->items == 0) return False;

	/* Prevent recursively bringing up menus. */
	if (menu->mapped == MAPPED) return False;

	/*
	 * Dynamically set the parent;	this allows pull-ups to also be main
	 * menus, or to be brought up from more than one place.
	 */
	menu->prev = ActiveMenu;

	XGrabPointer(dpy, Scr->Root, True,
	ButtonPressMask | ButtonReleaseMask |
	ButtonMotionMask | PointerMotionHintMask,
	GrabModeAsync, GrabModeAsync,
	Scr->Root, Scr->MenuCursor, CurrentTime);

	ActiveMenu = menu;
	menu->mapped = MAPPED;
	menu->entered = FALSE;

	if (center) {
	x -= (menu->width / 2);
	y -= (Scr->EntryHeight / 2);	/* sticky menus would be nice here */
	}

	/*
	 * clip to screen
	 */
	if (x + menu->width > Scr->MyDisplayWidth) {
	x = Scr->MyDisplayWidth - menu->width;
	}
	if (x < 0) x = 0;
	if (y + menu->height > Scr->MyDisplayHeight) {
	y = Scr->MyDisplayHeight - menu->height;
	}
	if (y < 0) y = 0;

	MenuOrigins[MenuDepth].x = x;
	MenuOrigins[MenuDepth].y = y;
	MenuDepth++;

	XMoveWindow(dpy, menu->w, x, y);
	if (Scr->Shadow) {
	XMoveWindow (dpy, menu->shadow, x + SHADOWWIDTH, y + SHADOWWIDTH);
	}
	if (Scr->Shadow) {
	XRaiseWindow (dpy, menu->shadow);
	}
	XMapRaised(dpy, menu->w);
	if (Scr->Shadow) {
	XMapWindow (dpy, menu->shadow);
	}
	XSync(dpy, 0);
	return True;
}



/***********************************************************************
 *
 *	Procedure:
 *	PopDownMenu - unhighlight the current menu selection and
 *		take down the menus
 *
 ***********************************************************************
 */

PopDownMenu()
{
	MenuRoot *tmp;

	if (ActiveMenu == NULL)
	return;

	if (ActiveItem)
	{
	ActiveItem->state = 0;
	PaintEntry(ActiveMenu, ActiveItem, False);
	}

	for (tmp = ActiveMenu; tmp != NULL; tmp = tmp->prev)
	{
	if (Scr->Shadow) {
		XUnmapWindow (dpy, tmp->shadow);
	}
	XUnmapWindow(dpy, tmp->w);
	tmp->mapped = UNMAPPED;
	UninstallRootColormap();
	}

	XFlush(dpy);
	ActiveMenu = NULL;
	ActiveItem = NULL;
	MenuDepth = 0;
	if (Context == C_WINDOW || Context == C_FRAME || Context == C_TITLE)
	{  menuFromFrameOrWindowOrTitlebar = TRUE;
	}
}



/***********************************************************************
 *
 *	Procedure:
 *	FindMenuRoot - look for a menu root
 *
 *	Returned Value:
 *	(MenuRoot *)  - a pointer to the menu root structure
 *
 *	Inputs:
 *	name	- the name of the menu root
 *
 ***********************************************************************
 */

MenuRoot *
FindMenuRoot(name)
	char *name;
{
	MenuRoot *tmp;

	for (tmp = Scr->MenuList; tmp != NULL; tmp = tmp->next)
	{
	if (strcmp(name, tmp->name) == 0)
		return (tmp);
	}
	return NULL;
}



static Bool belongs_to_twm_window (t, w)
	register TwmWindow *t;
	register Window w;
{
	if (!t) return False;

#if 0
StayUpMenus
	if (w == t->frame || w == t->title_w || w == t->hilite_w ||
	w == t->icon_w || w == t->icon_bm_w) return True;
#endif

	if (t && t->titlebuttons) {
	register TBWindow *tbw;
	register int nb = Scr->TBInfo.nleft + Scr->TBInfo.nright;
	for (tbw = t->titlebuttons; nb > 0; tbw++, nb--) {
		if (tbw->window == w) return True;
	}
	}
	return False;
}




/***********************************************************************
 *
 *	Procedure:
 *	resizeFromCenter -
 *
 ***********************************************************************
 */


extern int AddingX;
extern int AddingY;
extern int AddingW;
extern int AddingH;

void resizeFromCenter(w, tmp_win)
	 Window w;
	 TwmWindow *tmp_win;
{
  int lastx, lasty, width, height, bw2;
  int namelen;
  int stat;
  XEvent event;
  Window junk;

  namelen = strlen (tmp_win->name);
  bw2 = tmp_win->frame_bw * 2;
  AddingW = tmp_win->attr.width + bw2;
  AddingH = tmp_win->attr.height + tmp_win->title_height + bw2;
  width = (SIZE_HINDENT + XTextWidth (Scr->SizeFont.font,
					  tmp_win->name, namelen));
  height = Scr->SizeFont.height + SIZE_VINDENT * 2;
  XGetGeometry(dpy, w, &JunkRoot, &origDragX, &origDragY,
		   (unsigned int *)&DragWidth, (unsigned int *)&DragHeight,
		   &JunkBW, &JunkDepth);
  XWarpPointer(dpy, None, w,
		   0, 0, 0, 0, DragWidth/2, DragHeight/2);
  XQueryPointer (dpy, Scr->Root, &JunkRoot,
		 &JunkChild, &JunkX, &JunkY,
		 &AddingX, &AddingY, &JunkMask);
/*****
  Scr->SizeStringOffset = width +
	XTextWidth(Scr->SizeFont.font, ": ", 2);
  XResizeWindow (dpy, Scr->SizeWindow, Scr->SizeStringOffset +
		 Scr->SizeStringWidth, height);
  XDrawImageString (dpy, Scr->SizeWindow, Scr->NormalGC, width,
			SIZE_VINDENT + Scr->SizeFont.font->ascent,
			": ", 2);
*****/
  lastx = -10000;
  lasty = -10000;
/*****
  MoveOutline(Scr->Root,
		  origDragX - JunkBW, origDragY - JunkBW,
		  DragWidth * JunkBW, DragHeight * JunkBW,
		  tmp_win->frame_bw,
		  tmp_win->title_height);
*****/
  MenuStartResize(tmp_win, origDragX, origDragY, DragWidth, DragHeight);
  while (TRUE)
	{
	  XMaskEvent(dpy,
		 ButtonPressMask | PointerMotionMask, &event);

	  if (event.type == MotionNotify) {
	/* discard any extra motion events before a release */
	while(XCheckMaskEvent(dpy,
				  ButtonMotionMask | ButtonPressMask, &event))
	  if (event.type == ButtonPress)
		break;
	  }

	  if (event.type == ButtonPress)
	{
	  MenuEndResize(tmp_win);
	/*
	  XMoveResizeWindow(dpy, w, AddingX, AddingY, AddingW, AddingH);
*/
	  break;
	}

/*	  if (!DispatchEvent ()) continue; */

	  if (event.type != MotionNotify) {
	continue;
	  }

	  /*
	   * XXX - if we are going to do a loop, we ought to consider
	   * using multiple GXxor lines so that we don't need to
	   * grab the server.
	   */
	  XQueryPointer(dpy, Scr->Root, &JunkRoot, &JunkChild,
			&JunkX, &JunkY, &AddingX, &AddingY, &JunkMask);

	  if (lastx != AddingX || lasty != AddingY)
	{
	  MenuDoResize(AddingX, AddingY, tmp_win);

	  lastx = AddingX;
	  lasty = AddingY;
	}

	}
}

/* Jason P. Venner jason@tfs.com
** This function is used by the WARPTO call to match the action name
** against window names
*/

int MatchWinName( action, action_len, t )
char* action;
int		  action_len;
struct TwmWindow* t;
{
	return ( ! strncmp( action, t->full_name, action_len )
		|| ! strncmp( action, t->name, action_len )
		|| ! strncmp( action, t->class.res_class, action_len )
		|| ! strncmp( action, t->class.res_name, action_len ));
}



/***********************************************************************
 *
 *	Procedure:
 *	ExecuteFunction - execute a twm root function
 *
 *	Inputs:
 *	func	- the function to execute
 *	action	- the menu action to execute
 *	w	- the window to execute this function on
 *	tmp_win	- the twm window structure
 *	event	- the event that caused the function
 *	context - the context in which the button was pressed
 *	pulldown- flag indicating execution from pull down menu
 *
 *	Returns:
 *	TRUE if should continue with remaining actions else FALSE to abort
 *
 ***********************************************************************
 */

extern int MovedFromKeyPress;

int
ExecuteFunction(func, action, w, tmp_win, eventp, context, pulldown)
	int func;
	char *action;
	Window w;
	TwmWindow *tmp_win;
	XEvent *eventp;
	int context;
	int pulldown;
{
	static Time last_time = 0;
	char tmp[200];
	char *ptr;
	char buff[MAX_FILE_SIZE];
	int count, fd;
	Window rootw;
	int origX, origY;
	int do_next_action = TRUE;
	int moving_icon = FALSE;
	Bool fromtitlebar = False;
	extern int ConstrainedMoveTime;

	RootFunction = NULL;
	if (Cancel)
	return TRUE;			/* XXX should this be FALSE? */

	switch (func)
	{
	case F_UPICONMGR:
	case F_LEFTICONMGR:
	case F_RIGHTICONMGR:
	case F_DOWNICONMGR:
	case F_FORWICONMGR:
	case F_BACKICONMGR:
	case F_NEXTICONMGR:
	case F_PREVICONMGR:
	case F_NOP:
	case F_TITLE:
	case F_DELTASTOP:
	case F_RAISELOWER:
	case F_WARP:          /* PF */
	case F_WARPCLASSNEXT: /* PF */
	case F_WARPCLASSPREV: /* PF */
	case F_WARPTOSCREEN:
	case F_WARPTO:
	case F_WARPRING:
	case F_WARPTOICONMGR:
	case F_COLORMAP:
	break;
	default:
		XGrabPointer(dpy, Scr->Root, True,
			ButtonPressMask | ButtonReleaseMask,
			GrabModeAsync, GrabModeAsync,
			Scr->Root, Scr->WaitCursor, CurrentTime);
	break;
	}

	switch (func)
	{
	case F_NOP:
	case F_TITLE:
	break;

	case F_DELTASTOP:
	if (WindowMoved) do_next_action = FALSE;
	break;

	case F_RESTART:
	SetRealScreen(0,0);
	XSync (dpy, 0);
	Reborder (eventp->xbutton.time);
	XSync (dpy, 0);
	execvp(*Argv, Argv);
	fprintf (stderr, "%s:  unable to restart:  %s\n", ProgramName, *Argv);
	break;

	case F_UPICONMGR:
	case F_DOWNICONMGR:
	case F_LEFTICONMGR:
	case F_RIGHTICONMGR:
    case F_FORWICONMGR:
    case F_BACKICONMGR:
	MoveIconManager(func);
        break;

    case F_NEXTICONMGR:
    case F_PREVICONMGR:
	JumpIconManager(func);
        break;

    case F_SHOWLIST:
	if (Scr->NoIconManagers)
	    break;
	DeIconify(Scr->iconmgr.twm_win);
	XRaiseWindow(dpy, Scr->iconmgr.twm_win->frame);
	XRaiseWindow(dpy, Scr->iconmgr.twm_win->VirtualDesktopDisplayWindow);
	
	RaiseStickyAbove(); /* DSE */
	RaiseAutoPan();
	
	break;

    case F_HIDELIST:
	if (Scr->NoIconManagers)
	    break;
	HideIconManager ();
	break;

    case F_SORTICONMGR:
	if (DeferExecution(context, func, Scr->SelectCursor))
	    return TRUE;

	{
	    int save_sort;

	    save_sort = Scr->SortIconMgr;
	    Scr->SortIconMgr = TRUE;

	    if (context == C_ICONMGR)
		SortIconManager((IconMgr *) NULL);
	    else if (tmp_win->iconmgr)
		SortIconManager(tmp_win->iconmgrp);
	    else
		XBell(dpy, 0);

	    Scr->SortIconMgr = save_sort;
	}
	break;

    case F_IDENTIFY:
	if (DeferExecution(context, func, Scr->SelectCursor))
	{
	    return TRUE;
	}

	Identify(tmp_win);
	break;

    case F_VERSION:
	Identify ((TwmWindow *) NULL);
	break;

	case F_ZOOMZOOM: /* RFB silly */
		Zoom( None, None );
		break;

	case F_AUTOPAN:/*RFB F_AUTOPAN*/
	{ /* toggle autopan *//*RFB F_AUTOPAN*/
		static int saved;/*RFB F_AUTOPAN*/

		if ( Scr->AutoPanX )
		{	saved = Scr->AutoPanX;/*RFB F_AUTOPAN*/
			Scr->AutoPanX = 0;/*RFB F_AUTOPAN*/
		} else { /*RFB F_AUTOPAN*/
			Scr->AutoPanX = saved;/*RFB F_AUTOPAN*/
			/* if restart with no autopan, we'll set the
			** variable but we won't pan
			*/
			RaiseAutoPan(); /* DSE */
		}/*RFB F_AUTOPAN*/
		break;/*RFB F_AUTOPAN*/
	}/*RFB F_AUTOPAN*/
	
	case F_STICKYABOVE: /* DSE */
		if (Scr->StickyAbove) {
			LowerSticky(); Scr->StickyAbove = FALSE;
			/* don't change the order of execution! */
		} else {
			Scr->StickyAbove = TRUE; RaiseStickyAbove(); RaiseAutoPan();
			/* don't change the order of execution! */
		}
		return TRUE;
		break;

    case F_AUTORAISE:
	if (DeferExecution(context, func, Scr->SelectCursor))
	    return TRUE;

	tmp_win->auto_raise = !tmp_win->auto_raise;
	if (tmp_win->auto_raise) ++(Scr->NumAutoRaises);
	else --(Scr->NumAutoRaises);
	break;

    case F_BEEP:
	XBell(dpy, 0);
	break;

    case F_POPUP:
	tmp_win = (TwmWindow *)action;
	if (Scr->WindowFunction.func != NULL)
	{
	   ExecuteFunction(Scr->WindowFunction.func,
			   Scr->WindowFunction.item->action,
			   w, tmp_win, eventp, C_FRAME, FALSE);
	}
	else
	{
	    DeIconify(tmp_win);
	    XRaiseWindow (dpy, tmp_win->frame);
	    XRaiseWindow (dpy, tmp_win->VirtualDesktopDisplayWindow);
	    
	    RaiseStickyAbove();
	    RaiseAutoPan();
	}
	break;

    case F_RESIZE:
	EventHandler[EnterNotify] = HandleUnknown;
	EventHandler[LeaveNotify] = HandleUnknown;
	if (DeferExecution(context, func, Scr->MoveCursor))
	{
	    return TRUE;
	}

	PopDownMenu();

	if (pulldown)
	    XWarpPointer(dpy, None, Scr->Root,
		0, 0, 0, 0, eventp->xbutton.x_root, eventp->xbutton.y_root);

	if ((w != tmp_win->icon_w) && (context != C_DOOR)) {	/* can't resize icons or doors */

	  if ((Context == C_FRAME || Context == C_WINDOW || Context == C_TITLE)
	      && ( fromMenu | menuFromFrameOrWindowOrTitlebar ))
	  {	resizeFromCenter(w, tmp_win);
		}
	  else {
			long releaseEvent;
			long movementMask;
	    /*
	     * see if this is being done from the titlebar
	     */
	    fromtitlebar =
	      belongs_to_twm_window (tmp_win, eventp->xbutton.window);
	    /* Save pointer position so we can tell if it was moved or
	       not during the resize. */
	    ResizeOrigX = eventp->xbutton.x_root;
	    ResizeOrigY = eventp->xbutton.y_root;

	    StartResize (eventp, tmp_win, fromtitlebar, context);

	    do {
			releaseEvent = menuFromFrameOrWindowOrTitlebar ?
				ButtonPress : ButtonRelease;
			movementMask = menuFromFrameOrWindowOrTitlebar ?
				PointerMotionMask : ButtonMotionMask;

	      XMaskEvent(dpy,
			   ButtonPressMask | ButtonReleaseMask |
			   EnterWindowMask | LeaveWindowMask |
			   movementMask, &Event);

		if (fromtitlebar && Event.type == ButtonPress) {
		  fromtitlebar = False;
		    continue;
		  }

	    	if (Event.type == MotionNotify) {
		  /* discard any extra motion events before a release */
		  while
		    (XCheckMaskEvent
		     (dpy, ButtonMotionMask | ButtonReleaseMask, &Event))
		      {
			if (Event.type == releaseEvent )
			break;
		}
		}

	      if (!DispatchEvent ()) continue;

	    } while (!(Event.type == ButtonRelease || Cancel));
	    return TRUE;
	  }
	}

	break;


    case F_ZOOM:
    case F_HORIZOOM:
    case F_FULLZOOM:
    case F_LEFTZOOM:
    case F_RIGHTZOOM:
    case F_TOPZOOM:
    case F_BOTTOMZOOM:
	if (DeferExecution(context, func, Scr->SelectCursor))
	    return TRUE;
	fullzoom(tmp_win, func);
	/* UpdateDesktop(tmp_win); Stig */
	MoveResizeDesktop(tmp_win, Scr->NoRaiseMove); /* Stig */
	break;

    case F_MOVE:
    case F_FORCEMOVE:
	{
		if ( DeferExecution( context, func, Scr->MoveCursor ))
		{
			return TRUE;
		}
		PopDownMenu();
		rootw = eventp->xbutton.root;
		MoveFunction = func;

		if (pulldown)
		{
			XWarpPointer(dpy, None, Scr->Root,
				0, 0, 0, 0, eventp->xbutton.x_root,
				eventp->xbutton.y_root);
		}

		EventHandler[EnterNotify] = HandleUnknown;
		EventHandler[LeaveNotify] = HandleUnknown;

		if (!Scr->NoGrabServer || !Scr->OpaqueMove)
		{	XGrabServer(dpy);
		}
		XGrabPointer(dpy, eventp->xbutton.root, True,
			ButtonPressMask | ButtonReleaseMask |
			ButtonMotionMask | PointerMotionMask,
			/* PointerMotionHintMask */
			GrabModeAsync, GrabModeAsync,
			Scr->Root, Scr->MoveCursor, CurrentTime);

		if (context == C_ICON && tmp_win->icon_w)
		{
			w = tmp_win->icon_w;
			DragX = eventp->xbutton.x;
			DragY = eventp->xbutton.y;
			moving_icon = TRUE;
		}
		else if (w != tmp_win->icon_w)
		{
			XTranslateCoordinates(dpy, w, tmp_win->frame,
				eventp->xbutton.x,
				eventp->xbutton.y,
				&DragX, &DragY, &JunkChild);

			w = tmp_win->frame;
		}

		DragWindow = None;

		XGetGeometry(dpy, w, &JunkRoot, &origDragX, &origDragY,
			(unsigned int *)&DragWidth, (unsigned int *)&DragHeight, &JunkBW,
			&JunkDepth);

		origX = eventp->xbutton.x_root;
		origY = eventp->xbutton.y_root;
		CurrentDragX = origDragX;
		CurrentDragY = origDragY;

		/*
		* only do the constrained move if timer is set; need to check it
		* in case of stupid or wicked fast servers
		*/
		if ( ConstrainedMoveTime
		&& ( eventp->xbutton.time - last_time ) < ConstrainedMoveTime )
		{	int width, height;

			ConstMove = TRUE;
			ConstMoveDir = MOVE_NONE;
			ConstMoveX = eventp->xbutton.x_root - DragX - JunkBW;
			ConstMoveY = eventp->xbutton.y_root - DragY - JunkBW;
			width = DragWidth + 2 * JunkBW;
			height = DragHeight + 2 * JunkBW;
			ConstMoveXL = ConstMoveX + width/3;
			ConstMoveXR = ConstMoveX + 2*(width/3);
			ConstMoveYT = ConstMoveY + height/3;
			ConstMoveYB = ConstMoveY + 2*(height/3);

			XWarpPointer(dpy, None, w,
				0, 0, 0, 0, DragWidth/2, DragHeight/2);

			XQueryPointer(dpy, w, &JunkRoot, &JunkChild,
				&JunkX, &JunkY, &DragX, &DragY, &JunkMask);
		}
		last_time = eventp->xbutton.time;

		if (!Scr->OpaqueMove)
		{
			InstallRootColormap();
			if ( !Scr->MoveDelta )
			{
				/*
				* Draw initial outline.  This was previously done the
				* first time though the outer loop by dropping out of
				* the XCheckMaskEvent inner loop down to one of the
				* MoveOutline's below.
				*/
				MoveOutline( rootw,
					origDragX - JunkBW, origDragY - JunkBW,
					DragWidth + 2 * JunkBW, DragHeight + 2 * JunkBW,
					tmp_win->frame_bw,
					moving_icon ? 0 : tmp_win->title_height);
				/*
				* This next line causes HandleReleaseNotify to call
				* XRaiseWindow().  This is solely to preserve the
				* previous behaviour that raises a window being moved
				* on button release even if you never actually moved
				* any distance (unless you move less than MoveDelta or
				* NoRaiseMove is set or OpaqueMove is set).
				*/
				DragWindow = w;
			}
		}

		/*
		* see if this is being done from the titlebar
		*/
		fromtitlebar =
			belongs_to_twm_window (tmp_win, eventp->xbutton.window);

		if ( menuFromFrameOrWindowOrTitlebar
		&& ! fromtitlebar
		)
		{	/* warp the pointer to the middle of the window */
			XWarpPointer(dpy, None, Scr->Root, 0, 0, 0, 0,
				origDragX + DragWidth / 2,
				origDragY + DragHeight / 2);
				XFlush(dpy);
		}

		while (TRUE)
		{
			long releaseEvent = menuFromFrameOrWindowOrTitlebar ?
				ButtonPress : ButtonRelease;
			long movementMask = menuFromFrameOrWindowOrTitlebar ?
				PointerMotionMask : ButtonMotionMask;

			/* block until there is an interesting event */
			XMaskEvent(dpy, ButtonPressMask | ButtonReleaseMask |
				EnterWindowMask | LeaveWindowMask |
				ExposureMask | movementMask |
				VisibilityChangeMask, &Event);

			/* throw away enter and leave events until release */
			if (Event.xany.type == EnterNotify
			|| Event.xany.type == LeaveNotify)
			{	continue;
			}

			if (Event.type == MotionNotify)
			{	/* discard any extra motion events before a logical release */
				while(XCheckMaskEvent(dpy,
					movementMask | releaseEvent, &Event))
						if (Event.type == releaseEvent)
					break;
			}

			/* test to see if we have a second button press to abort move */
			/*	    if (!menuFromFrameOrWindowOrTitlebar)
			** if (Event.type == ButtonPress && DragWindow != None)
			** {	if (Scr->OpaqueMove)
			**		XMoveWindow (dpy, DragWindow, origDragX, origDragY);
			**	else
			**		MoveOutline(Scr->Root, 0, 0, 0, 0, 0, 0);
			**		DragWindow = None;
			** }
			*/
			if (!menuFromFrameOrWindowOrTitlebar &&  !MovedFromKeyPress)
			{	if (Event.type == ButtonPress && DragWindow != None)
				{	if (Scr->OpaqueMove)
						XMoveWindow (dpy, DragWindow, origDragX, origDragY);
					else
						MoveOutline(Scr->Root, 0, 0, 0, 0, 0, 0);
						DragWindow = None;
				}
			}



			if (fromtitlebar && Event.type == ButtonPress)
			{	fromtitlebar = False;
				CurrentDragX = origX = Event.xbutton.x_root;
				CurrentDragY = origY = Event.xbutton.y_root;
				XTranslateCoordinates (dpy, rootw, tmp_win->frame,
					origX, origY,
					&DragX, &DragY, &JunkChild);
				continue;
			}

			if (!DispatchEvent2 ()) continue;

			if (Cancel)
			{
				WindowMoved = FALSE;
				if (!Scr->OpaqueMove) UninstallRootColormap();
				return TRUE;	/* XXX should this be FALSE? */
			}
			if ( Event.type == releaseEvent )
			{
				MoveOutline(rootw, 0, 0, 0, 0, 0, 0);
				if (moving_icon
				&& ((CurrentDragX != origDragX
					|| CurrentDragY != origDragY)))
				{	tmp_win->icon_moved = TRUE;
				}

				if (!Scr->OpaqueMove && menuFromFrameOrWindowOrTitlebar)
				{	XMoveWindow(dpy, DragWindow,
						Event.xbutton.x_root - DragWidth / 2,
						Event.xbutton.y_root - DragHeight / 2);
				}
				DragWindow=NULL;	/* SatyUpMenus */
				break;
			}

			/* something left to do only if the pointer moved */
			if (Event.type != MotionNotify) continue;

			XQueryPointer(dpy, rootw, &(eventp->xmotion.root), &JunkChild,
				&(eventp->xmotion.x_root), &(eventp->xmotion.y_root),
				&JunkX, &JunkY, &JunkMask);

			if (DragWindow == None &&
				abs(eventp->xmotion.x_root - origX) < Scr->MoveDelta &&
				abs(eventp->xmotion.y_root - origY) < Scr->MoveDelta)
					continue;

			WindowMoved = TRUE;
			DragWindow = w;

			if (!Scr->NoRaiseMove && Scr->OpaqueMove)	/* can't restore... */
				XRaiseWindow(dpy, DragWindow);

			if (ConstMove)
			{
				switch (ConstMoveDir)
				{
					case MOVE_NONE:
						if ( eventp->xmotion.x_root < ConstMoveXL
						|| eventp->xmotion.x_root > ConstMoveXR )
							ConstMoveDir = MOVE_HORIZ;

						if ( eventp->xmotion.y_root < ConstMoveYT
						|| eventp->xmotion.y_root > ConstMoveYB)
							ConstMoveDir = MOVE_VERT;

						XQueryPointer(dpy, DragWindow, &JunkRoot,
							&JunkChild, &JunkX, &JunkY,
							&DragX, &DragY, &JunkMask);
						break;

					case MOVE_VERT:
						ConstMoveY =
							eventp->xmotion.y_root - DragY - JunkBW;
						break;

					case MOVE_HORIZ:
						ConstMoveX =
							eventp->xmotion.x_root - DragX - JunkBW;
						break;
				}

				if ( ConstMoveDir != MOVE_NONE )
				{
					int xl, yt, xr, yb, w, h;

					xl = ConstMoveX;
					yt = ConstMoveY;
					w = DragWidth + 2 * JunkBW;
					h = DragHeight + 2 * JunkBW;

					if ( Scr->DontMoveOff
					&& MoveFunction != F_FORCEMOVE )
					{
						xr = xl + w;
						yb = yt + h;

						if (xl < 0) xl = 0;
						if (xr > Scr->MyDisplayWidth)
							xl = Scr->MyDisplayWidth - w;

						if (yt < 0) yt = 0;
						if (yb > Scr->MyDisplayHeight)
							yt = Scr->MyDisplayHeight - h;
					}
					CurrentDragX = xl;
					CurrentDragY = yt;
					if (Scr->OpaqueMove)
						XMoveWindow(dpy, DragWindow, xl, yt);
					else
						MoveOutline(eventp->xmotion.root, xl, yt, w, h,
							tmp_win->frame_bw,
							moving_icon ? 0 : tmp_win->title_height);

					/* move the small representation window */
					/* this knows a bit much about the internals i guess */
					XMoveWindow(dpy, tmp_win->VirtualDesktopDisplayWindow,
						SCALE_D(xl), SCALE_D(yt));
				}
			}
			else if (DragWindow != None)
			{
				int xl, yt, xr, yb, w, h;

				if (!menuFromFrameOrWindowOrTitlebar)
				{
					xl = eventp->xmotion.x_root - DragX - JunkBW;
					yt = eventp->xmotion.y_root - DragY - JunkBW;
				}
				else
				{
					xl = eventp->xmotion.x_root - (DragWidth / 2);
					yt = eventp->xmotion.y_root - (DragHeight / 2);
				}
				w = DragWidth + 2 * JunkBW;
				h = DragHeight + 2 * JunkBW;

				if (Scr->DontMoveOff && MoveFunction != F_FORCEMOVE)
				{
					xr = xl + w;
					yb = yt + h;

					if (xl < 0) xl = 0;
					if (xr > Scr->MyDisplayWidth)
						xl = Scr->MyDisplayWidth - w;

					if (yt < 0) yt = 0;
					if (yb > Scr->MyDisplayHeight)
						yt = Scr->MyDisplayHeight - h;
				}

				CurrentDragX = xl;
				CurrentDragY = yt;
				if (Scr->OpaqueMove)
					XMoveWindow(dpy, DragWindow, xl, yt);
				else
					MoveOutline(eventp->xmotion.root, xl, yt, w, h,
						tmp_win->frame_bw,
						moving_icon ? 0 : tmp_win->title_height);

				/* move the small representation window */
				/* this knows a bit much about the internals i guess */
				/* left out at the minute */
				/* XMoveWindow(dpy, tmp_win->VirtualDesktopDisplayWindow,
					SCALE_D(xl), SCALE_D(yt)); */
			}

		}

		MovedFromKeyPress = False;

		if (!Scr->OpaqueMove && DragWindow == None)
			UninstallRootColormap();

		/* update virtual coords */
		tmp_win->virtual_frame_x = Scr->VirtualDesktopX + tmp_win->frame_x;
		tmp_win->virtual_frame_y = Scr->VirtualDesktopY + tmp_win->frame_y;

		UpdateDesktop(tmp_win);

		break;
	}
    case F_FUNCTION:
	{
	    MenuRoot *mroot;
	    MenuItem *mitem;

	    if ((mroot = FindMenuRoot(action)) == NULL)
	    {
		fprintf (stderr, "%s: couldn't find function \"%s\"\n",
			 ProgramName, action);
		return TRUE;
	    }

	    if (NeedToDefer(mroot) && DeferExecution(context, func, Scr->SelectCursor))
		return TRUE;
	    else
	    {
		for (mitem = mroot->first; mitem != NULL; mitem = mitem->next)
		{
		    if (!ExecuteFunction (mitem->func, mitem->action, w,
					  tmp_win, eventp, context, pulldown))
		      break;
		}
	    }
	}
	break;

    case F_DEICONIFY:
    case F_ICONIFY:
	if (DeferExecution(context, func, Scr->SelectCursor))
	    return TRUE;

	if (tmp_win->icon)
	{
	    DeIconify(tmp_win);
	}
        else if (func == F_ICONIFY &&
            (tmp_win->list || !Scr->NoIconifyIconManagers)) /* PF */
	{
	    Iconify (tmp_win, eventp->xbutton.x_root - EDGE_OFFSET, /* DSE */
		     eventp->xbutton.y_root - EDGE_OFFSET); /* DSE */
	}
	break;

    case F_RAISELOWER:
	if (DeferExecution(context, func, Scr->SelectCursor))
	    return TRUE;

	if (!WindowMoved) {
	    XWindowChanges xwc;

	    xwc.stack_mode = Opposite;
	    if (w != tmp_win->icon_w)
	      w = tmp_win->frame;
	    XConfigureWindow (dpy, w, CWStackMode, &xwc);
	    XConfigureWindow (dpy, tmp_win->VirtualDesktopDisplayWindow, CWStackMode, &xwc);
	    /* ug */
	    XLowerWindow(dpy, Scr->VirtualDesktopDScreen);
	}
	break;

    case F_RAISE:
	if (DeferExecution(context, func, Scr->SelectCursor))
	    return TRUE;

	/* check to make sure raise is not from the WindowFunction */
	if (w == tmp_win->icon_w && Context != C_ROOT)
	    XRaiseWindow(dpy, tmp_win->icon_w);
	else
	{
	    XRaiseWindow(dpy, tmp_win->frame);
	    XRaiseWindow(dpy, tmp_win->VirtualDesktopDisplayWindow);
	}

	RaiseStickyAbove(); /* DSE */
	RaiseAutoPan();

	break;

    case F_LOWER:
	if (DeferExecution(context, func, Scr->SelectCursor))
	    return TRUE;

	if (!(Scr->StickyAbove && tmp_win->nailed)) { /* DSE */
		if (w == tmp_win->icon_w)
		    XLowerWindow(dpy, tmp_win->icon_w);
		else
		{    XLowerWindow(dpy, tmp_win->frame);
			XLowerWindow(dpy, tmp_win->VirtualDesktopDisplayWindow);
			XLowerWindow(dpy, Scr->VirtualDesktopDScreen);
		}
	} /* DSE */

	break;

    case F_FOCUS:
	if (DeferExecution(context, func, Scr->SelectCursor))
	    return TRUE;

	if (tmp_win->icon == FALSE)
	{
	    if (!Scr->FocusRoot && Scr->Focus == tmp_win)
	    {
		FocusOnRoot();
	    }
	    else
	    {
		if (Scr->Focus != NULL) {
		    SetBorder (Scr->Focus, False);
		    if (Scr->Focus->hilite_w)
		      XUnmapWindow (dpy, Scr->Focus->hilite_w);
		}

		InstallWindowColormaps (0, tmp_win);
		if (tmp_win->hilite_w) XMapWindow (dpy, tmp_win->hilite_w);
		SetBorder (tmp_win, True);
		SetFocus (tmp_win, eventp->xbutton.time);
		Scr->FocusRoot = FALSE;
		Scr->Focus = tmp_win;
	    }
	}
	break;

    case F_DESTROY:
	if (DeferExecution(context, func, Scr->DestroyCursor))
	    return TRUE;

	if (tmp_win->iconmgr)
	    XBell(dpy, 0);
	else
	    XKillClient(dpy, tmp_win->w);
	break;

    case F_DELETE:
	if (DeferExecution(context, func, Scr->DestroyCursor))
	    return TRUE;

	if (tmp_win->iconmgr)		/* don't send ourself a message */
	  HideIconManager ();
	else if (tmp_win->protocols & DoesWmDeleteWindow)
	  SendDeleteWindowMessage (tmp_win, LastTimestamp());
	else
	  XBell (dpy, 0);
	break;

    case F_SAVEYOURSELF:
	if (DeferExecution (context, func, Scr->SelectCursor))
	  return TRUE;

	if (tmp_win->protocols & DoesWmSaveYourself)
	  SendSaveYourselfMessage (tmp_win, LastTimestamp());
	else
	  XBell (dpy, 0);
	break;

    case F_CIRCLEUP:
	XCirculateSubwindowsUp(dpy, Scr->Root);
	break;

    case F_CIRCLEDOWN:
	XCirculateSubwindowsDown(dpy, Scr->Root);
	break;

    case F_EXEC:
	PopDownMenu();
	if (!Scr->NoGrabServer) {
	    XUngrabServer (dpy);
	    XSync (dpy, 0);
	}
	Execute(action);
	break;

    case F_UNFOCUS:
	FocusOnRoot();
	break;

    case F_CUT:
	strcpy(tmp, action);
	strcat(tmp, "\n");
	XStoreBytes(dpy, tmp, strlen(tmp));
	break;

    case F_CUTFILE:
	ptr = XFetchBytes(dpy, &count);
	if (ptr) {
	    if (sscanf (ptr, "%s", tmp) == 1) {
		XFree (ptr);
		ptr = ExpandFilename(tmp);
		if (ptr) {
		    fd = open (ptr, 0);
		    if (fd >= 0) {
			count = read (fd, buff, MAX_FILE_SIZE - 1);
			if (count > 0) XStoreBytes (dpy, buff, count);
			close(fd);
		    } else {
			fprintf (stderr,
				 "%s:  unable to open cut file \"%s\"\n",
				 ProgramName, tmp);
		    }
		    if (ptr != tmp) free (ptr);
		}
	    } else {
		XFree(ptr);
	    }
	} else {
	    fprintf(stderr, "%s:  cut buffer is empty\n", ProgramName);
	}
	break;

    case F_WARPTOSCREEN:
	{
	    if (strcmp (action, WARPSCREEN_NEXT) == 0) {
		WarpToScreen (Scr->screen + 1, 1);
	    } else if (strcmp (action, WARPSCREEN_PREV) == 0) {
		WarpToScreen (Scr->screen - 1, -1);
	    } else if (strcmp (action, WARPSCREEN_BACK) == 0) {
		WarpToScreen (PreviousScreen, 0);
	    } else {
		WarpToScreen (atoi (action), 0);
	    }
	}
	break;

    case F_COLORMAP:
	{
	    if (strcmp (action, COLORMAP_NEXT) == 0) {
		BumpWindowColormap (tmp_win, 1);
	    } else if (strcmp (action, COLORMAP_PREV) == 0) {
		BumpWindowColormap (tmp_win, -1);
	    } else {
		BumpWindowColormap (tmp_win, 0);
	    }
	}
	break;

    case F_WARPCLASSNEXT: /* PF */
    case F_WARPCLASSPREV: /* PF */
		WarpClass(func == F_WARPCLASSNEXT, tmp_win, action);
		break;

    case F_WARPTONEWEST: /* PF */
		if (Scr->Newest)
		    WarpToWindow(Scr->Newest);
		else
	    	XBell (dpy, 0);
		break;
	
    case F_WARPTO:                                              
		{                                                       
	    register TwmWindow *t;                                  
	    int len = strlen(action);                               
                                                                
	    for (t = Scr->TwmRoot.next; t != NULL; t = t->next)     
		{	/* jason@tfs.com */                                 
			if( MatchWinName( action, len, t ) ) break;         
			}                                                   
                                                                
	    tmp_win = t;                                            /* PF */
		}                                                       /* PF */
	/* fall through */                                          /* PF */
                                                                
    case F_WARP:                                                /* PF */
	{                                                           /* PF */
	    if (tmp_win) {                                          /* PF */
		if (Scr->WarpUnmapped || tmp_win->mapped) {             /* PF */
		    if (!tmp_win->mapped) DeIconify (tmp_win);          /* PF */
                                                                
		    if (!Scr->NoRaiseWarp) {                            
			    XRaiseWindow (dpy, tmp_win->frame);             /* PF */
		    }                                                   
		    XRaiseWindow (dpy,                                  
		              tmp_win->VirtualDesktopDisplayWindow);    /* PF */
		    RaiseStickyAbove();                                 /* DSE */
		    RaiseAutoPan();                                     
			WarpToWindow (tmp_win);                             /* PF */
		}                                                       
	    } else {                                                
		XBell (dpy, 0);                                         
	    }                                                       
	}                                                           /* PF */
	break;

    case F_WARPTOICONMGR:
	{
	    TwmWindow *t;
	    int len;
	    Window raisewin = None, iconwin = None;

	    len = strlen(action);
	    if (len == 0) {
		if (tmp_win && tmp_win->list) {
		    raisewin = tmp_win->list->iconmgr->twm_win->frame;
		    iconwin = tmp_win->list->icon;
		} else if (Scr->iconmgr.active) {
		    raisewin = Scr->iconmgr.twm_win->frame;
		    iconwin = Scr->iconmgr.active->w;
		}
	    } else {
		for (t = Scr->TwmRoot.next; t != NULL; t = t->next) {
		    if (strncmp (action, t->icon_name, len) == 0) {
			if (t->list && t->list->iconmgr->twm_win->mapped) {
			    raisewin = t->list->iconmgr->twm_win->frame;
			    iconwin = t->list->icon;
			    break;
			}
		    }
		}
	    }

	    if (raisewin) {
		XRaiseWindow (dpy, raisewin);
		XWarpPointer (dpy, None, iconwin, 0,0,0,0,
			EDGE_OFFSET, EDGE_OFFSET); /* DSE */
	    } else {
		XBell (dpy, 0);
	    }
	}
	break;

	case F_SQUEEZELEFT:/*RFB*/
	{	static SqueezeInfo left_squeeze = { J_LEFT, 0, 0 };
		if (DeferExecution (context, func, Scr->SelectCursor))
		  return TRUE;

		if ( tmp_win->title_height )
		{	/* Not for untitled windows! */
			tmp_win->squeeze_info = &left_squeeze;
			SetFrameShape( tmp_win );
		}
		break;/*RFB*/
	}/*RFB*/
	case F_SQUEEZERIGHT:/*RFB*/
	{	static SqueezeInfo left_squeeze = { J_RIGHT, 0, 0 };
		if (DeferExecution (context, func, Scr->SelectCursor))
		  return TRUE;

		if ( tmp_win->title_height )
		{	/* Not for untitled windows! */
			tmp_win->squeeze_info = &left_squeeze;
			SetFrameShape( tmp_win );
		}
		break;
	}/*RFB*/
	case F_SQUEEZECENTER:/*RFB*/
	{	static SqueezeInfo left_squeeze = { J_CENTER, 0, 0 };
		if (DeferExecution (context, func, Scr->SelectCursor))
		  return TRUE;

		if ( tmp_win->title_height )
		{	/* Not for untitled windows! */
			tmp_win->squeeze_info = &left_squeeze;
			SetFrameShape( tmp_win );
		}
		break;
	}/*RFB*/

	/*RFB RING*/
	case F_RING:/*RFB*/
	if (DeferExecution (context, func, Scr->SelectCursor))
	  return TRUE;
	if ( tmp_win->ring.next || tmp_win->ring.prev )
	{	/* It's in the ring, let's take it out. */
		TwmWindow *prev = tmp_win->ring.prev, *next = tmp_win->ring.next;

		/*
		* 1. Unlink window
		* 2. If window was only thing in ring, null out ring
		* 3. If window was ring leader, set to next (or null)
		*/
		if (prev) prev->ring.next = next;
		if (next) next->ring.prev = prev;
		if (Scr->Ring == tmp_win)
			Scr->Ring = (next != tmp_win ? next : (TwmWindow *) NULL);

		if (!Scr->Ring || Scr->RingLeader == tmp_win)
			Scr->RingLeader = Scr->Ring;
		tmp_win->ring.next = tmp_win->ring.prev = NULL;
	}
	else
	{	/* Not in the ring, so put it in. */
		if (Scr->Ring)
		{	tmp_win->ring.next = Scr->Ring->ring.next;
		    if (Scr->Ring->ring.next->ring.prev)
		      Scr->Ring->ring.next->ring.prev = tmp_win;
		    Scr->Ring->ring.next = tmp_win;
		    tmp_win->ring.prev = Scr->Ring;
		} else
		{	tmp_win->ring.next = tmp_win->ring.prev = Scr->Ring = tmp_win;
		}
	}
    tmp_win->ring.cursor_valid = False;
	break;

    case F_WARPRING:
	switch (action[0]) {
	  case 'n':
	    WarpAlongRing (&eventp->xbutton, True);
	    break;
	  case 'p':
	    WarpAlongRing (&eventp->xbutton, False);
	    break;
	  default:
	    XBell (dpy, 0);
	    break;
	}
	break;

    case F_FILE:
	action = ExpandFilename(action);
	fd = open(action, 0);
	if (fd >= 0)
	{
	    count = read(fd, buff, MAX_FILE_SIZE - 1);
	    if (count > 0)
		XStoreBytes(dpy, buff, count);

	    close(fd);
	}
	else
	{
	    fprintf (stderr, "%s:  unable to open file \"%s\"\n",
		     ProgramName, action);
	}
	break;

    case F_REFRESH:
	{
	    XSetWindowAttributes attributes;
	    unsigned long valuemask;

	    valuemask = (CWBackPixel | CWBackingStore | CWSaveUnder);
	    attributes.background_pixel = Scr->Black;
	    attributes.backing_store = NotUseful;
	    attributes.save_under = False;
	    w = XCreateWindow (dpy, Scr->Root, 0, 0,
			       (unsigned int) Scr->MyDisplayWidth,
			       (unsigned int) Scr->MyDisplayHeight,
			       (unsigned int) 0,
			       CopyFromParent, (unsigned int) CopyFromParent,
			       (Visual *) CopyFromParent, valuemask,
			       &attributes);
	    XMapWindow (dpy, w);
	    XDestroyWindow (dpy, w);
	    XFlush (dpy);
	}
	break;

    case F_WINREFRESH:
	if (DeferExecution(context, func, Scr->SelectCursor))
	    return TRUE;

	if (context == C_ICON && tmp_win->icon_w)
	    w = XCreateSimpleWindow(dpy, tmp_win->icon_w,
		0, 0, 9999, 9999, 0, Scr->Black, Scr->Black);
	else
	    w = XCreateSimpleWindow(dpy, tmp_win->frame,
		0, 0, 9999, 9999, 0, Scr->Black, Scr->Black);

	XMapWindow(dpy, w);
	XDestroyWindow(dpy, w);
	XFlush(dpy);
	break;

     case F_NAIL:
 	if (DeferExecution(context, func, Scr->SelectCursor))
 	    return TRUE;

 	tmp_win->nailed = !tmp_win->nailed;
 	/* update the vd display */
	/* UpdateDesktop(tmp_win); Stig */
 	NailDesktop(tmp_win); /* Stig */

#ifdef DEBUG
 	fprintf(stdout, "%s:  nail state of %s is now %s\n",
 		ProgramName, tmp_win->name, (tmp_win->nailed ? "nailed" : "free"));
#endif /* DEBUG */

	RaiseStickyAbove(); /* DSE */
	RaiseAutoPan(); /* DSE */

 	break;

	/*
	 * move a percentage in a particular direction
	 */
    case F_PANDOWN:
 	PanRealScreen(0, (atoi(action) * Scr->MyDisplayHeight) / 100
 		/* DSE */ ,NULL,NULL);
 	break;
    case F_PANLEFT:
 	PanRealScreen(-((atoi(action) * Scr->MyDisplayWidth) / 100), 0
 		/* DSE */ ,NULL,NULL);
 	break;
    case F_PANRIGHT:
 	PanRealScreen((atoi(action) * Scr->MyDisplayWidth) / 100, 0
 		/* DSE */ ,NULL,NULL);
 	break;
    case F_PANUP:
 	PanRealScreen(0, -((atoi(action) * Scr->MyDisplayHeight) / 100)
 		/* DSE */ ,NULL,NULL);
 	break;
 	
    case F_RESETDESKTOP:
 		SetRealScreen(0, 0);
 		break;

/*SNUG*/ 	/* Robert Forsman added these two functions <thoth@ufl.edu> */
/*SNUG*/ 	{
/*SNUG*/ 	  TwmWindow	*scan;
/*SNUG*/ 	  int		right, left, up, down;
/*SNUG*/ 	  int		inited;
/*SNUG*/    case F_SNUGDESKTOP:
/*SNUG*/
/*SNUG*/ 	  inited = 0;
/*SNUG*/ 	  for (scan = Scr->TwmRoot.next; scan!=NULL; scan = scan->next)
/*SNUG*/ 	    {
/*SNUG*/ 	      if (scan->nailed)
/*SNUG*/ 		continue;
/*SNUG*/ 	      if (scan->frame_x > Scr->MyDisplayWidth ||
/*SNUG*/ 		  scan->frame_y > Scr->MyDisplayHeight)
/*SNUG*/ 		continue;
/*SNUG*/ 	      if (scan->frame_x+scan->frame_width < 0 ||
/*SNUG*/ 		  scan->frame_y+scan->frame_height < 0)
/*SNUG*/ 		continue;
/*SNUG*/ 	      if ( inited==0 || scan->frame_x<right )
/*SNUG*/ 		right = scan->frame_x;
/*SNUG*/ 	      if ( inited==0 || scan->frame_y<up )
/*SNUG*/ 		up = scan->frame_y;
/*SNUG*/ 	      if ( inited==0 || scan->frame_x+scan->frame_width>left )
/*SNUG*/ 		left = scan->frame_x+scan->frame_width;
/*SNUG*/ 	      if ( inited==0 || scan->frame_y+scan->frame_height>down )
/*SNUG*/ 		down = scan->frame_y+scan->frame_height;
/*SNUG*/ 	      inited = 1;
/*SNUG*/ 	    }
/*SNUG*/ 	  if (inited)
/*SNUG*/ 	    {
/*SNUG*/ 	      int	dx,dy;
/*SNUG*/ 	      if (left-right < Scr->MyDisplayWidth && (right<0 || left>Scr->MyDisplayWidth) )
/*SNUG*/ 		dx = right - ( Scr->MyDisplayWidth - (left-right) ) /2;
/*SNUG*/ 	      else
/*SNUG*/ 		dx = 0;
/*SNUG*/ 	      if (down-up < Scr->MyDisplayHeight && (up<0 || down>Scr->MyDisplayHeight) )
/*SNUG*/ 		dy = up - (Scr->MyDisplayHeight - (down-up) ) /2;
/*SNUG*/ 	      else
/*SNUG*/ 		dy = 0;
/*SNUG*/ 	      if (dx!=0 || dy!=0)
/*SNUG*/ 		PanRealScreen(dx,dy,NULL,NULL);
/*SNUG*/ 		                    /* DSE */
/*SNUG*/ 	      else
/*SNUG*/ 		XBell (dpy, 0);
/*SNUG*/ 	    }
/*SNUG*/ 	  else
/*SNUG*/ 	    XBell (dpy, 0);
/*SNUG*/ 	  break;
/*SNUG*/
/*SNUG*/     case F_SNUGWINDOW:
/*SNUG*/ 	  if (DeferExecution(context, func, Scr->SelectCursor))
/*SNUG*/ 	    return TRUE;
/*SNUG*/
/*SNUG*/ 	  inited = 0;
/*SNUG*/ 	  right = tmp_win->frame_x;
/*SNUG*/ 	  left = tmp_win->frame_x + tmp_win->frame_width;
/*SNUG*/ 	  up = tmp_win->frame_y;
/*SNUG*/ 	  down = tmp_win->frame_y + tmp_win->frame_height;
/*SNUG*/ 	  inited = 1;
/*SNUG*/ 	  if (inited)
/*SNUG*/ 	    {
/*SNUG*/ 	      int	dx,dy;
/*SNUG*/ 	      dx = 0;
/*SNUG*/ 	      if (left-right < Scr->MyDisplayWidth)
/*SNUG*/ 		if (right<0)
/*SNUG*/ 		  dx = right;
/*SNUG*/ 		else if (left>Scr->MyDisplayWidth)
/*SNUG*/ 		  dx = left - Scr->MyDisplayWidth;
/*SNUG*/
/*SNUG*/ 	      dy = 0;
/*SNUG*/ 	      if (down-up < Scr->MyDisplayHeight)
/*SNUG*/ 		if (up<0)
/*SNUG*/ 		  dy = up;
/*SNUG*/ 		else if (down>Scr->MyDisplayHeight)
/*SNUG*/ 		  dy = down - Scr->MyDisplayHeight;
/*SNUG*/
/*SNUG*/ 	      if (dx!=0 || dy!=0)
/*SNUG*/ 		PanRealScreen(dx,dy,NULL,NULL);
/*SNUG*/ 		                    /* DSE */
/*SNUG*/ 	      else
/*SNUG*/ 		XBell (dpy, 0);
/*SNUG*/ 	    }
/*SNUG*/ 	  else
/*SNUG*/ 	    XBell (dpy, 0);
/*SNUG*/
/*SNUG*/ 	break;
/*SNUG*/ 	}

 	/* move the real screen representation around the virtual desktop display */
    case F_MOVESCREEN:
 	/* this breaks BADLY if this is not called by the default button press
 	 * that's why i started writing movescreenarounddesktop ! */
 	/*MoveScreenAroundDesktop(eventp, context);*/
 	StartMoveWindowInDesktop(eventp->xmotion);
 	break;

    case F_SNAP:
	SnapRealScreen();
	/* and update the data structures */
	SetRealScreen(Scr->VirtualDesktopX, Scr->VirtualDesktopY);
	break;

	case F_SNAPREALSCREEN:
		Scr->SnapRealScreen = ! Scr->SnapRealScreen;
		break;

    case F_SETREALSCREEN:
	{
		int newx = Scr->VirtualDesktopX;
		int newy = Scr->VirtualDesktopY;

		/* parse the geometry */
		JunkMask = XParseGeometry (action, &JunkX, &JunkY, &JunkWidth, &JunkHeight);

		if (JunkMask & XValue)
			newx = JunkX;
		if (JunkMask & YValue)
			newy = JunkY;

		if (newx < 0)
			newx = Scr->VirtualDesktopWidth + newx;
		if (newy < 0)
			newy = Scr->VirtualDesktopHeight + newy;

		SetRealScreen(newx, newy);

		break;
	}

    case F_HIDEDESKTOP:
	if (Scr->Virtual)
		XUnmapWindow(dpy, Scr->VirtualDesktopDisplayTwin->frame);
	break;

    case F_SHOWDESKTOP:
	if (Scr->Virtual)
		XMapWindow(dpy, Scr->VirtualDesktopDisplayTwin->frame);
	break;

    case F_ENTERDOOR:
	{
		TwmDoor *d;

		if (XFindContext(dpy, tmp_win->w, DoorContext,
				 (caddr_t *) &d) != XCNOENT)
 			door_enter(tmp_win->w, d);
		break;
	}

    case F_DELETEDOOR:
	{	/*marcel@duteca.et.tudelft.nl*/
		TwmDoor *d;

		if (DeferExecution(context, func, Scr->DestroyCursor))
	    		return TRUE;
		if (XFindContext(dpy, tmp_win->w, DoorContext,
				 (caddr_t *) &d) != XCNOENT)
 			door_delete(tmp_win->w, d);
		break;
	}

     case F_NEWDOOR:
 	door_new();
	break;

     case F_QUIT:
	SetRealScreen(0,0);
	Done();
	break;

     case F_VIRTUALGEOMETRIES:
	Scr->GeometriesAreVirtual = ! Scr->GeometriesAreVirtual;
	break;

    }

    if (ButtonPressed == -1) XUngrabPointer(dpy, CurrentTime);
    return do_next_action;
}



/***********************************************************************
 *
 *  Procedure:
 *	DeferExecution - defer the execution of a function to the
 *	    next button press if the context is C_ROOT
 *
 *  Inputs:
 *	context	- the context in which the mouse button was pressed
 *	func	- the function to defer
 *	cursor	- the cursor to display while waiting
 *
 ***********************************************************************
 */

int
DeferExecution(context, func, cursor)
int context, func;
Cursor cursor;
{
  if (context == C_ROOT)
    {
	LastCursor = cursor;
	XGrabPointer(dpy, Scr->Root, True,
	    ButtonPressMask | ButtonReleaseMask,
	    GrabModeAsync, GrabModeAsync,
	    Scr->Root, cursor, CurrentTime);

	RootFunction = func;

	return (TRUE);
    }

    return (FALSE);
}



/***********************************************************************
 *
 *  Procedure:
 *	ReGrab - regrab the pointer with the LastCursor;
 *
 ***********************************************************************
 */

ReGrab()
{
    XGrabPointer(dpy, Scr->Root, True,
	ButtonPressMask | ButtonReleaseMask,
	GrabModeAsync, GrabModeAsync,
	Scr->Root, LastCursor, CurrentTime);
}



/***********************************************************************
 *
 *  Procedure:
 *	NeedToDefer - checks each function in the list to see if it
 *		is one that needs to be defered.
 *
 *  Inputs:
 *	root	- the menu root to check
 *
 ***********************************************************************
 */

NeedToDefer(root)
MenuRoot *root;
{
    MenuItem *mitem;

    for (mitem = root->first; mitem != NULL; mitem = mitem->next)
    {
	switch (mitem->func)
	{
	case F_IDENTIFY:
	case F_RESIZE:
	case F_MOVE:
	case F_FORCEMOVE:
	case F_DEICONIFY:
	case F_ICONIFY:
	case F_RAISELOWER:
	case F_RAISE:
	case F_LOWER:
	case F_FOCUS:
	case F_DESTROY:
	case F_WINREFRESH:
	case F_ZOOM:
	case F_FULLZOOM:
	case F_HORIZOOM:
	case F_RIGHTZOOM:
	case F_LEFTZOOM:
	case F_TOPZOOM:
	case F_BOTTOMZOOM:
	case F_AUTORAISE:
	case F_NAIL:
	case F_SNUGWINDOW:
	    return TRUE;
	}
    }
    return FALSE;
}



void
Execute(s)
    char *s;
{
    static char buf[256];
    char *ds = DisplayString (dpy);
    char *colon, *dot1;
    char oldDisplay[256];
    char *doisplay;
    int restorevar = 0;
    
    char *append_this = " &";
    char *es = (char *)malloc(strlen(s)+strlen(append_this)+1);
    sprintf(es,s);
    	/* a new copy of s, with extra space incase -- DSE */

	if (Scr->EnhancedExecResources) /* DSE */
		{    
	    /* chop all space characters from the end of the string */
    	while ( isspace ( es[strlen(es)-1] ) )
	    	{
    		es[strlen(es)-1] = '\0';
			}
		switch ( es[strlen(es)-1] ) /* last character */
			{
			case ';':
				es[strlen(es)-1] = '\0'; /* remove the semicolon */
				break;
			case '&': /* already there so do nothing */
				break;
			default:
				strcat(es,append_this); /* don't block the window manager */
				break;
			}
		}

    oldDisplay[0] = '\0';
    doisplay=getenv("DISPLAY");
    if (doisplay)
		strcpy (oldDisplay, doisplay);

    /*
     * Build a display string using the current screen number, so that
     * X programs which get fired up from a menu come up on the screen
     * that they were invoked from, unless specifically overridden on
     * their command line.
     */
    colon = rindex (ds, ':');
    if (colon) {			/* if host[:]:dpy */
	strcpy (buf, "DISPLAY=");
	strcat (buf, ds);
	colon = buf + 8 + (colon - ds);	/* use version in buf */
	dot1 = index (colon, '.');	/* first period after colon */
	if (!dot1) dot1 = colon + strlen (colon);  /* if not there, append */
	(void) sprintf (dot1, ".%d", Scr->screen);
	putenv (buf);
	restorevar = 1;
    }

    (void) system (es); /* DSE */
    free (es); /* DSE */

    if (restorevar) {		/* why bother? */
	(void) sprintf (buf, "DISPLAY=%s", oldDisplay);
	putenv (buf);
    }
}



/***********************************************************************
 *
 *  Procedure:
 *	FocusOnRoot - put input focus on the root window
 *
 ***********************************************************************
 */

void
FocusOnRoot()
{
    SetFocus ((TwmWindow *) NULL, LastTimestamp());
    if (Scr->Focus != NULL)
    {
	SetBorder (Scr->Focus, False);
	if (Scr->Focus->hilite_w) XUnmapWindow (dpy, Scr->Focus->hilite_w);
    }
    InstallWindowColormaps(0, &Scr->TwmRoot);
    Scr->Focus = NULL;
    Scr->FocusRoot = TRUE;
}

DeIconify(tmp_win)
TwmWindow *tmp_win;
{
    TwmWindow *t;

    /* de-iconify the main window */
	if (tmp_win->icon)
	{
		if (tmp_win->icon_on)
			Zoom(tmp_win->icon_w, tmp_win->frame);
		else if (tmp_win->group != NULL)
		{
			for (t = Scr->TwmRoot.next; t != NULL; t = t->next)
			{
				if (tmp_win->group == t->w && t->icon_on)
				{
					Zoom(t->icon_w, tmp_win->frame);
					break;
				}
			}
		}
		else Zoom( None, tmp_win->frame );  /* RFBZOOM */
	}
	else Zoom( None, tmp_win->frame );	/* RFBZOOM */


    XMapWindow(dpy, tmp_win->w);
    tmp_win->mapped = TRUE;
    if (Scr->NoRaiseDeicon)
	XMapWindow(dpy, tmp_win->frame);
    else
    {
	XMapRaised(dpy, tmp_win->frame);
	XRaiseWindow(dpy, tmp_win->VirtualDesktopDisplayWindow);
	
	RaiseAutoPan();
    }
    SetMapStateProp(tmp_win, NormalState);

    if (tmp_win->icon_w) {
	XUnmapWindow(dpy, tmp_win->icon_w);
	IconDown (tmp_win);
    }
    if (tmp_win->list)
	XUnmapWindow(dpy, tmp_win->list->icon);
    if ((Scr->WarpCursor ||
	 LookInList(Scr->WarpCursorL, tmp_win->full_name, &tmp_win->class)) &&
	tmp_win->icon)
      WarpToWindow (tmp_win);
    tmp_win->icon = FALSE;
    tmp_win->icon_on = FALSE;
    UpdateDesktop(tmp_win);

    /* Now de-iconify transients */
	for (t = Scr->TwmRoot.next; t != NULL; t = t->next)
	{
	  if (t->transient && t->transientfor == tmp_win->w)
	    {
	      if (t->icon_on)
		Zoom(t->icon_w, t->frame);
	      else
		Zoom(tmp_win->icon_w, t->frame);

	      XMapWindow(dpy, t->w);
	      t->mapped = TRUE;
	      if (Scr->NoRaiseDeicon)
		XMapWindow(dpy, t->frame);
	      else
	      {
		XMapRaised(dpy, t->frame);
		XRaiseWindow(dpy, t->VirtualDesktopDisplayWindow);
	      }
	      SetMapStateProp(t, NormalState);

	      if (t->icon_w) {
		XUnmapWindow(dpy, t->icon_w);
		IconDown (t);
	      }
	      if (t->list) XUnmapWindow(dpy, t->list->icon);
	      t->icon = FALSE;
	      t->icon_on = FALSE;
	      UpdateDesktop(t);
      }

  }

	RaiseStickyAbove(); /* DSE */
    RaiseAutoPan();

    XSync (dpy, 0);
}



Iconify(tmp_win, def_x, def_y)
TwmWindow *tmp_win;
int def_x, def_y;
{
    TwmWindow *t;
    int iconify;
    XWindowAttributes winattrs;
    unsigned long eventMask;

    iconify = ((!tmp_win->iconify_by_unmapping) || tmp_win->transient);
    if (iconify)
    {
	if (tmp_win->icon_w == NULL)
	    CreateIconWindow(tmp_win, def_x, def_y);
	else
	    IconUp(tmp_win);
	XMapRaised(dpy, tmp_win->icon_w);
	
	RaiseStickyAbove(); /* DSE */
	RaiseAutoPan();
    }
    if (tmp_win->list)
	XMapWindow(dpy, tmp_win->list->icon);

    XGetWindowAttributes(dpy, tmp_win->w, &winattrs);
    eventMask = winattrs.your_event_mask;

    /* iconify transients first */
    for (t = Scr->TwmRoot.next; t != NULL; t = t->next)
      {
	if (t->transient && t->transientfor == tmp_win->w)
	  {

	    /* RemoveFromDesktop(t); Stig */

	    /*
	     * Prevent the receipt of an UnmapNotify, since that would
	     * cause a transition to the Withdrawn state.
	     */
	    t->mapped = FALSE;
	    XSelectInput(dpy, t->w, eventMask & ~StructureNotifyMask);
	    XUnmapWindow(dpy, t->w);
	    XSelectInput(dpy, t->w, eventMask);
	    XUnmapWindow(dpy, t->frame);

		/* moved to make iconify animation more 
		   aesthetically pleasing -- DSE */
		/***********************************************************/
	    if (iconify)
	      {
		if (t->icon_on)
		  Zoom(t->icon_w, tmp_win->icon_w);
		else
		  Zoom(t->frame, tmp_win->icon_w);
	      }
		/***********************************************************/

	    if (t->icon_w)
	      XUnmapWindow(dpy, t->icon_w);
	    SetMapStateProp(t, IconicState);
	    SetBorder (t, False);
	    if (t == Scr->Focus)
	      {
		SetFocus ((TwmWindow *) NULL, LastTimestamp());
		Scr->Focus = NULL;
		Scr->FocusRoot = TRUE;
	      }
	    if (t->list) XMapWindow(dpy, t->list->icon);
	    t->icon = TRUE;
	    t->icon_on = FALSE;


	    UpdateDesktop(t);
	  }
      }

/*    if (iconify) RFBZOOM*/


    /* RemoveFromDesktop(tmp_win); Stig */

    /*
     * Prevent the receipt of an UnmapNotify, since that would
     * cause a transition to the Withdrawn state.
     */
    tmp_win->mapped = FALSE;
    XSelectInput(dpy, tmp_win->w, eventMask & ~StructureNotifyMask);
    XUnmapWindow(dpy, tmp_win->w);
    XSelectInput(dpy, tmp_win->w, eventMask);
    XUnmapWindow(dpy, tmp_win->frame);

	/* moved to make iconify animation more 
	   aesthetically pleasing -- DSE */
	/***********************************************************/
	Zoom(tmp_win->frame, tmp_win->icon_w);
	/***********************************************************/

    SetMapStateProp(tmp_win, IconicState);

    SetBorder (tmp_win, False);
    if (tmp_win == Scr->Focus)
    {
	SetFocus ((TwmWindow *) NULL, LastTimestamp());
	Scr->Focus = NULL;
	Scr->FocusRoot = TRUE;
    }
    tmp_win->icon = TRUE;
    if (iconify)
	tmp_win->icon_on = TRUE;
    else
	tmp_win->icon_on = FALSE;



    UpdateDesktop(tmp_win);
    XSync (dpy, 0);
}



static void Identify (t)
TwmWindow *t;
{
    int i, n, twidth, width, height;
    int x, y;
    unsigned int wwidth, wheight, bw, depth;
    Window junk;
    int px, py, dummy;
    unsigned udummy;

    n = 0;
    (void) sprintf(Info[n++], "Twm version:  %s", Version);
    Info[n++][0] = '\0';

    if (t) {
	XGetGeometry (dpy, t->w, &JunkRoot, &JunkX, &JunkY,
		      &wwidth, &wheight, &bw, &depth);
	(void) XTranslateCoordinates (dpy, t->w, Scr->Root, 0, 0,
				      &x, &y, &junk);
	(void) sprintf(Info[n++], "Name             = \"%s\"", t->full_name);
	(void) sprintf(Info[n++], "Class.res_name   = \"%s\"", t->class.res_name);
	(void) sprintf(Info[n++], "Class.res_class  = \"%s\"", t->class.res_class);
	Info[n++][0] = '\0';
	(void) sprintf(Info[n++], "Geometry/root    = %dx%d+%d+%d", wwidth, wheight,
		x, y);
	(void) sprintf(Info[n++], "Border width     = %d", bw);
	(void) sprintf(Info[n++], "Depth            = %d", depth);
    }

    Info[n++][0] = '\0';
    (void) sprintf(Info[n++], "Click to dismiss....");

    /* figure out the width and height of the info window */
    height = n * (Scr->DefaultFont.height+2);
    width = 1;
    for (i = 0; i < n; i++)
    {
	twidth = XTextWidth(Scr->DefaultFont.font, Info[i],
	    strlen(Info[i]));
	if (twidth > width)
	    width = twidth;
    }
    if (InfoLines) XUnmapWindow(dpy, Scr->InfoWindow);

    width += 10;		/* some padding */
    if (XQueryPointer (dpy, Scr->Root, &JunkRoot, &JunkChild, &px, &py,
		       &dummy, &dummy, &udummy)) {
	px -= (width / 2);
	py -= (height / 3);
	if (px + width + BW2 >= Scr->MyDisplayWidth)
	  px = Scr->MyDisplayWidth - width - BW2;
	if (py + height + BW2 >= Scr->MyDisplayHeight)
	  py = Scr->MyDisplayHeight - height - BW2;
	if (px < 0) px = 0;
	if (py < 0) py = 0;
    } else {
	px = py = 0;
    }
    XMoveResizeWindow(dpy, Scr->InfoWindow, px, py, width, height);
    XMapRaised(dpy, Scr->InfoWindow);
    InfoLines = n;
}



SetMapStateProp(tmp_win, state)
TwmWindow *tmp_win;
int state;
{
    unsigned long data[2];		/* "suggested" by ICCCM version 1 */

    data[0] = (unsigned long) state;
    data[1] = (unsigned long) (tmp_win->iconify_by_unmapping ? None :
			   tmp_win->icon_w);

    XChangeProperty (dpy, tmp_win->w, _XA_WM_STATE, _XA_WM_STATE, 32,
		 PropModeReplace, (unsigned char *) data, 2);
}



Bool GetWMState (w, statep, iwp)
    Window w;
    int *statep;
    Window *iwp;
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytesafter;
    unsigned long *datap = NULL;
    Bool retval = False;

    if (XGetWindowProperty (dpy, w, _XA_WM_STATE, 0L, 2L, False, _XA_WM_STATE,
			    &actual_type, &actual_format, &nitems, &bytesafter,
			    (unsigned char **) &datap) != Success || !datap)
      return False;

    if (nitems <= 2) {			/* "suggested" by ICCCM version 1 */
	*statep = (int) datap[0];
	*iwp = (Window) datap[1];
	retval = True;
    }

    XFree ((char *) datap);
    return retval;
}



WarpToScreen (n, inc)
    int n, inc;
{
    Window dumwin;
    int x, y, dumint;
    unsigned int dummask;
    ScreenInfo *newscr = NULL;

    while (!newscr) {
					/* wrap around */
	if (n < 0)
	  n = NumScreens - 1;
	else if (n >= NumScreens)
	  n = 0;

	newscr = ScreenList[n];
	if (!newscr) {			/* make sure screen is managed */
	    if (inc) {			/* walk around the list */
		n += inc;
		continue;
	    }
	    fprintf (stderr, "%s:  unable to warp to unmanaged screen %d\n",
		     ProgramName, n);
	    XBell (dpy, 0);
	    return;
	}
    }

    if (Scr->screen == n) return;	/* already on that screen */

    PreviousScreen = Scr->screen;
    XQueryPointer (dpy, Scr->Root, &dumwin, &dumwin, &x, &y,
		   &dumint, &dumint, &dummask);

    XWarpPointer (dpy, None, newscr->Root, 0, 0, 0, 0, x, y);
    return;
}




/*
 * BumpWindowColormap - rotate our internal copy of WM_COLORMAP_WINDOWS
 */

BumpWindowColormap (tmp, inc)
    TwmWindow *tmp;
    int inc;
{
    int i, j, previously_installed;
    ColormapWindow **cwins;

    if (!tmp) return;

    if (inc && tmp->cmaps.number_cwins > 0) {
	cwins = (ColormapWindow **) malloc(sizeof(ColormapWindow *)*
					   tmp->cmaps.number_cwins);
	if (cwins) {
	    if (previously_installed =
		/* SUPPRESS 560 */(Scr->cmapInfo.cmaps == &tmp->cmaps &&
	        tmp->cmaps.number_cwins)) {
		for (i = tmp->cmaps.number_cwins; i-- > 0; )
		    tmp->cmaps.cwins[i]->colormap->state = 0;
	    }

	    for (i = 0; i < tmp->cmaps.number_cwins; i++) {
		j = i - inc;
		if (j >= tmp->cmaps.number_cwins)
		    j -= tmp->cmaps.number_cwins;
		else if (j < 0)
		    j += tmp->cmaps.number_cwins;
		cwins[j] = tmp->cmaps.cwins[i];
	    }

	    free((char *) tmp->cmaps.cwins);

	    tmp->cmaps.cwins = cwins;

	    if (tmp->cmaps.number_cwins > 1)
		memset( tmp->cmaps.scoreboard, 0,
		       ColormapsScoreboardLength(&tmp->cmaps));

	    if (previously_installed)
		InstallWindowColormaps(PropertyNotify, (TwmWindow *) NULL);
	}
    } else
	FetchWmColormapWindows (tmp);
}



HideIconManager ()
{
    SetMapStateProp (Scr->iconmgr.twm_win, WithdrawnState);
    XUnmapWindow(dpy, Scr->iconmgr.twm_win->frame);
    if (Scr->iconmgr.twm_win->icon_w)
      XUnmapWindow (dpy, Scr->iconmgr.twm_win->icon_w);
    Scr->iconmgr.twm_win->mapped = FALSE;
    Scr->iconmgr.twm_win->icon = TRUE;
}

TwmWindow *
next_by_class (t, class)
TwmWindow *t;
char *class;
{
    TwmWindow *tt;
    int len = strlen(class);

    if (t)
	for (tt = t->next; tt != NULL; tt = tt->next)
	    if (!strncmp(class, tt->class.res_class, len)) return tt;
    for (tt = Scr->TwmRoot.next; tt != NULL; tt = tt->next)
	if (!strncmp(class, tt->class.res_class, len)) return tt;
    return NULL;
}

WarpClass (next, t, class)
    int next;
    TwmWindow *t;
    char *class;
{
    int len = strlen(class);

    if (!strncmp(class, t->class.res_class, len))
	t = next_by_class(t, class);
    else
	t = next_by_class(NULL, class);
    if (t) {
	if (Scr->WarpUnmapped || t->mapped) {
	    if (!t->mapped) DeIconify (t);
	    if (!Scr->NoRaiseWarp)
		{
		    XRaiseWindow (dpy, t->frame);
		}
	    XRaiseWindow (dpy, t->VirtualDesktopDisplayWindow);

	    RaiseStickyAbove(); /* DSE */
	    RaiseAutoPan();

	    WarpToWindow (t);
	}
    }
}



SetBorder (tmp, onoroff)
    TwmWindow *tmp;
    Bool onoroff;
{
    if (tmp->highlight) {
	if (onoroff) {
	    XSetWindowBorder (dpy, tmp->frame, tmp->border);
	    if (tmp->title_w)
	      XSetWindowBorder (dpy, tmp->title_w, tmp->border);
	} else {
	    XSetWindowBorderPixmap (dpy, tmp->frame, tmp->gray);
	    if (tmp->title_w)
	      XSetWindowBorderPixmap (dpy, tmp->title_w, tmp->gray);
	}
    }
}



DestroyMenu (menu)
    MenuRoot *menu;
{
    MenuItem *item;

    if (menu->w) {
	XDeleteContext (dpy, menu->w, MenuContext);
	XDeleteContext (dpy, menu->w, ScreenContext);
	if (Scr->Shadow) XDestroyWindow (dpy, menu->shadow);
	XDestroyWindow(dpy, menu->w);
    }

    for (item = menu->first; item; ) {
	MenuItem *tmp = item;
	item = item->next;
	free ((char *) tmp);
    }
}



/*
 * warping routines
 */
void WarpAlongRing (ev, forward)
    XButtonEvent *ev;
    Bool forward;
{
    TwmWindow *r, *head;

    if (Scr->RingLeader)
      head = Scr->RingLeader;
    else if (!(head = Scr->Ring))
      return;

    if (forward) {
	for (r = head->ring.next; r != head; r = r->ring.next) {
	    if (!r || r->mapped) break;
	}
    } else {
	for (r = head->ring.prev; r != head; r = r->ring.prev) {
	    if (!r || r->mapped) break;
	}
    }

    if (r && r != head) {
	TwmWindow *p = Scr->RingLeader, *t;

	Scr->RingLeader = r;
	WarpToWindow (r);

	if (p && p->mapped &&
	    XFindContext (dpy, ev->window, TwmContext, (caddr_t *)&t) == XCSUCCESS &&
	    p == t) {
	    p->ring.cursor_valid = True;
	    p->ring.curs_x = ev->x_root - t->frame_x;
	    p->ring.curs_y = ev->y_root - t->frame_y;
	    if (p->ring.curs_x < -p->frame_bw ||
		p->ring.curs_x >= p->frame_width + p->frame_bw ||
		p->ring.curs_y < -p->frame_bw ||
		p->ring.curs_y >= p->frame_height + p->frame_bw) {
		/* somehow out of window */
		p->ring.curs_x = p->frame_width / 2;
		p->ring.curs_y = p->frame_height / 2;
	    }
	}
    }
}



void WarpToWindow (t)
TwmWindow *t;
	{
	int x, y;
	
	/* 
	 * we are either moving the window onto the screen, or the screen to the
     * window, the distances remain the same
     */

	if ((t->frame_x < Scr->MyDisplayWidth)
	    && (t->frame_y < Scr->MyDisplayHeight)
	    && (t->frame_x + t->frame_width >= 0)
	    && (t->frame_y + t->frame_height >= 0))
		{
		
		/*
		 *	window is visible; you can simply
		 *	snug it if WarpSnug or WarpWindows is set -- DSE
		 */
		
		if (Scr->WarpSnug || Scr->WarpWindows)
			{ 
			int right,left,up,down,dx,dy;
			right = t->frame_x;
			left = t->frame_x + t->frame_width;
			up = t->frame_y;
			down = t->frame_y + t->frame_height;
	
			dx = 0;
			if (left-right < Scr->MyDisplayWidth)
				if (right<0)
					dx = right;
				else if (left>Scr->MyDisplayWidth)
					dx = left - Scr->MyDisplayWidth;
	
			dy = 0;
			if (down-up < Scr->MyDisplayHeight)
				if (up<0)
					dy = up;
				else if (down>Scr->MyDisplayHeight)
					dy = down - Scr->MyDisplayHeight;
	
			if (dx!=0 || dy!=0) {
				if (Scr->WarpWindows)
					{
					/* move the window */
					VirtualMoveWindow(t, t->virtual_frame_x - dx,
					    t->virtual_frame_y - dy);
					}
				else
					{
					/* move the screen */
					PanRealScreen(dx,dy,NULL,NULL);
					}
				}
			}
		}
	else {

		/*
		 *	Window is invisible; we need to move it or the screen.
		 */
		 
		int xdiff, ydiff;

		xdiff = ((Scr->MyDisplayWidth - t->frame_width) / 2) - t->frame_x;
		ydiff = ((Scr->MyDisplayHeight - t->frame_height) / 2) - t->frame_y;

		if (Scr->WarpWindows)
			{
			/* move the window */
			VirtualMoveWindow(t, t->virtual_frame_x + xdiff,
			    t->virtual_frame_y + ydiff);
			}
		else
			{
			/* move the screen */
			PanRealScreen(-xdiff, -ydiff,NULL,NULL); /* DSE */
			}
		}

	if (t->auto_raise || !Scr->NoRaiseWarp)
		AutoRaiseWindow (t);
	if (t->ring.cursor_valid) {
		x = t->ring.curs_x;
		y = t->ring.curs_y;
		}
	else {
		x = t->frame_width / 2;
		y = t->frame_height / 2;
		}
	XWarpPointer (dpy, None, t->frame, 0, 0, 0, 0, x, y);
	}


/*
 * ICCCM Client Messages - Section 4.2.8 of the ICCCM dictates that all
 * client messages will have the following form:
 *
 *     event type	ClientMessage
 *     message type	_XA_WM_PROTOCOLS
 *     window		tmp->w
 *     format		32
 *     data[0]		message atom
 *     data[1]		time stamp
 */
static void send_clientmessage (w, a, timestamp)
    Window w;
    Atom a;
    Time timestamp;
{
    XClientMessageEvent ev;

    ev.type = ClientMessage;
    ev.window = w;
    ev.message_type = _XA_WM_PROTOCOLS;
    ev.format = 32;
    ev.data.l[0] = a;
    ev.data.l[1] = timestamp;
    XSendEvent (dpy, w, False, 0L, (XEvent *) &ev);
}

SendDeleteWindowMessage (tmp, timestamp)
    TwmWindow *tmp;
    Time timestamp;
{
    send_clientmessage (tmp->w, _XA_WM_DELETE_WINDOW, timestamp);
}

SendSaveYourselfMessage (tmp, timestamp)
    TwmWindow *tmp;
    Time timestamp;
{
    send_clientmessage (tmp->w, _XA_WM_SAVE_YOURSELF, timestamp);
}


SendTakeFocusMessage (tmp, timestamp)
    TwmWindow *tmp;
    Time timestamp;
{
    send_clientmessage (tmp->w, _XA_WM_TAKE_FOCUS, timestamp);
}
