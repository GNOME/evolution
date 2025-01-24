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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ECellDateEdit - a subclass of ECellPopup used to show a date with a popup
 * window to edit it.
 */

#include "evolution-config.h"

#include "e-cell-date-edit.h"

#include <string.h>
#include <time.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <libedataserver/libedataserver.h>

#include "e-calendar.h"
#include "e-cell-text.h"
#include "e-table-item.h"

static void e_cell_date_edit_get_property	(GObject	*object,
						 guint		 property_id,
						 GValue		*value,
						 GParamSpec	*pspec);
static void e_cell_date_edit_set_property	(GObject	*object,
						 guint		 property_id,
						 const GValue	*value,
						 GParamSpec	*pspec);
static void e_cell_date_edit_dispose		(GObject	*object);

static gint e_cell_date_edit_do_popup		(ECellPopup	*ecp,
						 GdkEvent	*event,
						 gint             row,
						 gint             view_col);
static void e_cell_date_edit_set_popup_values	(ECellDateEdit	*ecde);
static void e_cell_date_edit_select_matching_time (ECellDateEdit	*ecde,
						  gchar		*time);
static void e_cell_date_edit_show_popup		(ECellDateEdit	*ecde,
						 gint             row,
						 gint             view_col);
static void e_cell_date_edit_get_popup_pos	(ECellDateEdit	*ecde,
						 gint             row,
						 gint             view_col,
						 gint		*x,
						 gint		*y,
						 gint		*height,
						 gint		*width);

static void e_cell_date_edit_rebuild_time_list	(ECellDateEdit	*ecde);

static gint e_cell_date_edit_key_press		(GtkWidget	*popup_window,
						 GdkEventKey	*event,
						 ECellDateEdit	*ecde);
static gint  e_cell_date_edit_button_press	(GtkWidget	*popup_window,
						 GdkEvent	*button_event,
						 ECellDateEdit	*ecde);
static void e_cell_date_edit_on_ok_clicked	(GtkWidget	*button,
						 ECellDateEdit	*ecde);
static void e_cell_date_edit_show_time_invalid_warning	(ECellDateEdit	*ecde);
static void e_cell_date_edit_on_now_clicked	(GtkWidget	*button,
						 ECellDateEdit	*ecde);
static void e_cell_date_edit_on_none_clicked	(GtkWidget	*button,
						 ECellDateEdit	*ecde);
static void e_cell_date_edit_on_today_clicked	(GtkWidget	*button,
						 ECellDateEdit	*ecde);
static void e_cell_date_edit_update_cell	(ECellDateEdit	*ecde,
						 const gchar	*text);
static void e_cell_date_edit_on_time_selected	(GtkTreeSelection *selection,
						 ECellDateEdit *ecde);
static void e_cell_date_edit_hide_popup		(ECellDateEdit	*ecde);

/* Our arguments. */
enum {
	PROP_0,
	PROP_SHOW_TIME,
	PROP_SHOW_NOW_BUTTON,
	PROP_SHOW_TODAY_BUTTON,
	PROP_ALLOW_NO_DATE_SET,
	PROP_USE_24_HOUR_FORMAT,
	PROP_LOWER_HOUR,
	PROP_UPPER_HOUR
};

