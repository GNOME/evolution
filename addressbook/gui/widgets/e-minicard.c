/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-minicard.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkmain.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include <libgnomecanvas/gnome-canvas-pixbuf.h>
#include <gal/e-text/e-text.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-canvas-utils.h>
#include <gal/widgets/e-canvas.h>
#include "addressbook/backend/ebook/e-book.h"
#include "e-addressbook-marshal.h"
#include "e-addressbook-util.h"
#include "e-minicard.h"
#include "e-minicard-label.h"
#include "e-minicard-view.h"
#include "e-contact-editor.h"
#include "e-card-merging.h"
#include "ebook/e-destination.h"

static void e_minicard_init		(EMinicard		 *card);
static void e_minicard_class_init	(EMinicardClass	 *klass);
static void e_minicard_set_property  (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_minicard_get_property  (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_minicard_dispose (GObject *object);
static void e_minicard_finalize (GObject *object);
static gboolean e_minicard_event (GnomeCanvasItem *item, GdkEvent *event);
static void e_minicard_realize (GnomeCanvasItem *item);
static void e_minicard_unrealize (GnomeCanvasItem *item);
static void e_minicard_reflow ( GnomeCanvasItem *item, int flags );
static void e_minicard_style_set (EMinicard *minicard, GtkStyle *previous_style);

static void e_minicard_resize_children( EMinicard *e_minicard );
static void remodel( EMinicard *e_minicard );

static gint e_minicard_drag_begin (EMinicard *minicard, GdkEvent *event);

static GnomeCanvasGroupClass *parent_class = NULL;

typedef struct _EMinicardField EMinicardField;

struct _EMinicardField {
	ECardSimpleField field;
	GnomeCanvasItem *label;
};

#define d(x)

#define LIST_ICON_FILENAME "contact-list-16.png"

#define E_MINICARD_FIELD(field) ((EMinicardField *)(field))

static void
e_minicard_field_destroy(EMinicardField *field)
{
	gtk_object_destroy(GTK_OBJECT(field->label));
	g_free(field);
}

/* The arguments we take */
enum {
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_HAS_FOCUS,
	PROP_SELECTED,
	PROP_HAS_CURSOR,
	PROP_EDITABLE,
	PROP_CARD
};

enum {
	SELECTED,
	DRAG_BEGIN,
	STYLE_SET,
	LAST_SIGNAL
};

static guint e_minicard_signals [LAST_SIGNAL] = {0, };

GType
e_minicard_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EMinicardClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_minicard_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EMinicard),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_minicard_init,
		};

		type = g_type_register_static (gnome_canvas_group_get_type (), "EMinicard", &info, 0);
	}

	return type;
}

