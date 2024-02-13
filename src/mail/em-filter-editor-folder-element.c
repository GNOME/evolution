/*
 *
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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "em-folder-selection-button.h"
#include "em-utils.h"
#include "shell/e-shell.h"

#include "em-filter-editor-folder-element.h"

struct _EMFilterEditorFolderElementPrivate {
	EMailSession *session;
};

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE_WITH_PRIVATE (EMFilterEditorFolderElement, em_filter_editor_folder_element, EM_TYPE_FILTER_FOLDER_ELEMENT)

static void
filter_editor_folder_element_set_session (EMFilterEditorFolderElement *element,
                                   EMailSession *session)
{
	if (session == NULL) {
		EShell *shell;
		EShellBackend *shell_backend;
		EMailBackend *backend;

		shell = e_shell_get_default ();
		shell_backend = e_shell_get_backend_by_name (shell, "mail");

		backend = E_MAIL_BACKEND (shell_backend);
		session = e_mail_backend_get_session (backend);
	}

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (element->priv->session == NULL);

	element->priv->session = g_object_ref (session);
}

static void
filter_editor_folder_element_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			filter_editor_folder_element_set_session (
				EM_FILTER_EDITOR_FOLDER_ELEMENT (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
filter_editor_folder_element_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				em_filter_editor_folder_element_get_session (
				EM_FILTER_EDITOR_FOLDER_ELEMENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
filter_editor_folder_element_selected_cb (EMFolderSelectionButton *button,
                                   EMFilterEditorFolderElement *ff)
{
	GtkWidget *toplevel;
	const gchar *uri;

	uri = em_folder_selection_button_get_folder_uri (button);

	em_filter_folder_element_set_uri ((EMFilterFolderElement *) ff, uri);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	gtk_window_present (GTK_WINDOW (toplevel));
}

static void
filter_editor_folder_element_dispose (GObject *object)
{
	EMFilterEditorFolderElement *self = EM_FILTER_EDITOR_FOLDER_ELEMENT (object);

	g_clear_object (&self->priv->session);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_filter_editor_folder_element_parent_class)->dispose (object);
}

static GtkWidget *
filter_editor_folder_element_get_widget (EFilterElement *fe)
{
	EMFilterEditorFolderElement *ff = (EMFilterEditorFolderElement *) fe;
	EMailSession *session;
	GtkWidget *button;
	const gchar *uri;

	session = em_filter_editor_folder_element_get_session (ff);
	uri = em_filter_folder_element_get_uri ((EMFilterFolderElement *) ff);

	button = em_folder_selection_button_new (
		session, _("Select Folder"), NULL);
	em_folder_selection_button_set_folder_uri (
		EM_FOLDER_SELECTION_BUTTON (button), uri);
	gtk_widget_show (button);

	g_signal_connect (
		button, "selected",
		G_CALLBACK (filter_editor_folder_element_selected_cb), ff);

	return button;
}

static void
filter_editor_folder_element_describe (EFilterElement *fe,
				       GString *out)
{
	EMFilterEditorFolderElement *ff = (EMFilterEditorFolderElement *) fe;
	EMailSession *mail_session;

	mail_session = em_filter_editor_folder_element_get_session (ff);
	em_filter_folder_element_describe (EM_FILTER_FOLDER_ELEMENT (ff), CAMEL_SESSION (mail_session), out);
}

static void
em_filter_editor_folder_element_class_init (EMFilterEditorFolderElementClass *class)
{
	GObjectClass *object_class;
	EFilterElementClass *filter_element_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = filter_editor_folder_element_set_property;
	object_class->get_property = filter_editor_folder_element_get_property;
	object_class->dispose = filter_editor_folder_element_dispose;

	filter_element_class = E_FILTER_ELEMENT_CLASS (class);
	filter_element_class->get_widget = filter_editor_folder_element_get_widget;
	filter_element_class->describe = filter_editor_folder_element_describe;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
em_filter_editor_folder_element_init (EMFilterEditorFolderElement *element)
{
	element->priv = em_filter_editor_folder_element_get_instance_private (element);
}

EFilterElement *
em_filter_editor_folder_element_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		EM_TYPE_FILTER_EDITOR_FOLDER_ELEMENT,
		"session", session, NULL);
}

EMailSession *
em_filter_editor_folder_element_get_session (EMFilterEditorFolderElement *element)
{
	g_return_val_if_fail (EM_IS_FILTER_EDITOR_FOLDER_ELEMENT (element), NULL);

	return element->priv->session;
}
