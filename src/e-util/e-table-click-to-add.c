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

#include "e-table-click-to-add.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "e-canvas-utils.h"
#include "e-canvas.h"
#include "e-marshal.h"
#include "e-table-defines.h"
#include "e-table-header.h"
#include "e-table-one.h"
#include "e-text.h"
#include "gal-a11y-e-table-click-to-add.h"

enum {
	CURSOR_CHANGE,
	STYLE_UPDATED,
	LAST_SIGNAL
};

static guint etcta_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (
	ETableClickToAdd,
	e_table_click_to_add,
	GNOME_TYPE_CANVAS_GROUP)

enum {
	PROP_0,
	PROP_HEADER,
	PROP_MODEL,
	PROP_MESSAGE,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_IS_EDITING
};

static void
etcta_cursor_change (GObject *object,
                     gint row,
                     gint col,
                     ETableClickToAdd *etcta)
{
	g_signal_emit (
		etcta,
		etcta_signals[CURSOR_CHANGE], 0,
		row, col);
}

static void
etcta_style_updated (ETableClickToAdd *etcta)
{
	GtkWidget *widget;
	GdkRGBA fg, bg;

	widget = GTK_WIDGET (GNOME_CANVAS_ITEM (etcta)->canvas);

	e_utils_get_theme_color (widget, "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &fg);
	e_utils_get_theme_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &bg);

	if (etcta->rect)
		gnome_canvas_item_set (
			etcta->rect,
			"fill-color", &bg,
			NULL);

	if (etcta->text)
		gnome_canvas_item_set (
			etcta->text,
			"fill-color", &fg,
			NULL);
}

static void
etcta_add_table_header (ETableClickToAdd *etcta,
                        ETableHeader *header)
{
	etcta->eth = header;
	if (etcta->eth)
		g_object_ref (etcta->eth);
	if (etcta->row)
		gnome_canvas_item_set (
			GNOME_CANVAS_ITEM (etcta->row),
			"ETableHeader", header,
			NULL);
}

static void
etcta_drop_table_header (ETableClickToAdd *etcta)
{
	if (!etcta->eth)
		return;

	g_object_unref (etcta->eth);
	etcta->eth = NULL;
}

static void
etcta_add_one (ETableClickToAdd *etcta,
               ETableModel *one)
{
	etcta->one = one;
	if (etcta->one)
		g_object_ref (etcta->one);
	if (etcta->row)
		gnome_canvas_item_set (
			GNOME_CANVAS_ITEM (etcta->row),
			"ETableModel", one,
			NULL);
	g_object_set (
		etcta->selection,
		"model", one,
		NULL);
}

static void
etcta_drop_one (ETableClickToAdd *etcta)
{
	if (!etcta->one)
		return;
	g_object_unref (etcta->one);
	etcta->one = NULL;
	g_object_set (
		etcta->selection,
		"model", NULL,
		NULL);
}

static void
etcta_add_model (ETableClickToAdd *etcta,
                 ETableModel *model)
{
	etcta->model = model;
	if (etcta->model)
		g_object_ref (etcta->model);
}

static void
etcta_drop_model (ETableClickToAdd *etcta)
{
	etcta_drop_one (etcta);
	if (!etcta->model)
		return;
	g_object_unref (etcta->model);
	etcta->model = NULL;
}

static void
etcta_add_message (ETableClickToAdd *etcta,
                   const gchar *message)
{
	etcta->message = g_strdup (message);
}

static void
etcta_drop_message (ETableClickToAdd *etcta)
{
	g_free (etcta->message);
	etcta->message = NULL;
}

static void
etcta_dispose (GObject *object)
{
	ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (object);

	etcta_drop_table_header (etcta);
	etcta_drop_model (etcta);
	etcta_drop_message (etcta);
	g_clear_object (&etcta->selection);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_click_to_add_parent_class)->dispose (object);
}

