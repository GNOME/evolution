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

#include "em-vfolder-editor.h"
#include "em-vfolder-rule.h"

#define d(x)

static FilterRule *create_rule (RuleEditor *re);

static RuleEditorClass *parent_class = NULL;

static void
em_vfolder_editor_finalise (GObject *obj)
{
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
em_vfolder_editor_class_init (EMVFolderEditorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	RuleEditorClass *re_class = (RuleEditorClass *) klass;

	parent_class = g_type_class_ref (rule_editor_get_type ());

	gobject_class->finalize = em_vfolder_editor_finalise;

	/* override methods */
	re_class->create_rule = create_rule;
}

static void
em_vfolder_editor_init (EMVFolderEditor *ve)
{
	;
}

GType
em_vfolder_editor_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMVFolderEditorClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_vfolder_editor_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMVFolderEditor),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_vfolder_editor_init,
		};

		type = g_type_register_static (RULE_TYPE_EDITOR, "EMVFolderEditor", &info, 0);
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
EMVFolderEditor *
em_vfolder_editor_new (EMVFolderContext *vc)
{
	EMVFolderEditor *ve = (EMVFolderEditor *) g_object_new (em_vfolder_editor_get_type(), NULL);
	GladeXML *gui;
	gchar *gladefile;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "filter.glade",
				      NULL);
	gui = glade_xml_new (gladefile, "rule_editor", NULL);
	g_free (gladefile);

	rule_editor_construct ((RuleEditor *) ve, (RuleContext *) vc, gui, "incoming", _("Search _Folders"));
	gtk_widget_hide (glade_xml_get_widget (gui, "label17"));
	gtk_widget_hide (glade_xml_get_widget (gui, "filter_source_combobox"));
	g_object_unref (gui);

	return ve;
}

static FilterRule *
create_rule (RuleEditor *re)
{
	FilterRule *rule = filter_rule_new ();
	FilterPart *part;

	/* create a rule with 1 part in it */
	rule = (FilterRule *) em_vfolder_rule_new ();
	part = rule_context_next_part (re->context, NULL);
	filter_rule_add_part (rule, filter_part_clone (part));

	return rule;
}
