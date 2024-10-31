/*
 * SPDX-FileCopyrightText: (C) 2021 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "e-estimated-duration-entry.h"

struct _EEstimatedDurationEntryPrivate {
	ICalDuration *value;

	GtkWidget *popover;
	GtkWidget *days_spin;
	GtkWidget *hours_spin;
	GtkWidget *minutes_spin;
	GtkWidget *set_button;
	GtkWidget *unset_button;
	GtkSizeGroup *size_group;

	GtkWidget *entry;
	GtkWidget *button;
};

enum {
	PROP_0,
	PROP_VALUE
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EEstimatedDurationEntry, e_estimated_duration_entry, GTK_TYPE_BOX)

static void
estimated_duration_entry_emit_changed (EEstimatedDurationEntry *self)
{
	g_signal_emit (self, signals[CHANGED], 0);
}

static void
estimated_duration_entry_update_entry (EEstimatedDurationEntry *self)
{
	gchar *tmp = NULL;
	ICalDuration *value;

	value = e_estimated_duration_entry_get_value (self);

	if (value != NULL) {
		gint64 seconds = i_cal_duration_as_int (value);

		if (seconds > 0)
			tmp = e_cal_util_seconds_to_string (seconds);
	}

	gtk_entry_set_text (GTK_ENTRY (self->priv->entry), tmp ? tmp : C_("estimated-duration", "None"));

	g_free (tmp);
}

static void
estimated_duration_entry_add_relation (EEstimatedDurationEntry *self)
{
	AtkObject *a11y_estimated_duration_entry;
	AtkObject *a11y_widget;
	AtkRelationSet *set;
	AtkRelation *relation;
	GtkWidget *widget;
	GPtrArray *target;
	gpointer target_object;

	/* add a labelled_by relation for widget for accessibility */

	widget = GTK_WIDGET (self);
	a11y_estimated_duration_entry = gtk_widget_get_accessible (widget);

	widget = self->priv->entry;
	a11y_widget = gtk_widget_get_accessible (widget);

	set = atk_object_ref_relation_set (a11y_widget);
	if (set != NULL) {
		relation = atk_relation_set_get_relation_by_type (set, ATK_RELATION_LABELLED_BY);
		/* check whether has a labelled_by relation already */
		if (relation != NULL) {
			g_object_unref (set);
			return;
		}
	}

	g_clear_object (&set);

	set = atk_object_ref_relation_set (a11y_estimated_duration_entry);
	if (!set)
		return;

	relation = atk_relation_set_get_relation_by_type (set, ATK_RELATION_LABELLED_BY);
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

static void
estimated_duration_set_button_clicked_cb (GtkButton *button,
					  gpointer user_data)
{
	EEstimatedDurationEntry *self = user_data;
	ICalDuration *duration;
	gint new_minutes;

	g_return_if_fail (E_IS_ESTIMATED_DURATION_ENTRY (self));

	new_minutes =
		gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->priv->minutes_spin)) +
		(60 * gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->priv->hours_spin))) +
		(24 * 60 * gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->priv->days_spin)));
	g_return_if_fail (new_minutes > 0);

	gtk_widget_hide (self->priv->popover);

	duration = i_cal_duration_new_from_int (60 * new_minutes);
	e_estimated_duration_entry_set_value (self, duration);
	g_clear_object (&duration);
}

static void
estimated_duration_unset_button_clicked_cb (GtkButton *button,
					    gpointer user_data)
{
	EEstimatedDurationEntry *self = user_data;

	g_return_if_fail (E_IS_ESTIMATED_DURATION_ENTRY (self));

	gtk_widget_hide (self->priv->popover);

	e_estimated_duration_entry_set_value (self, NULL);
}

static void
estimated_duration_update_sensitize_cb (GtkSpinButton *spin,
					gpointer user_data)
{
	EEstimatedDurationEntry *self = user_data;

	g_return_if_fail (E_IS_ESTIMATED_DURATION_ENTRY (self));

	gtk_widget_set_sensitive (self->priv->set_button,
		gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->priv->minutes_spin)) +
		gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->priv->hours_spin)) +
		gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->priv->days_spin)) > 0);
}

