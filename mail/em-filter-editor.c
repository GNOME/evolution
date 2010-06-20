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

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "e-util/gconf-bridge.h"

#include "em-filter-editor.h"
#include "em-filter-rule.h"

static gpointer parent_class;

static EFilterRule *
filter_editor_create_rule (ERuleEditor *rule_editor)
{
	EFilterRule *rule;
	EFilterPart *part;

	/* create a rule with 1 part & 1 action in it */
	rule = (EFilterRule *)em_filter_rule_new ();
	part = e_rule_context_next_part (rule_editor->context, NULL);
	e_filter_rule_add_part (rule, e_filter_part_clone (part));
	part = em_filter_context_next_action (
		(EMFilterContext *)rule_editor->context, NULL);
	em_filter_rule_add_action (
		(EMFilterRule *)rule, e_filter_part_clone (part));

	return rule;
}

static void
filter_editor_class_init (EMFilterEditorClass *class)
{
	ERuleEditorClass *rule_editor_class;

	parent_class = g_type_class_peek_parent (class);

	rule_editor_class = E_RULE_EDITOR_CLASS (class);
	rule_editor_class->create_rule = filter_editor_create_rule;
}

static void
filter_editor_init (EMFilterEditor *filter_editor)
{
	GConfBridge *bridge;
	const gchar *key_prefix;

	bridge = gconf_bridge_get ();
	key_prefix = "/apps/evolution/mail/filter_editor";

	gconf_bridge_bind_window_size (
		bridge, key_prefix, GTK_WINDOW (filter_editor));
}

GType
em_filter_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMFilterEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) filter_editor_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMFilterEditor),
			0,     /* n_preallocs */
			(GInstanceInitFunc) filter_editor_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_RULE_EDITOR, "EMFilterEditor", &type_info, 0);
	}

	return type;
}

/**
 * em_filter_editor_new:
 *
 * Create a new EMFilterEditor object.
 *
 * Return value: A new #EMFilterEditor object.
 **/
EMFilterEditor *
em_filter_editor_new (EMFilterContext *fc,
                      const EMFilterSource *source_names)
{
	EMFilterEditor *fe;
	GtkBuilder *builder;

	fe = g_object_new (EM_TYPE_FILTER_EDITOR, NULL);

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "filter.ui");
	em_filter_editor_construct (fe, fc, builder, source_names);
	g_object_unref (builder);

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

	e_rule_editor_set_source ((ERuleEditor *)fe, source);
}

void
em_filter_editor_construct (EMFilterEditor *fe,
                            EMFilterContext *fc,
                            GtkBuilder *builder,
                            const EMFilterSource *source_names)
{
	GtkWidget *combobox;
	gint i;
	GtkTreeViewColumn *column;
	GtkTreeModel *model;
	GSList *sources = NULL;

	combobox = e_builder_get_widget (builder, "filter_source_combobox");
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
	gtk_list_store_clear (GTK_LIST_STORE (model));

	for (i = 0; source_names[i].source; i++) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), source_names[i].name);
		sources = g_slist_append (sources, g_strdup(source_names[i].source));
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
	g_signal_connect (combobox, "changed", G_CALLBACK (select_source), fe);
	g_object_set_data_full (G_OBJECT (combobox), "sources", sources, free_sources);
	gtk_widget_show (combobox);

	e_rule_editor_construct (
		(ERuleEditor *) fe, (ERuleContext *) fc,
		builder, source_names[0].source, _("_Filter Rules"));

	/* Show the Enabled column, we support it here */
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (E_RULE_EDITOR (fe)->list), 0);
	gtk_tree_view_column_set_visible (column, TRUE);
}
