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

#include "calendar-config.h"
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

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ECompEditorPropertyPart, e_comp_editor_property_part, G_TYPE_OBJECT)

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
e_comp_editor_property_part_impl_sensitize_widgets (ECompEditorPropertyPart *property_part,
						    gboolean force_insensitive)
{
	GtkWidget *widget;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part));

	widget = e_comp_editor_property_part_get_label_widget (property_part);
	if (widget)
		gtk_widget_set_sensitive (widget, !force_insensitive);

	widget = e_comp_editor_property_part_get_edit_widget (property_part);
	if (widget) {
		if (GTK_IS_ENTRY (widget))
			g_object_set (G_OBJECT (widget), "editable", !force_insensitive, NULL);
		else
			gtk_widget_set_sensitive (widget, !force_insensitive);
	}
}

static void
e_comp_editor_property_part_init (ECompEditorPropertyPart *property_part)
{
	property_part->priv = e_comp_editor_property_part_get_instance_private (property_part);
	property_part->priv->visible = TRUE;
	property_part->priv->sensitize_handled = FALSE;
}

static void
e_comp_editor_property_part_class_init (ECompEditorPropertyPartClass *klass)
{
	GObjectClass *object_class;

	klass->sensitize_widgets = e_comp_editor_property_part_impl_sensitize_widgets;

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
	g_return_if_fail (klass != NULL);
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
					 ICalComponent *component)
{
	ECompEditorPropertyPartClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part));

	klass = E_COMP_EDITOR_PROPERTY_PART_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->fill_widget != NULL);

	klass->fill_widget (property_part, component);
}

void
e_comp_editor_property_part_fill_component (ECompEditorPropertyPart *property_part,
					    ICalComponent *component)
{
	ECompEditorPropertyPartClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part));

	klass = E_COMP_EDITOR_PROPERTY_PART_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->fill_component != NULL);

	klass->fill_component (property_part, component);
}

void
e_comp_editor_property_part_sensitize_widgets (ECompEditorPropertyPart *property_part,
					       gboolean force_insensitive)
{
	ECompEditorPropertyPartClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part));

	if (e_comp_editor_property_part_get_sensitize_handled (property_part))
		return;

	klass = E_COMP_EDITOR_PROPERTY_PART_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);

	if (klass->sensitize_widgets)
		klass->sensitize_widgets (property_part, force_insensitive);
}

void
e_comp_editor_property_part_emit_changed (ECompEditorPropertyPart *property_part)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (property_part));

	g_signal_emit (property_part, property_part_signals[PROPERTY_PART_CHANGED], 0, NULL);
}

/* ************************************************************************* */

struct _ECompEditorPropertyPartStringPrivate {
	gboolean is_multivalue;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ECompEditorPropertyPartString, e_comp_editor_property_part_string, E_TYPE_COMP_EDITOR_PROPERTY_PART)

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
	g_return_if_fail (klass->entry_type > 0);

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
			  ICalComponent *component)
{
	ECompEditorPropertyPartStringClass *klass;
	GtkWidget *edit_widget;
	ICalProperty *prop;
	gchar *text = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (property_part));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	edit_widget = e_comp_editor_property_part_string_get_real_edit_widget (E_COMP_EDITOR_PROPERTY_PART_STRING (property_part));
	g_return_if_fail (GTK_IS_ENTRY (edit_widget) || GTK_IS_TEXT_VIEW (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_STRING_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->prop_kind != I_CAL_NO_PROPERTY);
	g_return_if_fail (klass->i_cal_get_func != NULL);

	if (e_comp_editor_property_part_string_is_multivalue (E_COMP_EDITOR_PROPERTY_PART_STRING (property_part))) {
		GString *multivalue = NULL;

		for (prop = i_cal_component_get_first_property (component, klass->prop_kind);
		     prop;
		     g_object_unref (prop), prop = i_cal_component_get_next_property (component, klass->prop_kind)) {
			const gchar *value;

			value = klass->i_cal_get_func (prop);

			if (!value || !*value)
				continue;

			if (!multivalue)
				multivalue = g_string_new ("");
			else if (multivalue->len)
				g_string_append_c (multivalue, ',');

			g_string_append (multivalue, value);
		}

		if (multivalue)
			text = g_string_free (multivalue, FALSE);
	} else {
		prop = i_cal_component_get_first_property (component, klass->prop_kind);
		if (prop) {
			text = g_strdup (klass->i_cal_get_func (prop));
			g_object_unref (prop);
		}
	}

	if (GTK_IS_ENTRY (edit_widget)) {
		gtk_entry_set_text (GTK_ENTRY (edit_widget), text ? text : "");
	} else /* if (GTK_IS_TEXT_VIEW (edit_widget)) */ {
		GtkTextBuffer *buffer;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit_widget));
		gtk_text_buffer_set_text (buffer, text ? text : "", -1);
	}

	e_widget_undo_reset (edit_widget);

	g_free (text);
}

