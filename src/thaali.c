/* thaali
 * Copyright (c) 2003, 2005 Mohammed Sameer.
 * Copyright (c) 2005, 2007 Eric Piel.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
/* Sun OpenWindows */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0
#define _NET_SYSTEM_TRAY_ORIENTATION_VERT 1

/*
 * XEMBED messages
 */
#define XEMBED_EMBEDDED_NOTIFY		0
#define XEMBED_WINDOW_ACTIVATE		1
#define XEMBED_WINDOW_DEACTIVATE	2
#define XEMBED_REQUEST_FOCUS		3
#define XEMBED_FOCUS_IN			4
#define XEMBED_FOCUS_OUT		5
#define XEMBED_FOCUS_NEXT		6
#define XEMBED_FOCUS_PREV		7
/* 8-9 were used for XEMBED_GRAB_KEY/XEMBED_UNGRAB_KEY */
#define XEMBED_MODALITY_ON		10
#define XEMBED_MODALITY_OFF		11
#define XEMBED_REGISTER_ACCELERATOR	12
#define XEMBED_UNREGISTER_ACCELERATOR	13
#define XEMBED_ACTIVATE_ACCELERATOR	14


/* Flags for _XEMBED_INFO */
#define XEMBED_MAPPED                   (1 << 0)

//#define DEBUG
#ifdef DEBUG
#define dbg_printf(fmt,arg...) fprintf(stderr,fmt,##arg)
#else
#define dbg_printf(fmt,arg...) do { } while (0)
#endif

Atom selection_atom, opcode_atom, data_atom;

struct dock {
	Window main;		/* window representing the dock */
	Window container;	/* window containing the slots (has the same size as main)*/	
	Window *slots;		/* an array of the slots = displayed icons */
};

struct dock *docks;	/* the docks which are displayed */
// XXX should it be a chained list?
int num_docks = 0;		/* number of docks created */
int slots_per_dock;		/* number of slots that fit in one dock (depends on their size) */

int no_multiple = 0;
int icon_size = 16;
int border_width = 0;
int dock_width = 64; // with WindowMaker it might be possible to retrieve it dynamically?
int dock_height = -1; // -1 means it is equal to icon_size

Display *display = NULL;
char *display_name = NULL;
Window Root;

void
quit (char *err)
{
	
	fprintf (stderr, "%s", err); /* RM: fixed formatting issue 2015/1/13 */
	/* fprintf (stderr, err);*/  /* deleted from original peksystray */
	exit (1);
}

void
display_help ()
{
  fprintf (stdout, ""
	   "%s - version %s\n"
	   "Copyright 2003-2005, Mohammed Sameer <msameer (at) foolab.org>\n"
	   "Copyright 2005-2007, Eric Piel <eric.piel (at) tremplin-utc.net>\n"
	   "\n"
	   "Usage: %s [OPTIONS]\n"
	   "\n"
	   "Options:\n"
	   " --help\t\t\t Display this help.\n"
	   " --version\t\t Display version number and exit.\n"
	   " --display DISPLAY\t The X display to connect to.\n"
	   " --icon-size SIZE\t Icon size. Default is 16.\n"
	   " --width SIZE\t\t Dock width. Default is 64.\n"
	   " --height SIZE\t\t Dock height. Default is to be equal to the icon size.\n"
	   " --border SIZE\t\t Border width. Default is 0.\n"
	   " --no-multiple\t\t Do not automatically create a new dock when one is full.\n"
	   "\n"
	   "", PACKAGE, VERSION, PACKAGE);
  exit (0);
}

void
display_version ()
{
  fprintf (stdout, "%s\n", VERSION);
  exit (0);
}

