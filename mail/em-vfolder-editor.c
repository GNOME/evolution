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

#include "e-util/e-util-private.h"
#include "e-util/gconf-bridge.h"

#include "em-vfolder-editor.h"
#include "em-vfolder-rule.h"

static gpointer parent_class;

static FilterRule *
vfolder_editor_create_rule (RuleEditor *rule_editor)
{
	FilterRule *rule = filter_rule_new ();
	FilterPart *part;

	/* create a rule with 1 part in it */
	rule = (FilterRule *) em_vfolder_rule_new ();
	part = rule_context_next_part (rule_editor->context, NULL);
	filter_rule_add_part (rule, filter_part_clone (part));

	return rule;
}

static void
vfolder_editor_class_init (EMVFolderEditorClass *class)
{
	RuleEditorClass *rule_editor_class;

	parent_class = g_type_class_peek_parent (class);

	rule_editor_class = RULE_EDITOR_CLASS (class);
	rule_editor_class->create_rule = vfolder_editor_create_rule;
}

static void
vfolder_editor_init (EMVFolderEditor *vfolder_editor)
{
	GConfBridge *bridge;
	const gchar *key_prefix;

	bridge = gconf_bridge_get ();
	key_prefix = "/apps/evolution/mail/vfolder_editor";

	gconf_bridge_bind_window_size (
		bridge, key_prefix, GTK_WINDOW (vfolder_editor));
}

GType
em_vfolder_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMVFolderEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) vfolder_editor_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMVFolderEditor),
			0,     /* n_preallocs */
			(GInstanceInitFunc) vfolder_editor_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			RULE_TYPE_EDITOR, "EMVFolderEditor", &type_info, 0);
	}

	return type;
}

/**
 * em_vfolder_editor_new:
 *
 * Create a new EMVFolderEditor object.
 *
 * Return value: A new #EMVFolderEditor object.
 **/
GtkWidget *
em_vfolder_editor_new (EMVFolderContext *vc)
{
	EMVFolderEditor *ve;
	GladeXML *gui;
	gchar *gladefile;

	ve = g_object_new (EM_TYPE_VFOLDER_EDITOR, NULL);

	gladefile = g_build_filename (
		EVOLUTION_GLADEDIR, "filter.glade", NULL);
	gui = glade_xml_new (gladefile, "rule_editor", NULL);
	g_free (gladefile);

	rule_editor_construct ((RuleEditor *) ve, (RuleContext *) vc, gui, "incoming", _("Search _Folders"));
	gtk_widget_hide (glade_xml_get_widget (gui, "label17"));
	gtk_widget_hide (glade_xml_get_widget (gui, "filter_source_combobox"));
	g_object_unref (gui);

	return GTK_WIDGET (ve);
}
