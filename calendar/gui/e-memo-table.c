/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *		Rodrigo Moya <rodrigo@ximian.com>
 *		Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * EMemoTable - displays the ECalComponent objects in a table (an ETable).
 * Used for calendar events and tasks.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <e-util/e-binding.h>
#include <widgets/misc/e-gui-utils.h>
#include <table/e-cell-checkbox.h>
#include <table/e-cell-toggle.h>
#include <table/e-cell-text.h>
#include <table/e-cell-combo.h>
#include <table/e-cell-date.h>
#include <e-util/e-selection.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-util-private.h>
#include <table/e-cell-date-edit.h>
#include <table/e-cell-percent.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-time-utils.h>

#include "dialogs/delete-comp.h"
#include "dialogs/delete-error.h"
#include "dialogs/memo-editor.h"
#include "e-cal-model-memos.h"
#include "e-memo-table.h"
#include "e-calendar-view.h"
#include "e-cell-date-edit-text.h"
#include "print.h"
#include <e-util/e-icon-factory.h>
#include "misc.h"

#define E_MEMO_TABLE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MEMO_TABLE, EMemoTablePrivate))

struct _EMemoTablePrivate {
	gpointer shell_view;  /* weak pointer */
	ECalModel *model;
};

enum {
	PROP_0,
	PROP_MODEL,
	PROP_SHELL_VIEW
};

enum {
	OPEN_COMPONENT,
	POPUP_EVENT,
	STATUS_MESSAGE,
	USER_CREATED,
	LAST_SIGNAL
};

static struct tm e_memo_table_get_current_time (ECellDateEdit *ecde, gpointer data);

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

/* The icons to represent the task. */
#define NUM_ICONS 2
static const gchar *icon_names[NUM_ICONS] = {
	"stock_notes",
	"stock_insert-note"
};
static GdkPixbuf *icon_pixbufs[NUM_ICONS] = { NULL };

static void
memo_table_emit_open_component (EMemoTable *memo_table,
                                ECalModelComponent *comp_data)
{
	guint signal_id = signals[OPEN_COMPONENT];

	g_signal_emit (memo_table, signal_id, 0, comp_data);
}

static void
memo_table_emit_popup_event (EMemoTable *memo_table,
                             GdkEvent *event)
{
	guint signal_id = signals[POPUP_EVENT];

	g_signal_emit (memo_table, signal_id, 0, event);
}

static void
memo_table_emit_status_message (EMemoTable *memo_table,
                                const gchar *message,
                                gdouble percent)
{
	guint signal_id = signals[STATUS_MESSAGE];

	g_signal_emit (memo_table, signal_id, 0, message, percent);
}

static void
memo_table_emit_user_created (EMemoTable *memo_table)
{
	guint signal_id = signals[USER_CREATED];

	g_signal_emit (memo_table, signal_id, 0);
}

static gint
memo_table_date_compare_cb (gconstpointer a,
                            gconstpointer b)
{
	ECellDateEditValue *dv1 = (ECellDateEditValue *) a;
	ECellDateEditValue *dv2 = (ECellDateEditValue *) b;
	struct icaltimetype tt;

	/* First check if either is NULL. NULL dates sort last. */
	if (!dv1 || !dv2) {
		if (dv1 == dv2)
			return 0;
		else if (dv1)
			return -1;
		else
			return 1;
	}

	/* Copy the 2nd value and convert it to the same timezone as the
	   first. */
	tt = dv2->tt;

	icaltimezone_convert_time (&tt, dv2->zone, dv1->zone);

	/* Now we can compare them. */

	return icaltime_compare (dv1->tt, tt);
}

static void
memo_table_double_click_cb (EMemoTable *memo_table,
                            gint row,
                            gint col,
                            GdkEvent *event)
{
	ECalModel *model;
	ECalModelComponent *comp_data;

	model = e_memo_table_get_model (memo_table);
	comp_data = e_cal_model_get_component_at (model, row);
	memo_table_emit_open_component (memo_table, comp_data);
}