static void
e_minicard_class_init (EMinicardClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) klass;

	object_class->set_property  = e_minicard_set_property;
	object_class->get_property  = e_minicard_get_property;
	object_class->dispose    = e_minicard_dispose;
	object_class->finalize      = e_minicard_finalize;

	klass->style_set = e_minicard_style_set;

	parent_class = gtk_type_class (gnome_canvas_group_get_type ());

	g_object_class_install_property (object_class, PROP_WIDTH,
					 g_param_spec_double ("width",
							      _("Width"),
							      /*_( */"XXX blurb" /*)*/,
							      0.0, G_MAXDOUBLE, 10.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_HEIGHT,
					 g_param_spec_double ("height",
							      _("Height"),
							      /*_( */"XXX blurb" /*)*/,
							      0.0, G_MAXDOUBLE, 10.0,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_HAS_FOCUS,
					 /* XXX should be _enum */
					 g_param_spec_int ("has_focus",
							   _("Has Focus"),
							   /*_( */"XXX blurb" /*)*/,
							   E_MINICARD_FOCUS_TYPE_START, E_MINICARD_FOCUS_TYPE_END,
							   E_MINICARD_FOCUS_TYPE_START,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_SELECTED,
					 g_param_spec_boolean ("selected",
							       _("Selected"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_HAS_CURSOR,
					 g_param_spec_boolean ("has_cursor",
							       _("Has Cursor"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));
  
	g_object_class_install_property (object_class, PROP_CARD,
					 g_param_spec_object ("card",
							      _("Card"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_CARD,
							      G_PARAM_READWRITE));
  
	e_minicard_signals [SELECTED] =
		g_signal_new ("selected",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMinicardClass, selected),
			      NULL, NULL,
			      e_addressbook_marshal_INT__POINTER,
			      G_TYPE_INT, 1, G_TYPE_POINTER);

	e_minicard_signals [DRAG_BEGIN] =
		g_signal_new ("drag_begin",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMinicardClass, drag_begin),
			      NULL, NULL,
			      e_addressbook_marshal_INT__POINTER,
			      G_TYPE_INT, 1, G_TYPE_POINTER);

	e_minicard_signals [STYLE_SET] =
		g_signal_new ("style_set",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMinicardClass, style_set),
			      NULL, NULL,
			      e_addressbook_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1,
			      GTK_TYPE_STYLE);

	/* GnomeCanvasItem method overrides */
	item_class->realize    = e_minicard_realize;
	item_class->unrealize  = e_minicard_unrealize;
	item_class->event      = e_minicard_event;

	klass->selected        = NULL;
}

static void
e_minicard_init (EMinicard *minicard)
{
	/*   minicard->card = NULL;*/
	minicard->rect             = NULL;
	minicard->fields           = NULL;
	minicard->width            = 10;
	minicard->height           = 10;
	minicard->has_focus        = FALSE;
	minicard->selected         = FALSE;
	minicard->editable         = FALSE;
	minicard->has_cursor       = FALSE;

	minicard->card             = NULL;
	minicard->simple           = e_card_simple_new(NULL);

	minicard->list_icon_pixbuf = gdk_pixbuf_new_from_file (EVOLUTION_IMAGESDIR "/" LIST_ICON_FILENAME, NULL);
	minicard->list_icon_size   = gdk_pixbuf_get_height (minicard->list_icon_pixbuf);

	minicard->editor           = NULL;

	minicard->changed          = FALSE;

	e_canvas_item_set_reflow_callback(GNOME_CANVAS_ITEM(minicard), e_minicard_reflow);
}

static void
set_selected (EMinicard *minicard, gboolean selected)
{
	GtkWidget *canvas = GTK_WIDGET(GNOME_CANVAS_ITEM(minicard)->canvas);
	if (selected) {
		gnome_canvas_item_set (minicard->rect, 
				       "outline_color_gdk", &canvas->style->bg[GTK_STATE_ACTIVE],
				       NULL);
		gnome_canvas_item_set (minicard->header_rect, 
				       "fill_color_gdk", &canvas->style->bg[GTK_STATE_SELECTED],
				       NULL);
		gnome_canvas_item_set (minicard->header_text, 
				       "fill_color_gdk", &canvas->style->text[GTK_STATE_SELECTED],
				       NULL);
	} else {
		gnome_canvas_item_set (minicard->rect, 
				       "outline_color", NULL, 
				       NULL);
		gnome_canvas_item_set (minicard->header_rect, 
				       "fill_color_gdk", &canvas->style->bg[GTK_STATE_NORMAL],
				       NULL);
		gnome_canvas_item_set (minicard->header_text, 
				       "fill_color_gdk", &canvas->style->text[GTK_STATE_NORMAL],
				       NULL);
	}
	minicard->selected = selected;
}

static void
set_has_cursor (EMinicard *minicard, gboolean has_cursor)
{
	if (!minicard->has_focus && has_cursor)
		e_canvas_item_grab_focus(GNOME_CANVAS_ITEM (minicard), FALSE);
	minicard->has_cursor = has_cursor;
}


static void
e_minicard_set_property  (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	EMinicard *e_minicard;
	GList *l;

	item = GNOME_CANVAS_ITEM (object);
	e_minicard = E_MINICARD (object);
	
	switch (prop_id){
	case PROP_WIDTH:
		if (e_minicard->width != g_value_get_double (value)) {
			e_minicard->width = g_value_get_double (value);
			e_minicard_resize_children(e_minicard);
			if ( GTK_OBJECT_FLAGS( e_minicard ) & GNOME_CANVAS_ITEM_REALIZED )
				e_canvas_item_request_reflow(item);
		}
	  break;
	case PROP_HAS_FOCUS:
		if (e_minicard->fields) {
			if ( g_value_get_int(value) == E_FOCUS_START ||
			     g_value_get_int(value) == E_FOCUS_CURRENT) {
				gnome_canvas_item_set(E_MINICARD_FIELD(e_minicard->fields->data)->label,
						      "has_focus", g_value_get_int (value),
						      NULL);
			} else if ( g_value_get_int (value) == E_FOCUS_END ) {
				gnome_canvas_item_set(E_MINICARD_FIELD(g_list_last(e_minicard->fields)->data)->label,
						      "has_focus", g_value_get_int (value),
						      NULL);
			}
		}
		else {
			if (!e_minicard->has_focus)
				e_canvas_item_grab_focus(item, FALSE);
		}
		break;
	case PROP_SELECTED:
		if (e_minicard->selected != g_value_get_boolean (value))
			set_selected (e_minicard, g_value_get_boolean (value));
		break;
	case PROP_EDITABLE:
		e_minicard->editable = g_value_get_boolean (value);
		for (l = e_minicard->fields; l; l = l->next)
			g_object_set (E_MINICARD_FIELD (l->data)->label,
				      "editable", e_minicard->editable,
				      NULL);
		break;
	case PROP_HAS_CURSOR:
		d(g_print("%s: PROP_HAS_CURSOR\n", G_GNUC_FUNCTION));
		if (e_minicard->has_cursor != g_value_get_boolean (value))
			set_has_cursor (e_minicard, g_value_get_boolean (value));
		break;
	case PROP_CARD:
		if (e_minicard->card)
			g_object_unref (e_minicard->card);
		e_minicard->card = E_CARD(g_value_get_object (value));
		if (e_minicard->card)
			g_object_ref (e_minicard->card);
		g_object_set(e_minicard->simple,
			     "card", e_minicard->card,
			     NULL);
		remodel(e_minicard);
		e_canvas_item_request_reflow(item);
		e_minicard->changed = FALSE;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_minicard_get_property  (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	EMinicard *e_minicard;

	e_minicard = E_MINICARD (object);

	switch (prop_id) {
	case PROP_WIDTH:
		g_value_set_double (value, e_minicard->width);
		break;
	case PROP_HEIGHT:
		g_value_set_double (value, e_minicard->height);
		break;
	case PROP_HAS_FOCUS:
		g_value_set_int (value, e_minicard->has_focus ? E_FOCUS_CURRENT : E_FOCUS_NONE);
		break;
	case PROP_SELECTED:
		g_value_set_boolean (value, e_minicard->selected);
		break;
	case PROP_HAS_CURSOR:
		g_value_set_boolean (value, e_minicard->has_cursor);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, e_minicard->editable);
		break;
	case PROP_CARD:
		e_card_simple_sync_card(e_minicard->simple);
		g_value_set_object (value, e_minicard->card);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_minicard_dispose (GObject *object)
{
	EMinicard *e_minicard;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_MINICARD (object));

	e_minicard = E_MINICARD (object);
	
	if (e_minicard->fields) {
		g_list_foreach(e_minicard->fields, (GFunc) e_minicard_field_destroy, NULL);
		g_list_free(e_minicard->fields);
		e_minicard->fields = NULL;
	}

	if (e_minicard->list_icon_pixbuf) {
		gdk_pixbuf_unref (e_minicard->list_icon_pixbuf);
		e_minicard->list_icon_pixbuf = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}



static void
e_minicard_finalize (GObject *object)
{
	EMinicard *e_minicard;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_MINICARD (object));

	e_minicard = E_MINICARD (object);
	
	if (e_minicard->card)
		g_object_unref (e_minicard->card);
	if (e_minicard->simple)
		g_object_unref (e_minicard->simple);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
e_minicard_style_set (EMinicard *minicard, GtkStyle *previous_style)
{
	if ( (GTK_OBJECT_FLAGS( minicard ) & GNOME_CANVAS_ITEM_REALIZED) )
		set_selected (minicard, minicard->selected);
}

static void
e_minicard_realize (GnomeCanvasItem *item)
{
	EMinicard *e_minicard;
	GnomeCanvasGroup *group;
	GtkWidget *canvas;

	e_minicard = E_MINICARD (item);
	group = GNOME_CANVAS_GROUP( item );
	canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (item)->canvas);

	if (GNOME_CANVAS_ITEM_CLASS(parent_class)->realize)
		(* GNOME_CANVAS_ITEM_CLASS(parent_class)->realize) (item);
	
	e_minicard->rect =
	  gnome_canvas_item_new( group,
				 gnome_canvas_rect_get_type(),
				 "x1", (double) 0,
				 "y1", (double) 0,
				 "x2", (double) MAX (e_minicard->width - 1, 0),
				 "y2", (double) MAX (e_minicard->height - 1, 0),
				 "outline_color", NULL,
				 NULL );

	e_minicard->header_rect =
	  gnome_canvas_item_new( group,
				 gnome_canvas_rect_get_type(),
				 "x1", (double) 2,
				 "y1", (double) 2,
				 "x2", (double) MAX (e_minicard->width - 3, 0),
				 "y2", (double) MAX (e_minicard->height - 3, 0),
				 "fill_color_gdk", &canvas->style->bg[GTK_STATE_NORMAL],
				 NULL );

	e_minicard->header_text =
	  gnome_canvas_item_new( group,
				 e_text_get_type(),
				 "anchor", GTK_ANCHOR_NW,
				 "width", (double) MAX( e_minicard->width - 12, 0 ),
				 "clip", TRUE,
				 "use_ellipsis", TRUE,
				 "fill_color_gdk", &canvas->style->fg[GTK_STATE_NORMAL],
				 "text", "",
				 "draw_background", FALSE,
				 NULL );

	e_canvas_item_move_absolute(e_minicard->header_text, 6, 6);

	e_minicard->list_icon = 
		gnome_canvas_item_new ( group,
					gnome_canvas_pixbuf_get_type(),
					"pixbuf", e_minicard->list_icon_pixbuf,
					NULL);

	set_selected (e_minicard, e_minicard->selected);

	remodel(e_minicard);
	e_canvas_item_request_reflow(item);
}

static void
e_minicard_unrealize (GnomeCanvasItem *item)
{
	EMinicard *e_minicard;

	e_minicard = E_MINICARD (item);

	if (GNOME_CANVAS_ITEM_CLASS(parent_class)->unrealize)
		(* GNOME_CANVAS_ITEM_CLASS(parent_class)->unrealize) (item);
}

static void
card_modified_cb (EBook* book, EBookStatus status, gpointer user_data)
{
	d(g_print ("%s: %s(): a card was modified\n", __FILE__, G_GNUC_FUNCTION));
	if (status != E_BOOK_STATUS_SUCCESS)
		e_addressbook_error_dialog (_("Error modifying card"), status);
}

/* Callback used when the contact editor is closed */
static void
editor_closed_cb (GtkObject *editor, gpointer data)
{
	EMinicard *minicard = data;
	g_object_unref (editor);
	minicard->editor = NULL;
}

static gboolean
e_minicard_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EMinicard *e_minicard;
	GtkWidget *canvas;
	
	e_minicard = E_MINICARD (item);
	canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (item)->canvas);

	switch( event->type ) {
	case GDK_FOCUS_CHANGE:
		{
			GdkEventFocus *focus_event = (GdkEventFocus *) event;
			d(g_print("%s: GDK_FOCUS_CHANGE: %s\n", G_GNUC_FUNCTION, focus_event->in?"in":"out"));
			if (focus_event->in) {
				/* Chris: When EMinicard gets the cursor, if it doesn't have the focus, it should take it.  */
				e_minicard->has_focus = TRUE;
				if (!e_minicard->selected) {
					e_minicard_selected(e_minicard, event);
				}
			} else {
				EBook *book = NULL;

				if (e_minicard->changed) {
				
					e_card_simple_sync_card(e_minicard->simple);

					if (E_IS_MINICARD_VIEW(GNOME_CANVAS_ITEM(e_minicard)->parent)) {
					
						g_object_get(GNOME_CANVAS_ITEM(e_minicard)->parent,
							     "book", &book,
							     NULL);
					
					}
				
					if (book) {
					
						/* Add the card in the contact editor to our ebook */
						e_card_merging_book_commit_card (book,
										 e_minicard->card,
										 card_modified_cb,
										 NULL);
						g_object_unref(book);
					} else {
						remodel(e_minicard);
						e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(e_minicard));
					}
					e_minicard->changed = FALSE;
				}
				e_minicard->has_focus = FALSE;
			}
		}
		break;
	case GDK_BUTTON_PRESS: {
		if (1 <= event->button.button && event->button.button <= 2) {
			int ret_val = e_minicard_selected(e_minicard, event);
			GdkEventMask mask = ((1 << (4 + event->button.button)) |
					     GDK_POINTER_MOTION_MASK |
					     GDK_BUTTON_PRESS_MASK |
					     GDK_BUTTON_RELEASE_MASK);
			
			e_canvas_item_grab_focus(item, TRUE);

			if (gnome_canvas_item_grab (GNOME_CANVAS_ITEM (e_minicard),
						    mask, NULL, event->button.time)) {
				return FALSE;
			}
			gtk_grab_add (GTK_WIDGET (GNOME_CANVAS_ITEM (e_minicard)->canvas));
			e_minicard->button_x = event->button.x;
			e_minicard->button_y = event->button.y;
			e_minicard->drag_button = event->button.button;
			e_minicard->drag_button_down = TRUE;
			return ret_val;
		} else if (event->button.button == 3) {
			int ret_val = e_minicard_selected(e_minicard, event);
			if (ret_val != 0)
				return ret_val;
		}
		break;
	}
	case GDK_BUTTON_RELEASE:
		e_minicard_selected(e_minicard, event);
		if (e_minicard->drag_button == event->button.button) {
			e_minicard->drag_button = 0;
			e_minicard->drag_button_down = FALSE;
			e_minicard->button_x = -1;
			e_minicard->button_y = -1;

			if (GTK_WIDGET_HAS_GRAB (GNOME_CANVAS_ITEM (e_minicard)->canvas)) {
				gtk_grab_remove (GTK_WIDGET (GNOME_CANVAS_ITEM (e_minicard)->canvas));
				gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM (e_minicard), event->button.time);
			}
		}
		break;
	case GDK_MOTION_NOTIFY:
		if (e_minicard->drag_button_down && event->motion.state & GDK_BUTTON1_MASK) {
			if (MAX (abs (e_minicard->button_x - event->motion.x),
				 abs (e_minicard->button_y - event->motion.y)) > 3) {
				gint ret_val;

				ret_val = e_minicard_drag_begin(e_minicard, event);

				e_minicard->drag_button_down = FALSE;

				return ret_val;
			}
		}
		break;
	case GDK_2BUTTON_PRESS:
		if (event->button.button == 1 && E_IS_MINICARD_VIEW(item->parent)) {
			if (e_minicard->editor) {
				if (e_card_evolution_list (e_minicard->card))
					e_contact_list_editor_raise (E_CONTACT_LIST_EDITOR(e_minicard->editor));
				else
					e_contact_editor_raise(E_CONTACT_EDITOR(e_minicard->editor));
			} else {
				EBook *book = NULL;
				if (E_IS_MINICARD_VIEW(item->parent)) {
					g_object_get(item->parent,
						     "book", &book,
						     NULL);
				}

				if (book != NULL) {
					if (e_card_evolution_list (e_minicard->card)) {
						EContactListEditor *editor = e_addressbook_show_contact_list_editor (book, e_minicard->card,
														     FALSE, e_minicard->editable);
						e_minicard->editor = G_OBJECT (editor);
					}
					else {
						EContactEditor *editor = e_addressbook_show_contact_editor (book, e_minicard->card,
													    FALSE, e_minicard->editable);
						e_minicard->editor = G_OBJECT (editor);
					}
					g_object_ref (e_minicard->editor);

					g_signal_connect (e_minicard->editor, "editor_closed",
							  G_CALLBACK (editor_closed_cb), e_minicard);

					g_object_unref (book);
				}
			}
			return TRUE;
		}
		break;
	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_Tab || 
		    event->key.keyval == GDK_KP_Tab || 
		    event->key.keyval == GDK_ISO_Left_Tab) {
			GList *list;
			for (list = e_minicard->fields; list; list = list->next) {
				EMinicardField *field = E_MINICARD_FIELD(list->data);
				GnomeCanvasItem *item = field->label;
				EFocus has_focus;
				g_object_get(item,
					     "has_focus", &has_focus,
					     NULL);
				if (has_focus != E_FOCUS_NONE) {
					if (event->key.state & GDK_SHIFT_MASK)
						list = list->prev;
					else
						list = list->next;
					if (list) {
						EMinicardField *field = E_MINICARD_FIELD(list->data);
						GnomeCanvasItem *item = field->label;
						gnome_canvas_item_set(item,
								      "has_focus", (event->key.state & GDK_SHIFT_MASK) ? E_FOCUS_END : E_FOCUS_START,
								      NULL);
						return 1;
					} else {
						return 0;
					}
				}
			}
		}
	default:
		break;
	}
	
	if (GNOME_CANVAS_ITEM_CLASS( parent_class )->event)
		return (* GNOME_CANVAS_ITEM_CLASS( parent_class )->event) (item, event);
	else
		return 0;
}

