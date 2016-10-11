/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-minicard.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <libgnomecanvas/libgnomecanvas.h>

#include "e-util/e-util.h"

#include "eab-book-util.h"
#include "eab-gui-util.h"
#include "e-minicard-label.h"
#include "e-minicard-view.h"
#include "ea-addressbook.h"

static void e_minicard_set_property  (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void e_minicard_get_property  (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void e_minicard_dispose (GObject *object);
static void e_minicard_finalize (GObject *object);
static gboolean e_minicard_event (GnomeCanvasItem *item, GdkEvent *event);
static void e_minicard_realize (GnomeCanvasItem *item);
static void e_minicard_reflow (GnomeCanvasItem *item, gint flags);
static void e_minicard_style_updated (EMinicard *minicard);

static void e_minicard_resize_children (EMinicard *e_minicard);
static void remodel (EMinicard *e_minicard);

static gint e_minicard_drag_begin (EMinicard *minicard, GdkEvent *event);

#define d(x)

#define LIST_ICON_NAME "stock_contact-list"

static void
e_minicard_field_destroy (EMinicardField *field)
{
	g_object_run_dispose (G_OBJECT (field->label));
	g_free (field);
}

enum {
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_HAS_FOCUS,
	PROP_SELECTED,
	PROP_HAS_CURSOR,
	PROP_EDITABLE,
	PROP_CONTACT
};

enum {
	SELECTED,
	DRAG_BEGIN,
	OPEN_CONTACT,
	STYLE_UPDATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0, };

G_DEFINE_TYPE (EMinicard, e_minicard, GNOME_TYPE_CANVAS_GROUP)

static void
e_minicard_class_init (EMinicardClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = e_minicard_set_property;
	object_class->get_property = e_minicard_get_property;
	object_class->dispose = e_minicard_dispose;
	object_class->finalize = e_minicard_finalize;

	item_class = GNOME_CANVAS_ITEM_CLASS (class);
	item_class->realize = e_minicard_realize;
	item_class->event = e_minicard_event;

	class->style_updated = e_minicard_style_updated;
	class->selected = NULL;

	g_object_class_install_property (
		object_class,
		PROP_WIDTH,
		g_param_spec_double (
			"width",
			"Width",
			NULL,
			0.0, G_MAXDOUBLE, 10.0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HEIGHT,
		g_param_spec_double (
			"height",
			"Height",
			NULL,
			0.0, G_MAXDOUBLE, 10.0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_HAS_FOCUS,
		/* XXX should be _enum */
		g_param_spec_int (
			"has_focus",
			"Has Focus",
			NULL,
			E_MINICARD_FOCUS_TYPE_START,
			E_MINICARD_FOCUS_TYPE_END,
			E_MINICARD_FOCUS_TYPE_START,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SELECTED,
		g_param_spec_boolean (
			"selected",
			"Selected",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HAS_CURSOR,
		g_param_spec_boolean (
			"has_cursor",
			"Has Cursor",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_EDITABLE,
		g_param_spec_boolean (
			"editable",
			"Editable",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CONTACT,
		g_param_spec_object (
			"contact",
			"Contact",
			NULL,
			E_TYPE_CONTACT,
			G_PARAM_READWRITE));

	signals[SELECTED] = g_signal_new (
		"selected",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMinicardClass, selected),
		NULL, NULL,
		e_marshal_INT__POINTER,
		G_TYPE_INT, 1,
		G_TYPE_POINTER);

	signals[DRAG_BEGIN] = g_signal_new (
		"drag_begin",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMinicardClass, drag_begin),
		NULL, NULL,
		e_marshal_INT__POINTER,
		G_TYPE_INT, 1,
		G_TYPE_POINTER);

	signals[OPEN_CONTACT] = g_signal_new (
		"open-contact",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMinicardClass, open_contact),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CONTACT);

	signals[STYLE_UPDATED] = g_signal_new (
		"style_updated",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMinicardClass, style_updated),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/* init the accessibility support for e_minicard */
	e_minicard_a11y_init ();
}

static void
e_minicard_init (EMinicard *minicard)
{
	minicard->rect = NULL;
	minicard->fields = NULL;
	minicard->width = 10;
	minicard->height = 10;
	minicard->has_focus = FALSE;
	minicard->selected = FALSE;
	minicard->editable = FALSE;
	minicard->has_cursor = FALSE;

	minicard->contact = NULL;

	minicard->list_icon_pixbuf = e_icon_factory_get_icon (LIST_ICON_NAME, GTK_ICON_SIZE_MENU);
	minicard->list_icon_size = gdk_pixbuf_get_height (minicard->list_icon_pixbuf);

	minicard->changed = FALSE;

	e_canvas_item_set_reflow_callback (GNOME_CANVAS_ITEM (minicard), e_minicard_reflow);
}

static void
set_selected (EMinicard *minicard,
              gboolean selected)
{
	GtkWidget *widget;
	GdkColor outline, header, text;

	widget = GTK_WIDGET (GNOME_CANVAS_ITEM (minicard)->canvas);

	if (selected) {
		e_utils_get_theme_color_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &outline);
		e_utils_get_theme_color_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &header);
		e_utils_get_theme_color_color (widget, "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &text);

		gnome_canvas_item_set (
			minicard->rect,
			"outline_color_gdk", &outline,
			NULL);
		gnome_canvas_item_set (
			minicard->header_rect,
			"fill_color_gdk", &header,
			NULL);
		gnome_canvas_item_set (
			minicard->header_text,
			"fill_color_gdk", &text,
			NULL);
	} else {
		e_utils_get_theme_color_color (widget, "theme_bg_color", E_UTILS_DEFAULT_THEME_BG_COLOR, &header);
		e_utils_get_theme_color_color (widget, "theme_text_color,theme_fg_color", E_UTILS_DEFAULT_THEME_TEXT_COLOR, &text);

		gnome_canvas_item_set (
			minicard->rect,
			"outline_color", NULL,
			NULL);
		gnome_canvas_item_set (
			minicard->header_rect,
			"fill_color_gdk", &header,
			NULL);
		gnome_canvas_item_set (
			minicard->header_text,
			"fill_color_gdk", &text,
			NULL);
	}
	minicard->selected = selected;
}

static void
set_has_cursor (EMinicard *minicard,
                gboolean has_cursor)
{
	if (!minicard->has_focus && has_cursor)
		e_canvas_item_grab_focus (GNOME_CANVAS_ITEM (minicard), FALSE);
	minicard->has_cursor = has_cursor;
}

static void
e_minicard_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	EMinicard *e_minicard;
	EContact *contact;
	GList *l;

	item = GNOME_CANVAS_ITEM (object);
	e_minicard = E_MINICARD (object);

	switch (property_id) {
	case PROP_WIDTH:
		if (e_minicard->width != g_value_get_double (value)) {
			e_minicard->width = g_value_get_double (value);
			e_minicard_resize_children (e_minicard);
			if (item->flags & GNOME_CANVAS_ITEM_REALIZED)
				e_canvas_item_request_reflow (item);
		}
	  break;
	case PROP_HAS_FOCUS:
		if (e_minicard->fields) {
			if (g_value_get_int (value) == E_FOCUS_START ||
			     g_value_get_int (value) == E_FOCUS_CURRENT) {
				gnome_canvas_item_set (
					E_MINICARD_FIELD (e_minicard->fields->data)->label,
					"has_focus", g_value_get_int (value),
					NULL);
			} else if (g_value_get_int (value) == E_FOCUS_END) {
				gnome_canvas_item_set (
					E_MINICARD_FIELD (g_list_last (e_minicard->fields)->data)->label,
					"has_focus", g_value_get_int (value),
					NULL);
			}
		}
		else {
			if (!e_minicard->has_focus)
				e_canvas_item_grab_focus (item, FALSE);
		}
		break;
	case PROP_SELECTED:
		if (e_minicard->selected != g_value_get_boolean (value))
			set_selected (e_minicard, g_value_get_boolean (value));
		break;
	case PROP_EDITABLE:
		e_minicard->editable = g_value_get_boolean (value);
		for (l = e_minicard->fields; l; l = l->next) {
			g_object_set (
				E_MINICARD_FIELD (l->data)->label,
				"editable", FALSE /* e_minicard->editable */,
				NULL);
		}
		break;
	case PROP_HAS_CURSOR:
		d (g_print ("%s: PROP_HAS_CURSOR\n", G_STRFUNC));
		if (e_minicard->has_cursor != g_value_get_boolean (value))
			set_has_cursor (e_minicard, g_value_get_boolean (value));
		break;
	case PROP_CONTACT:
		contact = E_CONTACT (g_value_get_object (value));
		if (contact)
			g_object_ref (contact);

		if (e_minicard->contact)
			g_object_unref (e_minicard->contact);

		e_minicard->contact = contact;

		remodel (e_minicard);
		e_canvas_item_request_reflow (item);
		e_minicard->changed = FALSE;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_minicard_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	EMinicard *e_minicard;

	e_minicard = E_MINICARD (object);

	switch (property_id) {
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
	case PROP_CONTACT:
		g_value_set_object (value, e_minicard->contact);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
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
		g_list_foreach (e_minicard->fields, (GFunc) e_minicard_field_destroy, NULL);
		g_list_free (e_minicard->fields);
		e_minicard->fields = NULL;
	}

	if (e_minicard->list_icon_pixbuf) {
		g_object_unref (e_minicard->list_icon_pixbuf);
		e_minicard->list_icon_pixbuf = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_minicard_parent_class)->dispose (object);
}

static void
e_minicard_finalize (GObject *object)
{
	EMinicard *e_minicard;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_MINICARD (object));

	e_minicard = E_MINICARD (object);

	if (e_minicard->contact) {
		g_object_unref (e_minicard->contact);
		e_minicard->contact = NULL;
	}

	if (e_minicard->list_icon_pixbuf) {
		g_object_unref (e_minicard->list_icon_pixbuf);
		e_minicard->list_icon_pixbuf = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_minicard_parent_class)->finalize (object);
}

static void
e_minicard_style_updated (EMinicard *minicard)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (minicard);

	if ((item->flags & GNOME_CANVAS_ITEM_REALIZED))
		set_selected (minicard, minicard->selected);
}

static void
e_minicard_realize (GnomeCanvasItem *item)
{
	EMinicard *e_minicard;
	GnomeCanvasGroup *group;

	e_minicard = E_MINICARD (item);
	group = GNOME_CANVAS_GROUP (item);

	GNOME_CANVAS_ITEM_CLASS (e_minicard_parent_class)->realize (item);

	e_minicard->rect = gnome_canvas_item_new (
		group,
		gnome_canvas_rect_get_type (),
		"x1", (gdouble) 0,
		"y1", (gdouble) 0,
		"x2", (gdouble) MAX (e_minicard->width - 1, 0),
		"y2", (gdouble) MAX (e_minicard->height - 1, 0),
		"outline_color", NULL,
		NULL);

	e_minicard->header_rect = gnome_canvas_item_new (
		group,
		gnome_canvas_rect_get_type (),
		"x1", (gdouble) 2,
		"y1", (gdouble) 2,
		"x2", (gdouble) MAX (e_minicard->width - 3, 0),
		"y2", (gdouble) MAX (e_minicard->height - 3, 0),
		"fill_color_gdk", NULL,
		NULL);

	e_minicard->header_text = gnome_canvas_item_new (
		group,
		e_text_get_type (),
		"width", (gdouble) MAX (e_minicard->width - 12, 0),
		"clip", TRUE,
		"use_ellipsis", TRUE,
		"fill_color_gdk", NULL,
		"text", "",
		NULL);

	e_canvas_item_move_absolute (e_minicard->header_text, 6, 6);

	e_minicard->list_icon = gnome_canvas_item_new (
		group,
		gnome_canvas_pixbuf_get_type (),
		"pixbuf", e_minicard->list_icon_pixbuf,
		NULL);

	set_selected (e_minicard, e_minicard->selected);

	remodel (e_minicard);
	e_canvas_item_request_reflow (item);
}

void
e_minicard_activate_editor (EMinicard *minicard)
{
	g_return_if_fail (E_IS_MINICARD (minicard));

	g_signal_emit (minicard, signals[OPEN_CONTACT], 0, minicard->contact);
}

static gboolean
e_minicard_event (GnomeCanvasItem *item,
                  GdkEvent *event)
{
	EMinicard *e_minicard;

	e_minicard = E_MINICARD (item);

	switch (event->type) {
	case GDK_FOCUS_CHANGE:
		{
			GdkEventFocus *focus_event = (GdkEventFocus *) event;
			d (g_print ("%s: GDK_FOCUS_CHANGE: %s\n", G_STRFUNC, focus_event->in?"in":"out"));
			if (focus_event->in) {
				/* Chris: When EMinicard gets the cursor, if it doesn't have the focus, it should take it.  */
				e_minicard->has_focus = TRUE;
				if (!e_minicard->selected) {
					e_minicard_selected (e_minicard, event);
				}
			}
			else {
				e_minicard->has_focus = FALSE;
			}
		}
		break;
	case GDK_BUTTON_PRESS: {
		if (1 <= event->button.button && event->button.button <= 2) {
			gint ret_val = e_minicard_selected (e_minicard, event);
			GdkGrabStatus grab_status;
			GdkDevice *event_device;
			guint32 event_time;

			e_canvas_item_grab_focus (item, TRUE);

			event_device = gdk_event_get_device (event);
			event_time = gdk_event_get_time (event);

			grab_status = gnome_canvas_item_grab (
				GNOME_CANVAS_ITEM (e_minicard),
				(1 << (4 + event->button.button)) |
				GDK_POINTER_MOTION_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK,
				NULL,
				event_device,
				event_time);

			if (grab_status != GDK_GRAB_SUCCESS)
				return FALSE;

			gtk_grab_add (GTK_WIDGET (GNOME_CANVAS_ITEM (e_minicard)->canvas));
			e_minicard->button_x = event->button.x;
			e_minicard->button_y = event->button.y;
			e_minicard->drag_button = event->button.button;
			e_minicard->drag_button_down = TRUE;
			return ret_val;
		} else if (event->button.button == 3) {
			gint ret_val = e_minicard_selected (e_minicard, event);
			if (ret_val != 0)
				return ret_val;
		}
		break;
	}
	case GDK_BUTTON_RELEASE:
		e_minicard_selected (e_minicard, event);
		if (e_minicard->drag_button == event->button.button) {
			e_minicard->drag_button = 0;
			e_minicard->drag_button_down = FALSE;
			e_minicard->button_x = -1;
			e_minicard->button_y = -1;

			if (gtk_widget_has_grab (GTK_WIDGET (GNOME_CANVAS_ITEM (e_minicard)->canvas))) {
				gtk_grab_remove (GTK_WIDGET (GNOME_CANVAS_ITEM (e_minicard)->canvas));
				gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM (e_minicard), event->button.time);
			}
		}
		break;
	case GDK_MOTION_NOTIFY:
		if (e_minicard->drag_button_down && event->motion.state & GDK_BUTTON1_MASK) {
			if (gtk_drag_check_threshold (GTK_WIDGET (item->canvas),
				e_minicard->button_x, e_minicard->button_y,
				event->motion.x, event->motion.y)) {
				gint ret_val;

				ret_val = e_minicard_drag_begin (e_minicard, event);

				if (gtk_widget_has_grab (GTK_WIDGET (GNOME_CANVAS_ITEM (e_minicard)->canvas))) {
					gtk_grab_remove (GTK_WIDGET (GNOME_CANVAS_ITEM (e_minicard)->canvas));
					gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM (e_minicard), event->motion.time);
				}

				e_minicard->drag_button = 0;
				e_minicard->drag_button_down = FALSE;
				e_minicard->button_x = -1;
				e_minicard->button_y = -1;

				return ret_val;
			}
		}
		break;
	case GDK_2BUTTON_PRESS:
		if (event->button.button == 1 && E_IS_MINICARD_VIEW (item->parent)) {
			e_minicard_activate_editor (e_minicard);
			return TRUE;
		}
		break;
	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_KEY_Tab ||
			event->key.keyval == GDK_KEY_KP_Tab ||
			event->key.keyval == GDK_KEY_ISO_Left_Tab) {

			EMinicardView *view = E_MINICARD_VIEW (item->parent);
			EReflow *reflow = E_REFLOW (view);

			if (reflow == NULL) {
				return FALSE;
			}

			if (event->key.state & GDK_SHIFT_MASK) {
				if (event->key.state & GDK_CONTROL_MASK) {
					return FALSE;
				}
				else {
					gint row_count = e_selection_model_row_count (reflow->selection);
					gint model_index = e_selection_model_cursor_row (reflow->selection);
					gint view_index = e_sorter_model_to_sorted (reflow->selection->sorter, model_index);

					if (view_index == 0)
						view_index = row_count - 1;
					else
						view_index--;

					model_index = e_sorter_sorted_to_model (E_SORTER (reflow->sorter), view_index);
					if (reflow->items[model_index] == NULL) {
						reflow->items[model_index] = e_reflow_model_incarnate (reflow->model, model_index, GNOME_CANVAS_GROUP (reflow));
						g_object_set (
							reflow->items[model_index],
							"width", (gdouble) reflow->column_width,
							NULL);

					}
					e_canvas_item_grab_focus (reflow->items[model_index], FALSE);
					return TRUE;
				}
			}
			else {
				if (event->key.state & GDK_CONTROL_MASK) {
					return FALSE;
				}
				else {
					gint row_count = e_selection_model_row_count (reflow->selection);
					gint model_index = e_selection_model_cursor_row (reflow->selection);
					gint view_index = e_sorter_model_to_sorted (reflow->selection->sorter, model_index);

					if (view_index == row_count - 1)
						view_index = 0;
					else
						view_index++;

					model_index = e_sorter_sorted_to_model (E_SORTER (reflow->sorter), view_index);
					if (reflow->items[model_index] == NULL) {
						reflow->items[model_index] = e_reflow_model_incarnate (reflow->model, model_index, GNOME_CANVAS_GROUP (reflow));
						g_object_set (
							reflow->items[model_index],
							"width", (gdouble) reflow->column_width,
							NULL);

					}
					e_canvas_item_grab_focus (reflow->items[model_index], FALSE);
					return TRUE;
				}
			}
		} else if (event->key.keyval == GDK_KEY_Left ||
			   event->key.keyval == GDK_KEY_Right) {
			EMinicardView *view = E_MINICARD_VIEW (item->parent);
			EReflow *reflow = E_REFLOW (view);
			gdouble current_x, current_y, adept_x, adept_y, check_x;
			gint row_count, model_index, view_index, inc, ii, adept_index = -1;

			if (!reflow ||
			    (event->key.state & GDK_SHIFT_MASK) != 0 ||
			    (event->key.state & GDK_CONTROL_MASK) != 0)
				return FALSE;

			inc = event->key.keyval == GDK_KEY_Left ? -1 : +1;
			row_count = e_selection_model_row_count (reflow->selection);
			model_index = e_selection_model_cursor_row (reflow->selection);
			view_index = e_sorter_model_to_sorted (reflow->selection->sorter, model_index);

			g_object_get (G_OBJECT (item),
				"x", &current_x,
				"y", &current_y,
				NULL);

			check_x = current_x;

			for (ii = view_index + inc; ii >= 0 && ii < row_count; ii += inc) {
				gdouble xx, yy;

				model_index = e_sorter_sorted_to_model (E_SORTER (reflow->sorter), ii);
				if (reflow->items[model_index] == NULL) {
					reflow->items[model_index] = e_reflow_model_incarnate (reflow->model, model_index, GNOME_CANVAS_GROUP (reflow));
					g_object_set (
						reflow->items[model_index],
						"width", (gdouble) reflow->column_width,
						NULL);
				}

				g_object_get (G_OBJECT (reflow->items[model_index]),
					"x", &xx,
					"y", &yy,
					NULL);

				/* Is it a different column? */
				if (xx - check_x > 1e-9 || xx - check_x < -1e-9) {
					if (adept_index == -1) {
						check_x = xx;
						adept_index = model_index;
						adept_x = xx;
						adept_y = yy;
						continue;
					} else
						break;
				} else if (adept_index == -1) {
					continue;
				}

				#define SQR(x) ((x) * (x))
				#define distance(x1, y1, x2, y2) (SQR ((x1) - (x2)) + SQR ((y1) - (y2)))

				if (distance (adept_x, adept_y, current_x, current_y) >
				    distance (xx, yy, current_x, current_y)) {
					adept_index = model_index;
					adept_x = xx;
					adept_y = yy;
				}

				#undef distance
				#undef SQR
			}

			if (adept_index == -1 && row_count > 0) {
				if (inc == -1)
					adept_index = e_sorter_sorted_to_model (reflow->selection->sorter, 0);
				else
					adept_index = e_sorter_sorted_to_model (reflow->selection->sorter, row_count - 1);
			}

			if (adept_index != -1) {
				if (reflow->items[adept_index] == NULL) {
					reflow->items[adept_index] = e_reflow_model_incarnate (reflow->model, adept_index, GNOME_CANVAS_GROUP (reflow));
					g_object_set (
						reflow->items[adept_index],
						"width", (gdouble) reflow->column_width,
						NULL);
				}

				e_canvas_item_grab_focus (reflow->items[adept_index], FALSE);
			}

			return TRUE;
		} else if (event->key.keyval == GDK_KEY_Return ||
			   event->key.keyval == GDK_KEY_KP_Enter) {
			e_minicard_activate_editor (e_minicard);
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

static void
e_minicard_resize_children (EMinicard *e_minicard)
{
	GList *list;
	gboolean is_list = GPOINTER_TO_INT (e_contact_get (e_minicard->contact, E_CONTACT_IS_LIST));

	if (e_minicard->header_text) {
		gnome_canvas_item_set (
			e_minicard->header_text,
			"width", ((gdouble) e_minicard->width - 12
			- (is_list ? e_minicard->list_icon_size : 0.0)),
			NULL);
	}
	if (e_minicard->list_icon) {
		e_canvas_item_move_absolute (
			e_minicard->list_icon,
			e_minicard->width - e_minicard->list_icon_size - 3,
			3);
	}
	for (list = e_minicard->fields; list; list = g_list_next (list)) {
		gnome_canvas_item_set (
			E_MINICARD_FIELD (list->data)->label,
			"width", (gdouble) e_minicard->width - 4.0,
			NULL);
	}
}

static void
add_field (EMinicard *e_minicard,
           EContactField field,
           gdouble left_width)
{
	GnomeCanvasItem *new_item;
	GnomeCanvasGroup *group;
	EMinicardField *minicard_field;
	gchar *name;
	gchar *string;
	gboolean is_rtl = (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);

	group = GNOME_CANVAS_GROUP (e_minicard);

	name = g_strdup_printf ("%s:", e_contact_pretty_name (field));
	string = e_contact_get (e_minicard->contact, field);

	new_item = e_minicard_label_new (group);

	if (e_minicard->contact && e_contact_get (e_minicard->contact, E_CONTACT_IS_LIST))
		gnome_canvas_item_set (
			new_item,
			"fieldname", is_rtl ? "" : string,
			"field", is_rtl ? string : "",
			"max_field_name_length", left_width,
			"editable", FALSE /* e_minicard->editable */,
			"width", e_minicard->width - 4.0,
			NULL);
	else
		gnome_canvas_item_set (
			new_item,
			"fieldname", is_rtl ? string : name,
			"field", is_rtl ? name : string,
			"max_field_name_length", left_width,
			"editable", FALSE /* e_minicard->editable */,
			"width", e_minicard->width - 4.0,
			NULL);

#ifdef notyet
	g_object_set (
		E_MINICARD_LABEL (new_item)->field,
		"allow_newlines", e_card_simple_get_allow_newlines (e_minicard->contact, field),
		NULL);
#endif
	g_object_set_data (
		G_OBJECT (E_MINICARD_LABEL (new_item)->field),
		"EMinicard:field",
		GINT_TO_POINTER (field));

	minicard_field = g_new (EMinicardField, 1);
	minicard_field->field = field;
	minicard_field->label = new_item;

	e_minicard->fields = g_list_append (e_minicard->fields, minicard_field);
	e_canvas_item_move_absolute (new_item, 2, e_minicard->height);
	g_free (name);
	g_free (string);
}

static void
add_email_field (EMinicard *e_minicard,
                 GList *email_list,
                 gdouble left_width,
                 gint limit,
                 gboolean is_list)
{
	GnomeCanvasItem *new_item;
	GnomeCanvasGroup *group;
	EMinicardField *minicard_field;
	gchar *name;
	GList *l, *le;
	gint count =0;
	gboolean is_rtl = (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);
	GList *emails = e_contact_get (e_minicard->contact, E_CONTACT_EMAIL);
	group = GNOME_CANVAS_GROUP (e_minicard);

	for (l = email_list, le = emails; l != NULL && count < limit && le != NULL; l = l->next, le = le->next) {
		const gchar *tmp;
		gchar *email = NULL;
		gchar *string = NULL;
		gchar *parsed_name = NULL;
		gboolean parser_check;

		/* do not use name for fields in the contact list */
		if (is_list) {
			name = (gchar *)"";
		} else {
			tmp = eab_get_email_label_text ((EVCardAttribute *) l->data);
			name = g_strdup_printf ("%s:", tmp);
		}

		parser_check = eab_parse_qp_email ((const gchar *) le->data, &parsed_name, &email);
		if (parser_check) {
			/* if true, we had a quoted printable mail address */
			string = g_strdup_printf ("%s <%s>", parsed_name, email);
		} else {
			/* we got a NON-quoted printable string */
			string = g_strdup (le->data);
		}

		new_item = e_minicard_label_new (group);

		gnome_canvas_item_set (
			new_item,
			"fieldname", is_rtl ? string : name,
			"field", is_rtl ? name : string,
			"max_field_name_length", left_width,
			"editable", FALSE /* e_minicard->editable */,
			"width", e_minicard->width - 4.0,
			NULL);

#ifdef notyet
		g_object_set (
			E_MINICARD_LABEL (new_item)->field,
			"allow_newlines", e_card_simple_get_allow_newlines (e_minicard->contact, field),
			NULL);
#endif
		g_object_set_data (
			G_OBJECT (E_MINICARD_LABEL (new_item)->field),
			"EMinicard:field",
			GINT_TO_POINTER (E_CONTACT_EMAIL));

		minicard_field = g_new (EMinicardField, 1);
		minicard_field->field = E_CONTACT_EMAIL;
		minicard_field->label = new_item;

		e_minicard->fields = g_list_append (e_minicard->fields, minicard_field);
		e_canvas_item_move_absolute (new_item, 2, e_minicard->height);
		count++;
		if (!is_list)
			g_free (name);
		g_free (string);
		g_free (parsed_name);
		g_free (email);
	}
	g_list_foreach (emails, (GFunc) g_free, NULL);
	g_list_free (emails);
}

static gint
get_left_width (EMinicard *e_minicard,
                gboolean is_list)
{
	gchar *name;
	EContactField field;
	gint width = -1;
	PangoLayout *layout;

	if (is_list)
		return 0;

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (GNOME_CANVAS_ITEM (e_minicard)->canvas), "");
	for (field = E_CONTACT_FULL_NAME; field != E_CONTACT_LAST_SIMPLE_STRING; field++) {
		gint this_width;

		if (field == E_CONTACT_FAMILY_NAME || field == E_CONTACT_GIVEN_NAME)
			continue;

		name = g_strdup_printf ("%s:", e_contact_pretty_name (field));
		pango_layout_set_text (layout, name, -1);
		pango_layout_get_pixel_size (layout, &this_width, NULL);
		if (width < this_width)
			width = this_width;
		g_free (name);
	}
	g_object_unref (layout);
	return width;
}

static void
remodel (EMinicard *e_minicard)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (e_minicard);
	gint count = 0;

	if (!(item->flags & GNOME_CANVAS_ITEM_REALIZED))
		return;

	if (e_minicard->contact) {
		EContactField field;
		GList *list;
		gchar *file_as;
		gint left_width = -1;
		gboolean is_list = FALSE;
		gboolean email_rendered = FALSE;
		gboolean has_voice = FALSE, has_fax = FALSE;

		if (e_minicard->header_text) {
			file_as = e_contact_get (e_minicard->contact, E_CONTACT_FILE_AS);
			gnome_canvas_item_set (
				e_minicard->header_text,
				"text", file_as ? file_as : "",
				NULL);
			g_free (file_as);
		}

		if (e_minicard->contact && e_contact_get (e_minicard->contact, E_CONTACT_IS_LIST))
			is_list = TRUE;

		if (is_list)
			gnome_canvas_item_show (e_minicard->list_icon);
		else
			gnome_canvas_item_hide (e_minicard->list_icon);

		list = e_minicard->fields;
		e_minicard->fields = NULL;

		for (field = E_CONTACT_FULL_NAME; field != (E_CONTACT_LAST_SIMPLE_STRING -1) && count < 5; field++) {
			EMinicardField *minicard_field = NULL;
			gboolean is_email = FALSE;

			if (field == E_CONTACT_FAMILY_NAME || field == E_CONTACT_GIVEN_NAME ||
			    (has_voice && field == E_CONTACT_PHONE_OTHER) ||
			    (has_fax && field == E_CONTACT_PHONE_OTHER_FAX))
				continue;

			if (field == E_CONTACT_FULL_NAME && is_list)
				continue;

			if (field == E_CONTACT_EMAIL_1 || field == E_CONTACT_EMAIL_2 || field == E_CONTACT_EMAIL_3 || field == E_CONTACT_EMAIL_4) {
				if (email_rendered)
					continue;
				email_rendered = TRUE;
				is_email = TRUE;
			}

			if (list)
				minicard_field = list->data;
			if (minicard_field && minicard_field->field == field) {
				GList *this_list = list;
				gchar *string;

				string = e_contact_get (e_minicard->contact, field);
				if (string && *string) {
					e_minicard->fields = g_list_append (e_minicard->fields, minicard_field);
					g_object_set (
						minicard_field->label,
						"field", string,
						NULL);
					count++;
				} else {
					e_minicard_field_destroy (minicard_field);
				}
				list = g_list_delete_link (list, this_list);
				g_free (string);
			} else {
				gchar *string;
				if (left_width == -1) {
					left_width = get_left_width (e_minicard, is_list);
				}

				if (is_email) {
					GList *email;
					gint limit;

					limit = 5 - count;
					email = e_contact_get_attributes (e_minicard->contact, E_CONTACT_EMAIL);
					add_email_field (e_minicard, email, left_width, limit, is_list);
					if (count + limit >5)
						count = 5;
					else
						count = count + g_list_length (email);
					g_list_free_full (email, (GDestroyNotify) e_vcard_attribute_free);
				} else {
					string = e_contact_get (e_minicard->contact, field);
					if (string && *string) {
						add_field (e_minicard, field, left_width);
						count++;

						has_voice = has_voice ||
							    field == E_CONTACT_PHONE_BUSINESS ||
							    field == E_CONTACT_PHONE_BUSINESS_2 ||
							    field == E_CONTACT_PHONE_HOME ||
							    field == E_CONTACT_PHONE_HOME_2;
						has_fax = has_fax ||
							  field == E_CONTACT_PHONE_BUSINESS_FAX ||
							  field == E_CONTACT_PHONE_HOME_FAX;
					}
					g_free (string);
				}
			}
		}

		g_list_foreach (list, (GFunc) e_minicard_field_destroy, NULL);
		g_list_free (list);
	}
}