enum {
	BEFORE_POPUP,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (ECellDateEdit, e_cell_date_edit, E_TYPE_CELL_POPUP)

static void
e_cell_date_edit_class_init (ECellDateEditClass *class)
{
	GObjectClass *object_class;
	ECellPopupClass	*ecpc;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = e_cell_date_edit_get_property;
	object_class->set_property = e_cell_date_edit_set_property;
	object_class->dispose = e_cell_date_edit_dispose;

	ecpc = E_CELL_POPUP_CLASS (class);
	ecpc->popup = e_cell_date_edit_do_popup;

	g_object_class_install_property (
		object_class,
		PROP_SHOW_TIME,
		g_param_spec_boolean (
			"show_time",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_NOW_BUTTON,
		g_param_spec_boolean (
			"show_now_button",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_TODAY_BUTTON,
		g_param_spec_boolean (
			"show_today_button",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ALLOW_NO_DATE_SET,
		g_param_spec_boolean (
			"allow_no_date_set",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_USE_24_HOUR_FORMAT,
		g_param_spec_boolean (
			"use_24_hour_format",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_LOWER_HOUR,
		g_param_spec_int (
			"lower_hour",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_UPPER_HOUR,
		g_param_spec_int (
			"upper_hour",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			24,
			G_PARAM_READWRITE));

	signals[BEFORE_POPUP] = g_signal_new (
		"before-popup",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_INT, G_TYPE_INT);
}

static void
e_cell_date_edit_init (ECellDateEdit *ecde)
{
	GtkWidget *frame, *vbox, *hbox, *vbox2;
	GtkWidget *scrolled_window, *bbox, *tree_view;
	GtkWidget *now_button, *today_button, *none_button, *ok_button;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	PangoAttrList *tnum;
	PangoAttribute *attr;

	ecde->lower_hour = 0;
	ecde->upper_hour = 24;
	ecde->use_24_hour_format = TRUE;
	ecde->need_time_list_rebuild = TRUE;
	ecde->freeze_count = 0;
	ecde->time_callback = NULL;
	ecde->time_callback_data = NULL;
	ecde->time_callback_destroy = NULL;

	/* We create one popup window for the ECell, since there will only
	 * ever be one popup in use at a time. */
	ecde->popup_window = gtk_window_new (GTK_WINDOW_POPUP);

	gtk_window_set_type_hint (
		GTK_WINDOW (ecde->popup_window),
		GDK_WINDOW_TYPE_HINT_COMBO);
	gtk_window_set_resizable (GTK_WINDOW (ecde->popup_window), TRUE);

	frame = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (ecde->popup_window), frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_widget_show (frame);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_widget_show (vbox);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	ecde->calendar = e_calendar_new ();
	gnome_canvas_item_set (
		GNOME_CANVAS_ITEM (e_calendar_get_item (E_CALENDAR (ecde->calendar))),
		"move_selection_when_moving", FALSE,
		NULL);
	gtk_box_pack_start (GTK_BOX (hbox), ecde->calendar, TRUE, TRUE, 0);
	gtk_widget_show (ecde->calendar);

	vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_style_context_add_class (
		gtk_widget_get_style_context (vbox2), "linked");
	gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show (vbox2);

	ecde->time_entry = gtk_entry_new ();
	gtk_widget_set_size_request (ecde->time_entry, 50, -1);
	gtk_box_pack_start (
		GTK_BOX (vbox2), ecde->time_entry,
		FALSE, FALSE, 0);
	gtk_widget_show (ecde->time_entry);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (vbox2), scrolled_window, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_widget_show (scrolled_window);

	store = gtk_list_store_new (1, G_TYPE_STRING);
	tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = gtk_cell_renderer_text_new ();
	tnum = pango_attr_list_new ();
	attr = pango_attr_font_features_new ("tnum=1");
	pango_attr_list_insert_before (tnum, attr);
	g_object_set (renderer, "attributes", tnum, NULL);
	pango_attr_list_unref (tnum);

	gtk_tree_view_append_column (
		GTK_TREE_VIEW (tree_view),
		gtk_tree_view_column_new_with_attributes (
		"Text", renderer, "text", 0, NULL));

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);

	gtk_container_add (
		GTK_CONTAINER (scrolled_window), tree_view);
	gtk_container_set_focus_vadjustment (
		GTK_CONTAINER (tree_view),
		gtk_scrolled_window_get_vadjustment (
		GTK_SCROLLED_WINDOW (scrolled_window)));
	gtk_container_set_focus_hadjustment (
		GTK_CONTAINER (tree_view),
		gtk_scrolled_window_get_hadjustment (
		GTK_SCROLLED_WINDOW (scrolled_window)));
	gtk_widget_show (tree_view);
	ecde->time_tree_view = tree_view;
	g_signal_connect (
		gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)), "changed",
		G_CALLBACK (e_cell_date_edit_on_time_selected), ecde);

	e_binding_bind_property (ecde->time_entry, "visible",
		vbox2, "visible", G_BINDING_DEFAULT);

	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_set_spacing (GTK_BOX (bbox), 2);
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
	gtk_widget_show (bbox);

	now_button = gtk_button_new_with_label (_("Now"));
	gtk_container_add (GTK_CONTAINER (bbox), now_button);
	gtk_widget_show (now_button);
	g_signal_connect (
		now_button, "clicked",
		G_CALLBACK (e_cell_date_edit_on_now_clicked), ecde);
	ecde->now_button = now_button;

	today_button = gtk_button_new_with_label (_("Today"));
	gtk_container_add (GTK_CONTAINER (bbox), today_button);
	gtk_widget_show (today_button);
	g_signal_connect (
		today_button, "clicked",
		G_CALLBACK (e_cell_date_edit_on_today_clicked), ecde);
	ecde->today_button = today_button;

	/* Translators: "None" as a label of a button to unset date in a
	 * date table cell. */
	none_button = gtk_button_new_with_label (C_("table-date", "None"));
	gtk_container_add (GTK_CONTAINER (bbox), none_button);
	gtk_widget_show (none_button);
	g_signal_connect (
		none_button, "clicked",
		G_CALLBACK (e_cell_date_edit_on_none_clicked), ecde);
	ecde->none_button = none_button;

	ok_button = gtk_button_new_with_label (_("OK"));
	gtk_container_add (GTK_CONTAINER (bbox), ok_button);
	gtk_widget_show (ok_button);
	g_signal_connect (
		ok_button, "clicked",
		G_CALLBACK (e_cell_date_edit_on_ok_clicked), ecde);

	g_signal_connect (
		ecde->popup_window, "key_press_event",
		G_CALLBACK (e_cell_date_edit_key_press), ecde);
	g_signal_connect (
		ecde->popup_window, "button_press_event",
		G_CALLBACK (e_cell_date_edit_button_press), ecde);
}

/**
 * e_cell_date_edit_new:
 *
 * Creates a new ECellDateEdit renderer.
 *
 * Returns: an ECellDateEdit object.
 */
ECell *
e_cell_date_edit_new (void)
{
	return g_object_new (e_cell_date_edit_get_type (), NULL);
}

static void
e_cell_date_edit_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	ECellDateEdit *ecde;

	ecde = E_CELL_DATE_EDIT (object);

	switch (property_id) {
	case PROP_SHOW_TIME:
		g_value_set_boolean (value, gtk_widget_get_visible (ecde->time_entry));
		return;
	case PROP_SHOW_NOW_BUTTON:
		g_value_set_boolean (value, gtk_widget_get_visible (ecde->now_button));
		return;
	case PROP_SHOW_TODAY_BUTTON:
		g_value_set_boolean (value, gtk_widget_get_visible (ecde->today_button));
		return;
	case PROP_ALLOW_NO_DATE_SET:
		g_value_set_boolean (value, gtk_widget_get_visible (ecde->none_button));
		return;
	case PROP_USE_24_HOUR_FORMAT:
		g_value_set_boolean (value, ecde->use_24_hour_format);
		return;
	case PROP_LOWER_HOUR:
		g_value_set_int (value, ecde->lower_hour);
		return;
	case PROP_UPPER_HOUR:
		g_value_set_int (value, ecde->upper_hour);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_cell_date_edit_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	ECellDateEdit *ecde;
	gint ivalue;
	gboolean bvalue;

	ecde = E_CELL_DATE_EDIT (object);

	switch (property_id) {
	case PROP_SHOW_TIME:
		if (g_value_get_boolean (value)) {
			gtk_widget_show (ecde->time_entry);
			gtk_widget_show (ecde->time_tree_view);
		} else {
			gtk_widget_hide (ecde->time_entry);
			gtk_widget_hide (ecde->time_tree_view);
		}
		return;
	case PROP_SHOW_NOW_BUTTON:
		if (g_value_get_boolean (value)) {
			gtk_widget_show (ecde->now_button);
		} else {
			gtk_widget_hide (ecde->now_button);
		}
		return;
	case PROP_SHOW_TODAY_BUTTON:
		if (g_value_get_boolean (value)) {
			gtk_widget_show (ecde->today_button);
		} else {
			gtk_widget_hide (ecde->today_button);
		}
		return;
	case PROP_ALLOW_NO_DATE_SET:
		if (g_value_get_boolean (value)) {
			gtk_widget_show (ecde->none_button);
		} else {
			/* FIXME: What if we have no date set now. */
			gtk_widget_hide (ecde->none_button);
		}
		return;
	case PROP_USE_24_HOUR_FORMAT:
		bvalue = g_value_get_boolean (value);
		if (ecde->use_24_hour_format != bvalue) {
			ecde->use_24_hour_format = bvalue;
			ecde->need_time_list_rebuild = TRUE;
		}
		return;
	case PROP_LOWER_HOUR:
		ivalue = g_value_get_int (value);
		ivalue = CLAMP (ivalue, 0, 24);
		if (ecde->lower_hour != ivalue) {
			ecde->lower_hour = ivalue;
			ecde->need_time_list_rebuild = TRUE;
		}
		return;
	case PROP_UPPER_HOUR:
		ivalue = g_value_get_int (value);
		ivalue = CLAMP (ivalue, 0, 24);
		if (ecde->upper_hour != ivalue) {
			ecde->upper_hour = ivalue;
			ecde->need_time_list_rebuild = TRUE;
		}
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_cell_date_edit_dispose (GObject *object)
{
	ECellDateEdit *ecde = E_CELL_DATE_EDIT (object);

	e_cell_date_edit_set_get_time_callback (ecde, NULL, NULL, NULL);

	g_clear_pointer (&ecde->popup_window, gtk_widget_destroy);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cell_date_edit_parent_class)->dispose (object);
}

static gint
e_cell_date_edit_do_popup (ECellPopup *ecp,
                           GdkEvent *event,
                           gint row,
                           gint view_col)
{
	ECellDateEdit *ecde = E_CELL_DATE_EDIT (ecp);
	GdkWindow *window;

	g_signal_emit (ecp, signals[BEFORE_POPUP], 0, row, view_col);

	e_cell_date_edit_show_popup (ecde, row, view_col);
	e_cell_date_edit_set_popup_values (ecde);

	gtk_grab_add (ecde->popup_window);

	/* Set the focus to the first widget. */
	gtk_widget_grab_focus (ecde->time_entry);
	window = gtk_widget_get_window (ecde->popup_window);
	gdk_window_focus (window, GDK_CURRENT_TIME);

	return TRUE;
}

static void
e_cell_date_edit_set_popup_values (ECellDateEdit *ecde)
{
	ECellPopup *ecp = E_CELL_POPUP (ecde);
	ECellText *ecell_text = E_CELL_TEXT (ecp->child);
	ECellView *ecv = (ECellView *) ecp->popup_cell_view;
	ETableItem *eti;
	ETableCol *ecol;
	gchar *cell_text;
	ETimeParseStatus status;
	struct tm date_tm;
	GDate date;
	ECalendarItem *calitem;
	gchar buffer[64];
	gboolean is_date = TRUE;

	eti = E_TABLE_ITEM (ecp->popup_cell_view->cell_view.e_table_item_view);
	ecol = e_table_header_get_column (eti->header, ecp->popup_view_col);

	cell_text = e_cell_text_get_text (
		ecell_text, ecv->e_table_model,
		ecol->spec->model_col, ecp->popup_row);

	/* Try to parse just a date first. If the value is only a date, we
	 * use a DATE value. */
	status = e_time_parse_date (cell_text, &date_tm);
	if (status == E_TIME_PARSE_INVALID) {
		is_date = FALSE;
		status = e_time_parse_date_and_time (cell_text, &date_tm);
	}

	/* If there is no date and time set, or the date is invalid, we clear
	 * the selections, else we select the appropriate date & time. */
	calitem = E_CALENDAR_ITEM (e_calendar_get_item (E_CALENDAR (ecde->calendar)));
	if (status == E_TIME_PARSE_NONE || status == E_TIME_PARSE_INVALID) {
		gtk_entry_set_text (GTK_ENTRY (ecde->time_entry), "");
		e_calendar_item_set_selection (calitem, NULL, NULL);
		gtk_tree_selection_unselect_all (
			gtk_tree_view_get_selection (
			GTK_TREE_VIEW (ecde->time_tree_view)));
	} else {
		if (is_date) {
			buffer[0] = '\0';
		} else {
			e_time_format_time (
				&date_tm, ecde->use_24_hour_format,
				FALSE, buffer, sizeof (buffer));
		}
		gtk_entry_set_text (GTK_ENTRY (ecde->time_entry), buffer);

		g_date_clear (&date, 1);
		g_date_set_dmy (
			&date,
			date_tm.tm_mday,
			date_tm.tm_mon + 1,
			date_tm.tm_year + 1900);
		e_calendar_item_set_selection (calitem, &date, &date);

		if (is_date) {
			gtk_tree_selection_unselect_all (
				gtk_tree_view_get_selection (
				GTK_TREE_VIEW (ecde->time_tree_view)));
		} else {
			e_cell_date_edit_select_matching_time (ecde, buffer);
		}
	}

	e_cell_text_free_text (ecell_text, ecv->e_table_model,
		ecol->spec->model_col, cell_text);
}

static void
e_cell_date_edit_select_matching_time (ECellDateEdit *ecde,
                                       gchar *time)
{
	gboolean found = FALSE;
	gboolean valid;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ecde->time_tree_view));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ecde->time_tree_view));

	for (valid = gtk_tree_model_get_iter_first (model, &iter);
	     valid && !found;
	     valid = gtk_tree_model_iter_next (model, &iter)) {
		gchar *str = NULL;

		gtk_tree_model_get (model, &iter, 0, &str, -1);

		if (g_str_equal (str, time)) {
			GtkTreePath *path = gtk_tree_model_get_path (model, &iter);

			gtk_tree_view_set_cursor (
				GTK_TREE_VIEW (ecde->time_tree_view),
				path, NULL, FALSE);
			gtk_tree_view_scroll_to_cell (
				GTK_TREE_VIEW (ecde->time_tree_view),
				path, NULL, FALSE, 0.0, 0.0);
			gtk_tree_path_free (path);

			found = TRUE;
		}

		g_free (str);
	}

	if (!found) {
		gtk_tree_selection_unselect_all (selection);
		gtk_tree_view_scroll_to_point (GTK_TREE_VIEW (ecde->time_tree_view), 0, 0);
	}
}

static void
e_cell_date_edit_show_popup (ECellDateEdit *ecde,
                             gint row,
                             gint view_col)
{
	ECellView *ecv = (ECellView *) E_CELL_POPUP (ecde)->popup_cell_view;
	GtkWidget *toplevel;
	gint x, y, width, height;

	if (ecde->need_time_list_rebuild)
		e_cell_date_edit_rebuild_time_list (ecde);

	/* This code is practically copied from GtkCombo. */

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (GNOME_CANVAS_ITEM (ecv->e_table_item_view)->canvas));
	if (GTK_IS_WINDOW (toplevel))
		gtk_window_set_transient_for (GTK_WINDOW (ecde->popup_window), GTK_WINDOW (toplevel));

