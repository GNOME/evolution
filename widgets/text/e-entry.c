/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table.c: A graphical view of a Table.
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *   Chris Lahey (clahey@helixcode.com)
 *
 * Copyright 1999, Helix Code, Inc
 */
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <stdio.h>
#include <libgnomeui/gnome-canvas.h>
#include <gtk/gtksignal.h>
#include <gnome-xml/parser.h>
#include "gal/util/e-util.h"
#include "gal/widgets/e-canvas.h"
#include "gal/widgets/e-canvas-utils.h"
#include "e-entry.h"

#define MIN_ENTRY_WIDTH  150
#define INNER_BORDER     2

#define PARENT_TYPE gtk_table_get_type ()

static GtkObjectClass *parent_class;

enum {
	E_ENTRY_CHANGED,
	E_ENTRY_ACTIVATE,
	E_ENTRY_LAST_SIGNAL
};

static guint e_entry_signals[E_ENTRY_LAST_SIGNAL] = { 0 };

/* Object argument IDs */
enum {
	ARG_0,
	ARG_MODEL,
	ARG_EVENT_PROCESSOR,
	ARG_TEXT,
	ARG_FONT,
        ARG_FONTSET,
	ARG_FONT_GDK,
	ARG_ANCHOR,
	ARG_JUSTIFICATION,
	ARG_X_OFFSET,
	ARG_Y_OFFSET,
	ARG_FILL_COLOR,
	ARG_FILL_COLOR_GDK,
	ARG_FILL_COLOR_RGBA,
	ARG_FILL_STIPPLE,
	ARG_EDITABLE,
	ARG_USE_ELLIPSIS,
	ARG_ELLIPSIS,
	ARG_LINE_WRAP,
	ARG_BREAK_CHARACTERS,
	ARG_MAX_LINES,
	ARG_ALLOW_NEWLINES,
	ARG_DRAW_BORDERS,
	ARG_DRAW_BACKGROUND,
	ARG_CURSOR_POS
};

static void
canvas_size_allocate (GtkWidget *widget, GtkAllocation *alloc,
		      EEntry *e_entry)
{
	gint xthick;
	gint ythick;
	gnome_canvas_set_scroll_region (
		e_entry->canvas,
		0, 0, alloc->width, alloc->height);
	gtk_object_set (GTK_OBJECT (e_entry->item),
			"clip_width", (double) (alloc->width),
			"clip_height", (double) (alloc->height),
			NULL);

	if (e_entry->draw_borders) {
		xthick = 0;
		ythick = 0;
	} else {
		xthick = widget->style->klass->xthickness;
		ythick = widget->style->klass->ythickness;
	}

	switch (e_entry->justification) {
	case GTK_JUSTIFY_RIGHT:
		e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(e_entry->item), alloc->width - xthick, ythick);
		break;
	case GTK_JUSTIFY_CENTER:
		e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(e_entry->item), alloc->width / 2, ythick);
		break;
	default:
		e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(e_entry->item), xthick, ythick);
		break;
	}
}

static void
canvas_size_request (GtkWidget *widget, GtkRequisition *requisition,
		     EEntry *ee)
{
	int border;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNOME_IS_CANVAS (widget));
	g_return_if_fail (requisition != NULL);

	if (ee->draw_borders)
		border = INNER_BORDER;
	else
		border = 0;
	
	requisition->width = MIN_ENTRY_WIDTH + (widget->style->klass->xthickness + border) * 2;
	requisition->height = (widget->style->font->ascent +
			       widget->style->font->descent +
			       (widget->style->klass->ythickness + border) * 2);
}

static gint
canvas_focus_in_event (GtkWidget *widget, GdkEventFocus *focus, EEntry *e_entry)
{
	if (e_entry->canvas->focused_item != GNOME_CANVAS_ITEM(e_entry->item))
		gnome_canvas_item_grab_focus(GNOME_CANVAS_ITEM(e_entry->item));

	return 0;
}

static void
e_entry_proxy_changed (EText *text, EEntry *ee)
{
	gtk_signal_emit (GTK_OBJECT (ee), e_entry_signals [E_ENTRY_CHANGED]);
}

static void
e_entry_proxy_activate (EText *text, EEntry *ee)
{
	gtk_signal_emit (GTK_OBJECT (ee), e_entry_signals [E_ENTRY_ACTIVATE]);
}

