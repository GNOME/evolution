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
 * ETimezoneEntry - a field for setting a timezone. It shows the timezone in
 * a GtkEntry with a '...' button beside it which shows a dialog for changing
 * the timezone. The dialog contains a map of the world with a point for each
 * timezone, and an option menu as an alternative way of selecting the
 * timezone.
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <libedataserver/libedataserver.h>
#include <libecal/libecal.h>

#include "e-util/e-util.h"

#include "e-timezone-entry.h"

struct _ETimezoneEntryPrivate {
	/* The current timezone, set in e_timezone_entry_set_timezone()
	 * or from the timezone dialog. Note that we don't copy it or
	 * use a ref count - we assume it is never destroyed for the
	 * lifetime of this widget. */
	ICalTimezone *timezone;
	gboolean allow_none;

	GtkWidget *entry;
	GtkWidget *button;
};

enum {
	PROP_0,
	PROP_TIMEZONE
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (ETimezoneEntry, e_timezone_entry, GTK_TYPE_BOX)

static void
timezone_entry_emit_changed (ETimezoneEntry *timezone_entry)
{
	g_signal_emit (timezone_entry, signals[CHANGED], 0);
}

static void
timezone_entry_update_entry (ETimezoneEntry *timezone_entry)
{
	const gchar *display_name;
	gchar *name_buffer;
	ICalTimezone *timezone;

	timezone = e_timezone_entry_get_timezone (timezone_entry);

	if (timezone != NULL) {
		display_name = i_cal_timezone_get_display_name (timezone);

		/* We check if it is one of our builtin timezone
		 * names, in which case we call gettext to translate
		 * it. If it isn't a builtin timezone name, we don't. */
		if (i_cal_timezone_get_builtin_timezone (display_name))
			display_name = _(display_name);
	} else if (timezone_entry->priv->allow_none) {
		display_name = C_("timezone", "None");
	} else
		display_name = "";

	name_buffer = g_strdup (display_name);

	gtk_entry_set_text (GTK_ENTRY (timezone_entry->priv->entry), name_buffer);

	/* XXX Do we need to hide the timezone entry at all?  I know
	 *     this overrules the previous case of hiding the timezone
	 *     entry field when we select the default timezone. */
	gtk_widget_show (timezone_entry->priv->entry);

	g_free (name_buffer);
}
static void
timezone_entry_add_relation (ETimezoneEntry *timezone_entry)
{
	AtkObject *a11y_timezone_entry;
	AtkObject *a11y_widget;
	AtkRelationSet *set;
	AtkRelation *relation;
	GtkWidget *widget;
	GPtrArray *target;
	gpointer target_object;

	/* add a labelled_by relation for widget for accessibility */

	widget = GTK_WIDGET (timezone_entry);
	a11y_timezone_entry = gtk_widget_get_accessible (widget);

	widget = timezone_entry->priv->entry;
	a11y_widget = gtk_widget_get_accessible (widget);

	set = atk_object_ref_relation_set (a11y_widget);
	if (set != NULL) {
		relation = atk_relation_set_get_relation_by_type (
			set, ATK_RELATION_LABELLED_BY);
		/* check whether has a labelled_by relation already */
		if (relation != NULL) {
			g_object_unref (set);
			return;
		}
	}

	g_clear_object (&set);

	set = atk_object_ref_relation_set (a11y_timezone_entry);
	if (!set)
		return;

	relation = atk_relation_set_get_relation_by_type (
		set, ATK_RELATION_LABELLED_BY);
	if (relation != NULL) {
		target = atk_relation_get_target (relation);
		target_object = g_ptr_array_index (target, 0);
		if (ATK_IS_OBJECT (target_object)) {
			atk_object_add_relationship (
				a11y_widget,
				ATK_RELATION_LABELLED_BY,
				ATK_OBJECT (target_object));
		}
	}

	g_clear_object (&set);
}

/* The arrow button beside the date field has been clicked, so we show the
 * popup with the ECalendar in. */
static void
timezone_entry_button_clicked_cb (ETimezoneEntry *timezone_entry)
{
	ETimezoneDialog *timezone_dialog;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	ICalTimezone *timezone;

	timezone_dialog = e_timezone_dialog_new ();
	e_timezone_dialog_set_allow_none (timezone_dialog, e_timezone_entry_get_allow_none (timezone_entry));

	timezone = e_timezone_entry_get_timezone (timezone_entry);
	e_timezone_dialog_set_timezone (timezone_dialog, timezone);

	dialog = e_timezone_dialog_get_toplevel (timezone_dialog);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (timezone_entry));
	if (GTK_IS_WINDOW (toplevel))
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
		goto exit;

	timezone = e_timezone_dialog_get_timezone (timezone_dialog);
	e_timezone_entry_set_timezone (timezone_entry, timezone);
	timezone_entry_update_entry (timezone_entry);

exit:
	g_object_unref (timezone_dialog);
}