static void
ecepp_string_fill_component (ECompEditorPropertyPart *property_part,
			     ICalComponent *component)
{
	ECompEditorPropertyPartStringClass *klass;
	GtkWidget *edit_widget;
	ICalProperty *prop;
	gchar *value;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (property_part));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	edit_widget = e_comp_editor_property_part_string_get_real_edit_widget (E_COMP_EDITOR_PROPERTY_PART_STRING (property_part));
	g_return_if_fail (GTK_IS_ENTRY (edit_widget) || GTK_IS_TEXT_VIEW (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_STRING_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->prop_kind != I_CAL_NO_PROPERTY);
	g_return_if_fail (klass->i_cal_new_func != NULL);
	g_return_if_fail (klass->i_cal_set_func != NULL);

	if (GTK_IS_ENTRY (edit_widget)) {
		value = g_strdup (gtk_entry_get_text (GTK_ENTRY (edit_widget)));
	} else /* if (GTK_IS_TEXT_VIEW (edit_widget)) */ {
		GtkTextBuffer *buffer;
		GtkTextIter text_iter_start, text_iter_end;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (edit_widget));
		gtk_text_buffer_get_start_iter (buffer, &text_iter_start);
		gtk_text_buffer_get_end_iter (buffer, &text_iter_end);
		value = gtk_text_buffer_get_text (buffer, &text_iter_start, &text_iter_end, FALSE);
	}

	if (e_comp_editor_property_part_string_is_multivalue (E_COMP_EDITOR_PROPERTY_PART_STRING (property_part))) {
		/* Clear all multivalues first */
		e_cal_util_component_remove_property_by_kind (component, klass->prop_kind, TRUE);

		if (value && *value) {
			gchar **split_value;

			split_value = g_strsplit (value, ",", -1);
			if (split_value) {
				gint ii;

				/* Store multivalue properties into multiple properties,
				   to workaround i_cal_component_clone() bug, which escapes
				   commas in such properties. */
				for (ii = 0; split_value[ii]; ii++) {
					const gchar *item = split_value[ii];

					if (*item) {
						prop = klass->i_cal_new_func (item);
						i_cal_component_take_property (component, prop);
					}
				}

				g_strfreev (split_value);
			}
		}
	} else {
		prop = i_cal_component_get_first_property (component, klass->prop_kind);

		if (value && *value) {
			if (prop) {
				klass->i_cal_set_func (prop, value);
				g_object_unref (prop);
			} else {
				prop = klass->i_cal_new_func (value);
				i_cal_component_take_property (component, prop);
			}
		} else if (prop) {
			i_cal_component_remove_property (component, prop);
			g_object_unref (prop);
		}
	}

	g_free (value);
}

static void
ecepp_string_sensitize_widgets (ECompEditorPropertyPart *property_part,
				gboolean force_insensitive)
{
	GtkWidget *widget;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (property_part));

	widget = e_comp_editor_property_part_get_label_widget (property_part);
	if (widget)
		gtk_widget_set_sensitive (widget, !force_insensitive);

	widget = e_comp_editor_property_part_string_get_real_edit_widget (E_COMP_EDITOR_PROPERTY_PART_STRING (property_part));
	g_return_if_fail (GTK_IS_ENTRY (widget) || GTK_IS_TEXT_VIEW (widget));

	g_object_set (G_OBJECT (widget), "editable", !force_insensitive, NULL);
}

static GtkWidget *
ecepp_string_get_real_edit_widget (ECompEditorPropertyPartString *part_string)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (part_string), NULL);

	return e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_string));
}

static void
e_comp_editor_property_part_string_init (ECompEditorPropertyPartString *part_string)
{
	part_string->priv = e_comp_editor_property_part_string_get_instance_private (part_string);
	part_string->priv->is_multivalue = FALSE;
}

static void
e_comp_editor_property_part_string_class_init (ECompEditorPropertyPartStringClass *klass)
{
	ECompEditorPropertyPartClass *part_class;

	klass->entry_type = GTK_TYPE_ENTRY;

	klass->prop_kind = I_CAL_NO_PROPERTY;
	klass->i_cal_new_func = NULL;
	klass->i_cal_set_func = NULL;
	klass->i_cal_get_func = NULL;
	klass->get_real_edit_widget = ecepp_string_get_real_edit_widget;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_string_create_widgets;
	part_class->fill_widget = ecepp_string_fill_widget;
	part_class->fill_component = ecepp_string_fill_component;
	part_class->sensitize_widgets = ecepp_string_sensitize_widgets;
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

	edit_widget = e_comp_editor_property_part_string_get_real_edit_widget (part_string);

	if (edit_widget)
		e_widget_undo_attach (edit_widget, focus_tracker);
}

void
e_comp_editor_property_part_string_set_is_multivalue (ECompEditorPropertyPartString *part_string,
						      gboolean is_multivalue)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (part_string));

	part_string->priv->is_multivalue = is_multivalue;
}

gboolean
e_comp_editor_property_part_string_is_multivalue (ECompEditorPropertyPartString *part_string)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (part_string), FALSE);

	return part_string->priv->is_multivalue;
}

GtkWidget *
e_comp_editor_property_part_string_get_real_edit_widget (ECompEditorPropertyPartString *part_string)
{
	ECompEditorPropertyPartStringClass *klass;
	GtkWidget *edit_widget;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (part_string), NULL);

	klass = E_COMP_EDITOR_PROPERTY_PART_STRING_GET_CLASS (part_string);
	g_return_val_if_fail (klass != NULL, NULL);
	g_return_val_if_fail (klass->get_real_edit_widget != NULL, NULL);

	edit_widget = klass->get_real_edit_widget (part_string);

	if (GTK_IS_SCROLLED_WINDOW (edit_widget))
		edit_widget = gtk_bin_get_child (GTK_BIN (edit_widget));

	return edit_widget;
}

/* ************************************************************************* */

struct _ECompEditorPropertyPartDatetimePrivate {
	GWeakRef timezone_entry;
};

enum {
	ECEPP_DATETIME_LOOKUP_TIMEZONE,
	ECEPP_DATETIME_LAST_SIGNAL
};