static void
estimated_duration_entry_button_clicked_cb (EEstimatedDurationEntry *self)
{
	gint value;

	if (!self->priv->popover) {
		GtkWidget *widget;
		GtkBox *vbox, *box;

		self->priv->days_spin = gtk_spin_button_new_with_range (0.0, 366.0, 1.0);
		self->priv->hours_spin = gtk_spin_button_new_with_range (0.0, 23.0, 1.0);
		self->priv->minutes_spin = gtk_spin_button_new_with_range (0.0, 59.0, 1.0);

		g_object_set (G_OBJECT (self->priv->days_spin),
			"digits", 0,
			"numeric", TRUE,
			"snap-to-ticks", TRUE,
			NULL);

		g_object_set (G_OBJECT (self->priv->hours_spin),
			"digits", 0,
			"numeric", TRUE,
			"snap-to-ticks", TRUE,
			NULL);

		g_object_set (G_OBJECT (self->priv->minutes_spin),
			"digits", 0,
			"numeric", TRUE,
			"snap-to-ticks", TRUE,
			NULL);

		vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 2));

		widget = gtk_label_new (_("Set an estimated duration for"));
		gtk_box_pack_start (vbox, widget, FALSE, FALSE, 0);

		box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2));
		g_object_set (G_OBJECT (box),
			"halign", GTK_ALIGN_START,
			"hexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"vexpand", FALSE,
			NULL);

		gtk_box_pack_start (box, self->priv->days_spin, FALSE, FALSE, 4);
		/* Translators: this is part of: "Set an estimated duration for [nnn] days [nnn] hours [nnn] minutes", where the text in "[]" means a separate widget */
		widget = gtk_label_new_with_mnemonic (C_("estimated-duration", "_days"));
		gtk_label_set_mnemonic_widget (GTK_LABEL (widget), self->priv->days_spin);
		gtk_box_pack_start (box, widget, FALSE, FALSE, 4);

		gtk_box_pack_start (vbox, GTK_WIDGET (box), FALSE, FALSE, 0);

		box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2));
		g_object_set (G_OBJECT (box),
			"halign", GTK_ALIGN_START,
			"hexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"vexpand", FALSE,
			NULL);

		gtk_box_pack_start (box, self->priv->hours_spin, FALSE, FALSE, 4);
		/* Translators: this is part of: "Set an estimated duration for [nnn] days [nnn] hours [nnn] minutes", where the text in "[]" means a separate widget */
		widget = gtk_label_new_with_mnemonic (C_("estimated-duration", "_hours"));
		gtk_label_set_mnemonic_widget (GTK_LABEL (widget), self->priv->hours_spin);
		gtk_box_pack_start (box, widget, FALSE, FALSE, 4);

		gtk_box_pack_start (vbox, GTK_WIDGET (box), FALSE, FALSE, 0);

		box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2));
		g_object_set (G_OBJECT (box),
			"halign", GTK_ALIGN_START,
			"hexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"vexpand", FALSE,
			NULL);

		gtk_box_pack_start (box, self->priv->minutes_spin, FALSE, FALSE, 4);
		/* Translators: this is part of: "Set an estimated duration for [nnn] days [nnn] hours [nnn] minutes", where the text in "[]" means a separate widget */
		widget = gtk_label_new_with_mnemonic (C_("estimated-duration", "_minutes"));
		gtk_label_set_mnemonic_widget (GTK_LABEL (widget), self->priv->minutes_spin);
		gtk_box_pack_start (box, widget, FALSE, FALSE, 4);

		gtk_box_pack_start (vbox, GTK_WIDGET (box), FALSE, FALSE, 0);

		box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2));
		g_object_set (G_OBJECT (box),
			"halign", GTK_ALIGN_CENTER,
			"hexpand", TRUE,
			"valign", GTK_ALIGN_CENTER,
			"vexpand", FALSE,
			NULL);

		self->priv->unset_button = gtk_button_new_with_mnemonic (_("_Unset"));
		g_object_set (G_OBJECT (self->priv->unset_button),
			"halign", GTK_ALIGN_CENTER,
			NULL);

		gtk_box_pack_start (box, self->priv->unset_button, FALSE, FALSE, 1);

		self->priv->set_button = gtk_button_new_with_mnemonic (_("_Set"));
		g_object_set (G_OBJECT (self->priv->set_button),
			"halign", GTK_ALIGN_CENTER,
			NULL);

		gtk_box_pack_start (box, self->priv->set_button, FALSE, FALSE, 1);

		gtk_box_pack_start (vbox, GTK_WIDGET (box), FALSE, FALSE, 0);

		self->priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
		gtk_size_group_add_widget (self->priv->size_group, self->priv->unset_button);
		gtk_size_group_add_widget (self->priv->size_group, self->priv->set_button);

		gtk_widget_show_all (GTK_WIDGET (vbox));

		self->priv->popover = gtk_popover_new (GTK_WIDGET (self));
		gtk_popover_set_position (GTK_POPOVER (self->priv->popover), GTK_POS_BOTTOM);
		gtk_container_add (GTK_CONTAINER (self->priv->popover), GTK_WIDGET (vbox));
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

		g_signal_connect (self->priv->set_button, "clicked",
			G_CALLBACK (estimated_duration_set_button_clicked_cb), self);

		g_signal_connect (self->priv->unset_button, "clicked",
			G_CALLBACK (estimated_duration_unset_button_clicked_cb), self);

		g_signal_connect (self->priv->days_spin, "value-changed",
			G_CALLBACK (estimated_duration_update_sensitize_cb), self);

		g_signal_connect (self->priv->hours_spin, "value-changed",
			G_CALLBACK (estimated_duration_update_sensitize_cb), self);

		g_signal_connect (self->priv->minutes_spin, "value-changed",
			G_CALLBACK (estimated_duration_update_sensitize_cb), self);
	}

	value = self->priv->value ? i_cal_duration_as_int (self->priv->value) : 0;

	/* seconds are ignored */
	value = value / 60;

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->priv->minutes_spin), value % 60);

	value = value / 60;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->priv->hours_spin), value % 24);

	value = value / 24;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->priv->days_spin), value);

	gtk_widget_hide (self->priv->popover);
	gtk_popover_set_relative_to (GTK_POPOVER (self->priv->popover), self->priv->entry);
	gtk_widget_show (self->priv->popover);

	gtk_widget_grab_focus (self->priv->days_spin);

	estimated_duration_update_sensitize_cb (NULL, self);
}

