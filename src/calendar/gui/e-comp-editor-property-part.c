/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <glib-object.h>
#include <gtk/gtk.h>

#include <e-util/e-util.h>

#include "comp-util.h"
#include "e-comp-editor-property-part.h"

struct _ECompEditorPropertyPartPrivate {
	GtkWidget *label_widget;
	GtkWidget *edit_widget;
	gboolean visible;
	gboolean sensitize_handled;
};

enum {
	PROPERTY_PART_PROP_0,
	PROPERTY_PART_PROP_SENSITIZE_HANDLED,
	PROPERTY_PART_PROP_VISIBLE
};

enum {
	PROPERTY_PART_CHANGED,
	PROPERTY_PART_LAST_SIGNAL
};

static guint property_part_signals[PROPERTY_PART_LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE (ECompEditorPropertyPart, e_comp_editor_property_part, G_TYPE_OBJECT)

static void
e_comp_editor_property_part_set_property (GObject *object,
					  guint property_id,
					  const GValue *value,
					  GParamSpec *pspec)
{
	switch (property_id) {
		case PROPERTY_PART_PROP_SENSITIZE_HANDLED:
			e_comp_editor_property_part_set_sensitize_handled (
				E_COMP_EDITOR_PROPERTY_PART (object),
				g_value_get_boolean (value));
			return;

		case PROPERTY_PART_PROP_VISIBLE:
			e_comp_editor_property_part_set_visible (
				E_COMP_EDITOR_PROPERTY_PART (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_comp_editor_property_part_get_property (GObject *object,
					  guint property_id,
					  GValue *value,
					  GParamSpec *pspec)
{
	switch (property_id) {
		case PROPERTY_PART_PROP_SENSITIZE_HANDLED:
			g_value_set_boolean (
				value,
				e_comp_editor_property_part_get_sensitize_handled (
				E_COMP_EDITOR_PROPERTY_PART (object)));
			return;

		case PROPERTY_PART_PROP_VISIBLE:
			g_value_set_boolean (
				value,
				e_comp_editor_property_part_get_visible (
				E_COMP_EDITOR_PROPERTY_PART (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_comp_editor_property_part_constructed (GObject *object)
{
	ECompEditorPropertyPart *property_part;
	GtkWidget *label_widget = NULL, *edit_widget = NULL;

	G_OBJECT_CLASS (e_comp_editor_property_part_parent_class)->constructed (object);

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (object));

	property_part = E_COMP_EDITOR_PROPERTY_PART (object);

	e_comp_editor_property_part_create_widgets (property_part, &label_widget, &edit_widget);

	if (label_widget) {
		property_part->priv->label_widget = g_object_ref_sink (label_widget);

		e_binding_bind_property (
			property_part, "visible",
			label_widget, "visible",
			G_BINDING_SYNC_CREATE);
	}

	if (edit_widget) {
		property_part->priv->edit_widget = g_object_ref_sink (edit_widget);

		e_binding_bind_property (
			property_part, "visible",
			edit_widget, "visible",
			G_BINDING_SYNC_CREATE);
	}
}

static void
e_comp_editor_property_part_dispose (GObject *object)
{
	ECompEditorPropertyPart *property_part;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (object));

	property_part = E_COMP_EDITOR_PROPERTY_PART (object);

	g_clear_object (&property_part->priv->label_widget);
	g_clear_object (&property_part->priv->edit_widget);

	G_OBJECT_CLASS (e_comp_editor_property_part_parent_class)->dispose (object);
}

static void
e_comp_editor_property_part_init (ECompEditorPropertyPart *property_part)
{
	property_part->priv = G_TYPE_INSTANCE_GET_PRIVATE (property_part,
		E_TYPE_COMP_EDITOR_PROPERTY_PART,
		ECompEditorPropertyPartPrivate);
	property_part->priv->visible = TRUE;
	property_part->priv->sensitize_handled = FALSE;
}

static void
e_comp_editor_property_part_class_init (ECompEditorPropertyPartClass *klass)
{
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (ECompEditorPropertyPartPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_comp_editor_property_part_set_property;
	object_class->get_property = e_comp_editor_property_part_get_property;
	object_class->constructed = e_comp_editor_property_part_constructed;
	object_class->dispose = e_comp_editor_property_part_dispose;

	g_object_class_install_property (
		object_class,
		PROPERTY_PART_PROP_VISIBLE,
		g_param_spec_boolean (
			"visible",
			"Visible",
			"Whether the part is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROPERTY_PART_PROP_SENSITIZE_HANDLED,
		g_param_spec_boolean (
			"sensitize-handled",
			"Sensitize Handled",
			"Whether the part's sensitive property is handled by the owner of it",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	property_part_signals[PROPERTY_PART_CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECompEditorPropertyPartClass, changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);
}

gboolean
e_comp_editor_property_part_get_visible (ECompEditorPropertyPart *property_part)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part), FALSE);

	return property_part->priv->visible;
}

void
e_comp_editor_property_part_set_visible (ECompEditorPropertyPart *property_part,
					 gboolean visible)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part));

	if ((property_part->priv->visible ? 1 : 0) == (visible ? 1 : 0))
		return;

	property_part->priv->visible = visible;

	g_object_notify (G_OBJECT (property_part), "visible");
}

gboolean
e_comp_editor_property_part_get_sensitize_handled (ECompEditorPropertyPart *property_part)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part), FALSE);

	return property_part->priv->sensitize_handled;
}

void
e_comp_editor_property_part_set_sensitize_handled (ECompEditorPropertyPart *property_part,
						   gboolean sensitize_handled)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part));

	if ((property_part->priv->sensitize_handled ? 1 : 0) == (sensitize_handled ? 1 : 0))
		return;

	property_part->priv->sensitize_handled = sensitize_handled;

	g_object_notify (G_OBJECT (property_part), "sensitize-handled");
}

void
e_comp_editor_property_part_create_widgets (ECompEditorPropertyPart *property_part,
					    GtkWidget **out_label_widget,
					    GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part));

	g_return_if_fail (property_part->priv->label_widget == NULL);
	g_return_if_fail (property_part->priv->edit_widget == NULL);

	klass = E_COMP_EDITOR_PROPERTY_PART_GET_CLASS (property_part);
	g_return_if_fail (klass->create_widgets != NULL);

	klass->create_widgets (property_part, out_label_widget, out_edit_widget);
}