static guint ecepp_datetime_signals[ECEPP_DATETIME_LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ECompEditorPropertyPartDatetime, e_comp_editor_property_part_datetime, E_TYPE_COMP_EDITOR_PROPERTY_PART)

static ICalTimezone *
ecepp_datetime_lookup_timezone (ECompEditorPropertyPartDatetime *part_datetime,
				const gchar *tzid)
{
	ICalTimezone *zone = NULL;

	if (!tzid || !*tzid)
		return NULL;

	g_signal_emit (part_datetime, ecepp_datetime_signals[ECEPP_DATETIME_LOOKUP_TIMEZONE], 0, tzid, &zone);

	return zone;
}

static struct tm
ecepp_datetime_get_current_time_cb (EDateEdit *date_edit,
				    gpointer user_data)
{
	GWeakRef *weakref = user_data;
	ECompEditorPropertyPartDatetime *part_datetime;
	ICalTime *today = NULL;
	struct tm tm;

	memset (&tm, 0, sizeof (struct tm));

	g_return_val_if_fail (weakref != NULL, tm);

	part_datetime = g_weak_ref_get (weakref);
	if (part_datetime) {
		ETimezoneEntry *timezone_entry = g_weak_ref_get (&part_datetime->priv->timezone_entry);
		ICalTimezone *editor_zone = NULL;

		if (timezone_entry)
			editor_zone = e_timezone_entry_get_timezone (timezone_entry);

		if (editor_zone)
			today = i_cal_time_new_current_with_zone (editor_zone);

		g_clear_object (&timezone_entry);
		g_object_unref (part_datetime);
	}

	if (!today)
		today = i_cal_time_new_current_with_zone (calendar_config_get_icaltimezone ());

	tm = e_cal_util_icaltime_to_tm (today);

	g_clear_object (&today);

	return tm;
}

static void
ecepp_datetime_changed_cb (ECompEditorPropertyPart *property_part)
{
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (property_part));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);

	if (!edit_widget ||
	    !e_date_edit_date_is_valid (E_DATE_EDIT (edit_widget)) ||
	    !e_date_edit_time_is_valid (E_DATE_EDIT (edit_widget)))
		return;

	e_comp_editor_property_part_emit_changed (property_part);
}

