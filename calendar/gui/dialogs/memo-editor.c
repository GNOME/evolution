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
 *		Miguel de Icaza <miguel@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      JP Rosevear <jpr@ximian.com>
 *      Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <e-util/e-plugin-ui.h>
#include <e-util/e-util-private.h>
#include <evolution-shell-component-utils.h>

#include "memo-page.h"
#include "cancel-comp.h"
#include "memo-editor.h"

#define MEMO_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_MEMO_EDITOR, MemoEditorPrivate))

struct _MemoEditorPrivate {
	MemoPage *memo_page;

	gboolean updating;
};

/* Extends the UI definition in CompEditor */
static const gchar *ui =
"<ui>"
"  <menubar action='main-menu'>"
"    <menu action='view-menu'>"
"      <menuitem action='view-categories'/>"
"    </menu>"
"    <menu action='options-menu'>"
"      <menu action='classification-menu'>"
"        <menuitem action='classify-public'/>"
"        <menuitem action='classify-private'/>"
"        <menuitem action='classify-confidential'/>"
"      </menu>"
"    </menu>"
"  </menubar>"
"</ui>";

G_DEFINE_TYPE (MemoEditor, memo_editor, TYPE_COMP_EDITOR)

static void
memo_editor_show_categories (CompEditor *editor,
                             gboolean visible)
{
	MemoEditorPrivate *priv;

	priv = MEMO_EDITOR_GET_PRIVATE (editor);

	memo_page_set_show_categories (priv->memo_page, visible);
}

static void
memo_editor_dispose (GObject *object)
{
	MemoEditorPrivate *priv;

	priv = MEMO_EDITOR_GET_PRIVATE (object);

	if (priv->memo_page) {
		g_object_unref (priv->memo_page);
		priv->memo_page = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (memo_editor_parent_class)->dispose (object);
}

static void
memo_editor_constructed (GObject *object)
{
	MemoEditorPrivate *priv;
	CompEditor *editor;

	priv = MEMO_EDITOR_GET_PRIVATE (object);
	editor = COMP_EDITOR (object);

	priv->memo_page = memo_page_new (editor);
	comp_editor_append_page (
		editor, COMP_EDITOR_PAGE (priv->memo_page),
		_("Memo"), TRUE);
}

static void
memo_editor_class_init (MemoEditorClass *class)
{
	GObjectClass *object_class;
	CompEditorClass *editor_class;

	g_type_class_add_private (class, sizeof (MemoEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = memo_editor_dispose;
	object_class->constructed = memo_editor_constructed;

	/* TODO Add a help section for memos. */
	editor_class = COMP_EDITOR_CLASS (class);
	/*editor_class->help_section = "usage-calendar-memo";*/
	editor_class->show_categories = memo_editor_show_categories;
}

/* Object initialization function for the memo editor */
static void
memo_editor_init (MemoEditor *me)
{
	CompEditor *editor = COMP_EDITOR (me);
	GtkUIManager *ui_manager;
	GError *error = NULL;

	me->priv = MEMO_EDITOR_GET_PRIVATE (me);
	me->priv->updating = FALSE;

	ui_manager = comp_editor_get_ui_manager (editor);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	e_plugin_ui_register_manager ("memo-editor", ui_manager, me);

	if (error != NULL) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

/**
 * memo_editor_new:
 * @client: an ECal
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
CompEditor *
memo_editor_new (ECal *client, CompEditorFlags flags)
{
	g_return_val_if_fail (E_IS_CAL (client), NULL);

	return g_object_new (
		TYPE_MEMO_EDITOR,
		"flags", flags, "client", client, NULL);
}