GtkWidget *
e_comp_editor_property_part_get_label_widget (ECompEditorPropertyPart *property_part)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part), NULL);

	return property_part->priv->label_widget;
}

GtkWidget *
e_comp_editor_property_part_get_edit_widget (ECompEditorPropertyPart *property_part)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part), NULL);

	return property_part->priv->edit_widget;
}

void
e_comp_editor_property_part_fill_widget (ECompEditorPropertyPart *property_part,
					 icalcomponent *component)
{
	ECompEditorPropertyPartClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part));

	klass = E_COMP_EDITOR_PROPERTY_PART_GET_CLASS (property_part);
	g_return_if_fail (klass->fill_widget != NULL);

	klass->fill_widget (property_part, component);
}

void
e_comp_editor_property_part_fill_component (ECompEditorPropertyPart *property_part,
					    icalcomponent *component)
{
	ECompEditorPropertyPartClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part));

	klass = E_COMP_EDITOR_PROPERTY_PART_GET_CLASS (property_part);
	g_return_if_fail (klass->fill_component != NULL);

	klass->fill_component (property_part, component);
}

void
e_comp_editor_property_part_emit_changed (ECompEditorPropertyPart *property_part)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part));

	g_signal_emit (property_part, property_part_signals[PROPERTY_PART_CHANGED], 0, NULL);
}

/* ************************************************************************* */

struct _ECompEditorPropertyPartStringPrivate {
	gint dummy;
};

G_DEFINE_ABSTRACT_TYPE (ECompEditorPropertyPartString, e_comp_editor_property_part_string, E_TYPE_COMP_EDITOR_PROPERTY_PART)

static void
ecepp_string_create_widgets (ECompEditorPropertyPart *property_part,
			     GtkWidget **out_label_widget,
			     GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartStringClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	klass = E_COMP_EDITOR_PROPERTY_PART_STRING_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (g_type_is_a (klass->entry_type, GTK_TYPE_ENTRY) ||
			  g_type_is_a (klass->entry_type, GTK_TYPE_TEXT_VIEW));

	/* The descendant sets the 'out_label_widget' parameter */
	*out_edit_widget = g_object_new (klass->entry_type, NULL);
	g_return_if_fail (*out_edit_widget != NULL);

	g_object_set (G_OBJECT (*out_edit_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);

	gtk_widget_show (*out_edit_widget);

	if (g_type_is_a (klass->entry_type, GTK_TYPE_TEXT_VIEW)) {
		GtkScrolledWindow *scrolled_window;
		GtkTextBuffer *text_buffer;

		scrolled_window = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
		gtk_scrolled_window_set_policy (scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (scrolled_window, GTK_SHADOW_IN);
		gtk_widget_show (GTK_WIDGET (scrolled_window));

		gtk_container_add (GTK_CONTAINER (scrolled_window), *out_edit_widget);

		g_object_set (G_OBJECT (*out_edit_widget),
			"hexpand", TRUE,
			"halign", GTK_ALIGN_FILL,
			"vexpand", TRUE,
			"valign", GTK_ALIGN_FILL,
			NULL);

		g_object_set (G_OBJECT (scrolled_window),
			"hexpand", FALSE,
			"halign", GTK_ALIGN_FILL,
			"vexpand", FALSE,
			"valign", GTK_ALIGN_START,
			NULL);

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (*out_edit_widget));
		g_signal_connect_swapped (text_buffer, "changed",
			G_CALLBACK (e_comp_editor_property_part_emit_changed), property_part);

		*out_edit_widget = GTK_WIDGET (scrolled_window);
	} else {
		g_signal_connect_swapped (*out_edit_widget, "changed",
			G_CALLBACK (e_comp_editor_property_part_emit_changed), property_part);
	}
}

static void
ecepp_string_fill_widget (ECompEditorPropertyPart *property_part,
			  icalcomponent *component)
{
	ECompEditorPropertyPartStringClass *klass;
	GtkWidget *edit_widget;
	icalproperty *prop;
	const gchar *value = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (property_part));
	g_return_if_fail (component != NULL);

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_ENTRY (edit_widget) || GTK_IS_SCROLLED_WINDOW (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_STRING_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->ical_prop_kind != ICAL_NO_PROPERTY);
	g_return_if_fail (klass->ical_get_func != NULL);

	prop = icalcomponent_get_first_property (component, klass->ical_prop_kind);
	if (prop)
		value = klass->ical_get_func (prop);

	if (!value)
		value = "";

	if (GTK_IS_ENTRY (edit_widget)) {
		gtk_entry_set_text (GTK_ENTRY (edit_widget), value);
	} else /* if (GTK_IS_SCROLLED_WINDOW (edit_widget)) */ {
		GtkTextBuffer *buffer;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (gtk_bin_get_child (GTK_BIN (edit_widget))));
		gtk_text_buffer_set_text (buffer, value, -1);
	}

	e_widget_undo_reset (edit_widget);
}

static void
ecepp_string_fill_component (ECompEditorPropertyPart *property_part,
			     icalcomponent *component)
{
	ECompEditorPropertyPartStringClass *klass;
	GtkWidget *edit_widget;
	icalproperty *prop;
	gchar *value;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (property_part));
	g_return_if_fail (component != NULL);

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_ENTRY (edit_widget) || GTK_IS_SCROLLED_WINDOW (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_STRING_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->ical_prop_kind != ICAL_NO_PROPERTY);
	g_return_if_fail (klass->ical_new_func != NULL);
	g_return_if_fail (klass->ical_set_func != NULL);

	if (GTK_IS_ENTRY (edit_widget)) {
		value = g_strdup (gtk_entry_get_text (GTK_ENTRY (edit_widget)));
	} else /* if (GTK_IS_SCROLLED_WINDOW (edit_widget)) */ {
		GtkTextBuffer *buffer;
		GtkTextIter text_iter_start, text_iter_end;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (gtk_bin_get_child (GTK_BIN (edit_widget))));
		gtk_text_buffer_get_start_iter (buffer, &text_iter_start);
		gtk_text_buffer_get_end_iter (buffer, &text_iter_end);
		value = gtk_text_buffer_get_text (buffer, &text_iter_start, &text_iter_end, FALSE);
	}

	prop = icalcomponent_get_first_property (component, klass->ical_prop_kind);

	if (value && *value) {
		if (prop) {
			klass->ical_set_func (prop, value);
		} else {
			prop = klass->ical_new_func (value);
			icalcomponent_add_property (component, prop);
		}
	} else if (prop) {
		icalcomponent_remove_property (component, prop);
		icalproperty_free (prop);
	}

	g_free (value);
}