static void
ecepp_datetime_create_widgets (ECompEditorPropertyPart *property_part,
			       GtkWidget **out_label_widget,
			       GtkWidget **out_edit_widget)
{
	ECompEditorPropertyPartDatetimeClass *klass;
	EDateEdit *date_edit;
	const gchar *date_format;

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

	date_edit = E_DATE_EDIT (*out_edit_widget);

	e_date_edit_set_get_time_callback (date_edit,
		ecepp_datetime_get_current_time_cb,
		e_weak_ref_new (property_part), (GDestroyNotify) e_weak_ref_free);

	date_format = e_datetime_format_get_format ("calendar", "table",  DTFormatKindDate);
	/* the "%ad" is not a strftime format, thus avoid it, if included */
	if (date_format && *date_format && !strstr (date_format, "%ad"))
		e_date_edit_set_date_format (date_edit, date_format);

	g_signal_connect_object (*out_edit_widget, "changed",
		G_CALLBACK (ecepp_datetime_changed_cb), property_part, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	g_signal_connect_object (*out_edit_widget, "notify::show-time",
		G_CALLBACK (ecepp_datetime_changed_cb), property_part, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
}

static void
ecepp_datetime_fill_widget (ECompEditorPropertyPart *property_part,
			    ICalComponent *component)
{
	ECompEditorPropertyPartDatetime *part_datetime;
	ECompEditorPropertyPartDatetimeClass *klass;
	GtkWidget *edit_widget;
	ICalProperty *prop;
	ICalTime *value = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (property_part));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (E_IS_DATE_EDIT (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_DATETIME_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->prop_kind != I_CAL_NO_PROPERTY);
	g_return_if_fail (klass->i_cal_get_func != NULL);

	part_datetime = E_COMP_EDITOR_PROPERTY_PART_DATETIME (property_part);

	prop = i_cal_component_get_first_property (component, klass->prop_kind);
	if (prop) {
		ETimezoneEntry *timezone_entry = g_weak_ref_get (&part_datetime->priv->timezone_entry);

		value = klass->i_cal_get_func (prop);

		if (timezone_entry && value && !i_cal_time_is_date (value)) {
			ICalTimezone *editor_zone = e_timezone_entry_get_timezone (timezone_entry);

			/* Attempt to convert time to the zone used in the editor */
			if (editor_zone && !i_cal_time_get_timezone (value) && !i_cal_time_is_utc (value)) {
				ICalParameter *param;

				param = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);
				if (param) {
					const gchar *tzid;

					tzid = i_cal_parameter_get_tzid (param);

					if (tzid && *tzid) {
						if (editor_zone &&
						    (g_strcmp0 (i_cal_timezone_get_tzid (editor_zone), tzid) == 0 ||
						    g_strcmp0 (i_cal_timezone_get_location (editor_zone), tzid) == 0)) {
							i_cal_time_set_timezone (value, editor_zone);
						} else {
							i_cal_time_set_timezone (value, ecepp_datetime_lookup_timezone (part_datetime, tzid));
						}
					}

					g_object_unref (param);
				}
			}
		}

		g_clear_object (&timezone_entry);
		g_clear_object (&prop);
	}

	if (!value)
		value = i_cal_time_new_null_time ();

	e_comp_editor_property_part_datetime_set_value (part_datetime, value);

	g_clear_object (&value);
}

static void
ecepp_datetime_fill_component (ECompEditorPropertyPart *property_part,
			       ICalComponent *component)
{
	ECompEditorPropertyPartDatetime *part_datetime;
	ECompEditorPropertyPartDatetimeClass *klass;
	GtkWidget *edit_widget;
	EDateEdit *date_edit;
	ICalProperty *prop;
	ICalTime *value;
	time_t tt;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (property_part));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (E_IS_DATE_EDIT (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_DATETIME_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->prop_kind != I_CAL_NO_PROPERTY);
	g_return_if_fail (klass->i_cal_new_func != NULL);
	g_return_if_fail (klass->i_cal_get_func != NULL);
	g_return_if_fail (klass->i_cal_set_func != NULL);

	part_datetime = E_COMP_EDITOR_PROPERTY_PART_DATETIME (property_part);
	date_edit = E_DATE_EDIT (edit_widget);
	tt = e_date_edit_get_time (date_edit);

	prop = i_cal_component_get_first_property (component, klass->prop_kind);

	if (e_date_edit_get_allow_no_date_set (date_edit) && tt == (time_t) -1) {
		if (prop) {
			i_cal_component_remove_property (component, prop);
			g_object_unref (prop);
		}
	} else {
		ICalTimezone *zone;

		value = e_comp_editor_property_part_datetime_get_value (part_datetime);

		zone = value && !i_cal_time_is_null_time (value) ? i_cal_time_get_timezone (value) : NULL;

		if (zone)
			g_object_ref (zone);

		if (prop) {
			/* Remove the VALUE parameter, to correspond to the actual value being set */
			i_cal_property_remove_parameter_by_kind (prop, I_CAL_VALUE_PARAMETER);

			klass->i_cal_set_func (prop, value);

			/* Re-read the value, because it could be changed by the descendant */
			g_clear_object (&value);
			value = klass->i_cal_get_func (prop);

			/* The timezone can be dropped since libical 3.0.14, thus restore it
			   before updating the TZID parameter */
			if (zone && value && !i_cal_time_is_null_time (value) && !i_cal_time_is_date (value))
				i_cal_time_set_timezone (value, zone);

			cal_comp_util_update_tzid_parameter (prop, value);
		} else {
			prop = klass->i_cal_new_func (value);

			/* Re-read the value, because it could be changed by the descendant */
			g_clear_object (&value);
			value = klass->i_cal_get_func (prop);

			/* The timezone can be dropped since libical 3.0.14, thus restore it
			   before updating the TZID parameter */
			if (zone && value && !i_cal_time_is_null_time (value) && !i_cal_time_is_date (value))
				i_cal_time_set_timezone (value, zone);

			cal_comp_util_update_tzid_parameter (prop, value);
			i_cal_component_add_property (component, prop);
		}

		g_clear_object (&value);
		g_clear_object (&prop);
		g_clear_object (&zone);
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
	part_datetime->priv = e_comp_editor_property_part_datetime_get_instance_private (part_datetime);

	g_weak_ref_init (&part_datetime->priv->timezone_entry, NULL);
}

static void
e_comp_editor_property_part_datetime_class_init (ECompEditorPropertyPartDatetimeClass *klass)
{
	ECompEditorPropertyPartClass *part_class;
	GObjectClass *object_class;

	klass->prop_kind = I_CAL_NO_PROPERTY;
	klass->i_cal_new_func = NULL;
	klass->i_cal_set_func = NULL;
	klass->i_cal_get_func = NULL;

	part_class = E_COMP_EDITOR_PROPERTY_PART_CLASS (klass);
	part_class->create_widgets = ecepp_datetime_create_widgets;
	part_class->fill_widget = ecepp_datetime_fill_widget;
	part_class->fill_component = ecepp_datetime_fill_component;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = ecepp_datetime_finalize;

	/* ICalTimezone *lookup_timezone (datetime, const gchar *tzid); */
	ecepp_datetime_signals[ECEPP_DATETIME_LOOKUP_TIMEZONE] = g_signal_new (
		"lookup-timezone",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_ACTION,
		0,
		NULL, NULL, NULL,
		G_TYPE_POINTER, 1,
		G_TYPE_STRING);
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
						const ICalTime *value)
{
	GtkWidget *edit_widget;
	EDateEdit *date_edit;
	ICalTime *tmp_value = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (part_datetime));

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_datetime));
	g_return_if_fail (E_IS_DATE_EDIT (edit_widget));

	date_edit = E_DATE_EDIT (edit_widget);

	if (!e_date_edit_get_allow_no_date_set (date_edit) && (!value || i_cal_time_is_null_time (value) ||
	    !i_cal_time_is_valid_time (value))) {
		tmp_value = i_cal_time_new_current_with_zone (i_cal_timezone_get_utc_timezone ());
		value = tmp_value;
	}

	if (!value || i_cal_time_is_null_time (value) ||
	    !i_cal_time_is_valid_time (value)) {
		e_date_edit_set_time (date_edit, (time_t) -1);
	} else {
		ICalTimezone *zone;

		zone = i_cal_time_get_timezone (value);

		/* Convert to the same time zone as the editor uses, if different */
		if (!i_cal_time_is_date (value) && zone) {
			ETimezoneEntry *timezone_entry = g_weak_ref_get (&part_datetime->priv->timezone_entry);

			if (timezone_entry) {
				ICalTimezone *editor_zone = e_timezone_entry_get_timezone (timezone_entry);

				if (editor_zone && zone != editor_zone &&
				    g_strcmp0 (i_cal_timezone_get_tzid (editor_zone), i_cal_timezone_get_tzid (zone)) != 0 &&
				    g_strcmp0 (i_cal_timezone_get_location (editor_zone), i_cal_timezone_get_location (zone)) != 0) {
					if (tmp_value != value) {
						tmp_value = i_cal_time_clone (value);
						value = tmp_value;
					}

					i_cal_time_convert_timezone (tmp_value, zone, editor_zone);
					i_cal_time_set_timezone (tmp_value, editor_zone);
				}
			}

			g_clear_object (&timezone_entry);
		}

		e_date_edit_set_date (date_edit, i_cal_time_get_year (value), i_cal_time_get_month (value), i_cal_time_get_day (value));

		if (!i_cal_time_is_date (value))
			e_date_edit_set_time_of_day (date_edit, i_cal_time_get_hour (value), i_cal_time_get_minute (value));
		else if (e_date_edit_get_show_time (date_edit) && e_date_edit_get_allow_no_date_set (date_edit))
			e_date_edit_set_time_of_day (date_edit, -1, -1);
		else
			e_comp_editor_property_part_datetime_set_date_only (part_datetime, TRUE);
	}

	g_clear_object (&tmp_value);
}

