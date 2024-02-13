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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "eab-editor.h"
#include "e-util/e-util.h"
#include "addressbook/gui/widgets/eab-gui-util.h"

struct _EABEditorPrivate {
	EShell *shell;
};

enum {
	PROP_0,
	PROP_SHELL
};

enum {
	CONTACT_ADDED,
	CONTACT_MODIFIED,
	CONTACT_DELETED,
	EDITOR_CLOSED,
	LAST_SIGNAL
};

static GSList *all_editors;
static guint signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (EABEditor, eab_editor, G_TYPE_OBJECT)

static void
eab_editor_quit_requested_cb (EShell *shell,
                              EShellQuitReason reason,
                              EABEditor *editor)
{
	GtkWindow *window;

	/* Quit immediately if another Evolution process asked us to. */
	if (reason == E_SHELL_QUIT_REMOTE_REQUEST)
		return;

	window = eab_editor_get_window (editor);

	eab_editor_raise (editor);
	if (!eab_editor_prompt_to_save_changes (editor, window))
		e_shell_cancel_quit (shell);
}

static void
eab_editor_set_shell (EABEditor *editor,
                      EShell *shell)
{
	g_return_if_fail (editor->priv->shell == NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	editor->priv->shell = g_object_ref (shell);

	g_signal_connect (
		shell, "quit-requested",
		G_CALLBACK (eab_editor_quit_requested_cb), editor);
}

static void
eab_editor_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL:
			eab_editor_set_shell (
				EAB_EDITOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
eab_editor_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL:
			g_value_set_object (
				value, eab_editor_get_shell (
				EAB_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
eab_editor_dispose (GObject *object)
{
	EABEditor *self = EAB_EDITOR (object);

	if (self->priv->shell != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->shell, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_clear_object (&self->priv->shell);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (eab_editor_parent_class)->dispose (object);
}

static void
eab_editor_finalize (GObject *object)
{
	all_editors = g_slist_remove (all_editors, object);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (eab_editor_parent_class)->finalize (object);
}

static void
eab_editor_class_init (EABEditorClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = eab_editor_set_property;
	object_class->get_property = eab_editor_get_property;
	object_class->dispose = eab_editor_dispose;
	object_class->finalize = eab_editor_finalize;

	g_object_class_install_property (
		object_class,
		PROP_SHELL,
		g_param_spec_object (
			"shell",
			"Shell",
			"The EShell singleton",
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[CONTACT_ADDED] = g_signal_new (
		"contact_added",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EABEditorClass, contact_added),
		NULL, NULL,
		e_marshal_VOID__POINTER_OBJECT,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
		G_TYPE_OBJECT);

	signals[CONTACT_MODIFIED] = g_signal_new (
		"contact_modified",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EABEditorClass, contact_modified),
		NULL, NULL,
		e_marshal_VOID__POINTER_OBJECT,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
		G_TYPE_OBJECT);

	signals[CONTACT_DELETED] = g_signal_new (
		"contact_deleted",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EABEditorClass, contact_deleted),
		NULL, NULL,
		e_marshal_VOID__POINTER_OBJECT,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
		G_TYPE_OBJECT);

	signals[EDITOR_CLOSED] = g_signal_new (
		"editor_closed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EABEditorClass, editor_closed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
eab_editor_init (EABEditor *editor)
{
	editor->priv = eab_editor_get_instance_private (editor);

	all_editors = g_slist_prepend (all_editors, editor);
}

EShell *
eab_editor_get_shell (EABEditor *editor)
{
	g_return_val_if_fail (EAB_IS_EDITOR (editor), NULL);

	return E_SHELL (editor->priv->shell);
}

GSList *
eab_editor_get_all_editors (void)
{
	return all_editors;
}

void
eab_editor_show (EABEditor *editor)
{
	EABEditorClass *class;

	g_return_if_fail (EAB_IS_EDITOR (editor));

	class = EAB_EDITOR_GET_CLASS (editor);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->show != NULL);

	class->show (editor);
}

void
eab_editor_close (EABEditor *editor)
{
	EABEditorClass *class;

	g_return_if_fail (EAB_IS_EDITOR (editor));

	class = EAB_EDITOR_GET_CLASS (editor);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->close != NULL);

	class->close (editor);
}

void
eab_editor_raise (EABEditor *editor)
{
	EABEditorClass *class;

	g_return_if_fail (EAB_IS_EDITOR (editor));

	class = EAB_EDITOR_GET_CLASS (editor);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->raise != NULL);

	class->raise (editor);
}

void
eab_editor_save_contact (EABEditor *editor,
                         gboolean should_close)
{
	EABEditorClass *class;

	g_return_if_fail (EAB_IS_EDITOR (editor));

	class = EAB_EDITOR_GET_CLASS (editor);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->save_contact != NULL);

	class->save_contact (editor, should_close);
}

gboolean
eab_editor_is_changed (EABEditor *editor)
{
	EABEditorClass *class;

	g_return_val_if_fail (EAB_IS_EDITOR (editor), FALSE);

	class = EAB_EDITOR_GET_CLASS (editor);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->is_changed != NULL, FALSE);

	return class->is_changed (editor);
}

gboolean
eab_editor_is_valid (EABEditor *editor)
{
	EABEditorClass *class;

	g_return_val_if_fail (EAB_IS_EDITOR (editor), FALSE);

	class = EAB_EDITOR_GET_CLASS (editor);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->is_valid != NULL, FALSE);

	return class->is_valid (editor);
}

GtkWindow *
eab_editor_get_window (EABEditor *editor)
{
	EABEditorClass *class;

	g_return_val_if_fail (EAB_IS_EDITOR (editor), NULL);

	class = EAB_EDITOR_GET_CLASS (editor);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->get_window != NULL, NULL);

	return class->get_window (editor);
}

/* This function prompts for saving if editor conents are in changed state and
 * save or discards or cancels (just returns with out doing anything) according
 * to user input.  Editor gets destroyed in case of save and discard case. */

gboolean
eab_editor_prompt_to_save_changes (EABEditor *editor,
                                   GtkWindow *window)
{
	if (!eab_editor_is_changed (editor)) {
		eab_editor_close (EAB_EDITOR (editor));
		return TRUE;
	}

	switch (eab_prompt_save_dialog (window)) {
	case GTK_RESPONSE_YES:
		if (!eab_editor_is_valid (editor)) {
			return FALSE;
		}
		eab_editor_save_contact (editor, TRUE);
		return TRUE;
	case GTK_RESPONSE_NO:
		eab_editor_close (EAB_EDITOR (editor));
		return TRUE;
	case GTK_RESPONSE_CANCEL:
	default:
		return FALSE;
	}
}

void
eab_editor_contact_added (EABEditor *editor,
                          const GError *error,
                          EContact *contact)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));
	g_return_if_fail (E_IS_CONTACT (contact));

	g_signal_emit (editor, signals[CONTACT_ADDED], 0, error, contact);
}

void
eab_editor_contact_modified (EABEditor *editor,
                             const GError *error,
                             EContact *contact)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));
	g_return_if_fail (E_IS_CONTACT (contact));

	g_signal_emit (editor, signals[CONTACT_MODIFIED], 0, error, contact);
}

void
eab_editor_contact_deleted (EABEditor *editor,
                            const GError *error,
                            EContact *contact)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));
	g_return_if_fail (E_IS_CONTACT (contact));

	g_signal_emit (editor, signals[CONTACT_DELETED], 0, error, contact);
}

void
eab_editor_closed (EABEditor *editor)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));

	g_signal_emit (editor, signals[EDITOR_CLOSED], 0);
}