static void
timezone_entry_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TIMEZONE:
			e_timezone_entry_set_timezone (
				E_TIMEZONE_ENTRY (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
timezone_entry_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TIMEZONE:
			g_value_set_object (
				value, e_timezone_entry_get_timezone (
				E_TIMEZONE_ENTRY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
timezone_entry_get_finalize (GObject *object)
{
	ETimezoneEntry *tzentry = E_TIMEZONE_ENTRY (object);

	g_clear_object (&tzentry->priv->timezone);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_timezone_entry_parent_class)->finalize (object);
}

static gboolean
timezone_entry_mnemonic_activate (GtkWidget *widget,
                                  gboolean group_cycling)
{
	ETimezoneEntry *self = E_TIMEZONE_ENTRY (widget);

	if (gtk_widget_get_can_focus (widget)) {
		if (self->priv->button != NULL)
			gtk_widget_grab_focus (self->priv->button);
	}

	return TRUE;
}

static gboolean
timezone_entry_focus (GtkWidget *widget,
                      GtkDirectionType direction)
{
	ETimezoneEntry *self = E_TIMEZONE_ENTRY (widget);

	if (direction == GTK_DIR_TAB_FORWARD) {
		if (gtk_widget_has_focus (self->priv->entry))
			gtk_widget_grab_focus (self->priv->button);
		else if (gtk_widget_has_focus (self->priv->button))
			return FALSE;
		else if (gtk_widget_get_visible (self->priv->entry))
			gtk_widget_grab_focus (self->priv->entry);
		else
			gtk_widget_grab_focus (self->priv->button);

	} else if (direction == GTK_DIR_TAB_BACKWARD) {
		if (gtk_widget_has_focus (self->priv->entry))
			return FALSE;
		else if (gtk_widget_has_focus (self->priv->button)) {
			if (gtk_widget_get_visible (self->priv->entry))
				gtk_widget_grab_focus (self->priv->entry);
			else
				return FALSE;
		} else
			gtk_widget_grab_focus (self->priv->button);
	} else
		return FALSE;

	return TRUE;
}

static gboolean
timezone_entry_focus_out_event_cb (GtkWidget *widget,
				   GdkEvent *event,
				   gpointer user_data)
{
	ETimezoneEntry *self = user_data;

	timezone_entry_update_entry (self);

	return FALSE;
}

static gboolean
timezone_entry_completion_match_cb (GtkEntryCompletion *completion,
				    const gchar *key,
				    GtkTreeIter *iter,
				    gpointer user_data)
{
	GtkTreeModel *model = gtk_entry_completion_get_model (completion);
	gchar *value = NULL;
	gboolean match;

	if (!model || !key || !*key)
		return FALSE;

	gtk_tree_model_get (model, iter, gtk_entry_completion_get_text_column (completion), &value, -1);

	if (!value)
		return FALSE;

	match = e_util_utf8_strstrcase (value, key) != NULL;

	g_free (value);

	return match;
}

struct _zone_data {
	const gchar *location;
	ICalTimezone *zone;
};

static void
zone_data_clear (gpointer ptr)
{
	struct _zone_data *zone_data = ptr;

	if (zone_data)
		g_clear_object (&zone_data->zone);
}

static gint
timezone_entry_compare_zone_data (gconstpointer aa,
				  gconstpointer bb)
{
	const struct _zone_data *zda = aa;
	const struct _zone_data *zdb = bb;

	return g_utf8_collate (zda->location, zdb->location);
}

static gboolean
timezone_entry_match_selected_cb (GtkEntryCompletion *completion,
				  GtkTreeModel *model,
				  GtkTreeIter *iter,
				  gpointer user_data)
{
	ETimezoneEntry *self = user_data;
	ICalTimezone *zone = NULL;

	gtk_tree_model_get (model, iter, 1, &zone, -1);

	e_timezone_entry_set_timezone (self, zone);

	g_clear_object (&zone);

	return TRUE;
}

static void
timezone_entry_set_completion (ETimezoneEntry *self)
{
	struct _zone_data *zone_data;
	GtkEntryCompletion *completion;
	GtkListStore *store;
	GtkTreeIter iter;
	ICalArray *zones;
	GSList *sorted_zones = NULL, *link;
	gint ii, sz;

	zones = i_cal_timezone_get_builtin_timezones ();

	sz = i_cal_array_size (zones);
	if (sz <= 0)
		return;

	zone_data = g_new0 (struct _zone_data, sz);

	for (ii = 0; ii < sz; ii++) {
		ICalTimezone *zone;

		zone = i_cal_timezone_array_element_at (zones, ii);
		if (zone) {
			zone_data[ii].location = _(i_cal_timezone_get_location (zone));

			if (!zone_data[ii].location) {
				g_clear_object (&zone);
				continue;
			}

			zone_data[ii].zone = zone;

			sorted_zones = g_slist_prepend (sorted_zones, &(zone_data[ii]));
		}
	}

	sorted_zones = g_slist_sort (sorted_zones, timezone_entry_compare_zone_data);

	store = gtk_list_store_new (2, G_TYPE_STRING, I_CAL_TYPE_TIMEZONE);

	if (self->priv->allow_none) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, C_("timezone", "None"), -1);
	}

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, _("UTC"), 1, i_cal_timezone_get_utc_timezone (), -1);

	for (link = sorted_zones; link; link = g_slist_next (link)) {
		struct _zone_data *zd = link->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, zd->location, 1, zd->zone, -1);
	}

	g_slist_free_full (sorted_zones, zone_data_clear);
	g_free (zone_data);

	completion = gtk_entry_completion_new ();

	gtk_entry_completion_set_text_column (completion, 0);
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
	gtk_entry_completion_set_match_func (completion, timezone_entry_completion_match_cb, NULL, NULL);

	gtk_entry_set_completion (GTK_ENTRY (self->priv->entry), completion);

	g_signal_connect_object (completion, "match-selected",
		G_CALLBACK (timezone_entry_match_selected_cb), self, 0);

	g_clear_object (&completion);
	g_clear_object (&store);
}

