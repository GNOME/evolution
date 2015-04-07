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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-timezone-entry.h"

#include <glib/gi18n.h>

#include "e-util/e-util.h"

#define E_TIMEZONE_ENTRY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TIMEZONE_ENTRY, ETimezoneEntryPrivate))

struct _ETimezoneEntryPrivate {
	/* The current timezone, set in e_timezone_entry_set_timezone()
	 * or from the timezone dialog. Note that we don't copy it or
	 * use a ref count - we assume it is never destroyed for the
	 * lifetime of this widget. */
	icaltimezone *timezone;

	/* This can be set to the default timezone. If the current timezone
	 * setting in the ETimezoneEntry matches this, then the entry field
	 * is hidden. This makes the user interface simpler. */
	icaltimezone *default_zone;

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

G_DEFINE_TYPE (ETimezoneEntry, e_timezone_entry, GTK_TYPE_BOX)

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
	icaltimezone *timezone;

	timezone = e_timezone_entry_get_timezone (timezone_entry);

	if (timezone != NULL) {
		display_name = icaltimezone_get_display_name (timezone);

		/* We check if it is one of our builtin timezone
		 * names, in which case we call gettext to translate
		 * it. If it isn't a builtin timezone name, we don't. */
		if (icaltimezone_get_builtin_timezone (display_name))
			display_name = _(display_name);
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
		if (relation != NULL)
			return;
	}

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
}

/* The arrow button beside the date field has been clicked, so we show the
 * popup with the ECalendar in. */
static void
timezone_entry_button_clicked_cb (ETimezoneEntry *timezone_entry)
{
	ETimezoneDialog *timezone_dialog;
	GtkWidget *dialog;
	icaltimezone *timezone;

	timezone_dialog = e_timezone_dialog_new ();

	timezone = e_timezone_entry_get_timezone (timezone_entry);
	e_timezone_dialog_set_timezone (timezone_dialog, timezone);

	dialog = e_timezone_dialog_get_toplevel (timezone_dialog);

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
				g_value_get_pointer (value));
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
			g_value_set_pointer (
				value, e_timezone_entry_get_timezone (
				E_TIMEZONE_ENTRY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static gboolean
timezone_entry_mnemonic_activate (GtkWidget *widget,
                                  gboolean group_cycling)
{
	ETimezoneEntryPrivate *priv;

	priv = E_TIMEZONE_ENTRY_GET_PRIVATE (widget);

	if (gtk_widget_get_can_focus (widget)) {
		if (priv->button != NULL)
			gtk_widget_grab_focus (priv->button);
	}

	return TRUE;
}

static gboolean
timezone_entry_focus (GtkWidget *widget,
                      GtkDirectionType direction)
{
	ETimezoneEntryPrivate *priv;

	priv = E_TIMEZONE_ENTRY_GET_PRIVATE (widget);

	if (direction == GTK_DIR_TAB_FORWARD) {
		if (gtk_widget_has_focus (priv->entry))
			gtk_widget_grab_focus (priv->button);
		else if (gtk_widget_has_focus (priv->button))
			return FALSE;
		else if (gtk_widget_get_visible (priv->entry))
			gtk_widget_grab_focus (priv->entry);
		else
			gtk_widget_grab_focus (priv->button);

	} else if (direction == GTK_DIR_TAB_BACKWARD) {
		if (gtk_widget_has_focus (priv->entry))
			return FALSE;
		else if (gtk_widget_has_focus (priv->button)) {
			if (gtk_widget_get_visible (priv->entry))
				gtk_widget_grab_focus (priv->entry);
			else
				return FALSE;
		} else
			gtk_widget_grab_focus (priv->button);
	} else
		return FALSE;

	return TRUE;
}

static void
e_timezone_entry_class_init (ETimezoneEntryClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (ETimezoneEntryPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = timezone_entry_set_property;
	object_class->get_property = timezone_entry_get_property;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->mnemonic_activate = timezone_entry_mnemonic_activate;
	widget_class->focus = timezone_entry_focus;

	g_object_class_install_property (
		object_class,
		PROP_TIMEZONE,
		g_param_spec_pointer (
			"timezone",
			"Timezone",
			NULL,
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

	timezone_entry->priv = E_TIMEZONE_ENTRY_GET_PRIVATE (timezone_entry);

	gtk_widget_set_can_focus (GTK_WIDGET (timezone_entry), TRUE);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (timezone_entry), GTK_ORIENTATION_HORIZONTAL);

	widget = gtk_entry_new ();
	gtk_editable_set_editable (GTK_EDITABLE (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (timezone_entry), widget, TRUE, TRUE, 0);
	timezone_entry->priv->entry = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (timezone_entry_emit_changed), timezone_entry);

	widget = gtk_button_new_with_label (_("Select..."));
	gtk_box_pack_start (GTK_BOX (timezone_entry), widget, FALSE, FALSE, 6);
	timezone_entry->priv->button = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (timezone_entry_button_clicked_cb), timezone_entry);

	a11y = gtk_widget_get_accessible (timezone_entry->priv->button);
	if (a11y != NULL)
		atk_object_set_name (a11y, _("Select Timezone"));
}

GtkWidget *
e_timezone_entry_new (void)
{
	return g_object_new (E_TYPE_TIMEZONE_ENTRY, NULL);
}

icaltimezone *
e_timezone_entry_get_timezone (ETimezoneEntry *timezone_entry)
{
	g_return_val_if_fail (E_IS_TIMEZONE_ENTRY (timezone_entry), NULL);

	return timezone_entry->priv->timezone;
}

void
e_timezone_entry_set_timezone (ETimezoneEntry *timezone_entry,
                               icaltimezone *timezone)
{
	g_return_if_fail (E_IS_TIMEZONE_ENTRY (timezone_entry));

	if (timezone_entry->priv->timezone == timezone)
		return;

	timezone_entry->priv->timezone = timezone;

	timezone_entry_update_entry (timezone_entry);
	timezone_entry_add_relation (timezone_entry);

	g_object_notify (G_OBJECT (timezone_entry), "timezone");
}

/* Sets the default timezone. If the current timezone matches this,
 * then the entry field is hidden. This is useful since most people
 * do not use timezones so it makes the user interface simpler. */
void
e_timezone_entry_set_default_timezone (ETimezoneEntry *timezone_entry,
                                       icaltimezone *timezone)
{
	g_return_if_fail (E_IS_TIMEZONE_ENTRY (timezone_entry));

	timezone_entry->priv->default_zone = timezone;

	timezone_entry_update_entry (timezone_entry);
}