static void
etcta_set_property (GObject *object,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	ETableClickToAdd *etcta;

	item = GNOME_CANVAS_ITEM (object);
	etcta = E_TABLE_CLICK_TO_ADD (object);

	switch (property_id) {
	case PROP_HEADER:
		etcta_drop_table_header (etcta);
		etcta_add_table_header (etcta, E_TABLE_HEADER (g_value_get_object (value)));
		break;
	case PROP_MODEL:
		etcta_drop_model (etcta);
		etcta_add_model (etcta, E_TABLE_MODEL (g_value_get_object (value)));
		break;
	case PROP_MESSAGE:
		etcta_drop_message (etcta);
		etcta_add_message (etcta, g_value_get_string (value));
		break;
	case PROP_WIDTH:
		etcta->width = g_value_get_double (value);
		if (etcta->row)
			gnome_canvas_item_set (
				etcta->row,
				"minimum_width", etcta->width,
				NULL);
		if (etcta->text)
			gnome_canvas_item_set (
				etcta->text,
				"width", (etcta->width < 4 ? 4 : etcta->width) - 4,
				NULL);
		if (etcta->rect)
			gnome_canvas_item_set (
				etcta->rect,
				"x2", etcta->width,
				NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		return;

	}
	gnome_canvas_item_request_update (item);
}

static void
create_rect_and_text (ETableClickToAdd *etcta)
{
	GtkWidget *widget;
	GdkRGBA fg, bg;

	widget = GTK_WIDGET (GNOME_CANVAS_ITEM (etcta)->canvas);

	e_utils_get_theme_color (widget, "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &fg);
	e_utils_get_theme_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &bg);

	if (!etcta->rect)
		etcta->rect = gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (etcta),
			gnome_canvas_rect_get_type (),
			"x1", (gdouble) 0,
			"y1", (gdouble) 1,
			"x2", (gdouble) etcta->width,
			"y2", (gdouble) etcta->height,
			"fill-color", &bg,
			NULL);

	if (!etcta->text)
		etcta->text = gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (etcta),
			e_text_get_type (),
			"text", etcta->message ? etcta->message : "",
			"width", etcta->width - 4,
			"fill-color", &fg,
			NULL);
}

