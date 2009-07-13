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

#include <config.h>
#include <widgets/e-timezone-dialog/e-timezone-dialog.h>
#include <glib/gi18n.h>
#include "e-timezone-entry.h"

struct _ETimezoneEntryPrivate {
	/* The current timezone, set in e_timezone_entry_set_timezone()
	   or from the timezone dialog. Note that we don't copy it or
	   use a ref count - we assume it is never destroyed for the
	   lifetime of this widget. */
	icaltimezone *zone;

	/* This can be set to the default timezone. If the current timezone
	   setting in the ETimezoneEntry matches this, then the entry field
	   is hidden. This makes the user interface simpler. */
	icaltimezone *default_zone;

	GtkWidget *entry;
	GtkWidget *button;
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static void e_timezone_entry_destroy	(GtkObject	*object);

static gboolean e_timezone_entry_mnemonic_activate (GtkWidget *widget,
                                                    gboolean   group_cycling);
static gboolean e_timezone_entry_focus (GtkWidget *widget,
					GtkDirectionType direction);
static void on_entry_changed		(GtkEntry	*entry,
					 ETimezoneEntry *tentry);
static void on_button_clicked		(GtkWidget	*widget,
					 ETimezoneEntry	*tentry);
static void add_relation		(ETimezoneEntry *tentry,
					 GtkWidget *widget);

static void e_timezone_entry_set_entry  (ETimezoneEntry *tentry);

static guint timezone_entry_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ETimezoneEntry, e_timezone_entry, GTK_TYPE_HBOX)

static void
e_timezone_entry_class_init		(ETimezoneEntryClass	*class)
{
	GtkObjectClass *object_class = (GtkObjectClass *) class;
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	object_class = (GtkObjectClass*) class;

	widget_class->mnemonic_activate = e_timezone_entry_mnemonic_activate;
	widget_class->focus = e_timezone_entry_focus;
	timezone_entry_signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETimezoneEntryClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	object_class->destroy		= e_timezone_entry_destroy;

	class->changed = NULL;
}

static void
e_timezone_entry_init		(ETimezoneEntry	*tentry)
{
	ETimezoneEntryPrivate *priv;
	AtkObject *a11y;

	tentry->priv = priv = g_new0 (ETimezoneEntryPrivate, 1);

	priv->zone = NULL;
	priv->default_zone = NULL;

	priv->entry  = gtk_entry_new ();
	gtk_editable_set_editable (GTK_EDITABLE (priv->entry), FALSE);
	/*gtk_widget_set_usize (priv->date_entry, 90, 0);*/
	gtk_box_pack_start (GTK_BOX (tentry), priv->entry, TRUE, TRUE, 0);
	gtk_widget_show (priv->entry);
	g_signal_connect (priv->entry, "changed", G_CALLBACK (on_entry_changed), tentry);

	priv->button = gtk_button_new_with_label (_("Select..."));
	g_signal_connect (priv->button, "clicked", G_CALLBACK (on_button_clicked), tentry);
	gtk_box_pack_start (GTK_BOX (tentry), priv->button, FALSE, FALSE, 6);
	gtk_widget_show (priv->button);
	a11y = gtk_widget_get_accessible (priv->button);
	if (a11y != NULL) {
		atk_object_set_name (a11y, _("Select Timezone"));
	}
}

/**
 * e_timezone_entry_new:
 *
 * Description: Creates a new #ETimezoneEntry widget which can be used
 * to provide an easy to use way for entering dates and times.
 *
 * Returns: a new #ETimezoneEntry widget.
 */
GtkWidget *
e_timezone_entry_new			(void)
{
	ETimezoneEntry *tentry;

	tentry = g_object_new (e_timezone_entry_get_type (), NULL);

	GTK_WIDGET_SET_FLAGS (GTK_WIDGET(tentry), GTK_CAN_FOCUS);

	return GTK_WIDGET (tentry);
}

static void
e_timezone_entry_destroy		(GtkObject	*object)
{
	ETimezoneEntry *tentry;

	g_return_if_fail (E_IS_TIMEZONE_ENTRY (object));

	tentry = E_TIMEZONE_ENTRY (object);

	g_free (tentry->priv);
	tentry->priv = NULL;

	if (GTK_OBJECT_CLASS (e_timezone_entry_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (e_timezone_entry_parent_class)->destroy) (object);
}

/* The arrow button beside the date field has been clicked, so we show the
   popup with the ECalendar in. */
static void
on_button_clicked		(GtkWidget	*widget,
				 ETimezoneEntry	*tentry)
{
	ETimezoneEntryPrivate *priv;
	ETimezoneDialog *timezone_dialog;
	GtkWidget *dialog;

	priv = tentry->priv;

	timezone_dialog = e_timezone_dialog_new ();

	e_timezone_dialog_set_timezone (timezone_dialog, priv->zone);

	dialog = e_timezone_dialog_get_toplevel (timezone_dialog);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		priv->zone = e_timezone_dialog_get_timezone (timezone_dialog);
		e_timezone_entry_set_entry (tentry);
	}

	g_object_unref (timezone_dialog);
}

static void
on_entry_changed			(GtkEntry	*entry,
					 ETimezoneEntry *tentry)
{
	g_signal_emit (GTK_OBJECT (tentry), timezone_entry_signals[CHANGED], 0);
}