static void
memo_table_model_cal_view_progress_cb (EMemoTable *memo_table,
                                       const gchar *message,
                                       gint progress,
                                       ECalSourceType type)
{
	gdouble percent = (gdouble) progress;

	memo_table_emit_status_message (memo_table, message, percent);
}

static void
memo_table_model_cal_view_done_cb (EMemoTable *memo_table,
                                   ECalendarStatus status,
                                   ECalSourceType type)
{
	memo_table_emit_status_message (memo_table, NULL, -1.0);
}

static gboolean
memo_table_query_tooltip_cb (EMemoTable *memo_table,
                             gint x,
                             gint y,
                             gboolean keyboard_mode,
                             GtkTooltip *tooltip)
{
	ECalModel *model;
	ECalModelComponent *comp_data;
	gint row = -1, col = -1;
	GtkWidget *box, *l, *w;
	GtkStyle *style = gtk_widget_get_default_style ();
	gchar *tmp;
	const gchar *str;
	GString *tmp2;
	gchar buff[1001];
	gboolean free_text = FALSE;
	gboolean use_24_hour_format;
	ECalComponent *new_comp;
	ECalComponentOrganizer organizer;
	ECalComponentDateTime dtstart, dtdue;
	icalcomponent *clone;
	icaltimezone *zone, *default_zone;
	GSList *desc, *p;
	gint len;
	ETable *etable;
	ESelectionModel *esm;
	struct tm tmp_tm;

	if (keyboard_mode)
		return FALSE;

	etable = e_memo_table_get_table (memo_table);
	e_table_get_mouse_over_cell (etable, &row, &col);
	if (row == -1 || !etable)
		return FALSE;

	/* Respect sorting option; the 'e_table_get_mouse_over_cell'
	 * returns sorted row, not the model one. */
	esm = e_table_get_selection_model (etable);
	if (esm && esm->sorter && e_sorter_needs_sorting (esm->sorter))
		row = e_sorter_sorted_to_model (esm->sorter, row);

	model = e_memo_table_get_model (memo_table);
	comp_data = e_cal_model_get_component_at (model, row);
	if (!comp_data || !comp_data->icalcomp)
		return FALSE;

	new_comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	if (!e_cal_component_set_icalcomponent (new_comp, clone)) {
		g_object_unref (new_comp);
		return FALSE;
	}

	box = gtk_vbox_new (FALSE, 0);

	str = e_calendar_view_get_icalcomponent_summary (
		comp_data->client, comp_data->icalcomp, &free_text);
	if (!(str && *str)) {
		if (free_text)
			g_free ((gchar *)str);
		free_text = FALSE;
		str = _("* No Summary *");
	}

	l = gtk_label_new (NULL);
	tmp = g_markup_printf_escaped ("<b>%s</b>", str);
	gtk_label_set_line_wrap (GTK_LABEL (l), TRUE);
	gtk_label_set_markup (GTK_LABEL (l), tmp);
	gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
	w = gtk_event_box_new ();

	gtk_widget_modify_bg (w, GTK_STATE_NORMAL, &(style->bg[GTK_STATE_SELECTED]));
	gtk_widget_modify_fg (l, GTK_STATE_NORMAL, &(style->text[GTK_STATE_SELECTED]));
	gtk_container_add (GTK_CONTAINER (w), l);
	gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);
	g_free (tmp);

	if (free_text)
		g_free ((gchar *)str);
	free_text = FALSE;

	w = gtk_event_box_new ();
	gtk_widget_modify_bg (w, GTK_STATE_NORMAL, &(style->bg[GTK_STATE_NORMAL]));

	l = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (w), l);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	w = l;

	e_cal_component_get_organizer (new_comp, &organizer);
	if (organizer.cn) {
		gchar *ptr;
		ptr = strchr( organizer.value, ':');

		if (ptr) {
			ptr++;
			/* To Translators: It will display "Organizer: NameOfTheUser <email@ofuser.com>" */
			tmp = g_strdup_printf (_("Organizer: %s <%s>"), organizer.cn, ptr);
		} else {
			/* With SunOne accounts, there may be no ':' in organiser.value */
			tmp = g_strdup_printf (_("Organizer: %s"), organizer.cn);
		}

		l = gtk_label_new (tmp);
		gtk_label_set_line_wrap (GTK_LABEL (l), FALSE);
		gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (w), l, FALSE, FALSE, 0);
		g_free (tmp);
	}

	e_cal_component_get_dtstart (new_comp, &dtstart);
	e_cal_component_get_due (new_comp, &dtdue);

	default_zone = e_cal_model_get_timezone (model);

	if (dtstart.tzid) {
		zone = icalcomponent_get_timezone (e_cal_component_get_icalcomponent (new_comp), dtstart.tzid);
		if (!zone)
			e_cal_get_timezone (
				comp_data->client, dtstart.tzid, &zone, NULL);
		if (!zone)
			zone = default_zone;
	} else {
		zone = NULL;
	}

	tmp2 = g_string_new ("");
	use_24_hour_format = e_cal_model_get_use_24_hour_format (model);

	if (dtstart.value) {
		buff[0] = 0;

		tmp_tm = icaltimetype_to_tm_with_zone (
			dtstart.value, zone, default_zone);
		e_time_format_date_and_time (
			&tmp_tm, use_24_hour_format,
			FALSE, FALSE, buff, 1000);

		if (buff [0]) {
			g_string_append (tmp2, _("Start: "));
			g_string_append (tmp2, buff);
		}
	}

	if (dtdue.value) {
		buff[0] = 0;

		tmp_tm = icaltimetype_to_tm_with_zone (
			dtdue.value, zone, default_zone);
		e_time_format_date_and_time (
			&tmp_tm, use_24_hour_format,
			FALSE, FALSE, buff, 1000);

		if (buff [0]) {
			if (tmp2->len)
				g_string_append (tmp2, "; ");

			g_string_append (tmp2, _("Due: "));
			g_string_append (tmp2, buff);
		}
	}

	if (tmp2->len) {
		l = gtk_label_new (tmp2->str);
		gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (w), l, FALSE, FALSE, 0);
	}

	g_string_free (tmp2, TRUE);

	e_cal_component_free_datetime (&dtstart);
	e_cal_component_free_datetime (&dtdue);

	tmp2 = g_string_new ("");
	e_cal_component_get_description_list (new_comp, &desc);
	for (len = 0, p = desc; p != NULL; p = p->next) {
		ECalComponentText *text = p->data;

		if (text->value != NULL) {
			len += strlen (text->value);
			g_string_append (tmp2, text->value);
			if (len > 1024) {
				g_string_set_size (tmp2, 1020);
				g_string_append (tmp2, "...");
				break;
			}
		}
	}
	e_cal_component_free_text_list (desc);

	if (tmp2->len) {
		l = gtk_label_new (tmp2->str);
		gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (box), l, FALSE, FALSE, 0);
	}

	g_string_free (tmp2, TRUE);

	gtk_widget_show_all (box);
	gtk_tooltip_set_custom (tooltip, box);

	g_object_unref (new_comp);

	return TRUE;
}