static void
e_minicard_resize_children( EMinicard *e_minicard )
{
	GList *list;
	
	if (e_minicard->header_text) {
		gnome_canvas_item_set( e_minicard->header_text,
				       "width", ((double) e_minicard->width - 12 
						 - (e_card_evolution_list (e_minicard->card) ? e_minicard->list_icon_size : 0.0)),
				       NULL );
	}
	if (e_minicard->list_icon) {
		e_canvas_item_move_absolute(e_minicard->list_icon,
					    e_minicard->width - e_minicard->list_icon_size - 3,
					    3);
	}
	for ( list = e_minicard->fields; list; list = g_list_next( list ) ) {
		gnome_canvas_item_set( E_MINICARD_FIELD( list->data )->label,
				       "width", (double) e_minicard->width - 4.0,
				       NULL );
	}
}

static void
field_changed (EText *text, EMinicard *e_minicard)
{
	ECardSimpleType type;
	char *string;
	char *new_string;
	gboolean is_list = FALSE;

	type = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(text),
				   "EMinicard:field"));
	g_object_get(text,
		     "text", &string,
		     NULL);

	/* 
	 * If the card is coresponding with a contact list and the field be 
	 * changed is e-mail address, should wrap it before write it back.
	 */
	new_string = (char*)e_card_simple_get_const (e_minicard->simple, 
						     E_CARD_SIMPLE_FIELD_IS_LIST); 

	is_list = (NULL != new_string);	

	if  (is_list && (E_CARD_SIMPLE_FIELD_EMAIL == type || 
	    	  E_CARD_SIMPLE_FIELD_EMAIL_2 == type ||
		      E_CARD_SIMPLE_FIELD_EMAIL_3 == type)) {
		if (string && *string) {
			EDestination *dest = e_destination_new (); 
			if (dest != NULL){
				e_destination_set_email (dest, string);
				new_string = e_destination_export(dest);
				g_free(string);
				string=new_string;
				g_object_unref (dest);
			}
		}
	}

	e_card_simple_set(e_minicard->simple,
			  type,
			  string);
	g_free(string);
	e_minicard->changed = TRUE;
}