static void
e_entry_init (GtkObject *object)
{
	EEntry *e_entry = E_ENTRY (object);
	GtkTable *gtk_table = GTK_TABLE (object);
	
	e_entry->canvas = GNOME_CANVAS(e_canvas_new());
	gtk_signal_connect(GTK_OBJECT(e_entry->canvas), "size_allocate",
			   GTK_SIGNAL_FUNC(canvas_size_allocate), e_entry);
	gtk_signal_connect(GTK_OBJECT(e_entry->canvas), "size_request",
			   GTK_SIGNAL_FUNC(canvas_size_request), e_entry);
	gtk_signal_connect(GTK_OBJECT(e_entry->canvas), "focus_in_event",
			   GTK_SIGNAL_FUNC(canvas_focus_in_event), e_entry);
	e_entry->draw_borders = TRUE;
	e_entry->item = E_TEXT(gnome_canvas_item_new(gnome_canvas_root(e_entry->canvas),
						     e_text_get_type(),
						     "clip", TRUE,
						     "fill_clip_rectangle", TRUE,
						     "anchor", GTK_ANCHOR_NW,
						     "draw_borders", TRUE,
						     "draw_background", TRUE,
						     NULL));
	e_entry->justification = GTK_JUSTIFY_LEFT;
	gtk_table_attach(gtk_table, GTK_WIDGET(e_entry->canvas),
			 0, 1, 0, 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 0, 0);
	gtk_widget_show(GTK_WIDGET(e_entry->canvas));

	/*
	 * Proxy functions: we proxy the changed and activate signals
	 * from the item to outselves
	 */
	gtk_signal_connect (GTK_OBJECT (e_entry->item), "changed",
			    GTK_SIGNAL_FUNC (e_entry_proxy_changed), e_entry);
	gtk_signal_connect (GTK_OBJECT (e_entry->item), "activate",
			    GTK_SIGNAL_FUNC (e_entry_proxy_activate), e_entry);

}

/**
 * e_entry_construct
 * 
 * Constructs the given EEntry.
 * 
 * Returns: The EEntry
 **/
EEntry *
e_entry_construct (EEntry *e_entry)
{
	return e_entry;
}


/**
 * e_entry_new
 * 
 * Creates a new EEntry.
 * 
 * Returns: The new EEntry
 **/
GtkWidget *
e_entry_new (void)
{
	EEntry *e_entry;
	e_entry = gtk_type_new (e_entry_get_type ());
	e_entry = e_entry_construct (e_entry);

	return GTK_WIDGET (e_entry);
}