void
parse_cmd_line (int argc, char *argv[])
{
  int x;

  for (x = 1; x < argc; x++)
    {
      dbg_printf ("%s\n", argv[x]);
      if (!strcmp (argv[x], "--help"))
	  display_help ();
      
      else if (!strcmp (argv[x], "--version"))
	  display_version ();
      
      else if (!strcmp (argv[x], "--vertical"))
	  printf("WARNING: --vertical option has been ignored. It is now deprecated, please use the --width and --height options.\n");

      else if (!strcmp (argv[x], "--square"))
	  printf("WARNING: --square option has been ignored. It is now deprecated, please use the --width and --height options.\n");
      
      else if (!strcmp (argv[x], "--multiple"))
	  printf("WARNING: --multiple option is now deprecated and activated by default.\n");
      
      else if (!strcmp (argv[x], "--no-multiple"))
	  no_multiple = 1;
      
      else if (!strcmp (argv[x], "--display")) {
	  if (x + 1 == argc)
	      quit ("--display requires an argument");
	  else
	    {
	      display_name = argv[x + 1];
	      ++x;
	      continue;
	    }
	}
      else if (!strcmp (argv[x], "--width")) {
	  if (x + 1 == argc)
	      quit ("--width requires an argument");
	  else {
	      dock_width = atoi (argv[x + 1]);
	      if (dock_width < 1)
		  quit ("The width must be at least 1 pixel!");
	      ++x;
	      continue;
	    }
	}
      else if (!strcmp (argv[x], "--height")) {
	  if (x + 1 == argc)
	      quit ("--height requires an argument");
	  else {
	      dock_height = atoi (argv[x + 1]);
	      if (dock_height < 1)
		  quit ("The height must be at least 1 pixel!");
	      ++x;
	      continue;
	    }
	}
      else if (!strcmp (argv[x], "--icon-size")) {
	  if (x + 1 == argc)
	      quit ("--icon-size requires an argument");
	  else
	    {
	      icon_size = atoi (argv[x + 1]);
	      if (icon_size < 1)
		{
		  quit ("The icon size must be at least 1 pixel!");
		}
	      ++x;
	      continue;
	    }
	}
      else if (!strcmp (argv[x], "--border")) {
	  if (x + 1 == argc)
	      quit ("--border requires an argument");
	  else
	    {
	      border_width = atoi (argv[x + 1]);
	      if (border_width < 0)
		  quit ("The border width cannot be negative!");
	      ++x;
	      continue;
	    }
	}
      else
	  fprintf (stderr, "Unknown command line argument: %s\n", argv[x]);
    }
}

/*
 * Delete the dock passed in argument.
 * It will silently not delete the first dock as it's necessary to keep 
 * the systray active wrt the X server.
 */
void delete_dock(int dock_nb)
{
	/* never delete the systray window */
	if (dock_nb == 0)
		return;

	XDestroyWindow (display, docks[dock_nb].main);
	XDestroyWindow (display, docks[dock_nb].container);

	/* move the next docks to overwrite this one */
	memmove(&docks[dock_nb],
		&docks[dock_nb + 1],
		(num_docks - dock_nb - 1) * sizeof(struct dock));
	docks = realloc(docks, (num_docks - 1) * sizeof(struct dock));

	num_docks--;
}

/*
 * Manages the creation of one new dock. It includes updating the data structures
 * as well as displaying it's window.
 * Returns a non null value if an error occured.
 */