static gboolean
memo_table_popup_menu_cb (EMemoTable *memo_table)
{
	memo_table_emit_popup_event (memo_table, NULL);

	return TRUE;
}

static gint
memo_table_right_click_cb (EMemoTable *memo_table,
                           gint row,
                           gint col,
                           GdkEvent *event)
{
	memo_table_emit_popup_event (memo_table, event);

	return TRUE;
}

static void
memo_table_set_model (EMemoTable *memo_table,
                      ECalModel *model)
{
	g_return_if_fail (memo_table->priv->model == NULL);

	memo_table->priv->model = g_object_ref (model);

	g_signal_connect_swapped (
		model, "row-appended",
		G_CALLBACK (memo_table_emit_user_created), memo_table);

	g_signal_connect_swapped (
		model, "cal-view-progress",
		G_CALLBACK (memo_table_model_cal_view_progress_cb),
		memo_table);

	g_signal_connect_swapped (
		model, "cal-view-done",
		G_CALLBACK (memo_table_model_cal_view_done_cb),
		memo_table);
}

static void
memo_table_set_shell_view (EMemoTable *memo_table,
                           EShellView *shell_view)
{
	g_return_if_fail (memo_table->priv->shell_view == NULL);

	memo_table->priv->shell_view = shell_view;

	g_object_add_weak_pointer (
		G_OBJECT (shell_view),
		&memo_table->priv->shell_view);
}

