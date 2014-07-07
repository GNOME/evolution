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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include "e-minicard-view-widget.h"

static void	e_minicard_view_widget_set_property
						(GObject *object,
						 guint property_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void	e_minicard_view_widget_get_property
						(GObject *object,
						 guint property_id,
						 GValue *value,
						 GParamSpec *pspec);
static void	e_minicard_view_widget_dispose	(GObject *object);
static void	e_minicard_view_widget_reflow	(ECanvas *canvas);
static void	e_minicard_view_widget_size_allocate
						(GtkWidget *widget,
						 GtkAllocation *allocation);
static void	e_minicard_view_widget_style_updated
						(GtkWidget *widget);
static void	e_minicard_view_widget_realize	(GtkWidget *widget);
static gboolean	e_minicard_view_widget_real_focus_in_event
						(GtkWidget *widget,
						 GdkEventFocus *event);

enum {
	PROP_0,
	PROP_CLIENT,
	PROP_QUERY,
	PROP_EDITABLE,
	PROP_COLUMN_WIDTH
};

enum {
	CREATE_CONTACT,
	CREATE_CONTACT_LIST,
	SELECTION_CHANGE,
	COLUMN_WIDTH_CHANGED,
	RIGHT_CLICK,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0, };

G_DEFINE_TYPE (
	EMinicardViewWidget,
	e_minicard_view_widget,
	E_TYPE_CANVAS)

static void
e_minicard_view_widget_class_init (EMinicardViewWidgetClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ECanvasClass *canvas_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = e_minicard_view_widget_set_property;
	object_class->get_property = e_minicard_view_widget_get_property;
	object_class->dispose = e_minicard_view_widget_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->style_updated = e_minicard_view_widget_style_updated;
	widget_class->realize = e_minicard_view_widget_realize;
	widget_class->size_allocate = e_minicard_view_widget_size_allocate;
	widget_class->focus_in_event = e_minicard_view_widget_real_focus_in_event;

	canvas_class = E_CANVAS_CLASS (class);
	canvas_class->reflow = e_minicard_view_widget_reflow;

	class->selection_change = NULL;
	class->column_width_changed = NULL;
	class->right_click = NULL;

	g_object_class_install_property (
		object_class,
		PROP_CLIENT,
		g_param_spec_object (
			"client",
			"EBookClient",
			NULL,
			E_TYPE_BOOK_CLIENT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_QUERY,
		g_param_spec_string (
			"query",
			"Query",
			NULL,
			NULL,
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
		PROP_COLUMN_WIDTH,
		g_param_spec_double (
			"column_width",
			"Column Width",
			NULL,
			0.0, G_MAXDOUBLE, 225.0,
			G_PARAM_READWRITE));

	signals[CREATE_CONTACT] = g_signal_new (
		"create-contact",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMinicardViewWidgetClass, create_contact),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CREATE_CONTACT_LIST] = g_signal_new (
		"create-contact-list",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMinicardViewWidgetClass, create_contact_list),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SELECTION_CHANGE] = g_signal_new (
		"selection_change",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMinicardViewWidgetClass, selection_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[COLUMN_WIDTH_CHANGED] = g_signal_new (
		"column_width_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMinicardViewWidgetClass, column_width_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__DOUBLE,
		G_TYPE_NONE, 1,
		G_TYPE_DOUBLE);

	signals[RIGHT_CLICK] = g_signal_new (
		"right_click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMinicardViewWidgetClass, right_click),
		NULL, NULL,
		e_marshal_INT__POINTER,
		G_TYPE_INT, 1,
		G_TYPE_POINTER);
}

static void
e_minicard_view_widget_init (EMinicardViewWidget *view)
{
	view->emv = NULL;

	view->book_client = NULL;
	view->query = NULL;
	view->editable = FALSE;
	view->column_width = 225;
}

GtkWidget *
e_minicard_view_widget_new (EAddressbookReflowAdapter *adapter)
{
	EMinicardViewWidget *widget;

	widget = g_object_new (E_TYPE_MINICARD_VIEW_WIDGET, NULL);

	widget->adapter = g_object_ref (adapter);

	return GTK_WIDGET (widget);
}

static void
e_minicard_view_widget_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
	EMinicardViewWidget *emvw;

	emvw = E_MINICARD_VIEW_WIDGET (object);

	switch (property_id) {
	case PROP_CLIENT:
		if (emvw->book_client)
			g_object_unref (emvw->book_client);
		if (g_value_get_object (value)) {
			emvw->book_client = E_BOOK_CLIENT (g_value_get_object (value));
			if (emvw->book_client)
				g_object_ref (emvw->book_client);
		} else
			emvw->book_client = NULL;
		if (emvw->emv)
			g_object_set (
				emvw->emv,
				"client", emvw->book_client,
				NULL);
		break;
	case PROP_QUERY:
		emvw->query = g_strdup (g_value_get_string (value));
		if (emvw->emv)
			g_object_set (
				emvw->emv,
				"query", emvw->query,
				NULL);
		break;
	case PROP_EDITABLE:
		emvw->editable = g_value_get_boolean (value);
		if (emvw->emv)
			g_object_set (
				emvw->emv,
				"editable", emvw->editable,
				NULL);
		break;
	case PROP_COLUMN_WIDTH:
		emvw->column_width = g_value_get_double (value);
		if (emvw->emv) {
			g_object_set (
				emvw->emv,
				"column_width", emvw->column_width,
				NULL);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_minicard_view_widget_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
	EMinicardViewWidget *emvw;

	emvw = E_MINICARD_VIEW_WIDGET (object);

	switch (property_id) {
	case PROP_CLIENT:
		g_value_set_object (value, emvw->book_client);
		break;
	case PROP_QUERY:
		g_value_set_string (value, emvw->query);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, emvw->editable);
		break;
	case PROP_COLUMN_WIDTH:
		g_value_set_double (value, emvw->column_width);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_minicard_view_widget_dispose (GObject *object)
{
	EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET (object);

	if (view->book_client) {
		g_object_unref (view->book_client);
		view->book_client = NULL;
	}
	if (view->query) {
		g_free (view->query);
		view->query = NULL;
	}

	if (view->adapter) {
		g_object_unref (view->adapter);
		view->adapter = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_minicard_view_widget_parent_class)->dispose (object);
}

static void
selection_change (ESelectionModel *esm,
                  EMinicardViewWidget *widget)
{
	g_signal_emit (widget, signals[SELECTION_CHANGE], 0);
}

static void
selection_row_change (ESelectionModel *esm,
                      gint row,
                      EMinicardViewWidget *widget)
{
	selection_change (esm, widget);
}

static void
column_width_changed (ESelectionModel *esm,
                      gdouble width,
                      EMinicardViewWidget *widget)
{
	g_signal_emit (widget, signals[COLUMN_WIDTH_CHANGED], 0, width);
}

static void
create_contact (EMinicardView *view,
                EMinicardViewWidget *widget)
{
	g_signal_emit (widget, signals[CREATE_CONTACT], 0);
}

static void
create_contact_list (EMinicardView *view,
                     EMinicardViewWidget *widget)
{
	g_signal_emit (widget, signals[CREATE_CONTACT_LIST], 0);
}

static guint
right_click (EMinicardView *view,
             GdkEvent *event,
             EMinicardViewWidget *widget)
{
	guint ret_val;

	g_signal_emit (widget, signals[RIGHT_CLICK], 0, event, &ret_val);

	return ret_val;
}

static void
e_minicard_view_widget_style_updated (GtkWidget *widget)
{
	EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET (widget);

	if (view->background) {
		GdkColor color;

		e_utils_get_theme_color_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &color);

		gnome_canvas_item_set (view->background, "fill_color_gdk", &color, NULL);
	}

	GTK_WIDGET_CLASS (e_minicard_view_widget_parent_class)->style_updated (widget);
}

static void
e_minicard_view_widget_realize (GtkWidget *widget)
{
	EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET (widget);
	GdkColor color;

	e_utils_get_theme_color_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &color);

	view->background = gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (view)),
		e_canvas_background_get_type (),
		"fill_color_gdk", &color,
		NULL);

	view->emv = gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (view)),
		e_minicard_view_get_type (),
		"height", (gdouble) 100,
		"minimum_width", (gdouble) 100,
		"adapter", view->adapter,
		"column_width", view->column_width,
		NULL);

	g_signal_connect (
		E_REFLOW (view->emv)->selection,
		"selection_changed",
		G_CALLBACK (selection_change), view);
	g_signal_connect (
		E_REFLOW (view->emv)->selection,
		"selection_row_changed",
		G_CALLBACK (selection_row_change), view);
	g_signal_connect (
		view->emv, "column_width_changed",
		G_CALLBACK (column_width_changed), view);
	g_signal_connect (
		view->emv, "create-contact",
		G_CALLBACK (create_contact), view);
	g_signal_connect (
		view->emv, "create-contact-list",
		G_CALLBACK (create_contact_list), view);
	g_signal_connect (
		view->emv, "right_click",
		G_CALLBACK (right_click), view);

	GTK_WIDGET_CLASS (e_minicard_view_widget_parent_class)->
		realize (widget);
}