ICalTime *
e_comp_editor_property_part_datetime_get_value (ECompEditorPropertyPartDatetime *part_datetime)
{
	ETimezoneEntry *timezone_entry = NULL;
	GtkWidget *edit_widget;
	EDateEdit *date_edit;
	ICalTime *value = i_cal_time_new_null_time ();
	gint year, month, day;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (part_datetime), value);

	edit_widget = e_comp_editor_property_part_get_edit_widget (E_COMP_EDITOR_PROPERTY_PART (part_datetime));
	g_return_val_if_fail (E_IS_DATE_EDIT (edit_widget), value);

	date_edit = E_DATE_EDIT (edit_widget);

	if (!e_date_edit_get_date (date_edit, &year, &month, &day))
		return value;

	i_cal_time_set_date (value, year, month, day);

	if (!e_date_edit_get_show_time (date_edit)) {
		i_cal_time_set_is_date (value, TRUE);
	} else {
		gint hour, minute;

		i_cal_time_set_timezone (value, NULL);
		i_cal_time_set_is_date (value, !e_date_edit_get_time_of_day (date_edit, &hour, &minute));

		if (!i_cal_time_is_date (value)) {
			i_cal_time_set_time (value, hour, minute, 0);

			timezone_entry = g_weak_ref_get (&part_datetime->priv->timezone_entry);
			if (timezone_entry) {
				ICalTimezone *zone, *utc_zone;

				utc_zone = i_cal_timezone_get_utc_timezone ();
				zone = e_timezone_entry_get_timezone (timezone_entry);

				/* It's required to have set the built-in UTC zone, not its copy,
				   thus libical knows that it's a UTC time, not a time with UTC TZID. */
				if (zone && g_strcmp0 (i_cal_timezone_get_tzid (utc_zone), i_cal_timezone_get_tzid (zone)) == 0)
					zone = utc_zone;

				i_cal_time_set_timezone (value, zone);
			}
		}
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

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ECompEditorPropertyPartSpin, e_comp_editor_property_part_spin, E_TYPE_COMP_EDITOR_PROPERTY_PART)

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
			ICalComponent *component)
{
	ECompEditorPropertyPartSpinClass *klass;
	GtkWidget *edit_widget;
	ICalProperty *prop;
	gint value;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_SPIN (property_part));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_SPIN_BUTTON (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_SPIN_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->prop_kind != I_CAL_NO_PROPERTY);
	g_return_if_fail (klass->i_cal_get_func != NULL);

	prop = i_cal_component_get_first_property (component, klass->prop_kind);
	if (prop) {
		value = klass->i_cal_get_func (prop);
		g_object_unref (prop);
	} else {
		gdouble d_min, d_max;

		gtk_spin_button_get_range (GTK_SPIN_BUTTON (edit_widget), &d_min, &d_max);

		value = (gint) d_min;
	}

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (edit_widget), value);
}

static void
ecepp_spin_fill_component (ECompEditorPropertyPart *property_part,
			   ICalComponent *component)
{
	ECompEditorPropertyPartSpinClass *klass;
	GtkWidget *edit_widget;
	ICalProperty *prop;
	gint value;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_SPIN (property_part));
	g_return_if_fail (I_CAL_COMPONENT (component));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_SPIN_BUTTON (edit_widget));

	klass = E_COMP_EDITOR_PROPERTY_PART_SPIN_GET_CLASS (property_part);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->prop_kind != I_CAL_NO_PROPERTY);
	g_return_if_fail (klass->i_cal_new_func != NULL);
	g_return_if_fail (klass->i_cal_set_func != NULL);

	value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (edit_widget));
	prop = i_cal_component_get_first_property (component, klass->prop_kind);

	if (prop) {
		klass->i_cal_set_func (prop, value);
	} else {
		prop = klass->i_cal_new_func (value);
		i_cal_component_add_property (component, prop);
	}

	g_clear_object (&prop);
}

static void
e_comp_editor_property_part_spin_init (ECompEditorPropertyPartSpin *part_spin)
{
	part_spin->priv = e_comp_editor_property_part_spin_get_instance_private (part_spin);
}