static void
memo_table_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MODEL:
			memo_table_set_model (
				E_MEMO_TABLE (object),
				g_value_get_object (value));
			return;

		case PROP_SHELL_VIEW:
			memo_table_set_shell_view (
				E_MEMO_TABLE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
memo_table_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MODEL:
			g_value_set_object (
				value, e_memo_table_get_model (
				E_MEMO_TABLE (object)));
			return;

		case PROP_SHELL_VIEW:
			g_value_set_object (
				value, e_memo_table_get_shell_view (
				E_MEMO_TABLE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
memo_table_dispose (GObject *object)
{
	EMemoTablePrivate *priv;

	priv = E_MEMO_TABLE_GET_PRIVATE (object);

	if (priv->shell_view != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell_view), &priv->shell_view);
		priv->shell_view = NULL;
	}

	if (priv->model != NULL) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
memo_table_constructed (GObject *object)
{
	EMemoTable *memo_table;
	GtkWidget *widget;
	ECalModel *model;
	ETable *table;
	ECell *cell, *popup_cell;
	ETableExtras *extras;
	gint i;
	AtkObject *a11y;
	gchar *etspecfile;

	memo_table = E_MEMO_TABLE (object);
	model = e_memo_table_get_model (memo_table);

	/* Create the header columns */

	extras = e_table_extras_new ();

	/*
	 * Normal string fields.
	 */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (cell, "bg_color_column", E_CAL_MODEL_FIELD_COLOR, NULL);
	e_table_extras_add_cell (extras, "calstring", cell);

	/*
	 * Date fields.
	 */
	cell = e_cell_date_edit_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (cell, "bg_color_column", E_CAL_MODEL_FIELD_COLOR, NULL);

	e_mutual_binding_new (
		model, "timezone",
		cell, "timezone");

	e_mutual_binding_new (
		model, "use-24-hour-format",
		cell, "use-24-hour-format");

	popup_cell = e_cell_date_edit_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	e_mutual_binding_new (
		model, "use-24-hour-format",
		popup_cell, "use-24-hour-format");

	e_table_extras_add_cell (extras, "dateedit", popup_cell);
	memo_table->dates_cell = E_CELL_DATE_EDIT (popup_cell);

	e_cell_date_edit_set_get_time_callback (
		E_CELL_DATE_EDIT (popup_cell),
		e_memo_table_get_current_time, memo_table, NULL);

	/* Sorting */
	e_table_extras_add_compare (
		extras, "date-compare", memo_table_date_compare_cb);

	/* Create pixmaps */

	if (!icon_pixbufs[0])
		for (i = 0; i < NUM_ICONS; i++) {
			icon_pixbufs[i] = e_icon_factory_get_icon (icon_names[i], GTK_ICON_SIZE_MENU);
		}

	cell = e_cell_toggle_new (0, NUM_ICONS, icon_pixbufs);
	e_table_extras_add_cell (extras, "icon", cell);
	e_table_extras_add_pixbuf (extras, "icon", icon_pixbufs[0]);

	/* set proper format component for a default 'date' cell renderer */
	cell = e_table_extras_get_cell (extras, "date");
	e_cell_date_set_format_component (E_CELL_DATE (cell), "calendar");

	/* set proper format component for a default 'date' cell renderer */
	cell = e_table_extras_get_cell (extras, "date");
	e_cell_date_set_format_component (E_CELL_DATE (cell), "calendar");

	/* Create the table */

	etspecfile = g_build_filename (
		EVOLUTION_ETSPECDIR, "e-memo-table.etspec", NULL);
	widget = e_table_scrolled_new_from_spec_file (
		E_TABLE_MODEL (model), extras, etspecfile, NULL);
	gtk_table_attach (
		GTK_TABLE (memo_table), widget, 0, 1, 0, 1,
		GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	memo_table->etable = widget;
	gtk_widget_show (widget);
	g_free (etspecfile);

	table = e_table_scrolled_get_table (E_TABLE_SCROLLED (widget));
	g_signal_connect_swapped (
		table, "double-click",
		G_CALLBACK (memo_table_double_click_cb), memo_table);
	g_signal_connect_swapped (
		table, "query-tooltip",
		G_CALLBACK (memo_table_query_tooltip_cb), memo_table);
	g_signal_connect_swapped (
		table, "popup-menu",
		G_CALLBACK (memo_table_popup_menu_cb), memo_table);
	g_signal_connect_swapped (
		table, "right-click",
		G_CALLBACK (memo_table_right_click_cb), memo_table);
	gtk_widget_set_has_tooltip (GTK_WIDGET (table), TRUE);

	a11y = gtk_widget_get_accessible (GTK_WIDGET (table));
	if (a11y)
		atk_object_set_name (a11y, _("Memos"));
}

static void
memo_table_class_init (EMemoTableClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMemoTablePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = memo_table_set_property;
	object_class->get_property = memo_table_get_property;
	object_class->dispose = memo_table_dispose;
	object_class->constructed = memo_table_constructed;

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			_("Model"),
			NULL,
			E_TYPE_CAL_MODEL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			_("Shell View"),
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[OPEN_COMPONENT] = g_signal_new (
		"open-component",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMemoTableClass, open_component),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL_MODEL_COMPONENT);

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMemoTableClass, popup_event),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMemoTableClass, status_message),
		NULL, NULL,
		e_marshal_VOID__STRING_DOUBLE,
		G_TYPE_NONE, 2,
		G_TYPE_STRING, G_TYPE_DOUBLE);

	signals[USER_CREATED] = g_signal_new (
		"user-created",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMemoTableClass, user_created),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