static void
et_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EEntry *ee = E_ENTRY (o);
	GtkObject *item = GTK_OBJECT (ee->item);
	
	switch (arg_id){
	case ARG_MODEL:
		gtk_object_get(item,
			       "model", &GTK_VALUE_OBJECT (*arg),
			       NULL);
		break;

	case ARG_EVENT_PROCESSOR:
		gtk_object_get(item,
			       "event_processor", &GTK_VALUE_OBJECT (*arg),
			       NULL);
		break;

	case ARG_TEXT:
		gtk_object_get(item,
			       "text", &GTK_VALUE_STRING (*arg),
			       NULL);
		break;

	case ARG_FONT_GDK:
		gtk_object_get(item,
			       "font_gdk", &GTK_VALUE_BOXED (*arg),
			       NULL);
		break;

	case ARG_JUSTIFICATION:
		gtk_object_get(item,
			       "justification", &GTK_VALUE_ENUM (*arg),
			       NULL);
		break;

	case ARG_FILL_COLOR_GDK:
		gtk_object_get(item,
			       "fill_color_gdk", &GTK_VALUE_BOXED (*arg),
			       NULL);
		break;

	case ARG_FILL_COLOR_RGBA:
		gtk_object_get(item,
			       "fill_color_rgba", &GTK_VALUE_UINT (*arg),
			       NULL);
		break;

	case ARG_FILL_STIPPLE:
		gtk_object_get(item,
			       "fill_stiple", &GTK_VALUE_BOXED (*arg),
			       NULL);
		break;

	case ARG_EDITABLE:
		gtk_object_get(item,
			       "editable", &GTK_VALUE_BOOL (*arg),
			       NULL);
		break;

	case ARG_USE_ELLIPSIS:
		gtk_object_get(item,
			       "use_ellipsis", &GTK_VALUE_BOOL (*arg),
			       NULL);
		break;

	case ARG_ELLIPSIS:
		gtk_object_get(item,
			       "ellipsis", &GTK_VALUE_STRING (*arg),
			       NULL);
		break;

	case ARG_LINE_WRAP:
		gtk_object_get(item,
			       "line_wrap", &GTK_VALUE_BOOL (*arg),
			       NULL);
		break;
		
	case ARG_BREAK_CHARACTERS:
		gtk_object_get(item,
			       "break_characters", &GTK_VALUE_STRING (*arg),
			       NULL);
		break;

	case ARG_MAX_LINES:
		gtk_object_get(item,
			       "max_lines", &GTK_VALUE_INT (*arg),
			       NULL);
		break;
	case ARG_ALLOW_NEWLINES:
		gtk_object_get(item,
			       "allow_newlines", &GTK_VALUE_BOOL (*arg),
			       NULL);
		break;

	case ARG_DRAW_BORDERS:
		GTK_VALUE_BOOL (*arg) = ee->draw_borders;
		break;

	case ARG_DRAW_BACKGROUND:
		gtk_object_get (item,
				"draw_background", &GTK_VALUE_BOOL (*arg),
				NULL);
		break;

	case ARG_CURSOR_POS:
		gtk_object_get (item,
				"cursor_pos", &GTK_VALUE_INT (*arg),
				NULL);
		
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
et_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EEntry *ee = E_ENTRY (o);
	GtkObject *item = GTK_OBJECT (ee->item);
	GtkAnchorType anchor;
	double width, height;
	gint xthick;
	gint ythick;
	GtkWidget *widget = GTK_WIDGET(ee->canvas);
	
	switch (arg_id){
	case ARG_MODEL:
		gtk_object_set(item,
			       "model", GTK_VALUE_OBJECT (*arg),
			       NULL);
		break;

	case ARG_EVENT_PROCESSOR:
		gtk_object_set(item,
			       "event_processor", GTK_VALUE_OBJECT (*arg),
			       NULL);
		break;

	case ARG_TEXT:

		gtk_object_set(item,
			       "text", GTK_VALUE_STRING (*arg),
			       NULL);
		break;

	case ARG_FONT:
		gtk_object_set(item,
			       "font", GTK_VALUE_STRING (*arg),
			       NULL);
		break;

	case ARG_FONTSET:
		gtk_object_set(item,
			       "fontset", GTK_VALUE_STRING (*arg),
			       NULL);
		break;

	case ARG_FONT_GDK:
		gtk_object_set(item,
			       "font_gdk", GTK_VALUE_BOXED (*arg),
			       NULL);
		break;

	case ARG_JUSTIFICATION:
		ee->justification = GTK_VALUE_ENUM (*arg);
		gtk_object_get(item,
			       "clip_width", &width,
			       "clip_height", &height,
			       NULL);

		if (ee->draw_borders) {
			xthick = 0;
			ythick = 0;
		} else {
			xthick = widget->style->klass->xthickness;
			ythick = widget->style->klass->ythickness;
		}

		switch (ee->justification) {
		case GTK_JUSTIFY_CENTER:
			anchor = GTK_ANCHOR_N;
			e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(ee->item), width / 2, ythick);
			break;
		case GTK_JUSTIFY_RIGHT:
			anchor = GTK_ANCHOR_NE;
			e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(ee->item), width - xthick, ythick);
			break;
		default:
			anchor = GTK_ANCHOR_NW;
			e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(ee->item), xthick, ythick);
			break;
		}
		gtk_object_set(item,
			       "justification", ee->justification,
			       "anchor", anchor,
			       NULL);
		break;

	case ARG_FILL_COLOR:
		gtk_object_set(item,
			       "fill_color", GTK_VALUE_STRING (*arg),
			       NULL);
		break;

	case ARG_FILL_COLOR_GDK:
		gtk_object_set(item,
			       "fill_color_gdk", GTK_VALUE_BOXED (*arg),
			       NULL);
		break;

	case ARG_FILL_COLOR_RGBA:
		gtk_object_set(item,
			       "fill_color_rgba", GTK_VALUE_UINT (*arg),
			       NULL);
		break;

	case ARG_FILL_STIPPLE:
		gtk_object_set(item,
			       "fill_stiple", GTK_VALUE_BOXED (*arg),
			       NULL);
		break;

	case ARG_EDITABLE:
		gtk_object_set(item,
			       "editable", GTK_VALUE_BOOL (*arg),
			       NULL);
		break;

	case ARG_USE_ELLIPSIS:
		gtk_object_set(item,
			       "use_ellipsis", GTK_VALUE_BOOL (*arg),
			       NULL);
		break;

	case ARG_ELLIPSIS:
		gtk_object_set(item,
			       "ellipsis", GTK_VALUE_STRING (*arg),
			       NULL);
		break;

	case ARG_LINE_WRAP:
		gtk_object_set(item,
			       "line_wrap", GTK_VALUE_BOOL (*arg),
			       NULL);
		break;
		
	case ARG_BREAK_CHARACTERS:
		gtk_object_set(item,
			       "break_characters", GTK_VALUE_STRING (*arg),
			       NULL);
		break;

	case ARG_MAX_LINES:
		gtk_object_set(item,
			       "max_lines", GTK_VALUE_INT (*arg),
			       NULL);
		break;

	case ARG_ALLOW_NEWLINES:
		gtk_object_set(item,
			       "allow_newlines", GTK_VALUE_BOOL (*arg),
			       NULL);
		break;

	case ARG_DRAW_BORDERS: {
		gboolean need_queue;
		
		need_queue = (ee->draw_borders ^ GTK_VALUE_BOOL (*arg));
		gtk_object_set (item, "draw_borders", GTK_VALUE_BOOL (*arg), NULL);
		ee->draw_borders = GTK_VALUE_BOOL (*arg);
		if (need_queue)
			gtk_widget_queue_resize (GTK_WIDGET (ee));
		break;
	}

	case ARG_CURSOR_POS:
		gtk_object_set (item,
				"cursor_pos", GTK_VALUE_INT (*arg), NULL);
		break;
		
	case ARG_DRAW_BACKGROUND:
		gtk_object_set (item, "draw_background",
				GTK_VALUE_BOOL (*arg), NULL);
		break;
	}
}