static void
field_activated (EText *text, EMinicard *e_minicard)
{
	e_text_stop_editing (text);
	e_canvas_item_grab_focus (GNOME_CANVAS_ITEM (e_minicard), FALSE);
}

static void
add_field (EMinicard *e_minicard, ECardSimpleField field, gdouble left_width)
{
	GnomeCanvasItem *new_item;
	GnomeCanvasGroup *group;
	ECardSimpleType type;
	EMinicardField *minicard_field;
	char *name;
	char *string;
	
	group = GNOME_CANVAS_GROUP( e_minicard );
	
	type = e_card_simple_type(e_minicard->simple, field);
	name = g_strdup_printf("%s:", e_card_simple_get_name(e_minicard->simple, field));
	string = e_card_simple_get(e_minicard->simple, field);

	/* Magically convert embedded XML into an address. */
	if (!strncmp (string, "<?xml", 5)) {
		EDestination *dest = e_destination_import (string);
		if (dest != NULL) {
			gchar *new_string = g_strdup (e_destination_get_textrep (dest, TRUE));
			g_free (string);
			string = new_string;
			g_object_unref (dest);
		}
	}

	new_item = e_minicard_label_new(group);
	gnome_canvas_item_set( new_item,
			       "width", e_minicard->width - 4.0,
			       "fieldname", name,
			       "field", string,
			       "max_field_name_length", left_width,
			       "editable", e_minicard->editable,
			       NULL );
	g_signal_connect(E_MINICARD_LABEL(new_item)->field,
			 "changed", G_CALLBACK (field_changed), e_minicard);
	g_signal_connect(E_MINICARD_LABEL(new_item)->field,
			 "activate", G_CALLBACK (field_activated), e_minicard);
	g_object_set(E_MINICARD_LABEL(new_item)->field,
		     "allow_newlines", e_card_simple_get_allow_newlines (e_minicard->simple, field),
		     NULL);
	g_object_set_data(G_OBJECT (E_MINICARD_LABEL(new_item)->field),
			  "EMinicard:field",
			  GINT_TO_POINTER(field));

	minicard_field = g_new(EMinicardField, 1);
	minicard_field->field = field;
	minicard_field->label = new_item;

	e_minicard->fields = g_list_append( e_minicard->fields, minicard_field);
	e_canvas_item_move_absolute(new_item, 2, e_minicard->height);
	g_free(name);
	g_free(string);
}