memo_table_init (EMemoTable *memo_table)
{
	memo_table->priv = E_MEMO_TABLE_GET_PRIVATE (memo_table);
}

GType
e_memo_table_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMemoTableClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) memo_table_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMemoTable),
			0,     /* n_preallocs */
			(GInstanceInitFunc) memo_table_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_TABLE, "EMemoTable", &type_info, 0);
	}

	return type;
}

/**
 * e_memo_table_new:
 * @shell_view: an #EShellView
 * @model: an #ECalModel for the table
 *
 * Returns a new #EMemoTable.
 *
 * Returns: a new #EMemoTable
 **/
GtkWidget *
e_memo_table_new (EShellView *shell_view,
                  ECalModel *model)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return g_object_new (
		E_TYPE_MEMO_TABLE,
		"model", model, "shell-view", shell_view, NULL);
}

/**
 * e_memo_table_get_model:
 * @memo_table: A calendar table.
 *
 * Queries the calendar data model that a calendar table is using.
 *
 * Return value: A memo model.
 **/
ECalModel *
e_memo_table_get_model (EMemoTable *memo_table)
{
	g_return_val_if_fail (memo_table != NULL, NULL);
	g_return_val_if_fail (E_IS_MEMO_TABLE (memo_table), NULL);

	return memo_table->priv->model;
}

/**
 * e_memo_table_get_table:
 * @memo_table: A calendar table.
 *
 * Queries the #ETable widget that the calendar table is using.
 *
 * Return value: The #ETable widget that the calendar table uses to display its
 * data.
 **/
ETable *
e_memo_table_get_table (EMemoTable *memo_table)
{
	ETableScrolled *table_scrolled;

	g_return_val_if_fail (E_IS_MEMO_TABLE (memo_table), NULL);

	table_scrolled = E_TABLE_SCROLLED (memo_table->etable);

	return e_table_scrolled_get_table (table_scrolled);
}

EShellView *
e_memo_table_get_shell_view (EMemoTable *memo_table)
{
	g_return_val_if_fail (E_IS_MEMO_TABLE (memo_table), NULL);

	return memo_table->priv->shell_view;
}

/* Used from e_table_selected_row_foreach(); puts the selected row number in an
 * gint pointed to by the closure data.
 */
static void
get_selected_row_cb (gint model_row, gpointer data)
{
	gint *row;

	row = data;
	*row = model_row;
}