static void
e_comp_editor_property_part_string_init (ECompEditorPropertyPartString *part_string)
{
	part_string->priv = G_TYPE_INSTANCE_GET_PRIVATE (part_string,
		E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING,
		ECompEditorPropertyPartStringPrivate);
}

static void
e_comp_editor_property_part_string_class_init (ECompEditorPropertyPartStringClass *klass)
{
	ECompEditorPropertyPartClass *part_class;

	g_type_class_add_private (klass, sizeof (ECompEditorPropertyPartStringPrivate));

	klass->entry_type = GTK_TYPE_ENTRY;

	klass->ical_prop_kind = ICAL_NO_PROPERTY;
	klass->ical_new_func = NULL;
	klass->ical_set_func = NULL;
	klass->ical_get_func = NULL;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_string_create_widgets;
	part_class->fill_widget = ecepp_string_fill_widget;
	part_class->fill_component = ecepp_string_fill_component;
}

void
e_comp_editor_property_part_string_attach_focus_tracker (ECompEditorPropertyPartString *part_string,
							 EFocusTracker *focus_tracker)
{
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (part_string));

	if (!focus_tracker)
		return;

	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_string));
	if (edit_widget)
		e_widget_undo_attach (edit_widget, focus_tracker);
}

/* ************************************************************************* */

struct _ECompEditorPropertyPartDatetimePrivate {
	GWeakRef timezone_entry;
};

G_DEFINE_ABSTRACT_TYPE (ECompEditorPropertyPartDatetime, e_comp_editor_property_part_datetime, E_TYPE_COMP_EDITOR_PROPERTY_PART)

static void
ecepp_datetime_create_widgets (ECompEditorPropertyPart *property_part,
			       GtkWidget **out_label_widget,
			       GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartDatetimeClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	klass = E_COMP_EDITOR_PROPERTY_PART_DATETIME_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);

	/* The descendant sets the 'out_label_widget' parameter */
	*out_edit_widget = e_date_edit_new ();
	g_return_if_fail (*out_edit_widget != NULL);

	g_object_set (G_OBJECT (*out_edit_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);

	gtk_widget_show (*out_edit_widget);

	g_signal_connect_swapped (*out_edit_widget, "changed",
		G_CALLBACK (e_comp_editor_property_part_emit_changed), property_part);
	g_signal_connect_swapped (*out_edit_widget, "notify::show-time",
		G_CALLBACK (e_comp_editor_property_part_emit_changed), property_part);
}

static void
ecepp_datetime_fill_widget (ECompEditorPropertyPart *property_part,
			    icalcomponent *component)
{
	ECompEditorPropertyPartDatetime *part_datetime;
	ECompEditorPropertyPartDatetimeClass *klass;
	GtkWidget *edit_widget;
	icalproperty *prop;
	struct icaltimetype value;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (property_part));
	g_return_if_fail (component != NULL);

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (E_IS_DATE_EDIT (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_DATETIME_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->ical_prop_kind != ICAL_NO_PROPERTY);
	g_return_if_fail (klass->ical_get_func != NULL);

	part_datetime = E_COMP_EDITOR_PROPERTY_PART_DATETIME (property_part);

	prop = icalcomponent_get_first_property (component, klass->ical_prop_kind);
	if (prop)
		value = klass->ical_get_func (prop);
	else
		value = icaltime_null_time ();

	e_comp_editor_property_part_datetime_set_value (part_datetime, value);
}

static void
ecepp_datetime_fill_component (ECompEditorPropertyPart *property_part,
			       icalcomponent *component)
{
	ECompEditorPropertyPartDatetime *part_datetime;
	ECompEditorPropertyPartDatetimeClass *klass;
	GtkWidget *edit_widget;
	EDateEdit *date_edit;
	icalproperty *prop;
	struct icaltimetype value;
	time_t tt;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (property_part));
	g_return_if_fail (component != NULL);

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (E_IS_DATE_EDIT (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_DATETIME_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->ical_prop_kind != ICAL_NO_PROPERTY);
	g_return_if_fail (klass->ical_new_func != NULL);
	g_return_if_fail (klass->ical_set_func != NULL);

	part_datetime = E_COMP_EDITOR_PROPERTY_PART_DATETIME (property_part);
	date_edit = E_DATE_EDIT (edit_widget);
	tt = e_date_edit_get_time (date_edit);

	prop = icalcomponent_get_first_property (component, klass->ical_prop_kind);

	if (e_date_edit_get_allow_no_date_set (date_edit) && tt == (time_t) -1) {
		if (prop) {
			icalcomponent_remove_property (component, prop);
			icalproperty_free (prop);
		}
	} else {
		value = e_comp_editor_property_part_datetime_get_value (part_datetime);

		if (prop) {
			klass->ical_set_func (prop, value);
			cal_comp_util_update_tzid_parameter (prop, value);
		} else {
			prop = klass->ical_new_func (value);
			cal_comp_util_update_tzid_parameter (prop, value);
			icalcomponent_add_property (component, prop);
		}
	}
}

static void
ecepp_datetime_finalize (GObject *object)
{
	ECompEditorPropertyPartDatetime *part_datetime = E_COMP_EDITOR_PROPERTY_PART_DATETIME (object);

	g_weak_ref_clear (&part_datetime->priv->timezone_entry);

	G_OBJECT_CLASS (e_comp_editor_property_part_datetime_parent_class)->finalize (object);
}