	e_cell_date_edit_get_popup_pos (ecde, row, view_col, &x, &y, &height, &width);

	gtk_window_move (GTK_WINDOW (ecde->popup_window), x, y);
	gtk_widget_set_size_request (ecde->popup_window, width, height);
	gtk_widget_realize (ecde->popup_window);
	gdk_window_resize (gtk_widget_get_window (ecde->popup_window), width, height);
	gtk_widget_show (ecde->popup_window);

	e_cell_popup_set_shown (E_CELL_POPUP (ecde), TRUE);
}

/* Calculates the size and position of the popup window (like GtkCombo). */
static void
e_cell_date_edit_get_popup_pos (ECellDateEdit *ecde,
                                gint row,
                                gint view_col,
                                gint *x,
                                gint *y,
                                gint *height,
                                gint *width)
{
	ECellPopup *ecp = E_CELL_POPUP (ecde);
	ETableItem *eti;
	GtkWidget *canvas;
	GtkRequisition popup_requisition;
	GtkAdjustment *adjustment;
	GtkScrollable *scrollable;
	GdkWindow *window;
	gint avail_height, screen_width, column_width, row_height;
	gdouble x1, y1, wx, wy;
	gint value;

	eti = E_TABLE_ITEM (ecp->popup_cell_view->cell_view.e_table_item_view);
	canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (eti)->canvas);

	window = gtk_widget_get_window (canvas);
	gdk_window_get_origin (window, x, y);

	x1 = e_table_header_col_diff (eti->header, 0, view_col + 1);
	y1 = e_table_item_row_diff (eti, 0, row + 1);
	column_width = e_table_header_col_diff (
		eti->header, view_col, view_col + 1);
	row_height = e_table_item_row_diff (eti, row, row + 1);
	gnome_canvas_item_i2w (GNOME_CANVAS_ITEM (eti), &x1, &y1);

	gnome_canvas_world_to_window (
		GNOME_CANVAS (canvas), x1, y1, &wx, &wy);

	x1 = wx;
	y1 = wy;

	*x += x1;
	/* The ETable positions don't include the grid lines, I think, so we
	 * add 1. */
	scrollable = GTK_SCROLLABLE (&GNOME_CANVAS (canvas)->layout);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	value = (gint) gtk_adjustment_get_value (adjustment);
	*y += y1 + 1 - value + ((GnomeCanvas *)canvas)->zoom_yofs;

	avail_height = gdk_screen_height () - *y;

	/* We'll use the entire screen width if needed, but we save space for
	 * the vertical scrollbar in case we need to show that. */
	screen_width = gdk_screen_width ();

	gtk_widget_get_preferred_size (ecde->popup_window, &popup_requisition, NULL);

	/* Calculate the desired width. */
	*width = popup_requisition.width;

	/* Use at least the same width as the column. */
	if (*width < column_width)
		*width = column_width;

	/* Check if it fits in the available height. */
	if (popup_requisition.height > avail_height) {
		/* It doesn't fit, so we see if we have the minimum space
		 * needed. */
		if (*y - row_height > avail_height) {
			/* We don't, so we show the popup above the cell
			 * instead of below it. */
			*y -= (popup_requisition.height + row_height);
			if (*y < 0)
				*y = 0;
		}
	}

	/* We try to line it up with the right edge of the column, but we don't
	 * want it to go off the edges of the screen. */
	if (*x > screen_width)
		*x = screen_width;
	*x -= *width;
	if (*x < 0)
		*x = 0;

	*height = popup_requisition.height;
}