/*
 * Returns the component that is selected in the table; only works if there is
 * one and only one selected row.
 */
static ECalModelComponent *
get_selected_comp (EMemoTable *memo_table)
{
	ECalModel *model;
	ETable *etable;
	gint row;

	model = e_memo_table_get_model (memo_table);
	etable = e_memo_table_get_table (memo_table);
	if (e_table_selected_count (etable) != 1)
		return NULL;

	row = -1;
	e_table_selected_row_foreach (etable,
				      get_selected_row_cb,
				      &row);
	g_return_val_if_fail (row != -1, NULL);

	return e_cal_model_get_component_at (model, row);
}

struct get_selected_uids_closure {
	EMemoTable *memo_table;
	GSList *objects;
};

/* Used from e_table_selected_row_foreach(), builds a list of the selected UIDs */
static void
add_uid_cb (gint model_row, gpointer data)
{
	struct get_selected_uids_closure *closure;
	ECalModelComponent *comp_data;
	ECalModel *model;

	closure = data;

	model = e_memo_table_get_model (closure->memo_table);
	comp_data = e_cal_model_get_component_at (model, model_row);

	closure->objects = g_slist_prepend (closure->objects, comp_data);
}

/* Deletes all of the selected components in the table */
static void
delete_selected_components (EMemoTable *memo_table)
{
	GSList *objs, *l;
	const gchar *status_message;

	objs = e_memo_table_get_selected (memo_table);

	status_message = _("Deleting selected objects");
	memo_table_emit_status_message (memo_table, status_message, -1.0);

	for (l = objs; l; l = l->next) {
		ECalModelComponent *comp_data = (ECalModelComponent *) l->data;
		GError *error = NULL;

		e_cal_remove_object (comp_data->client,
				     icalcomponent_get_uid (comp_data->icalcomp), &error);
		delete_error_dialog (error, E_CAL_COMPONENT_JOURNAL);
		g_clear_error (&error);
	}

	memo_table_emit_status_message (memo_table, NULL, -1.0);

	g_slist_free (objs);
}

/**
 * e_memo_table_delete_selected:
 * @memo_table: A memo table.
 *
 * Deletes the selected components in the table; asks the user first.
 **/
void
e_memo_table_delete_selected (EMemoTable *memo_table)
{
	ETable *etable;
	gint n_selected;
	ECalModelComponent *comp_data;
	ECalComponent *comp = NULL;

	g_return_if_fail (memo_table != NULL);
	g_return_if_fail (E_IS_MEMO_TABLE (memo_table));

	etable = e_memo_table_get_table (memo_table);

	n_selected = e_table_selected_count (etable);
	if (n_selected <= 0)
		return;

	if (n_selected == 1)
		comp_data = get_selected_comp (memo_table);
	else
		comp_data = NULL;

	/* FIXME: this may be something other than a TODO component */

	if (comp_data) {
		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
	}

	if (delete_component_dialog (comp, FALSE, n_selected, E_CAL_COMPONENT_JOURNAL,
				     GTK_WIDGET (memo_table)))
		delete_selected_components (memo_table);

	/* free memory */
	if (comp)
		g_object_unref (comp);
}

/**
 * e_memo_table_get_selected:
 * @memo_table:
 *
 * Get the currently selected ECalModelComponent's on the table.
 *
 * Return value: A GSList of the components, which should be
 * g_slist_free'd when finished with.
 **/
GSList *
e_memo_table_get_selected (EMemoTable *memo_table)
{
	struct get_selected_uids_closure closure;
	ETable *etable;

	closure.memo_table = memo_table;
	closure.objects = NULL;

	etable = e_memo_table_get_table (memo_table);
	e_table_selected_row_foreach (etable, add_uid_cb, &closure);

	return closure.objects;
}

/**
 * e_memo_table_cut_clipboard:
 * @memo_table: A calendar table.
 *
 * Cuts selected tasks in the given calendar table
 */
void
e_memo_table_cut_clipboard (EMemoTable *memo_table)
{
	g_return_if_fail (E_IS_MEMO_TABLE (memo_table));

	e_memo_table_copy_clipboard (memo_table);
	delete_selected_components (memo_table);
}