static int
get_left_width(EMinicard *e_minicard)
{
	gchar *name;
	ECardSimpleField field;
	int width = -1;
	PangoLayout *layout;

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (GNOME_CANVAS_ITEM (e_minicard)->canvas), "");
	for(field = E_CARD_SIMPLE_FIELD_FULL_NAME; field != E_CARD_SIMPLE_FIELD_LAST; field++) {
		int this_width;
		name = g_strdup_printf("%s:", e_card_simple_get_name(e_minicard->simple, field));
		pango_layout_set_text (layout, name, -1);
		pango_layout_get_pixel_size (layout, &this_width, NULL);
		if (width < this_width)
			width = this_width;
		g_free(name);
	}
	g_object_unref (layout);
	return width;
}

static void
remodel( EMinicard *e_minicard )
{
	int count = 0;
	if ( !(GTK_OBJECT_FLAGS( e_minicard ) & GNOME_CANVAS_ITEM_REALIZED) )
		return;
	if (e_minicard->simple) {
		ECardSimpleField field;
		GList *list;
		char *file_as;
		int left_width = -1;

		if (e_minicard->header_text) {
			file_as = e_card_simple_get(e_minicard->simple, E_CARD_SIMPLE_FIELD_FILE_AS);
			gnome_canvas_item_set( e_minicard->header_text,
					       "text", file_as ? file_as : "",
					       NULL );
			g_free(file_as);
		}

		if (e_minicard->card && e_card_evolution_list (e_minicard->card) ) {
			gnome_canvas_item_show (e_minicard->list_icon);
		}
		else {
			gnome_canvas_item_hide (e_minicard->list_icon);
		}

		list = e_minicard->fields;
		e_minicard->fields = NULL;

		for(field = E_CARD_SIMPLE_FIELD_FULL_NAME; field != E_CARD_SIMPLE_FIELD_LAST_SIMPLE_STRING && count < 5; field++) {
			EMinicardField *minicard_field = NULL;

			if (list)
				minicard_field = list->data;
			if (minicard_field && minicard_field->field == field) {
				GList *this_list = list;
				char *string;

				string = e_card_simple_get(e_minicard->simple, field);
				if (string && *string) {
					/* Magically convert embedded XML into an address. */
					if (!strncmp (string, "<?xml", 4)) {
						EDestination *dest = e_destination_import (string);
						if (dest != NULL) {
							gchar *new_string = g_strdup (e_destination_get_textrep (dest, TRUE));
							g_free (string);
							string = new_string;
							g_object_unref (dest);
						}
					}

					e_minicard->fields = g_list_append(e_minicard->fields, minicard_field);
					g_object_set(minicard_field->label,
						     "field", string,
						     NULL);
					count ++;
				} else {
					e_minicard_field_destroy(minicard_field);
				}
				list = g_list_remove_link(list, this_list);
				g_list_free_1(this_list);
				g_free(string);
			} else {
				char *string;
				if (left_width == -1) {
					left_width = get_left_width(e_minicard);
				}

				string = e_card_simple_get(e_minicard->simple, field);
				if (string && *string) {
					add_field(e_minicard, field, left_width);
					count++;
				}
				g_free(string);
			}
		}

		g_list_foreach(list, (GFunc) e_minicard_field_destroy, NULL);
		g_list_free(list);
	}
}