static void
e_comp_editor_property_part_spin_class_init (ECompEditorPropertyPartSpinClass *klass)
{
	ECompEditorPropertyPartClass *part_class;

	klass->prop_kind = I_CAL_NO_PROPERTY;
	klass->i_cal_new_func = NULL;
	klass->i_cal_set_func = NULL;
	klass->i_cal_get_func = NULL;

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

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ECompEditorPropertyPartPicker, e_comp_editor_property_part_picker, E_TYPE_COMP_EDITOR_PROPERTY_PART)

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
			  ICalComponent *component)
{
	GtkWidget *edit_widget;
	gchar *id = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER (property_part));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

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
			     ICalComponent *component)
{
	GtkWidget *edit_widget;
	const gchar *id;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER (property_part));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	edit_widget = e_comp_editor_property_part_get_edit_widget (property_part);
	g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (edit_widget));

	id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (edit_widget));
	if (id) {
		e_comp_editor_property_part_picker_set_to_component (
			E_COMP_EDITOR_PROPERTY_PART_PICKER (property_part),
			id, component);
	}
}

static void
e_comp_editor_property_part_picker_init (ECompEditorPropertyPartPicker *part_picker)
{
	part_picker->priv = e_comp_editor_property_part_picker_get_instance_private (part_picker);
}

static void
e_comp_editor_property_part_picker_class_init (ECompEditorPropertyPartPickerClass *klass)
{
	ECompEditorPropertyPartClass *part_class;

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
						       ICalComponent *component,
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
						     ICalComponent *component)
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

	ICalPropertyKind prop_kind;
	ECompEditorPropertyPartPickerMapICalNewFunc i_cal_new_func;
	ECompEditorPropertyPartPickerMapICalSetFunc i_cal_set_func;
	ECompEditorPropertyPartPickerMapICalGetFunc i_cal_get_func;
};

enum {
	PICKER_WITH_MAP_PROP_0,
	PICKER_WITH_MAP_PROP_MAP,
	PICKER_WITH_MAP_PROP_LABEL
};

G_DEFINE_TYPE_WITH_PRIVATE (ECompEditorPropertyPartPickerWithMap, e_comp_editor_property_part_picker_with_map, E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER)

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
					  ICalComponent *component,
					  gchar **out_id)
{
	ECompEditorPropertyPartPickerWithMap *part_picker_with_map;
	ICalProperty *prop;
	gint ii, value;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);
	g_return_val_if_fail (out_id != NULL, FALSE);

	part_picker_with_map = E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker);
	g_return_val_if_fail (part_picker_with_map->priv->map != NULL, FALSE);
	g_return_val_if_fail (part_picker_with_map->priv->n_map_elems > 0, FALSE);
	g_return_val_if_fail (part_picker_with_map->priv->prop_kind != I_CAL_NO_PROPERTY, FALSE);
	g_return_val_if_fail (part_picker_with_map->priv->i_cal_get_func != NULL, FALSE);

	prop = i_cal_component_get_first_property (component, part_picker_with_map->priv->prop_kind);
	if (!prop)
		return FALSE;

	value = part_picker_with_map->priv->i_cal_get_func (prop);
	g_clear_object (&prop);

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
					ICalComponent *component)
{
	ECompEditorPropertyPartPickerWithMap *part_picker_with_map;
	ICalProperty *prop;
	gint ii, value;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker));
	g_return_if_fail (id != NULL);
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	part_picker_with_map = E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (part_picker);
	g_return_if_fail (part_picker_with_map->priv->map != NULL);
	g_return_if_fail (part_picker_with_map->priv->n_map_elems > 0);
	g_return_if_fail (part_picker_with_map->priv->prop_kind != I_CAL_NO_PROPERTY);
	g_return_if_fail (part_picker_with_map->priv->i_cal_new_func != NULL);
	g_return_if_fail (part_picker_with_map->priv->i_cal_set_func != NULL);

	ii = (gint) g_ascii_strtoll (id, NULL, 10);
	g_return_if_fail (ii >= 0 && ii < part_picker_with_map->priv->n_map_elems);

	prop = i_cal_component_get_first_property (component, part_picker_with_map->priv->prop_kind);
	value = part_picker_with_map->priv->map[ii].value;

	if (part_picker_with_map->priv->map[ii].delete_prop) {
		if (prop)
			i_cal_component_remove_property (component, prop);
	} else if (prop) {
		part_picker_with_map->priv->i_cal_set_func (prop, value);
	} else {
		prop = part_picker_with_map->priv->i_cal_new_func (value);
		i_cal_component_add_property (component, prop);
	}

	g_clear_object (&prop);
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
	part_picker_with_map->priv = e_comp_editor_property_part_picker_with_map_get_instance_private (part_picker_with_map);
}