/* This handles key press events in the popup window. If the Escape key is
 * pressed we hide the popup, and do not change the cell contents. */
static gint
e_cell_date_edit_key_press (GtkWidget *popup_window,
                            GdkEventKey *event,
                            ECellDateEdit *ecde)
{
	/* If the Escape key is pressed we hide the popup. */
	if (event->keyval != GDK_KEY_Escape)
		return FALSE;

	e_cell_date_edit_hide_popup (ecde);

	return TRUE;
}

/* This handles button press events in the popup window. If the button is
 * pressed outside the popup, we hide it and do not change the cell contents.
*/
static gint
e_cell_date_edit_button_press (GtkWidget *popup_window,
                               GdkEvent *button_event,
                               ECellDateEdit *ecde)
{
	GtkWidget *event_widget;

	event_widget = gtk_get_event_widget (button_event);

	if (gtk_widget_get_toplevel (event_widget) != popup_window)
		e_cell_date_edit_hide_popup (ecde);

	return TRUE;
}

/* Clears the time list and rebuilds it using the lower_hour, upper_hour
 * and use_24_hour_format settings. */
static void
e_cell_date_edit_rebuild_time_list (ECellDateEdit *ecde)
{
	GtkListStore *store;
	gchar buffer[40];
	struct tm tmp_tm;
	gint hour, min;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (
		GTK_TREE_VIEW (ecde->time_tree_view)));
	gtk_list_store_clear (store);

	/* Fill the struct tm with some sane values. */
	tmp_tm.tm_year = 2000;
	tmp_tm.tm_mon = 0;
	tmp_tm.tm_mday = 1;
	tmp_tm.tm_sec = 0;
	tmp_tm.tm_isdst = 0;

	for (hour = ecde->lower_hour; hour <= ecde->upper_hour; hour++) {
		/* We don't want to display midnight at the end, since that is
		 * really in the next day. */
		if (hour == 24)
			break;

		/* We want to finish on upper_hour, with min == 0. */
		for (min = 0;
		     min == 0 || (min < 60 && hour != ecde->upper_hour);
		     min += 30) {
			GtkTreeIter iter;

			tmp_tm.tm_hour = hour;
			tmp_tm.tm_min = min;
			e_time_format_time (&tmp_tm, ecde->use_24_hour_format,
					    FALSE, buffer, sizeof (buffer));

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter, 0, buffer, -1);
		}
	}

	ecde->need_time_list_rebuild = FALSE;
}

