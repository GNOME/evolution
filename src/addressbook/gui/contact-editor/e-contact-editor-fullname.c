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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "e-contact-editor-fullname.h"

static void fill_in_info (EContactEditorFullname *editor);
static void extract_info (EContactEditorFullname *editor);

enum {
	PROP_0,
	PROP_NAME,
	PROP_EDITABLE
};

G_DEFINE_TYPE (
	EContactEditorFullname,
	e_contact_editor_fullname,
	GTK_TYPE_DIALOG)

static void
e_contact_editor_fullname_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
	EContactEditorFullname *e_contact_editor_fullname;

	e_contact_editor_fullname = E_CONTACT_EDITOR_FULLNAME (object);

	switch (property_id) {
	case PROP_NAME:
		e_contact_name_free (e_contact_editor_fullname->name);

		if (g_value_get_pointer (value) != NULL) {
			e_contact_editor_fullname->name =
				e_contact_name_copy (
				g_value_get_pointer (value));
			fill_in_info (e_contact_editor_fullname);
		}
		else {
			e_contact_editor_fullname->name = NULL;
		}
		break;
	case PROP_EDITABLE: {
		gboolean editable;
		gint i;

		const gchar *widget_names[] = {
			"comboentry-title",
			"comboentry-suffix",
			"entry-first",
			"entry-middle",
			"entry-last",
			"label-title",
			"label-suffix",
			"label-first",
			"label-middle",
			"label-last",
			NULL
		};

		editable = g_value_get_boolean (value);
		e_contact_editor_fullname->editable = editable;

		for (i = 0; widget_names[i] != NULL; i++) {
			GtkWidget *widget;

			widget = e_builder_get_widget (
				e_contact_editor_fullname->builder,
				widget_names[i]);

			if (GTK_IS_ENTRY (widget)) {
				gtk_editable_set_editable (
					GTK_EDITABLE (widget), editable);

			} else if (GTK_IS_COMBO_BOX (widget)) {
				GtkWidget *child;

				child = gtk_bin_get_child (GTK_BIN (widget));

				gtk_editable_set_editable (
					GTK_EDITABLE (child), editable);
				gtk_widget_set_sensitive (widget, editable);

			} else if (GTK_IS_LABEL (widget)) {
				gtk_widget_set_sensitive (widget, editable);
			}
		}
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_contact_editor_fullname_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
	EContactEditorFullname *e_contact_editor_fullname;

	e_contact_editor_fullname = E_CONTACT_EDITOR_FULLNAME (object);

	switch (property_id) {
	case PROP_NAME:
		extract_info (e_contact_editor_fullname);
		g_value_set_pointer (
			value, e_contact_name_copy (
			e_contact_editor_fullname->name));
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (
			value, e_contact_editor_fullname->editable);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_contact_editor_fullname_dispose (GObject *object)
{
	EContactEditorFullname *e_contact_editor_fullname;

	e_contact_editor_fullname = E_CONTACT_EDITOR_FULLNAME (object);
	g_clear_object (&e_contact_editor_fullname->builder);
	g_clear_pointer (&e_contact_editor_fullname->name, e_contact_name_free);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_contact_editor_fullname_parent_class)->dispose (object);
}

static void
e_contact_editor_fullname_class_init (EContactEditorFullnameClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = e_contact_editor_fullname_set_property;
	object_class->get_property = e_contact_editor_fullname_get_property;
	object_class->dispose = e_contact_editor_fullname_dispose;

	g_object_class_install_property (
		object_class,
		PROP_NAME,
		g_param_spec_pointer (
			"name",
			"Name",
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
}

static void
e_contact_editor_fullname_init (EContactEditorFullname *e_contact_editor_fullname)
{
	GtkBuilder *builder;
	GtkDialog *dialog;
	GtkWidget *parent;
	GtkWidget *widget;
	GtkWidget *action_area;
	GtkWidget *content_area;
	const gchar *title;

	dialog = GTK_DIALOG (e_contact_editor_fullname);
	action_area = gtk_dialog_get_action_area (dialog);
	content_area = gtk_dialog_get_content_area (dialog);

	gtk_container_set_border_width (GTK_CONTAINER (action_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

	gtk_dialog_add_buttons (
		dialog,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK, NULL);

	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

	e_contact_editor_fullname->name = NULL;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "fullname.ui");

	e_contact_editor_fullname->builder = builder;

	widget = e_builder_get_widget (builder, "dialog-checkfullname");
	title = gtk_window_get_title (GTK_WINDOW (widget));
	gtk_window_set_title (GTK_WINDOW (e_contact_editor_fullname), title);

	widget = e_builder_get_widget (builder, "grid-checkfullname");
	parent = gtk_widget_get_parent (widget);
	g_object_ref (widget);
	gtk_container_remove (GTK_CONTAINER (parent), widget);
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 0);
	g_object_unref (widget);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);

	gtk_window_set_icon_name (
		GTK_WINDOW (e_contact_editor_fullname), "contact-new");
}

GtkWidget *
e_contact_editor_fullname_new (GtkWindow *parent,
			       const EContactName *name)
{
	GtkWidget *widget = g_object_new (E_TYPE_CONTACT_EDITOR_FULLNAME,
		"transient-for", parent,
		"use-header-bar", e_util_get_use_header_bar (),
		NULL);

	g_object_set (
		widget,
		"name", name,
		NULL);
	return widget;
}

static void
fill_in_field (EContactEditorFullname *editor,
               const gchar *field,
               const gchar *string)
{
	GtkWidget *widget = e_builder_get_widget (editor->builder, field);
	GtkEntry *entry = NULL;

	if (GTK_IS_ENTRY (widget))
		entry = GTK_ENTRY (widget);
	else if (GTK_IS_COMBO_BOX (widget))
		entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget)));

	if (entry) {
		if (string)
			gtk_entry_set_text (entry, string);
		else
			gtk_entry_set_text (entry, "");
	}
}

static void
fill_in_info (EContactEditorFullname *editor)
{
	EContactName *name = editor->name;
	if (name) {
		fill_in_field (editor, "comboentry-title",  name->prefixes);
		fill_in_field (editor, "entry-first",  name->given);
		fill_in_field (editor, "entry-middle", name->additional);
		fill_in_field (editor, "entry-last",   name->family);
		fill_in_field (editor, "comboentry-suffix", name->suffixes);
	}
}

static gchar *
extract_field (EContactEditorFullname *editor,
               const gchar *field)
{
	GtkWidget *widget = e_builder_get_widget (editor->builder, field);
	GtkEntry *entry = NULL;

	if (GTK_IS_ENTRY (widget))
		entry = GTK_ENTRY (widget);
	else if (GTK_IS_COMBO_BOX (widget))
		entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget)));

	if (entry)
		return g_strdup (gtk_entry_get_text (entry));
	else
		return NULL;
}

static void
extract_info (EContactEditorFullname *editor)
{
	EContactName *name = editor->name;
	if (!name) {
		name = e_contact_name_new ();
		editor->name = name;
	}

	name->prefixes = extract_field (editor, "comboentry-title");
	name->given = extract_field (editor, "entry-first");
	name->additional = extract_field (editor, "entry-middle");
	name->family = extract_field (editor, "entry-last");
	name->suffixes = extract_field (editor, "comboentry-suffix");
}