static void
e_comp_editor_property_part_picker_with_map_class_init (ECompEditorPropertyPartPickerWithMapClass *klass)
{
	ECompEditorPropertyPartPickerClass *part_picker_class;
	ECompEditorPropertyPartClass *part_class;
	GObjectClass *object_class;

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
						 ICalPropertyKind prop_kind,
						 ECompEditorPropertyPartPickerMapICalNewFunc i_cal_new_func,
						 ECompEditorPropertyPartPickerMapICalSetFunc i_cal_set_func,
						 ECompEditorPropertyPartPickerMapICalGetFunc i_cal_get_func)
{
	ECompEditorPropertyPartPickerWithMap *part_picker_with_map;
	ECompEditorPropertyPartPickerMap *map_copy;
	ECompEditorPropertyPart *property_part;
	gint ii;

	g_return_val_if_fail (map != NULL, NULL);
	g_return_val_if_fail (n_map_elements > 0, NULL);
	g_return_val_if_fail (label != NULL, NULL);
	g_return_val_if_fail (prop_kind != I_CAL_NO_PROPERTY, NULL);
	g_return_val_if_fail (i_cal_new_func != NULL, NULL);
	g_return_val_if_fail (i_cal_set_func != NULL, NULL);
	g_return_val_if_fail (i_cal_get_func != NULL, NULL);

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

	part_picker_with_map->priv->prop_kind = prop_kind;
	part_picker_with_map->priv->i_cal_new_func = i_cal_new_func;
	part_picker_with_map->priv->i_cal_set_func = i_cal_set_func;
	part_picker_with_map->priv->i_cal_get_func = i_cal_get_func;

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

void
e_comp_editor_property_part_util_ensure_same_value_type (ECompEditorPropertyPart *src_datetime,
							 ECompEditorPropertyPart *des_datetime)
{
	ECompEditorPropertyPartDatetime *src_dtm, *des_dtm;
	ICalTime *src_tt, *des_tt;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (src_datetime));
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (des_datetime));

	src_dtm = E_COMP_EDITOR_PROPERTY_PART_DATETIME (src_datetime);
	des_dtm = E_COMP_EDITOR_PROPERTY_PART_DATETIME (des_datetime);

	src_tt = e_comp_editor_property_part_datetime_get_value (src_dtm);
	des_tt = e_comp_editor_property_part_datetime_get_value (des_dtm);

	if (!src_tt || !des_tt ||
	    i_cal_time_is_null_time (src_tt) ||
	    i_cal_time_is_null_time (des_tt) ||
	    !i_cal_time_is_valid_time (src_tt) ||
	    !i_cal_time_is_valid_time (des_tt)) {
		g_clear_object (&src_tt);
		g_clear_object (&des_tt);
		return;
	}

	if (i_cal_time_is_date (src_tt) != i_cal_time_is_date (des_tt)) {
		gint hour = 0, minute = 0, second = 0;

		i_cal_time_set_is_date (des_tt, i_cal_time_is_date (src_tt));

		if (!i_cal_time_is_date (des_tt)) {
			i_cal_time_get_time (src_tt, &hour, &minute, &second);
			i_cal_time_set_time (des_tt, hour, minute, second);
		}

		e_comp_editor_property_part_datetime_set_value (des_dtm, des_tt);
	}

	g_clear_object (&src_tt);
	g_clear_object (&des_tt);
}

static gboolean
ecepp_check_start_before_end (ICalComponent *icomp,
			      ICalTime **pstart_tt,
			      ICalTime **pend_tt,
			      gboolean adjust_end_time,
			      gint *inout_last_duration)
{
	ICalTime *start_tt, *end_tt, *end_tt_copy;
	ICalTimezone *start_zone, *end_zone;
	gint duration = -1;
	gint cmp;

	start_tt = *pstart_tt;
	end_tt = *pend_tt;

	if (*inout_last_duration >= 0) {
		duration = *inout_last_duration;
	} else {
		if (icomp &&
		    e_cal_util_component_has_property (icomp, I_CAL_DTSTART_PROPERTY) &&
		    (e_cal_util_component_has_property (icomp, I_CAL_DTEND_PROPERTY) ||
		     e_cal_util_component_has_property (icomp, I_CAL_DUE_PROPERTY))) {
			ICalTime *orig_start, *orig_end;

			orig_start = i_cal_component_get_dtstart (icomp);
			if (e_cal_util_component_has_property (icomp, I_CAL_DTEND_PROPERTY))
				orig_end = i_cal_component_get_dtend (icomp);
			else
				orig_end = i_cal_component_get_due (icomp);

			if (orig_start && i_cal_time_is_valid_time (orig_start) &&
			    orig_end && i_cal_time_is_valid_time (orig_end)) {
				duration = i_cal_time_as_timet (orig_end) - i_cal_time_as_timet (orig_start);
				*inout_last_duration = duration;
			}

			g_clear_object (&orig_start);
			g_clear_object (&orig_end);
		}
	}

	start_zone = i_cal_time_get_timezone (start_tt);
	end_zone = i_cal_time_get_timezone (end_tt);

	/* Convert the end time to the same timezone as the start time. */
	end_tt_copy = i_cal_time_clone (end_tt);

	if (start_zone && end_zone && start_zone != end_zone)
		i_cal_time_convert_timezone (end_tt_copy, end_zone, start_zone);

	/* Now check if the start time is after the end time. If it is,
	 * we need to modify one of the times. */
	cmp = i_cal_time_compare (start_tt, end_tt_copy);
	if (cmp > 0) {
		if (adjust_end_time) {
			/* Try to switch only the date */
			i_cal_time_set_date (end_tt,
				i_cal_time_get_year (start_tt),
				i_cal_time_get_month (start_tt),
				i_cal_time_get_day (start_tt));

			g_clear_object (&end_tt_copy);
			end_tt_copy = i_cal_time_clone (end_tt);
			if (start_zone && end_zone && start_zone != end_zone)
				i_cal_time_convert_timezone (end_tt_copy, end_zone, start_zone);

			if (duration > 0)
				i_cal_time_adjust (end_tt_copy, 0, 0, 0, -duration);

			if (i_cal_time_compare (start_tt, end_tt_copy) >= 0) {
				g_clear_object (&end_tt);
				end_tt = i_cal_time_clone (start_tt);

				if (duration >= 0) {
					i_cal_time_adjust (end_tt, 0, 0, 0, duration);
				} else {
					/* Modify the end time, to be the start + 1 hour/day. */
					i_cal_time_adjust (end_tt, 0, i_cal_time_is_date (start_tt) ? 24 : 1, 0, 0);

					if (!i_cal_time_is_date (start_tt)) {
						GSettings *settings;
						gint shorten_by;
						gboolean shorten_end;

						settings = e_util_ref_settings ("org.gnome.evolution.calendar");
						shorten_by = g_settings_get_int (settings, "shorten-time");
						shorten_end = g_settings_get_int (settings, "shorten-time");
						g_clear_object (&settings);

						if (shorten_by > 0 && shorten_by < 60) {
							if (shorten_end)
								i_cal_time_adjust (end_tt, 0, 0, -shorten_by, 0);
							else
								i_cal_time_adjust (start_tt, 0, 0, shorten_by, 0);

							/* Revert the change, when it would make the time order reverse */
							if (i_cal_time_compare (start_tt, end_tt) >= 0) {
								if (shorten_end)
									i_cal_time_adjust (end_tt, 0, 0, shorten_by, 0);
								else
									i_cal_time_adjust (start_tt, 0, 0, -shorten_by, 0);
							}
						}
					}
				}

				if (start_zone && end_zone && start_zone != end_zone)
					i_cal_time_convert_timezone (end_tt, start_zone, end_zone);
			}
		} else {
			/* Try to switch only the date */
			i_cal_time_set_date (start_tt,
				i_cal_time_get_year (end_tt),
				i_cal_time_get_month (end_tt),
				i_cal_time_get_day (end_tt));

			if (i_cal_time_compare (start_tt, end_tt_copy) >= 0) {
				g_clear_object (&start_tt);
				start_tt = i_cal_time_clone (end_tt);

				if (duration >= 0) {
					i_cal_time_adjust (start_tt, 0, 0, 0, -duration);
				} else {
					/* Modify the start time, to be the end - 1 hour/day. */
					i_cal_time_adjust (start_tt, 0, i_cal_time_is_date (start_tt) ? -24 : -1, 0, 0);
				}

				if (start_zone && end_zone && start_zone != end_zone)
					i_cal_time_convert_timezone (start_tt, end_zone, start_zone);
			}
		}

		*pstart_tt = start_tt;
		*pend_tt = end_tt;

		g_clear_object (&end_tt_copy);

		return TRUE;
	}

	g_clear_object (&end_tt_copy);

	return FALSE;
}