static void
e_cell_date_edit_on_ok_clicked (GtkWidget *button,
                                ECellDateEdit *ecde)
{
	ECalendarItem *calitem;
	GDate start_date, end_date;
	gboolean day_selected;
	struct tm date_tm;
	gchar buffer[64];
	const gchar *text;
	ETimeParseStatus status;
	gboolean is_date = FALSE;

	calitem = E_CALENDAR_ITEM (e_calendar_get_item (E_CALENDAR (ecde->calendar)));
	day_selected = e_calendar_item_get_selection (
		calitem, &start_date, &end_date);

	text = gtk_entry_get_text (GTK_ENTRY (ecde->time_entry));
	status = e_time_parse_time (text, &date_tm);
	if (status == E_TIME_PARSE_INVALID) {
		e_cell_date_edit_show_time_invalid_warning (ecde);
		return;
	} else if (status == E_TIME_PARSE_NONE || !gtk_widget_get_visible (ecde->time_entry)) {
		is_date = TRUE;
	}

	if (day_selected) {
		date_tm.tm_year = g_date_get_year (&start_date) - 1900;
		date_tm.tm_mon = g_date_get_month (&start_date) - 1;
		date_tm.tm_mday = g_date_get_day (&start_date);
		/* We need to call this to set the weekday. */
		mktime (&date_tm);
		e_time_format_date_and_time (&date_tm,
					     ecde->use_24_hour_format,
					     !is_date, FALSE,
					     buffer, sizeof (buffer));
	} else {
		buffer[0] = '\0';
	}

	e_cell_date_edit_update_cell (ecde, buffer);
	e_cell_date_edit_hide_popup (ecde);
}