static void
e_entry_class_init (GtkObjectClass *object_class)
{
	EEntryClass *klass = E_ENTRY_CLASS(object_class);
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->set_arg = et_set_arg;
	object_class->get_arg = et_get_arg;

	klass->changed = NULL;
	klass->activate = NULL;

	e_entry_signals[E_ENTRY_CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EEntryClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_entry_signals[E_ENTRY_ACTIVATE] =
		gtk_signal_new ("activate",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EEntryClass, activate),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);


	gtk_object_class_add_signals (object_class, e_entry_signals, E_ENTRY_LAST_SIGNAL);

	gtk_object_add_arg_type ("EEntry::model",
				 GTK_TYPE_OBJECT, GTK_ARG_READWRITE, ARG_MODEL);  
	gtk_object_add_arg_type ("EEntry::event_processor",
				 GTK_TYPE_OBJECT, GTK_ARG_READWRITE, ARG_EVENT_PROCESSOR);
	gtk_object_add_arg_type ("EEntry::text",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_TEXT);
	gtk_object_add_arg_type ("EEntry::font",
				 GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_FONT);
	gtk_object_add_arg_type ("EEntry::fontset",
				 GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_FONTSET);
	gtk_object_add_arg_type ("EEntry::font_gdk",
				 GTK_TYPE_GDK_FONT, GTK_ARG_READWRITE, ARG_FONT_GDK);
	gtk_object_add_arg_type ("EEntry::justification",
				 GTK_TYPE_JUSTIFICATION, GTK_ARG_READWRITE, ARG_JUSTIFICATION);
	gtk_object_add_arg_type ("EEntry::fill_color",
				 GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_FILL_COLOR);
	gtk_object_add_arg_type ("EEntry::fill_color_gdk",
				 GTK_TYPE_GDK_COLOR, GTK_ARG_READWRITE, ARG_FILL_COLOR_GDK);
	gtk_object_add_arg_type ("EEntry::fill_color_rgba",
				 GTK_TYPE_UINT, GTK_ARG_READWRITE, ARG_FILL_COLOR_RGBA);
	gtk_object_add_arg_type ("EEntry::fill_stipple",
				 GTK_TYPE_GDK_WINDOW, GTK_ARG_READWRITE, ARG_FILL_STIPPLE);
	gtk_object_add_arg_type ("EEntry::editable",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_EDITABLE);
	gtk_object_add_arg_type ("EEntry::use_ellipsis",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_USE_ELLIPSIS);
	gtk_object_add_arg_type ("EEntry::ellipsis",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ELLIPSIS);
	gtk_object_add_arg_type ("EEntry::line_wrap",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_LINE_WRAP);
	gtk_object_add_arg_type ("EEntry::break_characters",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_BREAK_CHARACTERS);
	gtk_object_add_arg_type ("EEntry::max_lines",
				 GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_MAX_LINES);
	gtk_object_add_arg_type ("EEntry::allow_newlines",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_ALLOW_NEWLINES);
	gtk_object_add_arg_type ("EEntry::draw_borders",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_DRAW_BORDERS);
	gtk_object_add_arg_type ("EEntry::draw_background",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_DRAW_BACKGROUND);
	gtk_object_add_arg_type ("EEntry::cursor_pos",
				 GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_CURSOR_POS);
}

E_MAKE_TYPE(e_entry, "EEntry", EEntry, e_entry_class_init, e_entry_init, PARENT_TYPE);