static void
e_timezone_entry_class_init (ETimezoneEntryClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = timezone_entry_set_property;
	object_class->get_property = timezone_entry_get_property;
	object_class->finalize = timezone_entry_get_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->mnemonic_activate = timezone_entry_mnemonic_activate;
	widget_class->focus = timezone_entry_focus;

	g_object_class_install_property (
		object_class,
		PROP_TIMEZONE,
		g_param_spec_object (
			"timezone",
			"Timezone",
			NULL,
			I_CAL_TYPE_TIMEZONE,
			G_PARAM_READWRITE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETimezoneEntryClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_timezone_entry_init (ETimezoneEntry *timezone_entry)
{
	AtkObject *a11y;
	GtkWidget *widget;

	timezone_entry->priv = e_timezone_entry_get_instance_private (timezone_entry);
	timezone_entry->priv->allow_none = FALSE;

	gtk_widget_set_can_focus (GTK_WIDGET (timezone_entry), TRUE);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (timezone_entry), GTK_ORIENTATION_HORIZONTAL);

	widget = gtk_entry_new ();
	gtk_editable_set_editable (GTK_EDITABLE (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (timezone_entry), widget, TRUE, TRUE, 0);
	timezone_entry->priv->entry = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (timezone_entry_emit_changed), timezone_entry);

	g_signal_connect_object (widget, "focus-out-event",
		G_CALLBACK (timezone_entry_focus_out_event_cb), timezone_entry, G_CONNECT_AFTER);

	widget = gtk_button_new_with_label (_("Select…"));
	gtk_box_pack_start (GTK_BOX (timezone_entry), widget, FALSE, FALSE, 6);
	timezone_entry->priv->button = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (timezone_entry_button_clicked_cb), timezone_entry);

	a11y = gtk_widget_get_accessible (timezone_entry->priv->button);
	if (a11y != NULL)
		atk_object_set_name (a11y, _("Select Timezone"));

	timezone_entry_set_completion (timezone_entry);
}

GtkWidget *
e_timezone_entry_new (void)
{
	return g_object_new (E_TYPE_TIMEZONE_ENTRY, NULL);
}

ICalTimezone *
e_timezone_entry_get_timezone (ETimezoneEntry *timezone_entry)
{
	g_return_val_if_fail (E_IS_TIMEZONE_ENTRY (timezone_entry), NULL);

	return timezone_entry->priv->timezone;
}

void
e_timezone_entry_set_timezone (ETimezoneEntry *timezone_entry,
			       const ICalTimezone *timezone)
{
	g_return_if_fail (E_IS_TIMEZONE_ENTRY (timezone_entry));

	if (timezone_entry->priv->timezone == timezone)
		return;

	g_clear_object (&timezone_entry->priv->timezone);
	if (timezone)
		timezone_entry->priv->timezone = e_cal_util_copy_timezone (timezone);

	timezone_entry_update_entry (timezone_entry);
	timezone_entry_add_relation (timezone_entry);

	g_object_notify (G_OBJECT (timezone_entry), "timezone");
}

gboolean
e_timezone_entry_get_allow_none (ETimezoneEntry *timezone_entry)
{
	g_return_val_if_fail (E_IS_TIMEZONE_ENTRY (timezone_entry), FALSE);

	return timezone_entry->priv->allow_none;
}

void
e_timezone_entry_set_allow_none (ETimezoneEntry *timezone_entry,
				 gboolean allow_none)
{
	GtkEntryCompletion *completion;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_TIMEZONE_ENTRY (timezone_entry));

	if ((timezone_entry->priv->allow_none ? 1 : 0) == (allow_none ? 1 : 0))
		return;

	timezone_entry->priv->allow_none = allow_none;

	completion = gtk_entry_get_completion (GTK_ENTRY (timezone_entry->priv->entry));
	if (!completion)
		return;

	model = gtk_entry_completion_get_model (completion);
	if (!model)
		return;

	if (allow_none) {
		gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, C_("timezone", "None"), -1);
	} else if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			ICalTimezone *zone = NULL;

			gtk_tree_model_get (model, &iter, 1, &zone, -1);

			if (zone) {
				g_clear_object (&zone);
			} else {
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				break;
			}
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}