static void
e_comp_editor_property_part_datetime_init (ECompEditorPropertyPartDatetime *part_datetime)
{
	part_datetime->priv = G_TYPE_INSTANCE_GET_PRIVATE (part_datetime,
		E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME,
		ECompEditorPropertyPartDatetimePrivate);

	g_weak_ref_init (&part_datetime->priv->timezone_entry, NULL);
}

static void
e_comp_editor_property_part_datetime_class_init (ECompEditorPropertyPartDatetimeClass *klass)
{
	ECompEditorPropertyPartClass *part_class;
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (ECompEditorPropertyPartDatetimePrivate));

	klass->ical_prop_kind = ICAL_NO_PROPERTY;
	klass->ical_new_func = NULL;
	klass->ical_set_func = NULL;
	klass->ical_get_func = NULL;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_datetime_create_widgets;
	part_class->fill_widget = ecepp_datetime_fill_widget;
	part_class->fill_component = ecepp_datetime_fill_component;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = ecepp_datetime_finalize;
}

void
e_comp_editor_property_part_datetime_attach_timezone_entry (ECompEditorPropertyPartDatetime *part_datetime,
							    ETimezoneEntry *timezone_entry)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (part_datetime));
	if (timezone_entry)
		g_return_if_fail (E_IS_TIMEZONE_ENTRY (timezone_entry));

	g_weak_ref_set (&part_datetime->priv->timezone_entry, timezone_entry);
}

void
e_comp_editor_property_part_datetime_set_date_only (ECompEditorPropertyPartDatetime *part_datetime,
						    gboolean date_only)
{
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (part_datetime));

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_datetime));
	g_return_if_fail (E_IS_DATE_EDIT (edit_widget));

	if ((e_date_edit_get_show_time (E_DATE_EDIT (edit_widget)) ? 1 : 0) == ((!date_only) ? 1 : 0))
		return;

	e_date_edit_set_show_time (E_DATE_EDIT (edit_widget), !date_only);
}

gboolean
e_comp_editor_property_part_datetime_get_date_only (ECompEditorPropertyPartDatetime *part_datetime)
{
	GtkWidget *edit_widget;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (part_datetime), FALSE);

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_datetime));
	g_return_val_if_fail (E_IS_DATE_EDIT (edit_widget), FALSE);

	return !e_date_edit_get_show_time (E_DATE_EDIT (edit_widget));
}

void
e_comp_editor_property_part_datetime_set_allow_no_date_set (ECompEditorPropertyPartDatetime *part_datetime,
							    gboolean allow_no_date_set)
{
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (part_datetime));

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_datetime));
	g_return_if_fail (E_IS_DATE_EDIT (edit_widget));

	e_date_edit_set_allow_no_date_set (E_DATE_EDIT (edit_widget), allow_no_date_set);
}

gboolean
e_comp_editor_property_part_datetime_get_allow_no_date_set (ECompEditorPropertyPartDatetime *part_datetime)
{
	GtkWidget *edit_widget;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (part_datetime), FALSE);

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_datetime));
	g_return_val_if_fail (E_IS_DATE_EDIT (edit_widget), FALSE);

	return !e_date_edit_get_allow_no_date_set (E_DATE_EDIT (edit_widget));
}

void
e_comp_editor_property_part_datetime_set_value (ECompEditorPropertyPartDatetime *part_datetime,
						struct icaltimetype value)
{
	GtkWidget *edit_widget;
	EDateEdit *date_edit;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (part_datetime));

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_datetime));
	g_return_if_fail (E_IS_DATE_EDIT (edit_widget));

	date_edit = E_DATE_EDIT (edit_widget);

	if (!e_date_edit_get_allow_no_date_set (date_edit) && (icaltime_is_null_time (value) ||
	    !icaltime_is_valid_time (value))) {
		value = icaltime_current_time_with_zone (icaltimezone_get_utc_timezone ());
	}

	if (icaltime_is_null_time (value) ||
	    !icaltime_is_valid_time (value)) {
		e_date_edit_set_time (date_edit, (time_t) -1);
	} else {
		e_date_edit_set_date (date_edit, value.year, value.month, value.day);

		if (!value.is_date)
			e_date_edit_set_time_of_day (date_edit, value.hour, value.minute);
		else if (e_date_edit_get_show_time (date_edit))
			e_date_edit_set_time_of_day (date_edit, 0, 0);
		else
			e_date_edit_set_time_of_day (date_edit, -1, -1);

		e_comp_editor_property_part_datetime_set_date_only (part_datetime, value.is_date);
	}
}

struct icaltimetype
e_comp_editor_property_part_datetime_get_value (ECompEditorPropertyPartDatetime *part_datetime)
{
	ETimezoneEntry *timezone_entry = NULL;
	GtkWidget *edit_widget;
	EDateEdit *date_edit;
	struct icaltimetype value = icaltime_null_time ();

	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (part_datetime), value);

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_datetime));
	g_return_val_if_fail (E_IS_DATE_EDIT (edit_widget), value);

	date_edit = E_DATE_EDIT (edit_widget);

	if (!e_date_edit_get_date (date_edit, &value.year, &value.month, &value.day))
		return icaltime_null_time ();

	if (!e_date_edit_get_show_time (date_edit)) {
		value.is_date = 1;
	} else {
		value.is_date = 0;
		value.zone = NULL;

		e_date_edit_get_time_of_day (date_edit, &value.hour, &value.minute);

		timezone_entry = g_weak_ref_get (&part_datetime->priv->timezone_entry);
		if (timezone_entry)
			value.zone = e_timezone_entry_get_timezone (timezone_entry);
		if (!value.zone)
			value.zone = icaltimezone_get_utc_timezone ();

		value.is_utc = value.zone == icaltimezone_get_utc_timezone ();
	}

	g_clear_object (&timezone_entry);

	return value;
}