static void
e_cell_date_edit_show_time_invalid_warning (ECellDateEdit *ecde)
{
	GtkWidget *dialog;
	struct tm date_tm;
	gchar buffer[64];

	/* Create a useful error message showing the correct format. */
	date_tm.tm_year = 100;
	date_tm.tm_mon = 0;
	date_tm.tm_mday = 1;
	date_tm.tm_hour = 1;
	date_tm.tm_min = 30;
	date_tm.tm_sec = 0;
	date_tm.tm_isdst = -1;
	e_time_format_time (&date_tm, ecde->use_24_hour_format, FALSE,
			    buffer, sizeof (buffer));

	/* FIXME: Fix transient settings - I'm not sure it works with popup
	 * windows. Maybe we need to use a normal window without decorations.*/
	dialog = gtk_message_dialog_new (
		GTK_WINDOW (ecde->popup_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		_("The time must be in the format: %s"),
		buffer);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
e_cell_date_edit_on_now_clicked (GtkWidget *button,
                                 ECellDateEdit *ecde)
{
	struct tm tmp_tm;
	time_t t;
	gchar buffer[64];

	if (ecde->time_callback) {
		tmp_tm = ecde->time_callback (
			ecde, ecde->time_callback_data);
	} else {
		t = time (NULL);
		tmp_tm = *localtime (&t);
	}

	e_time_format_date_and_time (
		&tmp_tm, ecde->use_24_hour_format,
		TRUE, FALSE, buffer, sizeof (buffer));

	e_cell_date_edit_update_cell (ecde, buffer);
	e_cell_date_edit_hide_popup (ecde);
}

static void
e_cell_date_edit_on_none_clicked (GtkWidget *button,
                                  ECellDateEdit *ecde)
{
	e_cell_date_edit_update_cell (ecde, "");
	e_cell_date_edit_hide_popup (ecde);
}

static void
e_cell_date_edit_on_today_clicked (GtkWidget *button,
                                   ECellDateEdit *ecde)
{
	struct tm tmp_tm;
	time_t t;
	gchar buffer[64];

	if (ecde->time_callback) {
		tmp_tm = ecde->time_callback (
			ecde, ecde->time_callback_data);
	} else {
		t = time (NULL);
		tmp_tm = *localtime (&t);
	}

	tmp_tm.tm_hour = 0;
	tmp_tm.tm_min = 0;
	tmp_tm.tm_sec = 0;

	e_time_format_date_and_time (
		&tmp_tm, ecde->use_24_hour_format,
		FALSE, FALSE, buffer, sizeof (buffer));

	e_cell_date_edit_update_cell (ecde, buffer);
	e_cell_date_edit_hide_popup (ecde);
}

static void
e_cell_date_edit_update_cell (ECellDateEdit *ecde,
                              const gchar *text)
{
	ECellPopup *ecp = E_CELL_POPUP (ecde);
	ECellText *ecell_text = E_CELL_TEXT (ecp->child);
	ECellView *ecv = (ECellView *) ecp->popup_cell_view;
	ETableItem *eti = E_TABLE_ITEM (ecv->e_table_item_view);
	ETableCol *ecol;
	gchar *old_text;

	/* Compare the new text with the existing cell contents. */
	ecol = e_table_header_get_column (eti->header, ecp->popup_view_col);

	old_text = e_cell_text_get_text (
		ecell_text, ecv->e_table_model,
		ecol->spec->model_col, ecp->popup_row);

	/* If they are different, update the cell contents. */
	if (strcmp (old_text, text)) {
		e_cell_text_set_value (
			ecell_text, ecv->e_table_model,
			ecol->spec->model_col, ecp->popup_row, text);
		e_cell_leave_edit (
			ecv, ecol->spec->model_col,
			ecp->popup_view_col, ecp->popup_row, NULL);
	}

	e_cell_text_free_text (ecell_text, ecv->e_table_model,
		ecol->spec->model_col, old_text);
}

static void
e_cell_date_edit_on_time_selected (GtkTreeSelection *selection,
                                   ECellDateEdit *ecde)
{
	gchar *list_item_text = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, 0, &list_item_text, -1);

	g_return_if_fail (list_item_text != NULL);

	gtk_entry_set_text (GTK_ENTRY (ecde->time_entry), list_item_text);

	g_free (list_item_text);
}

static void
e_cell_date_edit_hide_popup (ECellDateEdit *ecde)
{
	gtk_grab_remove (ecde->popup_window);
	gtk_widget_hide (ecde->popup_window);
	e_cell_popup_set_shown (E_CELL_POPUP (ecde), FALSE);
}

/* These freeze and thaw the rebuilding of the time list. They are useful when
 * setting several properties which result in rebuilds of the list, e.g. the
 * lower_hour, upper_hour and use_24_hour_format properties. */
void
e_cell_date_edit_freeze (ECellDateEdit *ecde)
{
	g_return_if_fail (E_IS_CELL_DATE_EDIT (ecde));

	ecde->freeze_count++;
}

void
e_cell_date_edit_thaw (ECellDateEdit *ecde)
{
	g_return_if_fail (E_IS_CELL_DATE_EDIT (ecde));

	if (ecde->freeze_count > 0) {
		ecde->freeze_count--;

		if (ecde->freeze_count == 0)
			e_cell_date_edit_rebuild_time_list (ecde);
	}
}

/* Sets a callback to use to get the current time. This is useful if the
 * application needs to use its own timezone data rather than rely on the
 * Unix timezone. */
void
e_cell_date_edit_set_get_time_callback (ECellDateEdit *ecde,
                                        ECellDateEditGetTimeCallback cb,
                                        gpointer data,
                                        GDestroyNotify destroy)
{
	g_return_if_fail (E_IS_CELL_DATE_EDIT (ecde));

	if (ecde->time_callback_data && ecde->time_callback_destroy)
		(*ecde->time_callback_destroy) (ecde->time_callback_data);

	ecde->time_callback = cb;
	ecde->time_callback_data = data;
	ecde->time_callback_destroy = destroy;
}