void
e_comp_editor_property_part_util_ensure_start_before_end (ICalComponent *icomp,
							  ECompEditorPropertyPart *start_datetime,
							  ECompEditorPropertyPart *end_datetime,
							  gboolean change_end_datetime,
							  gint *inout_last_duration)
{
	ECompEditorPropertyPartDatetime *start_dtm, *end_dtm;
	ICalTime *start_tt, *end_tt;
	gboolean set_dtstart = FALSE, set_dtend = FALSE;

	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (start_datetime));
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (end_datetime));
	g_return_if_fail (inout_last_duration != NULL);

	start_dtm = E_COMP_EDITOR_PROPERTY_PART_DATETIME (start_datetime);
	end_dtm = E_COMP_EDITOR_PROPERTY_PART_DATETIME (end_datetime);

	start_tt = e_comp_editor_property_part_datetime_get_value (start_dtm);
	end_tt = e_comp_editor_property_part_datetime_get_value (end_dtm);

	if (!start_tt || !end_tt ||
	    i_cal_time_is_null_time (start_tt) ||
	    i_cal_time_is_null_time (end_tt) ||
	    !i_cal_time_is_valid_time (start_tt) ||
	    !i_cal_time_is_valid_time (end_tt)) {
		g_clear_object (&start_tt);
		g_clear_object (&end_tt);
		return;
	}

	if (i_cal_time_is_date (start_tt) || i_cal_time_is_date (end_tt)) {
		/* All Day Events are simple. We just compare the dates and if
		 * start > end we copy one of them to the other. */
		gint cmp;

		i_cal_time_set_is_date (start_tt, TRUE);
		i_cal_time_set_is_date (end_tt, TRUE);

		cmp = i_cal_time_compare_date_only (start_tt, end_tt);

		if (cmp > 0) {
			if (change_end_datetime) {
				g_clear_object (&end_tt);
				end_tt = start_tt;
				start_tt = NULL;
				set_dtend = TRUE;

				if (*inout_last_duration >= 24 * 60 * 60)
					i_cal_time_adjust (end_tt, *inout_last_duration / (24 * 60 * 60), 0, 0, 0);
			} else {
				g_clear_object (&start_tt);
				start_tt = end_tt;
				end_tt = NULL;
				set_dtstart = TRUE;

				if (*inout_last_duration >= 24 * 60 * 60)
					i_cal_time_adjust (start_tt, -(*inout_last_duration) / (24 * 60 * 60), 0, 0, 0);
			}
		}
	} else {
		if (ecepp_check_start_before_end (icomp, &start_tt, &end_tt, change_end_datetime, inout_last_duration)) {
			if (change_end_datetime)
				set_dtend = TRUE;
			else
				set_dtstart = TRUE;
		}
	}

	if (set_dtstart || set_dtend) {
		if (set_dtstart)
			e_comp_editor_property_part_datetime_set_value (start_dtm, start_tt);

		if (set_dtend)
			e_comp_editor_property_part_datetime_set_value (end_dtm, end_tt);
	}

	g_clear_object (&start_tt);
	g_clear_object (&end_tt);
}