gboolean
e_comp_editor_property_part_datetime_check_validity (ECompEditorPropertyPartDatetime *part_datetime,
						     gboolean *out_date_is_valid,
						     gboolean *out_time_is_valid)
{
	GtkWidget *edit_widget;
	EDateEdit *date_edit;
	gboolean date_is_valid = TRUE, time_is_valid = TRUE;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (part_datetime), FALSE);

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_datetime));
	g_return_val_if_fail (E_IS_DATE_EDIT (edit_widget), FALSE);

	date_edit = E_DATE_EDIT (edit_widget);

	if (!e_date_edit_get_allow_no_date_set (date_edit) ||
	    e_date_edit_get_time (date_edit) != (time_t) -1) {
		date_is_valid = e_date_edit_date_is_valid (date_edit);

		if (e_date_edit_get_show_time (date_edit))
			time_is_valid = e_date_edit_time_is_valid (date_edit);
	}

	if (out_date_is_valid)
		*out_date_is_valid = date_is_valid;
	if (out_time_is_valid)
		*out_time_is_valid = time_is_valid;

	return date_is_valid && time_is_valid;
}

/* ************************************************************************* */

struct _ECompEditorPropertyPartSpinPrivate {
	gint dummy;
};

G_DEFINE_ABSTRACT_TYPE (ECompEditorPropertyPartSpin, e_comp_editor_property_part_spin, E_TYPE_COMP_EDITOR_PROPERTY_PART)

static void
ecepp_spin_create_widgets (ECompEditorPropertyPart *property_part,
			   GtkWidget **out_label_widget,
			   GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartSpinClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_SPIN (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	klass = E_COMP_EDITOR_PROPERTY_PART_SPIN_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);

	/* The descendant sets the 'out_label_widget' parameter */
	*out_edit_widget = gtk_spin_button_new_with_range (-10, 10, 1);
	g_return_if_fail (*out_edit_widget != NULL);

	g_object_set (G_OBJECT (*out_edit_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);

	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (*out_edit_widget), 0);

	gtk_widget_show (*out_edit_widget);

	g_signal_connect_swapped (*out_edit_widget, "value-changed",
		G_CALLBACK (e_comp_editor_property_part_emit_changed), property_part);
}

static void
ecepp_spin_fill_widget (ECompEditorPropertyPart *property_part,
			icalcomponent *component)
{
	ECompEditorPropertyPartSpinClass *klass;
	GtkWidget *edit_widget;
	icalproperty *prop;
	gint value;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_SPIN (property_part));
	g_return_if_fail (component != NULL);

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_SPIN_BUTTON (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_SPIN_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->ical_prop_kind != ICAL_NO_PROPERTY);
	g_return_if_fail (klass->ical_get_func != NULL);

	prop = icalcomponent_get_first_property (component, klass->ical_prop_kind);
	if (prop) {
		value = klass->ical_get_func (prop);
	} else {
		gdouble d_min, d_max;

		gtk_spin_button_get_range (GTK_SPIN_BUTTON (edit_widget), &d_min, &d_max);

		value = (gint) d_min;
	}

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (edit_widget), value);
}

static void
ecepp_spin_fill_component (ECompEditorPropertyPart *property_part,
			   icalcomponent *component)
{
	ECompEditorPropertyPartSpinClass *klass;
	GtkWidget *edit_widget;
	icalproperty *prop;
	gint value;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_SPIN (property_part));
	g_return_if_fail (component != NULL);

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_SPIN_BUTTON (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_SPIN_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->ical_prop_kind != ICAL_NO_PROPERTY);
	g_return_if_fail (klass->ical_new_func != NULL);
	g_return_if_fail (klass->ical_set_func != NULL);

	value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (edit_widget));
	prop = icalcomponent_get_first_property (component, klass->ical_prop_kind);

	if (prop) {
		klass->ical_set_func (prop, value);
	} else {
		prop = klass->ical_new_func (value);
		icalcomponent_add_property (component, prop);
	}
}

static void
e_comp_editor_property_part_spin_init (ECompEditorPropertyPartSpin *part_spin)
{
	part_spin->priv = G_TYPE_INSTANCE_GET_PRIVATE (part_spin,
		E_TYPE_COMP_EDITOR_PROPERTY_PART_SPIN,
		ECompEditorPropertyPartSpinPrivate);
}

static void
e_comp_editor_property_part_spin_class_init (ECompEditorPropertyPartSpinClass *klass)
{
	ECompEditorPropertyPartClass *part_class;

	g_type_class_add_private (klass, sizeof (ECompEditorPropertyPartSpinPrivate));

	klass->ical_prop_kind = ICAL_NO_PROPERTY;
	klass->ical_new_func = NULL;
	klass->ical_set_func = NULL;
	klass->ical_get_func = NULL;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_spin_create_widgets;
	part_class->fill_widget = ecepp_spin_fill_widget;
	part_class->fill_component = ecepp_spin_fill_component;
}

void
e_comp_editor_property_part_spin_set_range (ECompEditorPropertyPartSpin *part_spin,
					    gint min_value,
					    gint max_value)
{
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_SPIN (part_spin));

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_spin));
	g_return_if_fail (GTK_IS_SPIN_BUTTON (edit_widget));

	gtk_spin_button_set_range (GTK_SPIN_BUTTON (edit_widget), min_value, max_value);
}

void
e_comp_editor_property_part_spin_get_range (ECompEditorPropertyPartSpin *part_spin,
					    gint *out_min_value,
					    gint *out_max_value)
{
	GtkWidget *edit_widget;
	gdouble d_min = 0, d_max = 0;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_SPIN (part_spin));

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_spin));
	g_return_if_fail (GTK_IS_SPIN_BUTTON (edit_widget));

	gtk_spin_button_get_range (GTK_SPIN_BUTTON (edit_widget), &d_min, &d_max);

	if (out_min_value)
		*out_min_value = (gint) d_min;
	if (out_max_value)
		*out_max_value = (gint) d_max;
}

/* ************************************************************************* */

struct _ECompEditorPropertyPartPickerPrivate {
	gint dummy;
};

G_DEFINE_ABSTRACT_TYPE (ECompEditorPropertyPartPicker, e_comp_editor_property_part_picker, E_TYPE_COMP_EDITOR_PROPERTY_PART)