static void
e_minicard_reflow (GnomeCanvasItem *item,
                   gint flags)
{
	EMinicard *e_minicard = E_MINICARD (item);

	if (item->flags & GNOME_CANVAS_ITEM_REALIZED) {
		GList *list;
		gdouble text_height;
		gint old_height;

		old_height = e_minicard->height;

		g_object_get (
			e_minicard->header_text,
			"text_height", &text_height,
			NULL);

		e_minicard->height = text_height + 10.0;

		gnome_canvas_item_set (
			e_minicard->header_rect,
			"y2", text_height + 9.0,
			NULL);

		for (list = e_minicard->fields; list; list = g_list_next (list)) {
			EMinicardField *field = E_MINICARD_FIELD (list->data);
			/* Why not use the item that is passed in? */
			GnomeCanvasItem *item = field->label;
			g_object_get (
				item,
				"height", &text_height,
				NULL);
			e_canvas_item_move_absolute (item, 2, e_minicard->height);
			e_minicard->height += text_height;
		}
		e_minicard->height += 2;

		gnome_canvas_item_set (
			e_minicard->rect,
			"x2", (gdouble) e_minicard->width - 1.0,
			"y2", (gdouble) e_minicard->height - 1.0,
			NULL);
		gnome_canvas_item_set (
			e_minicard->header_rect,
			"x2", (gdouble) e_minicard->width - 3.0,
			NULL);

		if (old_height != e_minicard->height)
			e_canvas_item_request_parent_reflow (item);
	}
}