static void
etcta_get_property (GObject *object,
                    guint property_id,
                    GValue *value,
                    GParamSpec *pspec)
{
	ETableClickToAdd *etcta;

	etcta = E_TABLE_CLICK_TO_ADD (object);

	switch (property_id) {
	case PROP_HEADER:
		g_value_set_object (value, etcta->eth);
		break;
	case PROP_MODEL:
		g_value_set_object (value, etcta->model);
		break;
	case PROP_MESSAGE:
		g_value_set_string (value, etcta->message);
		break;
	case PROP_WIDTH:
		g_value_set_double (value, etcta->width);
		break;
	case PROP_HEIGHT:
		g_value_set_double (value, etcta->height);
		break;
	case PROP_IS_EDITING:
		g_value_set_boolean (value, e_table_click_to_add_is_editing (etcta));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
etcta_realize (GnomeCanvasItem *item)
{
	ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (item);

	create_rect_and_text (etcta);
	e_canvas_item_move_absolute (etcta->text, 2, 2);

	if (GNOME_CANVAS_ITEM_CLASS (e_table_click_to_add_parent_class)->realize)
		(*GNOME_CANVAS_ITEM_CLASS (e_table_click_to_add_parent_class)->realize)(item);

	e_canvas_item_request_reflow (item);
}

static void
etcta_unrealize (GnomeCanvasItem *item)
{
	if (GNOME_CANVAS_ITEM_CLASS (e_table_click_to_add_parent_class)->unrealize)
		(*GNOME_CANVAS_ITEM_CLASS (e_table_click_to_add_parent_class)->unrealize)(item);
}

static void finish_editing (ETableClickToAdd *etcta);

static gint
item_key_press (ETableItem *item,
                gint row,
                gint col,
                GdkEvent *event,
                ETableClickToAdd *etcta)
{
	switch (event->key.keyval) {
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_ISO_Enter:
		case GDK_KEY_3270_Enter:
			finish_editing (etcta);
			return TRUE;
	}
	return FALSE;
}

static void
set_initial_selection (ETableClickToAdd *etcta)
{
	e_selection_model_do_something (
		E_SELECTION_MODEL (etcta->selection),
		0, e_table_header_prioritized_column (etcta->eth),
		0);
}

static void
table_click_to_add_row_is_editing_changed_cb (ETableItem *item,
                                              GParamSpec *param,
                                              ETableClickToAdd *etcta)
{
	g_return_if_fail (E_IS_TABLE_CLICK_TO_ADD (etcta));

	g_object_notify (G_OBJECT (etcta), "is-editing");
}

static void
finish_editing (ETableClickToAdd *etcta)
{
	if (etcta->row) {
		ETableModel *one;

		e_table_item_leave_edit (E_TABLE_ITEM (etcta->row));
		e_table_one_commit (E_TABLE_ONE (etcta->one));
		etcta_drop_one (etcta);
		g_object_run_dispose (G_OBJECT (etcta->row));
		etcta->row = NULL;

		if (etcta->text) {
			g_object_run_dispose (G_OBJECT (etcta->text));
			etcta->text = NULL;
		}
		if (etcta->rect) {
			g_object_run_dispose (G_OBJECT (etcta->rect));
			etcta->rect = NULL;
		}

		one = e_table_one_new (etcta->model);
		etcta_add_one (etcta, one);
		g_object_unref (one);

		e_selection_model_clear (E_SELECTION_MODEL (etcta->selection));

		etcta->row = gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (etcta),
			e_table_item_get_type (),
			"ETableHeader", etcta->eth,
			"ETableModel", etcta->one,
			"minimum_width", etcta->width,
			"horizontal_draw_grid", TRUE,
			"vertical_draw_grid", TRUE,
			"selection_model", etcta->selection,
			"cursor_mode", E_CURSOR_SPREADSHEET,
			NULL);

		g_signal_connect (
			etcta->row, "key_press",
			G_CALLBACK (item_key_press), etcta);

		e_signal_connect_notify (
			etcta->row, "notify::is-editing",
			G_CALLBACK (table_click_to_add_row_is_editing_changed_cb), etcta);

		set_initial_selection (etcta);

		g_object_notify (G_OBJECT (etcta), "is-editing");
	}
}

/* Handles the events on the ETableClickToAdd, particularly
 * it creates the ETableItem and passes in some events. */
static gint
etcta_event (GnomeCanvasItem *item,
             GdkEvent *e)
{
	ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (item);

	switch (e->type) {
	case GDK_FOCUS_CHANGE:
		if (!e->focus_change.in)
			return TRUE;
		/* coverity[fallthrough] */
		/* falls through */

	case GDK_BUTTON_PRESS:
		if (etcta->text) {
			g_object_run_dispose (G_OBJECT (etcta->text));
			etcta->text = NULL;
		}
		if (etcta->rect) {
			g_object_run_dispose (G_OBJECT (etcta->rect));
			etcta->rect = NULL;
		}
		if (!etcta->row) {
			ETableModel *one;

			one = e_table_one_new (etcta->model);
			etcta_add_one (etcta, one);
			g_object_unref (one);

			e_selection_model_clear (E_SELECTION_MODEL (etcta->selection));

			etcta->row = gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (item),
				e_table_item_get_type (),
				"ETableHeader", etcta->eth,
				"ETableModel", etcta->one,
				"minimum_width", etcta->width,
				"horizontal_draw_grid", TRUE,
				"vertical_draw_grid", TRUE,
				"selection_model", etcta->selection,
				"cursor_mode", E_CURSOR_SPREADSHEET,
				NULL);

			g_signal_connect (
				etcta->row, "key_press",
				G_CALLBACK (item_key_press), etcta);

			e_signal_connect_notify (
				etcta->row, "notify::is-editing",
				G_CALLBACK (table_click_to_add_row_is_editing_changed_cb), etcta);

			e_canvas_item_grab_focus (GNOME_CANVAS_ITEM (etcta->row), TRUE);

			set_initial_selection (etcta);

			g_object_notify (G_OBJECT (etcta), "is-editing");
		}
		break;

	case GDK_KEY_PRESS:
		switch (e->key.keyval) {
		case GDK_KEY_Tab:
		case GDK_KEY_KP_Tab:
		case GDK_KEY_ISO_Left_Tab:
			finish_editing (etcta);
			break;
		default:
			return FALSE;
		case GDK_KEY_Escape:
			if (etcta->row) {
				e_table_item_leave_edit (E_TABLE_ITEM (etcta->row));
				etcta_drop_one (etcta);
				g_object_run_dispose (G_OBJECT (etcta->row));
				etcta->row = NULL;
				create_rect_and_text (etcta);
				e_canvas_item_move_absolute (etcta->text, 3, 3);
			}
			break;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

static void
etcta_reflow (GnomeCanvasItem *item,
              gint flags)
{
	ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (item);

	gdouble old_height = etcta->height;

	if (etcta->text) {
		g_object_get (
			etcta->text,
			"height", &etcta->height,
			NULL);
		etcta->height += 6;
	}
	if (etcta->row) {
		g_object_get (
			etcta->row,
			"height", &etcta->height,
			NULL);
	}

	if (etcta->rect) {
		g_object_set (
			etcta->rect,
			"y2", etcta->height - 1,
			NULL);
	}

	if (old_height != etcta->height)
		e_canvas_item_request_parent_reflow (item);
}

static void
e_table_click_to_add_class_init (ETableClickToAddClass *class)
{
	GnomeCanvasItemClass *item_class = GNOME_CANVAS_ITEM_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	class->cursor_change = NULL;
	class->style_updated = etcta_style_updated;

	object_class->dispose = etcta_dispose;
	object_class->set_property = etcta_set_property;
	object_class->get_property = etcta_get_property;

	item_class->realize = etcta_realize;
	item_class->unrealize = etcta_unrealize;
	item_class->event = etcta_event;

	g_object_class_install_property (
		object_class,
		PROP_HEADER,
		g_param_spec_object (
			"header",
			"Header",
			NULL,
			E_TYPE_TABLE_HEADER,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			"Model",
			NULL,
			E_TYPE_TABLE_MODEL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MESSAGE,
		g_param_spec_string (
			"message",
			"Message",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WIDTH,
		g_param_spec_double (
			"width",
			"Width",
			NULL,
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READWRITE |
			G_PARAM_LAX_VALIDATION));

	g_object_class_install_property (
		object_class,
		PROP_HEIGHT,
		g_param_spec_double (
			"height",
			"Height",
			NULL,
			0.0, G_MAXDOUBLE, 0.0,
			G_PARAM_READABLE |
			G_PARAM_LAX_VALIDATION));

	g_object_class_install_property (
		object_class,
		PROP_IS_EDITING,
		g_param_spec_boolean (
			"is-editing",
			"Whether is in an editing mode",
			"Whether is in an editing mode",
			FALSE,
			G_PARAM_READABLE));

	etcta_signals[CURSOR_CHANGE] = g_signal_new (
		"cursor_change",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClickToAddClass, cursor_change),
		NULL, NULL,
		e_marshal_VOID__INT_INT,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_INT);

	etcta_signals[STYLE_UPDATED] = g_signal_new (
		"style_updated",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableClickToAddClass, style_updated),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	gal_a11y_e_table_click_to_add_init ();
}

static void
e_table_click_to_add_init (ETableClickToAdd *etcta)
{
	AtkObject *a11y;

	etcta->one = NULL;
	etcta->model = NULL;
	etcta->eth = NULL;

	etcta->message = NULL;

	etcta->row = NULL;
	etcta->text = NULL;
	etcta->rect = NULL;

	/* Pick some arbitrary defaults. */
	etcta->width = 12;
	etcta->height = 6;

	etcta->selection = e_table_selection_model_new ();
	g_signal_connect (
		etcta->selection, "cursor_changed",
		G_CALLBACK (etcta_cursor_change), etcta);

	e_canvas_item_set_reflow_callback (GNOME_CANVAS_ITEM (etcta), etcta_reflow);

	/* create its a11y object at this time if accessibility is enabled*/
	if (atk_get_root () != NULL) {
		a11y = atk_gobject_accessible_for_object (G_OBJECT (etcta));
		atk_object_set_name (a11y, _("click to add"));
	}
}

/* The colors in this need to be themefied. */
/**
 * e_table_click_to_add_commit:
 * @etcta: The %ETableClickToAdd to commit.
 *
 * This routine commits the current thing being edited and returns to
 * just displaying the click to add message.
 **/
void
e_table_click_to_add_commit (ETableClickToAdd *etcta)
{
	if (etcta->row) {
		e_table_one_commit (E_TABLE_ONE (etcta->one));
		etcta_drop_one (etcta);
		g_object_run_dispose (G_OBJECT (etcta->row));
		etcta->row = NULL;
	}
	create_rect_and_text (etcta);
	e_canvas_item_move_absolute (etcta->text, 3, 3);
}

gboolean
e_table_click_to_add_is_editing (ETableClickToAdd *etcta)
{
	g_return_val_if_fail (E_IS_TABLE_CLICK_TO_ADD (etcta), FALSE);

	return etcta->row && e_table_item_is_editing (E_TABLE_ITEM (etcta->row));
}