static void
ecepp_picker_create_widgets (ECompEditorPropertyPart *property_part,
			     GtkWidget **out_label_widget,
			     GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartPickerClass *klass;
	GtkComboBoxText *combo_box;
	GSList *ids = NULL, *display_names = NULL, *i_link, *dn_link;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	klass = E_COMP_EDITOR_PROPERTY_PART_PICKER_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);

	/* The descendant sets the 'out_label_widget' parameter */
	*out_edit_widget = gtk_combo_box_text_new ();
	g_return_if_fail (*out_edit_widget != NULL);

	g_object_set (G_OBJECT (*out_edit_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);

	gtk_widget_show (*out_edit_widget);

	e_comp_editor_property_part_picker_get_values (E_COMP_EDITOR_PROPERTY_PART_PICKER (property_part),
		&ids, &display_names);

	g_warn_if_fail (g_slist_length (ids) == g_slist_length (display_names));

	combo_box = GTK_COMBO_BOX_TEXT (*out_edit_widget);

	for (i_link = ids, dn_link = display_names; i_link && dn_link; i_link = g_slist_next (i_link), dn_link = g_slist_next (dn_link)) {
		const gchar *id, *display_name;

		id = i_link->data;
		display_name = dn_link->data;

		g_warn_if_fail (id != NULL);
		g_warn_if_fail (display_name != NULL);

		if (!id || !display_name)
			continue;

		gtk_combo_box_text_append (combo_box, id, display_name);
	}

	g_slist_free_full (ids, g_free);
	g_slist_free_full (display_names, g_free);

	g_signal_connect_swapped (*out_edit_widget, "changed",
		G_CALLBACK (e_comp_editor_property_part_emit_changed), property_part);
}

static void
ecepp_picker_fill_widget (ECompEditorPropertyPart *property_part,
			  icalcomponent *component)
{
	GtkWidget *edit_widget;
	gchar *id = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER (property_part));
	g_return_if_fail (component != NULL);

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (edit_widget));

	if (e_comp_editor_property_part_picker_get_from_component (
		E_COMP_EDITOR_PROPERTY_PART_PICKER (property_part),
		component, &id) && id) {
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (edit_widget), id);
		g_free (id);
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (edit_widget), 0);
	}
}

static void
ecepp_picker_fill_component (ECompEditorPropertyPart *property_part,
			     icalcomponent *component)
{
	GtkWidget *edit_widget;
	const gchar *id;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER (property_part));
	g_return_if_fail (component != NULL);

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (edit_widget));

	id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (edit_widget));
	g_return_if_fail (id != NULL);

	e_comp_editor_property_part_picker_set_to_component (
		E_COMP_EDITOR_PROPERTY_PART_PICKER (property_part),
		id, component);
}

static void
e_comp_editor_property_part_picker_init (ECompEditorPropertyPartPicker *part_picker)
{
	part_picker->priv = G_TYPE_INSTANCE_GET_PRIVATE (part_picker,
		E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER,
		ECompEditorPropertyPartPickerPrivate);
}

static void
e_comp_editor_property_part_picker_class_init (ECompEditorPropertyPartPickerClass *klass)
{
	ECompEditorPropertyPartClass *part_class;

	g_type_class_add_private (klass, sizeof (ECompEditorPropertyPartPickerPrivate));

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_picker_create_widgets;
	part_class->fill_widget = ecepp_picker_fill_widget;
	part_class->fill_component = ecepp_picker_fill_component;
}

/* Both out_ids and out_display_names contain newly allocated strings,
   where also n-th element of out_uids corresponds to n-th element of
   the out_display_names. */
void
e_comp_editor_property_part_picker_get_values (ECompEditorPropertyPartPicker *part_picker,
					       GSList **out_ids,
					       GSList **out_display_names)
{
	ECompEditorPropertyPartPickerClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER (part_picker));

	klass = E_COMP_EDITOR_PROPERTY_PART_PICKER_GET_CLASS (part_picker);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->get_values != NULL);

	klass->get_values (part_picker, out_ids, out_display_names);
}

gboolean
e_comp_editor_property_part_picker_get_from_component (ECompEditorPropertyPartPicker *part_picker,
						       icalcomponent *component,
						       gchar **out_id)
{
	ECompEditorPropertyPartPickerClass *klass;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER (part_picker), FALSE);

	klass = E_COMP_EDITOR_PROPERTY_PART_PICKER_GET_CLASS (part_picker);
	g_return_val_if_fail (klass != NULL, FALSE);
	g_return_val_if_fail (klass->get_from_component != NULL, FALSE);

	return klass->get_from_component (part_picker, component, out_id);
}

void
e_comp_editor_property_part_picker_set_to_component (ECompEditorPropertyPartPicker *part_picker,
						     const gchar *id,
						     icalcomponent *component)
{
	ECompEditorPropertyPartPickerClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER (part_picker));

	klass = E_COMP_EDITOR_PROPERTY_PART_PICKER_GET_CLASS (part_picker);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->set_to_component != NULL);

	klass->set_to_component (part_picker, id, component);
}

const gchar *
e_comp_editor_property_part_picker_get_selected_id (ECompEditorPropertyPartPicker *part_picker)
{
	GtkWidget *edit_widget;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER (part_picker), NULL);

	edit_widget = e_comp_editor_property_part_get_edit_widget (
		E_COMP_EDITOR_PROPERTY_PART (part_picker));
	g_return_val_if_fail (GTK_IS_COMBO_BOX_TEXT (edit_widget), NULL);

	return gtk_combo_box_get_active_id (GTK_COMBO_BOX (edit_widget));
}

void
e_comp_editor_property_part_picker_set_selected_id (ECompEditorPropertyPartPicker *part_picker,
						    const gchar *id)
{
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER (part_picker));
	g_return_if_fail (id != NULL);

	edit_widget = e_comp_editor_property_part_get_edit_widget (
		E_COMP_EDITOR_PROPERTY_PART (part_picker));
	g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (edit_widget));

	gtk_combo_box_set_active_id (GTK_COMBO_BOX (edit_widget), id);
}

/* ************************************************************************* */

struct _ECompEditorPropertyPartPickerWithMapPrivate {
	ECompEditorPropertyPartPickerMap *map;
	gint n_map_elems;
	gchar *label;

	icalproperty_kind ical_prop_kind;
	ECompEditorPropertyPartPickerMapICalNewFunc ical_new_func;
	ECompEditorPropertyPartPickerMapICalSetFunc ical_set_func;
	ECompEditorPropertyPartPickerMapICalGetFunc ical_get_func;
};