static void
e_minicard_reflow( GnomeCanvasItem *item, int flags )
{
	EMinicard *e_minicard = E_MINICARD(item);
	if ( GTK_OBJECT_FLAGS( e_minicard ) & GNOME_CANVAS_ITEM_REALIZED ) {
		GList *list;
		gdouble text_height;
		gint old_height;
		
		old_height = e_minicard->height;

		g_object_get( e_minicard->header_text,
			      "text_height", &text_height,
			      NULL );
		
		e_minicard->height = text_height + 10.0;
		
		gnome_canvas_item_set( e_minicard->header_rect,
				       "y2", text_height + 9.0,
				       NULL );
		
		for(list = e_minicard->fields; list; list = g_list_next(list)) {
			EMinicardField *field = E_MINICARD_FIELD(list->data);
			GnomeCanvasItem *item = field->label;
			g_object_get (item,
				      "height", &text_height,
				      NULL);
			e_canvas_item_move_absolute(item, 2, e_minicard->height);
			e_minicard->height += text_height;
		}
		e_minicard->height += 2;
		
		gnome_canvas_item_set( e_minicard->rect,
				       "x2", (double) e_minicard->width - 1.0,
				       "y2", (double) e_minicard->height - 1.0,
				       NULL );
		gnome_canvas_item_set( e_minicard->header_rect,
				       "x2", (double) e_minicard->width - 3.0,
				       NULL );

		if (old_height != e_minicard->height)
			e_canvas_item_request_parent_reflow(item);
	}
}