icaltimezone*
e_timezone_entry_get_timezone		(ETimezoneEntry	*tentry)
{
	ETimezoneEntryPrivate *priv;

	g_return_val_if_fail (E_IS_TIMEZONE_ENTRY (tentry), NULL);

	priv = tentry->priv;

	return priv->zone;
}

static void
add_relation				(ETimezoneEntry *tentry,
					 GtkWidget *widget)
{
	AtkObject *a11ytentry, *a11yWidget;
	AtkRelationSet *set;
	AtkRelation *relation;
	GPtrArray *target;
	gpointer target_object;

	/* add a labelled_by relation for widget for accessibility */

	a11ytentry = gtk_widget_get_accessible (GTK_WIDGET (tentry));
	a11yWidget = gtk_widget_get_accessible (widget);

	set = atk_object_ref_relation_set (a11yWidget);
	if (set != NULL) {
		relation = atk_relation_set_get_relation_by_type (set,
					ATK_RELATION_LABELLED_BY);
		/* check whether has a labelled_by relation already */
		if (relation != NULL)
			return;
	}

	set = atk_object_ref_relation_set (a11ytentry);
	if (!set)
		return;

	relation = atk_relation_set_get_relation_by_type (set,
					ATK_RELATION_LABELLED_BY);
	if (relation != NULL) {
		target = atk_relation_get_target (relation);
		target_object = g_ptr_array_index (target, 0);
		if (ATK_IS_OBJECT (target_object)) {
			atk_object_add_relationship (a11yWidget,
					ATK_RELATION_LABELLED_BY,
					ATK_OBJECT (target_object));
		}
	}
}

void
e_timezone_entry_set_timezone		(ETimezoneEntry	*tentry,
					 icaltimezone	*zone)
{
	ETimezoneEntryPrivate *priv;

	g_return_if_fail (E_IS_TIMEZONE_ENTRY (tentry));

	priv = tentry->priv;

	priv->zone = zone;

	e_timezone_entry_set_entry (tentry);

	add_relation (tentry, priv->entry);
}

/* Sets the default timezone. If the current timezone matches this, then the
   entry field is hidden. This is useful since most people do not use timezones
   so it makes the user interface simpler. */
void
e_timezone_entry_set_default_timezone	(ETimezoneEntry	*tentry,
					 icaltimezone	*zone)
{
	ETimezoneEntryPrivate *priv;

	g_return_if_fail (E_IS_TIMEZONE_ENTRY (tentry));

	priv = tentry->priv;

	priv->default_zone = zone;

	e_timezone_entry_set_entry (tentry);
}

static void
e_timezone_entry_set_entry (ETimezoneEntry *tentry)
{
	ETimezoneEntryPrivate *priv;
	const gchar *display_name;
	gchar *name_buffer;

	priv = tentry->priv;

	if (priv->zone) {
		display_name = icaltimezone_get_display_name (priv->zone);

		/* We check if it is one of our builtin timezone
		   names, in which case we call gettext to translate
		   it. If it isn't a builtin timezone name, we
		   don't. */
		if (icaltimezone_get_builtin_timezone (display_name))
			display_name = _(display_name);
	} else
		display_name = "";

	name_buffer = g_strdup (display_name);

	gtk_entry_set_text (GTK_ENTRY (priv->entry), name_buffer);

	/* do we need to hide the timezone entry at all? i know this overrules the previous case of hiding the timezone
	 * entry field when we select the default timezone
         */
	gtk_widget_show (priv->entry);

	g_free (name_buffer);
}

static gboolean
e_timezone_entry_mnemonic_activate (GtkWidget *widget,
                                    gboolean   group_cycling)
{
        GtkButton *button = NULL;

        if (GTK_WIDGET_CAN_FOCUS (widget)) {
                button = GTK_BUTTON (((ETimezoneEntryPrivate *) ((ETimezoneEntry *) widget)->priv)->button);
                if (button != NULL)
                        gtk_widget_grab_focus (GTK_WIDGET (button));
        }

        return TRUE;
}

static gboolean
e_timezone_entry_focus (GtkWidget *widget, GtkDirectionType direction)
{
	ETimezoneEntry *tentry;

	tentry = E_TIMEZONE_ENTRY (widget);

	if (direction == GTK_DIR_TAB_FORWARD) {
		if (GTK_WIDGET_HAS_FOCUS (tentry->priv->entry))
			gtk_widget_grab_focus (tentry->priv->button);
		else if (GTK_WIDGET_HAS_FOCUS (tentry->priv->button))
			return FALSE;
		else if (GTK_WIDGET_VISIBLE (tentry->priv->entry))
			gtk_widget_grab_focus (tentry->priv->entry);
		else
			gtk_widget_grab_focus (tentry->priv->button);
	} else if (direction == GTK_DIR_TAB_BACKWARD) {
		if (GTK_WIDGET_HAS_FOCUS (tentry->priv->entry))
			return FALSE;
		else if (GTK_WIDGET_HAS_FOCUS (tentry->priv->button)) {
			if (GTK_WIDGET_VISIBLE (tentry->priv->entry))
				gtk_widget_grab_focus (tentry->priv->entry);
			else
				return FALSE;
		} else
			gtk_widget_grab_focus (tentry->priv->button);
	} else
		return FALSE;
	return TRUE;
}