enum {
	PICKER_WITH_MAP_PROP_0,
	PICKER_WITH_MAP_PROP_MAP,
	PICKER_WITH_MAP_PROP_LABEL
};

G_DEFINE_TYPE (ECompEditorPropertyPartPickerWithMap, e_comp_editor_property_part_picker_with_map, E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER)

static void
ecepp_picker_with_map_get_values (ECompEditorPropertyPartPicker *part_picker,
				  GSList **out_ids,
				  GSList **out_display_names)
{
	ECompEditorPropertyPartPickerWithMap *part_picker_with_map;
	gint ii;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker));
	g_return_if_fail (out_ids != NULL);
	g_return_if_fail (out_display_names != NULL);

	part_picker_with_map = E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker);
	g_return_if_fail (part_picker_with_map->priv->map != NULL);
	g_return_if_fail (part_picker_with_map->priv->n_map_elems > 0);

	for (ii = 0; ii < part_picker_with_map->priv->n_map_elems; ii++) {
		*out_ids = g_slist_prepend (*out_ids, g_strdup_printf ("%d", ii));
		*out_display_names = g_slist_prepend (*out_display_names,
			g_strdup (part_picker_with_map->priv->map[ii].description));
	}

	*out_ids = g_slist_reverse (*out_ids);
	*out_display_names = g_slist_reverse (*out_display_names);
}

static gboolean
ecepp_picker_with_map_get_from_component (ECompEditorPropertyPartPicker *part_picker,
					  icalcomponent *component,
					  gchar **out_id)
{
	ECompEditorPropertyPartPickerWithMap *part_picker_with_map;
	icalproperty *prop;
	gint ii, value;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker), FALSE);
	g_return_val_if_fail (component != NULL, FALSE);
	g_return_val_if_fail (out_id != NULL, FALSE);

	part_picker_with_map = E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker);
	g_return_val_if_fail (part_picker_with_map->priv->map != NULL, FALSE);
	g_return_val_if_fail (part_picker_with_map->priv->n_map_elems > 0, FALSE);
	g_return_val_if_fail (part_picker_with_map->priv->ical_prop_kind != ICAL_NO_PROPERTY, FALSE);
	g_return_val_if_fail (part_picker_with_map->priv->ical_get_func != NULL, FALSE);

	prop = icalcomponent_get_first_property (component, part_picker_with_map->priv->ical_prop_kind);
	if (!prop)
		return FALSE;

	value = part_picker_with_map->priv->ical_get_func (prop);

	for (ii = 0; ii < part_picker_with_map->priv->n_map_elems; ii++) {
		gboolean matches;

		if (part_picker_with_map->priv->map[ii].matches_func) {
			matches = part_picker_with_map->priv->map[ii].matches_func (part_picker_with_map->priv->map[ii].value, value);
		} else {
			matches = value == part_picker_with_map->priv->map[ii].value;
		}

		if (matches) {
			*out_id = g_strdup_printf ("%d", ii);
			return TRUE;
		}
	}

	return FALSE;
}

static void
ecepp_picker_with_map_set_to_component (ECompEditorPropertyPartPicker *part_picker,
					const gchar *id,
					icalcomponent *component)
{
	ECompEditorPropertyPartPickerWithMap *part_picker_with_map;
	icalproperty *prop;
	gint ii, value;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker));
	g_return_if_fail (id != NULL);
	g_return_if_fail (component != NULL);

	part_picker_with_map = E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker);
	g_return_if_fail (part_picker_with_map->priv->map != NULL);
	g_return_if_fail (part_picker_with_map->priv->n_map_elems > 0);
	g_return_if_fail (part_picker_with_map->priv->ical_prop_kind != ICAL_NO_PROPERTY);
	g_return_if_fail (part_picker_with_map->priv->ical_new_func != NULL);
	g_return_if_fail (part_picker_with_map->priv->ical_set_func != NULL);

	ii = (gint) g_ascii_strtoll (id, NULL, 10);
	g_return_if_fail (ii >= 0 && ii < part_picker_with_map->priv->n_map_elems);

	prop = icalcomponent_get_first_property (component, part_picker_with_map->priv->ical_prop_kind);
	value = part_picker_with_map->priv->map[ii].value;

	if (part_picker_with_map->priv->map[ii].delete_prop) {
		if (prop) {
			icalcomponent_remove_property (component, prop);
			icalproperty_free (prop);
		}
	} else if (prop) {
		part_picker_with_map->priv->ical_set_func (prop, value);
	} else {
		prop = part_picker_with_map->priv->ical_new_func (value);
		icalcomponent_add_property (component, prop);
	}
}