int create_dock()
{
	XClassHint *class_hint;
	XWMHints *hints;
	struct dock *new_docks, *new_dock;

	/* update the data stuctures */
	new_docks = realloc(docks, (num_docks + 1) * sizeof(struct dock));
	if (new_docks == NULL)
		return -1;
	
	docks = new_docks;
	new_dock = &docks[num_docks];
	new_dock->slots = malloc(slots_per_dock * sizeof(Window));
	memset(new_dock->slots, 0, slots_per_dock * sizeof(Window));
	if (new_dock->slots == NULL)
		return -2;

	/* create the windows */
	new_dock->main =
	XCreateSimpleWindow (display, DefaultRootWindow (display), 0, 0,
			     dock_width, dock_height, border_width, border_width, 0);
	new_dock->container =
	XCreateSimpleWindow (display, new_dock->main, 0, 0,
			     dock_width, dock_height, border_width, border_width, 0);

	if ((class_hint = XAllocClassHint ()) == NULL) {
		fprintf(stderr, "Failed to allocate class hint\n");
		return -2;
	}

	class_hint->res_class = PACKAGE;
	class_hint->res_name = PACKAGE;
	XSetClassHint (display, new_dock->main, class_hint);
	XFree (class_hint);

	if ((hints = XAllocWMHints ()) == NULL) {
		fprintf(stderr, "Failed to allocate hints\n");
		return -2;
	}

	hints->flags = StateHint | WindowGroupHint | IconWindowHint;
	hints->window_group = new_dock->main;
	hints->icon_window = new_dock->container;
	hints->initial_state = WithdrawnState;

	XSetWMHints (display, new_dock->main, hints);
	XFree (hints);

	XSetWindowBackgroundPixmap (display, new_dock->container, ParentRelative);

	/* Add the systray info in case it's the first dock */
	if (num_docks == 0) {
		selection_atom = XInternAtom (display, "_NET_SYSTEM_TRAY_S0", False);
		XSetSelectionOwner (display, selection_atom, new_dock->main, CurrentTime);

		if (XGetSelectionOwner (display, selection_atom) == new_dock->main) {
			XClientMessageEvent xev;
			int scr = DefaultScreen (display);
			Root = RootWindow (display, scr);
			xev.type = ClientMessage;
			xev.window = Root;
			xev.message_type = XInternAtom (display, "MANAGER", False);

			xev.format = 32;
			xev.data.l[0] = CurrentTime;
			xev.data.l[1] = selection_atom;
			xev.data.l[2] = new_dock->main;
			xev.data.l[3] = 0;	/* manager specific data */
			xev.data.l[4] = 0;	/* manager specific data */

			XSendEvent (display, Root, False, StructureNotifyMask, (XEvent *) & xev);

			opcode_atom = XInternAtom (display, "_NET_SYSTEM_TRAY_OPCODE", False);
			data_atom = XInternAtom (display, "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);
		}
		// XXX no else ? shouldn't we quit?
	}

	XSelectInput (display, new_dock->main, SubstructureNotifyMask);
	XMapRaised (display, new_dock->main);
	XFlush (display);

	num_docks++;
	return 0;
}

/*
 * Removes an icon from a given dock. It will automatically delete the whole dock
 * if it's empty (and not the first one).
 */
void remove_icon(int dock_nb, int slot_nb)
{
	int i;

	docks[dock_nb].slots[slot_nb] = 0;

	/* see if the dock is completely empty */
	for (i = 0; i < slots_per_dock; i++) {
		if (docks[dock_nb].slots[i] != 0)
			return;
	}

	/* delete the dock */
	delete_dock(dock_nb);
}

/**
 * Compute the coordinates of a slot from its number in the dock
 */
void x_y_from_slot(int slot_nb, int *x, int *y)
{
	int icons_per_row;

	/* We must have at least one icon per row! */
	icons_per_row = dock_width / icon_size;
	if (icons_per_row == 0)
		icons_per_row = 1;

	*x = (slot_nb % icons_per_row) * icon_size;
	*y = (slot_nb / icons_per_row) * icon_size;

	*x += border_width;
	*y += border_width;

	dbg_printf ("x = %d, y = %d\n", x, y);
}


/**
 * Put a given icon (embed window) on the right place on the right dock.
 */
void add_icon(Window embed, int dock_nb, int slot_nb)
{
	int x, y;

	x_y_from_slot(slot_nb, &x, &y);
	XReparentWindow (display, embed, docks[dock_nb].container, x, y);
	docks[dock_nb].slots[slot_nb] = embed;
}

/* 
 * Returns the slot corresponding to a given icon.
 * It will return an integer as the number of the slot and a second one (by reference)
 * as the number of the dock.
 * In case of error, an negative value is returned.
 */
int find_slot(Window embed, int *dock_nb)
{
	int i, j;

	/* find first slot which corresponds to the window */
	for (i = 0; i < num_docks; i++) {
		for (j = 0; j < slots_per_dock; j++) {
			if (docks[i].slots[j] == embed) {
				*dock_nb = i;
				return j;
			}
		}
	}

	return -1;
}


/* 
 * Returns the first free slot usable for a new icon.
 * It automatically handles the creation of a new dock if there aren't any free
 * slot left.
 * The returned int is the slot number, while the dock number is set via reference.
 * If no free slots are available it will return a negative value.
 */
int find_first_free_slot(int *dock_nb)
{
	int slot_nb;

	slot_nb = find_slot(0, dock_nb);
	if (slot_nb >= 0)
		return slot_nb;

	/* nothing available?*/
	if (no_multiple)
		return -1;

	/* Create more! */
	if (create_dock() != 0)
		return -2;

	*dock_nb = num_docks - 1;
	return 0;
}

/*
 * Adds a new icon to the systray.
 * The function will return a non null value in case of error.
 */
int add_tray_icon(Window embed)
{
	XEvent xevent;
	int this_dock, this_slot;
	dbg_printf ("%s: %i\n", __FUNCTION__, (int) embed);

	this_slot = find_first_free_slot(&this_dock);
	dbg_printf ("slot = %d, dock = %d\n", this_slot, this_dock);
	if (this_slot < 0)
		return this_slot;

	XSelectInput (display, embed, StructureNotifyMask | PropertyChangeMask);
	XWithdrawWindow (display, embed, 0);

	add_icon(embed, this_dock, this_slot);

	XSync (display, False);
	XMapRaised (display, embed);
	xevent.xclient.window = embed;
	xevent.xclient.type = ClientMessage;
	xevent.xclient.message_type = XInternAtom (display, "_XEMBED", False);
	xevent.xclient.format = 32;
	xevent.xclient.data.l[0] = CurrentTime;
	xevent.xclient.data.l[1] = XEMBED_EMBEDDED_NOTIFY;
	xevent.xclient.data.l[2] = 0;
	xevent.xclient.data.l[3] = docks[this_dock].container;
	xevent.xclient.data.l[4] = 0;
	XSendEvent (display, Root, False, NoEventMask, &xevent);

	return 0;
}

void rm_tray_icon(Window embed)
{
	int dock, slot;
	dbg_printf ("%s: %i\n", __FUNCTION__, (int) embed);

	slot = find_slot(embed, &dock);
	if (slot < 0)
		return;

	remove_icon(dock, slot);
}

/**
 * Set up the correct size and place of the window for a slot
 */
void configure_tray_icon_attrib(Window embed)
{
	XWindowAttributes attrib;
	int dock_nb, slot_nb;
	int x, y;

	dbg_printf ("%s: %i\n", __FUNCTION__, (int) embed);

	XGetWindowAttributes (display, embed, &attrib);
	dbg_printf ("X: %i\t Y: %i\t Width: %i\t Height:%i\tBorder: %i\n",
				attrib.x,
				attrib.y,
				attrib.width,
				attrib.height,
				attrib.border_width);

	/* Find the slot and its attributes */
	slot_nb = find_slot(embed, &dock_nb);
	if (slot_nb < 0)
		return;
	x_y_from_slot(slot_nb, &x, &y);

	/* Force the size and position specified by the user */
	if ((attrib.width != icon_size) || (attrib.height != icon_size) ||
	    (attrib.x != x) || (attrib.y != y))
		XMoveResizeWindow (display, embed, x, y, icon_size, icon_size);
}

void handle_event (XEvent ev)
{
	XClientMessageEvent *event = (XClientMessageEvent *) &ev;
	XWindowAttributes tray_icon_attr;
	long opcode = event->data.l[1];
	Window win = event->data.l[2];

	dbg_printf ("%s\n", __FUNCTION__);

	switch (opcode) {
	case SYSTEM_TRAY_REQUEST_DOCK:
		XGetWindowAttributes (display, win, &tray_icon_attr);
		dbg_printf ("X: %i\t Y: %i\t Width: %i\t Height:%i\tBorder: %i\n",
				tray_icon_attr.x,
				tray_icon_attr.y,
				tray_icon_attr.width,
				tray_icon_attr.height,
				tray_icon_attr.border_width);
		XResizeWindow (display, win, icon_size, icon_size);

		if (add_tray_icon(win)) {
			// Not sure if it's necessary
			// XReparentWindow (display, win, DefaultRootWindow(display), 0, 0);
			dbg_printf ("Error when adding another icon.\n");
			return;
		}
		break;
	case SYSTEM_TRAY_BEGIN_MESSAGE:
		// for ballon messages, could be nice to handle them too
		break;
	case SYSTEM_TRAY_CANCEL_MESSAGE:
		break;
	default:
		break;
	}
}

void handle_message_data (XEvent ev)
{
	dbg_printf ("%s\n", __FUNCTION__);
}

void eventLoop(void)
{
  XEvent ev;

  while (1) {
    XNextEvent (display, &ev);
    switch (ev.type) {
    case ConfigureNotify:
	dbg_printf ("ConfigureNotify\n");
	if (ev.xany.window != docks[0].main)
		configure_tray_icon_attrib (ev.xproperty.window);
	break;
    case DestroyNotify:
      {
	XDestroyWindowEvent *xev = (XDestroyWindowEvent *) &ev;
	dbg_printf ("DestroyNotify\n");
	if (xev->window == docks[0].main)
	  return;
	else
	  rm_tray_icon (xev->window);
	break;
      }
    case ReparentNotify:
      dbg_printf ("ReparentNotify\n");
      break;
    case UnmapNotify:
      {
	XUnmapEvent *xev = (XUnmapEvent *) &ev;
	dbg_printf ("UnmapNotify\n");
	if (xev->window != docks[0].main)
	  rm_tray_icon (xev->window);
	break;
      }
    case ClientMessage:
      if (ev.xclient.message_type == opcode_atom)
	handle_event (ev);
      else if (ev.xclient.message_type == data_atom)
	handle_message_data (ev);
      break;
    case SelectionClear:
      dbg_printf ("SelectionClear\n");
      if (XGetSelectionOwner (display, selection_atom) == docks[0].main)
	XSetSelectionOwner (display, selection_atom, None, CurrentTime);
      return;
    default:
      dbg_printf ("message=%i\n", ev.type);
      break;
    }
  }
}

int main(int argc, char *argv[])
{
	parse_cmd_line (argc, argv);

	/* make dock_height same as icon_size if it was not specified */
	if (dock_height == -1)
		dock_height = icon_size;

	if ((display = XOpenDisplay (display_name)) == NULL)
		quit ("Cannot open display.\n");

	/* ?:1 is there to consider one slot instead of zero if their size is too big */
	slots_per_dock = (dock_height / icon_size ?:1) * (dock_width / icon_size ?:1);
	dbg_printf ("%d slots_per_dock\n", slots_per_dock);
	if (create_dock())
		quit("Error while creating the dock.\n");

	eventLoop ();

	XCloseDisplay (display);
	return EXIT_SUCCESS;
}
