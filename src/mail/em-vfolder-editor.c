/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Not Zed <notzed@lostzed.mmc.com.au>
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "em-vfolder-editor.h"
#include "em-vfolder-editor-rule.h"

G_DEFINE_TYPE (
	EMVFolderEditor,
	em_vfolder_editor,
	E_TYPE_RULE_EDITOR)

static EFilterRule *
vfolder_editor_create_rule (ERuleEditor *rule_editor)
{
	EMVFolderEditorContext *context;
	EMailSession *session;
	EFilterRule *rule;
	EFilterPart *part;

	context = EM_VFOLDER_EDITOR_CONTEXT (rule_editor->context);
	session = em_vfolder_editor_context_get_session (context);

	/* create a rule with 1 part in it */
	rule = em_vfolder_editor_rule_new (session);
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
	gtk_window_set_default_size (GTK_WINDOW (vfolder_editor), 400, 650);

	e_restore_window (
		GTK_WINDOW (vfolder_editor),
		"/org/gnome/evolution/mail/vfolder-window/",
		E_RESTORE_WINDOW_SIZE);
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
