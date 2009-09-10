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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
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

#include "em-filter-editor.h"
#include "em-filter-rule.h"

#define d(x)

static FilterRule *create_rule (RuleEditor *re);

static void em_filter_editor_class_init (EMFilterEditorClass *klass);
static void em_filter_editor_init (EMFilterEditor *fe);
static void em_filter_editor_finalise (GObject *obj);

static RuleEditorClass *parent_class = NULL;

GType
em_filter_editor_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMFilterEditorClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_filter_editor_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMFilterEditor),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_filter_editor_init,
		};

		type = g_type_register_static (RULE_TYPE_EDITOR, "EMFilterEditor", &info, 0);
	}

	return type;
}

static void
em_filter_editor_class_init (EMFilterEditorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	RuleEditorClass *re_class = (RuleEditorClass *) klass;

	parent_class = g_type_class_ref (rule_editor_get_type ());

	gobject_class->finalize = em_filter_editor_finalise;

	/* override methods */
	re_class->create_rule = create_rule;
}

static void
em_filter_editor_init (EMFilterEditor *fe)
{
	;
}

static void
em_filter_editor_finalise (GObject *obj)
{
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * em_filter_editor_new:
 *
 * Create a new EMFilterEditor object.
 *
 * Return value: A new #EMFilterEditor object.
 **/
EMFilterEditor *
em_filter_editor_new (EMFilterContext *fc, const EMFilterSource *source_names)
{
	EMFilterEditor *fe = (EMFilterEditor *) g_object_new (em_filter_editor_get_type(), NULL);
	GladeXML *gui;
	gchar *gladefile;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "filter.glade",
				      NULL);
	gui = glade_xml_new (gladefile, "rule_editor", NULL);
	g_free (gladefile);

	em_filter_editor_construct (fe, fc, gui, source_names);
	g_object_unref (gui);

	return fe;
}

static void
free_sources (gpointer data)
{
	GSList *sources = data;

	g_slist_foreach (sources, (GFunc)g_free, NULL);
	g_slist_free (sources);
}

static void
select_source (GtkComboBox *combobox, EMFilterEditor *fe)
{
	gchar *source;
	gint idx;
	GSList *sources;

	g_return_if_fail (GTK_IS_COMBO_BOX (combobox));

	idx = gtk_combo_box_get_active (combobox);
	sources = g_object_get_data (G_OBJECT (combobox), "sources");

	g_return_if_fail (idx >= 0 && idx < g_slist_length (sources));

	source = (gchar *) (g_slist_nth (sources, idx))->data;
	g_return_if_fail (source);

	rule_editor_set_source ((RuleEditor *)fe, source);
}

void
em_filter_editor_construct (EMFilterEditor *fe, EMFilterContext *fc, GladeXML *gui, const EMFilterSource *source_names)
{
	GtkWidget *combobox;
	gint i;
	GtkTreeViewColumn *column;
	GSList *sources = NULL;

        combobox = glade_xml_get_widget (gui, "filter_source_combobox");
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combobox))));

	for (i = 0; source_names[i].source; i++) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), source_names[i].name);
		sources = g_slist_append (sources, g_strdup(source_names[i].source));
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
	g_signal_connect (combobox, "changed", G_CALLBACK (select_source), fe);
	g_object_set_data_full (G_OBJECT (combobox), "sources", sources, free_sources);
	gtk_widget_show (combobox);

	rule_editor_construct ((RuleEditor *) fe, (RuleContext *) fc, gui, source_names[0].source, _("_Filter Rules"));

	/* Show the Enabled column, we support it here */
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (RULE_EDITOR (fe)->list), 0);
	gtk_tree_view_column_set_visible (column, TRUE);
}

static FilterRule *
create_rule (RuleEditor *re)
{
	FilterRule *rule = filter_rule_new ();
	FilterPart *part;

	/* create a rule with 1 part & 1 action in it */
	rule = (FilterRule *)em_filter_rule_new ();
	part = rule_context_next_part (re->context, NULL);
	filter_rule_add_part (rule, filter_part_clone (part));
	part = em_filter_context_next_action ((EMFilterContext *)re->context, NULL);
	em_filter_rule_add_action ((EMFilterRule *)rule, filter_part_clone (part));

	return rule;
}