const char *
e_minicard_get_card_id (EMinicard *minicard)
{
	g_return_val_if_fail(minicard != NULL, NULL);
	g_return_val_if_fail(E_IS_MINICARD(minicard), NULL);

	if (minicard->card) {
		return e_card_get_id(minicard->card);
	} else {
		return "";
	}
}

int
e_minicard_compare (EMinicard *minicard1, EMinicard *minicard2)
{
	int cmp = 0;
	
	g_return_val_if_fail(minicard1 != NULL, 0);
	g_return_val_if_fail(E_IS_MINICARD(minicard1), 0);
	g_return_val_if_fail(minicard2 != NULL, 0);
	g_return_val_if_fail(E_IS_MINICARD(minicard2), 0);

	if (minicard1->card && minicard2->card) {
		char *file_as1, *file_as2;

		g_object_get(minicard1->card,
			     "file_as", &file_as1,
			     NULL);
		g_object_get(minicard2->card,
			     "file_as", &file_as2,
			     NULL);

		if (file_as1 && file_as2) 
			cmp = g_utf8_collate(file_as1, file_as2);
		else if (file_as1)
			cmp = -1;
		else if (file_as2)
			cmp = 1;
		else 
			cmp = strcmp(e_minicard_get_card_id(minicard1), e_minicard_get_card_id(minicard2));

		g_free (file_as1);
		g_free (file_as2);
	}

	return cmp;
}

int
e_minicard_selected (EMinicard *minicard, GdkEvent *event)
{
	gint ret_val = 0;
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (minicard);
	if (item->parent) {
		guint signal_id = g_signal_lookup ("selection_event", G_OBJECT_TYPE (item->parent));
		/* We should probably check the signature here, but I
		 * don't think it's worth the time required to code
		 * it.
		 */
		if (signal_id != 0) {
			g_signal_emit(item->parent,
				      signal_id, 0,
				      item, event, &ret_val);
		}
	}
	return ret_val;
}

static gint
e_minicard_drag_begin (EMinicard *minicard, GdkEvent *event)
{
	gint ret_val = 0;
	GnomeCanvasItem *parent;
	g_signal_emit (minicard,
		       e_minicard_signals[DRAG_BEGIN], 0,
		       event, &ret_val);

	parent = GNOME_CANVAS_ITEM (minicard)->parent;
	if (parent && E_IS_REFLOW (parent)) {
		E_REFLOW (parent)->maybe_in_drag = FALSE;
	}
	return ret_val;
}
