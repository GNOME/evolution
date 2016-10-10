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

#include "e-minicard-label.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <libgnomecanvas/libgnomecanvas.h>

#include "e-util/e-util.h"

static void e_minicard_label_set_property  (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void e_minicard_label_get_property  (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static gboolean e_minicard_label_event (GnomeCanvasItem *item, GdkEvent *event);
static void e_minicard_label_realize (GnomeCanvasItem *item);
static void e_minicard_label_reflow (GnomeCanvasItem *item, gint flags);
static void e_minicard_label_style_updated (EMinicardLabel *label);

static void e_minicard_label_resize_children (EMinicardLabel *e_minicard_label);

static void set_colors (EMinicardLabel *label);

enum {
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_HAS_FOCUS,
	PROP_FIELD,
	PROP_FIELDNAME,
	PROP_TEXT_MODEL,
	PROP_MAX_FIELD_NAME_WIDTH,
	PROP_EDITABLE
};

enum {
	STYLE_UPDATED,
	LAST_SIGNAL
};

static guint e_minicard_label_signals[LAST_SIGNAL] = {0, };

G_DEFINE_TYPE (
	EMinicardLabel,
	e_minicard_label,
	GNOME_TYPE_CANVAS_GROUP)

static void
e_minicard_label_class_init (EMinicardLabelClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	item_class = (GnomeCanvasItemClass *) class;

	class->style_updated = e_minicard_label_style_updated;

	object_class->set_property = e_minicard_label_set_property;
	object_class->get_property = e_minicard_label_get_property;

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
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HAS_FOCUS,
		g_param_spec_boolean (
			"has_focus",
			"Has Focus",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FIELD,
		g_param_spec_string (
			"field",
			"Field",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FIELDNAME,
		g_param_spec_string (
			"fieldname",
			"Field Name",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TEXT_MODEL,
		g_param_spec_object (
			"text_model",
			"Text Model",
			NULL,
			E_TYPE_TEXT_MODEL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MAX_FIELD_NAME_WIDTH,
		g_param_spec_double (
			"max_field_name_length",
			"Max field name length",
			NULL,
			-1.0, G_MAXDOUBLE, -1.0,
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

	e_minicard_label_signals[STYLE_UPDATED] = g_signal_new (
		"style_updated",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMinicardLabelClass, style_updated),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/* GnomeCanvasItem method overrides */
	item_class->realize = e_minicard_label_realize;
	item_class->event = e_minicard_label_event;
}

static void
e_minicard_label_init (EMinicardLabel *minicard_label)
{
	minicard_label->width = 10;
	minicard_label->height = 10;
	minicard_label->rect = NULL;
	minicard_label->fieldname = NULL;
	minicard_label->field = NULL;

	minicard_label->max_field_name_length = -1;

	e_canvas_item_set_reflow_callback (
		GNOME_CANVAS_ITEM (minicard_label),
		e_minicard_label_reflow);
}

static void
e_minicard_label_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	EMinicardLabel *e_minicard_label;
	GnomeCanvasItem *item;

	e_minicard_label = E_MINICARD_LABEL (object);
	item = GNOME_CANVAS_ITEM (object);

	switch (property_id) {
	case PROP_WIDTH:
		e_minicard_label->width = g_value_get_double (value);
		e_minicard_label_resize_children (e_minicard_label);
		e_canvas_item_request_reflow (item);
		break;
	case PROP_HAS_FOCUS:
		if (e_minicard_label->field && (g_value_get_boolean (value) != E_FOCUS_NONE))
			e_canvas_item_grab_focus (e_minicard_label->field, FALSE);
		break;
	case PROP_FIELD:
		gnome_canvas_item_set (e_minicard_label->field, "text", g_value_get_string (value), NULL);
		break;
	case PROP_FIELDNAME:
		gnome_canvas_item_set (e_minicard_label->fieldname, "text", g_value_get_string (value), NULL);
		break;
	case PROP_TEXT_MODEL:
		gnome_canvas_item_set (e_minicard_label->field, "model", g_value_get_object (value), NULL);
		break;
	case PROP_MAX_FIELD_NAME_WIDTH:
		e_minicard_label->max_field_name_length = g_value_get_double (value);
		break;
	case PROP_EDITABLE:
		e_minicard_label->editable = g_value_get_boolean (value);
		g_object_set (e_minicard_label->field, "editable", FALSE /* e_minicard_label->editable */, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_minicard_label_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	EMinicardLabel *e_minicard_label;

	e_minicard_label = E_MINICARD_LABEL (object);

	switch (property_id) {
	case PROP_WIDTH:
		g_value_set_double (value, e_minicard_label->width);
		break;
	case PROP_HEIGHT:
		g_value_set_double (value, e_minicard_label->height);
		break;
	case PROP_HAS_FOCUS:
		g_value_set_boolean (value, e_minicard_label->has_focus ? E_FOCUS_CURRENT : E_FOCUS_NONE);
		break;
	case PROP_FIELD:
		g_object_get_property (
			G_OBJECT (e_minicard_label->field),
			"text", value);
		break;
	case PROP_FIELDNAME:
		g_object_get_property (
			G_OBJECT (e_minicard_label->fieldname),
			"text", value);
		break;
	case PROP_TEXT_MODEL:
		g_object_get_property (
			G_OBJECT (e_minicard_label->field),
			"model", value);
		break;
	case PROP_MAX_FIELD_NAME_WIDTH:
		g_value_set_double (value, e_minicard_label->max_field_name_length);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, e_minicard_label->editable);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_minicard_label_realize (GnomeCanvasItem *item)
{
	EMinicardLabel *e_minicard_label;
	GnomeCanvasGroup *group;

	e_minicard_label = E_MINICARD_LABEL (item);
	group = GNOME_CANVAS_GROUP (item);

	GNOME_CANVAS_ITEM_CLASS (e_minicard_label_parent_class)->realize (item);

	e_canvas_item_request_reflow (item);

	e_minicard_label->rect = gnome_canvas_item_new (
		group,
		gnome_canvas_rect_get_type (),
		"x1", (gdouble) 0,
		"y1", (gdouble) 0,
		"x2", (gdouble) e_minicard_label->width - 1,
		"y2", (gdouble) e_minicard_label->height - 1,
		"outline_color", NULL,
		NULL);

	e_minicard_label->fieldname = gnome_canvas_item_new (
		group,
		e_text_get_type (),
		"clip_width", (gdouble) (e_minicard_label->width / 2 - 4),
		"clip", TRUE,
		"use_ellipsis", TRUE,
		"fill_color", "black",
		"im_context", E_CANVAS (item->canvas)->im_context,
		NULL);

	e_canvas_item_move_absolute (e_minicard_label->fieldname, 2, 1);

	e_minicard_label->field = gnome_canvas_item_new (
		group,
		e_text_get_type (),
		"clip_width", (gdouble) ((e_minicard_label->width + 1) / 2 - 4),
		"clip", TRUE,
		"use_ellipsis", TRUE,
		"fill_color", "black",
		"editable", FALSE, /* e_minicard_label->editable, */
		"im_context", E_CANVAS (item->canvas)->im_context,
		NULL);

	e_canvas_item_move_absolute (
		e_minicard_label->field,
		(e_minicard_label->width / 2 + 2), 1);

	set_colors (e_minicard_label);

	e_canvas_item_request_reflow (item);
}

static gboolean
e_minicard_label_event (GnomeCanvasItem *item,
                        GdkEvent *event)
{
	EMinicardLabel *e_minicard_label;

	e_minicard_label = E_MINICARD_LABEL (item);

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_KEY_Escape) {
			GnomeCanvasItem *parent;

			e_text_cancel_editing (E_TEXT (e_minicard_label->field));

			parent = GNOME_CANVAS_ITEM (e_minicard_label)->parent;
			if (parent)
				e_canvas_item_grab_focus (parent, FALSE);
		}
		break;
	case GDK_FOCUS_CHANGE: {
		GdkEventFocus *focus_event = (GdkEventFocus *) event;

		e_minicard_label->has_focus = focus_event->in;
		set_colors (e_minicard_label);

		g_object_set (
			e_minicard_label->field,
			"handle_popup", e_minicard_label->has_focus,
			NULL);
		break;
	}
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	case GDK_MOTION_NOTIFY:
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY: {
		gboolean return_val;
		g_signal_emit_by_name (e_minicard_label->field, "event", event, &return_val);
		return return_val;
	}
	default:
		break;
	}

	return GNOME_CANVAS_ITEM_CLASS (e_minicard_label_parent_class)->
		event (item, event);
}

static void
e_minicard_label_resize_children (EMinicardLabel *e_minicard_label)
{
	gdouble left_width;
	gdouble fieldnamewidth;
	gdouble fieldwidth;
	gboolean is_rtl = (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);
	if (e_minicard_label->max_field_name_length != -1 && ((e_minicard_label->width / 2) - 4 > e_minicard_label->max_field_name_length))
		left_width = e_minicard_label->max_field_name_length;
	else
		left_width = e_minicard_label->width / 2 - 4;

	fieldnamewidth = (gdouble) MAX (left_width, 0);
	fieldwidth = (gdouble) MAX (e_minicard_label->width - 8 - left_width, 0);
	gnome_canvas_item_set (
		e_minicard_label->fieldname,
		"clip_width", is_rtl ? fieldwidth : fieldnamewidth,
		NULL);
	gnome_canvas_item_set (
		e_minicard_label->field,
		"clip_width", is_rtl ? fieldnamewidth : fieldwidth,
		NULL);
}

static void
set_colors (EMinicardLabel *label)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (label);

	if ((item->flags & GNOME_CANVAS_ITEM_REALIZED)) {
		GdkColor text;
		GtkWidget *widget;

		widget = GTK_WIDGET (GNOME_CANVAS_ITEM (label)->canvas);

		e_utils_get_theme_color_color (widget, "theme_text_color,theme_fg_color", E_UTILS_DEFAULT_THEME_TEXT_COLOR, &text);

		if (label->has_focus) {
			GdkColor outline, fill;

			e_utils_get_theme_color_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &outline);
			e_utils_get_theme_color_color (widget, "theme_bg_color", E_UTILS_DEFAULT_THEME_BG_COLOR, &fill);

			gnome_canvas_item_set (
				label->rect,
				"outline_color_gdk", &outline,
				"fill_color_gdk", &fill,
				NULL);

			gnome_canvas_item_set (
				label->field,
				"fill_color_gdk", &text,
				NULL);

			gnome_canvas_item_set (
				label->fieldname,
				"fill_color_gdk", &text,
				NULL);
		}
		else {
			gnome_canvas_item_set (
				label->rect,
				"outline_color_gdk", NULL,
				"fill_color_gdk", NULL,
				NULL);

			gnome_canvas_item_set (
				label->field,
				"fill_color_gdk", &text,
				NULL);

			gnome_canvas_item_set (
				label->fieldname,
				"fill_color_gdk", &text,
				NULL);
		}
	}
}

static void
e_minicard_label_style_updated (EMinicardLabel *label)
{
	set_colors (label);
}

static void
e_minicard_label_reflow (GnomeCanvasItem *item,
                         gint flags)
{
	EMinicardLabel *e_minicard_label = E_MINICARD_LABEL (item);

	gint old_height;
	gdouble text_height;
	gdouble left_width;

	old_height = e_minicard_label->height;

	g_object_get (
		e_minicard_label->fieldname,
		"text_height", &text_height,
		NULL);

	e_minicard_label->height = text_height;

	g_object_get (
		e_minicard_label->field,
		"text_height", &text_height,
		NULL);

	if (e_minicard_label->height < text_height)
		e_minicard_label->height = text_height;
	e_minicard_label->height += 3;

	gnome_canvas_item_set (
		e_minicard_label->rect,
		"x2", (gdouble) e_minicard_label->width - 1,
		"y2", (gdouble) e_minicard_label->height - 1,
		NULL);

	gnome_canvas_item_set (
		e_minicard_label->fieldname,
		"clip_height", (gdouble) e_minicard_label->height - 3,
		NULL);

	if (e_minicard_label->max_field_name_length != -1 && ((e_minicard_label->width / 2) - 4 > e_minicard_label->max_field_name_length))
		left_width = e_minicard_label->max_field_name_length;
	else
		left_width = e_minicard_label->width / 2 - 4;

	e_canvas_item_move_absolute (e_minicard_label->field, left_width + 6, 1);

	if (old_height != e_minicard_label->height)
		e_canvas_item_request_parent_reflow (item);
}

GnomeCanvasItem *
e_minicard_label_new (GnomeCanvasGroup *parent)
{
	return gnome_canvas_item_new (
		parent, e_minicard_label_get_type (), NULL);
}