static void
ecepp_picker_with_map_create_widgets (ECompEditorPropertyPart *property_part,
				      GtkWidget **out_label_widget,
				      GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartPickerWithMap *part_picker_with_map;
	ECompEditorPropertyPartClass *part_class;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (property_part));
	g_return_if_fail (out_label_widget != NULL);
	g_return_if_fail (out_edit_widget != NULL);

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (e_comp_editor_property_part_picker_with_map_parent_class);
	g_return_if_fail (part_class != NULL);
	g_return_if_fail (part_class->create_widgets != NULL);

	*out_label_widget = NULL;

	part_class->create_widgets (property_part, out_label_widget, out_edit_widget);
	g_return_if_fail (*out_label_widget == NULL);
	g_return_if_fail (*out_edit_widget != NULL);

	part_picker_with_map = E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (property_part);

	*out_label_widget = gtk_label_new_with_mnemonic (part_picker_with_map->priv->label);
	gtk_label_set_mnemonic_widget (GTK_LABEL (*out_label_widget), *out_edit_widget);

	g_object_set (G_OBJECT (*out_label_widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_widget_show (*out_label_widget);
}

static void
ecepp_picker_with_map_set_property (GObject *object,
				    guint property_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	ECompEditorPropertyPartPickerWithMap *part_picker_with_map;
	gint ii;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (object));

	part_picker_with_map = E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (object);

	switch (property_id) {
		case PICKER_WITH_MAP_PROP_MAP:
			g_return_if_fail (part_picker_with_map->priv->map == NULL);

			part_picker_with_map->priv->map = g_value_get_pointer (value);
			for (ii = 0; part_picker_with_map->priv->map[ii].description; ii++) {
				/* pre-count elements */
			}

			part_picker_with_map->priv->n_map_elems = ii;
			return;

		case PICKER_WITH_MAP_PROP_LABEL:
			g_free (part_picker_with_map->priv->label);
			part_picker_with_map->priv->label = g_value_dup_string (value);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
ecepp_picker_with_map_finalize (GObject *object)
{
	ECompEditorPropertyPartPickerWithMap *part_picker_with_map =
		E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (object);

	if (part_picker_with_map->priv->map && part_picker_with_map->priv->n_map_elems > 0) {
		gint ii;

		for (ii = 0; ii < part_picker_with_map->priv->n_map_elems; ii++) {
			g_free ((gchar *) part_picker_with_map->priv->map[ii].description);
		}

		g_free (part_picker_with_map->priv->map);
		part_picker_with_map->priv->map = NULL;
	}

	g_free (part_picker_with_map->priv->label);
	part_picker_with_map->priv->label = NULL;

	G_OBJECT_CLASS (e_comp_editor_property_part_picker_with_map_parent_class)->finalize (object);
}

static void
e_comp_editor_property_part_picker_with_map_init (ECompEditorPropertyPartPickerWithMap *part_picker_with_map)
{
	part_picker_with_map->priv = G_TYPE_INSTANCE_GET_PRIVATE (part_picker_with_map,
		E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP,
		ECompEditorPropertyPartPickerWithMapPrivate);
}

static void
e_comp_editor_property_part_picker_with_map_class_init (ECompEditorPropertyPartPickerWithMapClass *klass)
{
	ECompEditorPropertyPartPickerClass *part_picker_class;
	ECompEditorPropertyPartClass *part_class;
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (ECompEditorPropertyPartPickerWithMapPrivate));

	part_picker_class = E_COMP_EDITOR_PROPERTY_PART_PICKER_CLASS (klass);
	part_picker_class->get_values = ecepp_picker_with_map_get_values;
	part_picker_class->get_from_component = ecepp_picker_with_map_get_from_component;
	part_picker_class->set_to_component = ecepp_picker_with_map_set_to_component;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_picker_with_map_create_widgets;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = ecepp_picker_with_map_set_property;
	object_class->finalize = ecepp_picker_with_map_finalize;

	g_object_class_install_property (
		object_class,
		PICKER_WITH_MAP_PROP_MAP,
		g_param_spec_pointer (
			"map",
			"Map",
			"Map of values, .description-NULL-terminated",
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PICKER_WITH_MAP_PROP_LABEL,
		g_param_spec_string (
			"label",
			"Label",
			"Label of the picker",
			NULL,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

ECompEditorPropertyPart *
e_comp_editor_property_part_picker_with_map_new (const ECompEditorPropertyPartPickerMap map[],
						 gint n_map_elements,
						 const gchar *label,
						 icalproperty_kind ical_prop_kind,
						 ECompEditorPropertyPartPickerMapICalNewFunc ical_new_func,
						 ECompEditorPropertyPartPickerMapICalSetFunc ical_set_func,
						 ECompEditorPropertyPartPickerMapICalGetFunc ical_get_func)
{
	ECompEditorPropertyPartPickerWithMap *part_picker_with_map;
	ECompEditorPropertyPartPickerMap *map_copy;
	ECompEditorPropertyPart *property_part;
	gint ii;

	g_return_val_if_fail (map != NULL, NULL);
	g_return_val_if_fail (n_map_elements > 0, NULL);
	g_return_val_if_fail (label != NULL, NULL);
	g_return_val_if_fail (ical_prop_kind != ICAL_NO_PROPERTY, NULL);
	g_return_val_if_fail (ical_new_func != NULL, NULL);
	g_return_val_if_fail (ical_set_func != NULL, NULL);
	g_return_val_if_fail (ical_get_func != NULL, NULL);

	map_copy = g_new0 (ECompEditorPropertyPartPickerMap, n_map_elements + 1);
	for (ii = 0; ii < n_map_elements; ii++) {
		map_copy[ii] = map[ii];
		map_copy[ii].description = g_strdup (map[ii].description);
	}

	property_part = g_object_new (E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP,
		"map", map_copy,
		"label", label,
		NULL);

	part_picker_with_map = E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (property_part);

	part_picker_with_map->priv->ical_prop_kind = ical_prop_kind;
	part_picker_with_map->priv->ical_new_func = ical_new_func;
	part_picker_with_map->priv->ical_set_func = ical_set_func;
	part_picker_with_map->priv->ical_get_func = ical_get_func;

	return property_part;
}

gint
e_comp_editor_property_part_picker_with_map_get_selected (ECompEditorPropertyPartPickerWithMap *part_picker_with_map)
{
	gint ii;
	const gchar *id;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker_with_map), -1);
	g_return_val_if_fail (part_picker_with_map->priv->map != NULL, -1);

	id = e_comp_editor_property_part_picker_get_selected_id (E_COMP_EDITOR_PROPERTY_PART_PICKER (part_picker_with_map));
	if (!id)
		return -1;

	ii = (gint) g_ascii_strtoll (id, NULL, 10);
	if (ii < 0 || ii >= part_picker_with_map->priv->n_map_elems)
		return -1;

	return part_picker_with_map->priv->map[ii].value;
}

void
e_comp_editor_property_part_picker_with_map_set_selected (ECompEditorPropertyPartPickerWithMap *part_picker_with_map,
							  gint value)
{
	gint ii;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker_with_map));
	g_return_if_fail (part_picker_with_map->priv->map != NULL);

	for (ii = 0; ii < part_picker_with_map->priv->n_map_elems; ii++) {
		if (part_picker_with_map->priv->map[ii].value == value) {
			gchar *id;

			id = g_strdup_printf ("%d", ii);
			e_comp_editor_property_part_picker_set_selected_id (
				E_COMP_EDITOR_PROPERTY_PART_PICKER (part_picker_with_map), id);
			g_free (id);

			return;
		}
	}

	g_warn_if_reached ();
}

/* ************************************************************************* */