/* callback for e_table_selected_row_foreach */
static void
copy_row_cb (gint model_row, gpointer data)
{
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	ECalModel *model;
	gchar *comp_str;
	icalcomponent *child;

	memo_table = E_MEMO_TABLE (data);

	g_return_if_fail (memo_table->tmp_vcal != NULL);

	model = e_memo_table_get_model (memo_table);
	comp_data = e_cal_model_get_component_at (model, model_row);
	if (!comp_data)
		return;

	/* add timezones to the VCALENDAR component */
	e_cal_util_add_timezones_from_component (memo_table->tmp_vcal, comp_data->icalcomp);

	/* add the new component to the VCALENDAR component */
	comp_str = icalcomponent_as_ical_string_r (comp_data->icalcomp);
	child = icalparser_parse_string (comp_str);
	if (child) {
		icalcomponent_add_component (memo_table->tmp_vcal,
					     icalcomponent_new_clone (child));
		icalcomponent_free (child);
	}
	g_free (comp_str);
}

/**
 * e_memo_table_copy_clipboard:
 * @memo_table: A calendar table.
 *
 * Copies selected tasks into the clipboard
 */
void
e_memo_table_copy_clipboard (EMemoTable *memo_table)
{
	ETable *etable;
	GtkClipboard *clipboard;
	gchar *comp_str;

	g_return_if_fail (E_IS_MEMO_TABLE (memo_table));

	/* create temporary VCALENDAR object */
	memo_table->tmp_vcal = e_cal_util_new_top_level ();

	etable = e_memo_table_get_table (memo_table);
	e_table_selected_row_foreach (etable, copy_row_cb, memo_table);
	comp_str = icalcomponent_as_ical_string_r (memo_table->tmp_vcal);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	e_clipboard_set_calendar (clipboard, comp_str, -1);
	gtk_clipboard_store (clipboard);

	/* free memory */
	icalcomponent_free (memo_table->tmp_vcal);
	g_free (comp_str);
	memo_table->tmp_vcal = NULL;
}

static void
clipboard_get_calendar_data (EMemoTable *memo_table, const gchar *text)
{
	icalcomponent *icalcomp;
	gchar *uid;
	ECalComponent *comp;
	ECal *client;
	ECalModel *model;
	icalcomponent_kind kind;
	const gchar *status_message;

	g_return_if_fail (E_IS_MEMO_TABLE (memo_table));

	if (!text || !*text)
		return;

	icalcomp = icalparser_parse_string (text);
	if (!icalcomp)
		return;

	/* check the type of the component */
	kind = icalcomponent_isa (icalcomp);
	if (kind != ICAL_VCALENDAR_COMPONENT &&
	    kind != ICAL_VEVENT_COMPONENT &&
	    kind != ICAL_VTODO_COMPONENT &&
	    kind != ICAL_VJOURNAL_COMPONENT) {
		return;
	}

	model = e_memo_table_get_model (memo_table);
	client = e_cal_model_get_default_client (model);

	status_message = _("Updating objects");
	memo_table_emit_status_message (memo_table, status_message, -1.0);

	if (kind == ICAL_VCALENDAR_COMPONENT) {
		icalcomponent_kind child_kind;
		icalcomponent *subcomp;
		icalcomponent *vcal_comp;

		vcal_comp = icalcomp;
		subcomp = icalcomponent_get_first_component (
			vcal_comp, ICAL_ANY_COMPONENT);
		while (subcomp) {
			child_kind = icalcomponent_isa (subcomp);
			if (child_kind == ICAL_VEVENT_COMPONENT ||
			    child_kind == ICAL_VTODO_COMPONENT ||
			    child_kind == ICAL_VJOURNAL_COMPONENT) {
				ECalComponent *tmp_comp;

				uid = e_cal_component_gen_uid ();
				tmp_comp = e_cal_component_new ();
				e_cal_component_set_icalcomponent (
					tmp_comp, icalcomponent_new_clone (subcomp));
				e_cal_component_set_uid (tmp_comp, uid);
				free (uid);

				/* FIXME should we convert start/due/complete times? */
				/* FIXME Error handling */
				e_cal_create_object (client, e_cal_component_get_icalcomponent (tmp_comp), NULL, NULL);

				g_object_unref (tmp_comp);
			}
			subcomp = icalcomponent_get_next_component (
				vcal_comp, ICAL_ANY_COMPONENT);
		}
	} else {
		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomp);
		uid = e_cal_component_gen_uid ();
		e_cal_component_set_uid (comp, (const gchar *) uid);
		free (uid);

		e_cal_create_object (client, e_cal_component_get_icalcomponent (comp), NULL, NULL);

		g_object_unref (comp);
	}

	memo_table_emit_status_message (memo_table, NULL, -1.0);
}