const gchar *
e_minicard_get_card_id (EMinicard *minicard)
{
	g_return_val_if_fail (minicard != NULL, NULL);
	g_return_val_if_fail (E_IS_MINICARD (minicard), NULL);

	if (minicard->contact) {
		return e_contact_get_const (minicard->contact, E_CONTACT_UID);
	} else {
		return "";
	}
}

gint
e_minicard_compare (EMinicard *minicard1,
                    EMinicard *minicard2)
{
	gint cmp = 0;

	g_return_val_if_fail (minicard1 != NULL, 0);
	g_return_val_if_fail (E_IS_MINICARD (minicard1), 0);
	g_return_val_if_fail (minicard2 != NULL, 0);
	g_return_val_if_fail (E_IS_MINICARD (minicard2), 0);

	if (minicard1->contact && minicard2->contact) {
		gchar *file_as1, *file_as2;
		g_object_get (
			minicard1->contact,
			"file_as", &file_as1,
			NULL);
		g_object_get (
			minicard2->contact,
			"file_as", &file_as2,
			NULL);

		if (file_as1 && file_as2)
			cmp = g_utf8_collate (file_as1, file_as2);
		else if (file_as1)
			cmp = -1;
		else if (file_as2)
			cmp = 1;
		else
			cmp = strcmp (e_minicard_get_card_id (minicard1), e_minicard_get_card_id (minicard2));

		g_free (file_as1);
		g_free (file_as2);
	}

	return cmp;
}

gint
e_minicard_selected (EMinicard *minicard,
                     GdkEvent *event)
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
			g_signal_emit (
				item->parent,
				signal_id, 0,
				item, event, &ret_val);
		}
	}
	return ret_val;
}

static gint
e_minicard_drag_begin (EMinicard *minicard,
                       GdkEvent *event)
{
	gint ret_val = 0;
	GnomeCanvasItem *parent;
	g_signal_emit (
		minicard,
		signals[DRAG_BEGIN], 0,
		event, &ret_val);

	parent = GNOME_CANVAS_ITEM (minicard)->parent;
	if (parent && E_IS_REFLOW (parent)) {
		E_REFLOW (parent)->maybe_in_drag = FALSE;
	}
	return ret_val;
}