static void
e_minicard_view_widget_size_allocate (GtkWidget *widget,
                                      GtkAllocation *allocation)
{
	GTK_WIDGET_CLASS (e_minicard_view_widget_parent_class)->
		size_allocate (widget, allocation);

	if (gtk_widget_get_realized (widget)) {
		gdouble width;
		EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET (widget);

		gnome_canvas_item_set (
			view->emv, "height",
			(gdouble) allocation->height, NULL);
		gnome_canvas_item_set (
			view->emv, "minimum_width",
			(gdouble) allocation->width, NULL);
		g_object_get (view->emv, "width", &width, NULL);
		width = MAX (width, allocation->width);
		gnome_canvas_set_scroll_region (
			GNOME_CANVAS (view), 0, 0,
			width - 1, allocation->height - 1);
	}
}

static void
e_minicard_view_widget_reflow (ECanvas *canvas)
{
	gdouble width;
	EMinicardViewWidget *view = E_MINICARD_VIEW_WIDGET (canvas);
	GtkAllocation allocation;

	E_CANVAS_CLASS (e_minicard_view_widget_parent_class)->reflow (canvas);

	g_object_get (view->emv, "width", &width, NULL);
	gtk_widget_get_allocation (GTK_WIDGET (canvas), &allocation);

	gnome_canvas_set_scroll_region (
		GNOME_CANVAS (canvas), 0, 0,
		MAX (width, allocation.width) - 1,
		allocation.height - 1);
}

ESelectionModel *
e_minicard_view_widget_get_selection_model (EMinicardViewWidget *view)
{
	if (view->emv)
		return E_SELECTION_MODEL (E_REFLOW (view->emv)->selection);
	else
		return NULL;
}

EMinicardView *
e_minicard_view_widget_get_view (EMinicardViewWidget *view)
{
	if (view->emv)
		return E_MINICARD_VIEW (view->emv);
	else
		return NULL;
}

static gboolean
e_minicard_view_widget_real_focus_in_event (GtkWidget *widget,
                                            GdkEventFocus *event)
{
	GnomeCanvas *canvas;
	EMinicardViewWidget *view;

	canvas = GNOME_CANVAS (widget);
	view = E_MINICARD_VIEW_WIDGET (widget);

	if (!canvas->focused_item) {
		EReflow *reflow = E_REFLOW (view->emv);
		if (reflow->count) {
			gint unsorted = e_sorter_sorted_to_model (E_SORTER (reflow->sorter), 0);

			if (unsorted != -1)
				canvas->focused_item = reflow->items[unsorted];
		}
	}

	return GTK_WIDGET_CLASS (e_minicard_view_widget_parent_class)->
		focus_in_event (widget, event);
}