/**
 * e_memo_table_paste_clipboard:
 * @memo_table: A calendar table.
 *
 * Pastes tasks currently in the clipboard into the given calendar table
 */
void
e_memo_table_paste_clipboard (EMemoTable *memo_table)
{
	GtkClipboard *clipboard;
	GnomeCanvasItem *item;
	ETable *etable;

	g_return_if_fail (E_IS_MEMO_TABLE (memo_table));

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	etable = e_memo_table_get_table (memo_table);
	item = GNOME_CANVAS (etable->table_canvas)->focused_item;

	/* Paste text into a cell being edited. */
	if (gtk_clipboard_wait_is_text_available (clipboard) &&
		GTK_WIDGET_HAS_FOCUS (etable->table_canvas) &&
		E_IS_TABLE_ITEM (item) &&
		E_TABLE_ITEM (item)->editing_col >= 0 &&
		E_TABLE_ITEM (item)->editing_row >= 0) {

		ETableItem *etable_item = E_TABLE_ITEM (item);

		e_cell_text_paste_clipboard (
			etable_item->cell_views[etable_item->editing_col],
			etable_item->editing_col,
			etable_item->editing_row);

	/* Paste iCalendar data into the table. */
	} else if (e_clipboard_wait_is_calendar_available (clipboard)) {
		gchar *calendar_source;

		calendar_source = e_clipboard_wait_for_calendar (clipboard);
		clipboard_get_calendar_data (memo_table, calendar_source);
		g_free (calendar_source);
	}
}

/* Loads the state of the table (headers shown etc.) from the given file. */
void
e_memo_table_load_state	(EMemoTable *memo_table,
                         const gchar *filename)
{
	ETable *table;

	g_return_if_fail (E_IS_MEMO_TABLE (memo_table));
	g_return_if_fail (filename != NULL);

	table = e_memo_table_get_table (memo_table);
	e_table_load_state (table, filename);
}

/* Saves the state of the table (headers shown etc.) to the given file. */
void
e_memo_table_save_state (EMemoTable *memo_table,
                         const gchar *filename)
{
	ETable *table;

	g_return_if_fail (E_IS_MEMO_TABLE (memo_table));
	g_return_if_fail (filename != NULL);

	table = e_memo_table_get_table (memo_table);
	e_table_save_state (table, filename);
}

/* Returns the current time, for the ECellDateEdit items.
   FIXME: Should probably use the timezone of the item rather than the
   current timezone, though that may be difficult to get from here. */
static struct tm
e_memo_table_get_current_time (ECellDateEdit *ecde, gpointer data)
{
	EMemoTable *memo_table = data;
	ECalModel *model;
	icaltimezone *zone;
	struct tm tmp_tm = { 0 };
	struct icaltimetype tt;

	/* Get the current timezone. */
	model = e_memo_table_get_model (memo_table);
	zone = e_cal_model_get_timezone (model);

	tt = icaltime_from_timet_with_zone (time (NULL), FALSE, zone);

	/* Now copy it to the struct tm and return it. */
	tmp_tm.tm_year  = tt.year - 1900;
	tmp_tm.tm_mon   = tt.month - 1;
	tmp_tm.tm_mday  = tt.day;
	tmp_tm.tm_hour  = tt.hour;
	tmp_tm.tm_min   = tt.minute;
	tmp_tm.tm_sec   = tt.second;
	tmp_tm.tm_isdst = -1;

	return tmp_tm;
}