static void
estimated_duration_entry_set_property (GObject *object,
				       guint property_id,
				       const GValue *value,
				       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_VALUE:
			e_estimated_duration_entry_set_value (
				E_ESTIMATED_DURATION_ENTRY (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
estimated_duration_entry_get_property (GObject *object,
				       guint property_id,
				       GValue *value,
				       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_VALUE:
			g_value_set_object (
				value, e_estimated_duration_entry_get_value (
				E_ESTIMATED_DURATION_ENTRY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
estimated_duration_entry_get_finalize (GObject *object)
{
	EEstimatedDurationEntry *self = E_ESTIMATED_DURATION_ENTRY (object);

	g_clear_object (&self->priv->value);
	g_clear_object (&self->priv->size_group);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_estimated_duration_entry_parent_class)->finalize (object);
}

static gboolean
estimated_duration_entry_mnemonic_activate (GtkWidget *widget,
					    gboolean group_cycling)
{
	EEstimatedDurationEntry *self = E_ESTIMATED_DURATION_ENTRY (widget);

	if (gtk_widget_get_can_focus (widget)) {
		if (self->priv->button != NULL)
			gtk_widget_grab_focus (self->priv->button);
	}

	return TRUE;
}

static gboolean
estimated_duration_entry_focus (GtkWidget *widget,
				GtkDirectionType direction)
{
	EEstimatedDurationEntry *self = E_ESTIMATED_DURATION_ENTRY (widget);

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

static void
e_estimated_duration_entry_class_init (EEstimatedDurationEntryClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = estimated_duration_entry_set_property;
	object_class->get_property = estimated_duration_entry_get_property;
	object_class->finalize = estimated_duration_entry_get_finalize;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->mnemonic_activate = estimated_duration_entry_mnemonic_activate;
	widget_class->focus = estimated_duration_entry_focus;

	g_object_class_install_property (
		object_class,
		PROP_VALUE,
		g_param_spec_object (
			"value",
			"Value",
			NULL,
			I_CAL_TYPE_DURATION,
			G_PARAM_READWRITE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EEstimatedDurationEntryClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_estimated_duration_entry_init (EEstimatedDurationEntry *self)
{
	GtkWidget *widget;

	self->priv = e_estimated_duration_entry_get_instance_private (self);

	gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

	widget = gtk_entry_new ();
	gtk_editable_set_editable (GTK_EDITABLE (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (self), widget, TRUE, TRUE, 0);
	self->priv->entry = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (estimated_duration_entry_emit_changed), self);

	widget = gtk_button_new_with_mnemonic (_("Chan_ge…"));
	gtk_box_pack_start (GTK_BOX (self), widget, FALSE, FALSE, 6);
	self->priv->button = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (estimated_duration_entry_button_clicked_cb), self);

	estimated_duration_entry_update_entry (self);
}

GtkWidget *
e_estimated_duration_entry_new (void)
{
	return g_object_new (E_TYPE_ESTIMATED_DURATION_ENTRY, NULL);
}

ICalDuration *
e_estimated_duration_entry_get_value (EEstimatedDurationEntry *self)
{
	g_return_val_if_fail (E_IS_ESTIMATED_DURATION_ENTRY (self), NULL);

	return self->priv->value;
}

void
e_estimated_duration_entry_set_value (EEstimatedDurationEntry *self,
				      const ICalDuration *value)
{
	g_return_if_fail (E_IS_ESTIMATED_DURATION_ENTRY (self));

	if (value && !i_cal_duration_as_int ((ICalDuration *) value))
		value = NULL;

	if (self->priv->value == value)
		return;

	if (self->priv->value && value && i_cal_duration_as_int (self->priv->value) == i_cal_duration_as_int ((ICalDuration *) value))
		return;

	g_clear_object (&self->priv->value);
	if (value)
		self->priv->value = i_cal_duration_new_from_int (i_cal_duration_as_int ((ICalDuration *) value));

	estimated_duration_entry_update_entry (self);
	estimated_duration_entry_add_relation (self);

	g_object_notify (G_OBJECT (self), "value");
}
