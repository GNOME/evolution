/*
 *
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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "e-util/gconf-bridge.h"

#include "em-vfolder-editor.h"
#include "em-vfolder-rule.h"

G_DEFINE_TYPE (
	EMVFolderEditor,
	em_vfolder_editor,
	E_TYPE_RULE_EDITOR)

static EFilterRule *
vfolder_editor_create_rule (ERuleEditor *rule_editor)
{
	EMVFolderContext *context;
	EMailBackend *backend;
	EFilterRule *rule;
	EFilterPart *part;

	context = EM_VFOLDER_CONTEXT (rule_editor->context);
	backend = em_vfolder_context_get_backend (context);

	/* create a rule with 1 part in it */
	rule = em_vfolder_rule_new (backend);
	part = e_rule_context_next_part (rule_editor->context, NULL);
	e_filter_rule_add_part (rule, e_filter_part_clone (part));

	return rule;
}

static void
em_vfolder_editor_class_init (EMVFolderEditorClass *class)
{
	ERuleEditorClass *rule_editor_class;

	rule_editor_class = E_RULE_EDITOR_CLASS (class);
	rule_editor_class->create_rule = vfolder_editor_create_rule;
}

static void
em_vfolder_editor_init (EMVFolderEditor *vfolder_editor)
{
	GConfBridge *bridge;
	const gchar *key_prefix;

	bridge = gconf_bridge_get ();
	key_prefix = "/apps/evolution/mail/vfolder_editor";

	gconf_bridge_bind_window_size (
		bridge, key_prefix, GTK_WINDOW (vfolder_editor));
}

/**
 * em_vfolder_editor_new:
 *
 * Create a new EMVFolderEditor object.
 *
 * Returns: a new #EMVFolderEditor
 **/
GtkWidget *
em_vfolder_editor_new (EMVFolderContext *context)
{
	EMVFolderEditor *editor;
	GtkBuilder *builder;

	g_return_val_if_fail (EM_IS_VFOLDER_CONTEXT (context), NULL);

	editor = g_object_new (EM_TYPE_VFOLDER_EDITOR, NULL);

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "filter.ui");

	e_rule_editor_construct (
		E_RULE_EDITOR (editor), E_RULE_CONTEXT (context),
		builder, "incoming", _("Search _Folders"));
	gtk_widget_hide (e_builder_get_widget (builder, "label17"));
	gtk_widget_hide (e_builder_get_widget (builder, "filter_source_combobox"));
	g_object_unref (builder);

	return GTK_WIDGET (editor);
}
