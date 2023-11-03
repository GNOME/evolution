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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "shell/e-shell.h"
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "em-filter-editor.h"
#include "em-filter-rule.h"

G_DEFINE_TYPE (EMFilterEditor, em_filter_editor, E_TYPE_RULE_EDITOR)

static void
emfe_show_html (GtkWindow *parent,
		const gchar *html)
{
	GtkWidget *dialog, *widget, *container, *searchbar;

	dialog = gtk_dialog_new_with_buttons (_("Description of Filters"), parent,
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Close"), GTK_RESPONSE_CLOSE,
		NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 480, 410);
	gtk_window_set_position (GTK_WINDOW (dialog), parent ? GTK_WIN_POS_CENTER_ON_PARENT : GTK_WIN_POS_CENTER);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
	gtk_widget_show (widget);

	container = widget;

	widget = e_web_view_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		"editable", FALSE,
		NULL);
	gtk_container_add (GTK_CONTAINER (container), widget);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	searchbar = e_search_bar_new (E_WEB_VIEW (widget));
	g_object_set (G_OBJECT (searchbar),
		"can-hide", FALSE,
		"visible", TRUE,
		NULL);
	gtk_container_add (GTK_CONTAINER (container), searchbar);

	e_web_view_load_string (E_WEB_VIEW (widget), html);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}

/*  Some parts require special processing, which cannot access session from their place */
static void
emfe_describe_part (EFilterPart *part,
		    GString *out,
		    CamelSession *session)
{
	GList *link;

	g_return_if_fail (E_IS_FILTER_PART (part));
	g_return_if_fail (out != NULL);

	g_string_append (out, _(part->title));

	for (link = part->elements; link != NULL; link = g_list_next (link)) {
		EFilterElement *element = link->data;

		g_string_append_c (out, ' ');

		if (EM_IS_FILTER_FOLDER_ELEMENT (element)) {
			em_filter_folder_element_describe (EM_FILTER_FOLDER_ELEMENT (element), session, out);
		} else {
			e_filter_element_describe (element, out);
		}
	}
}

static void
emfe_describe_filters_cb (GtkWidget *button,
			  gpointer user_data)
{
	EShell *shell;
	EShellBackend *shell_backend;
	ESourceRegistry *registry;
	EMFilterEditor *fe = user_data;
	ERuleContext *context;
	EFilterRule *rule = NULL;
	CamelSession *session = NULL;
	GString *description;
	gchar *html;
	const gchar *source;

	g_return_if_fail (EM_IS_FILTER_EDITOR (fe));

	context = E_RULE_EDITOR (fe)->context;
	source = E_RULE_EDITOR (fe)->source;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	if (shell_backend)
		g_object_get (shell_backend, "session", &session, NULL);

	registry = e_shell_get_registry (shell);

	description = g_string_sized_new (2048);

	while (rule = e_rule_context_next_rule (context, rule, source), rule) {
		GList *link;
		gchar *account, *rule_name;

		account = g_strdup (em_filter_rule_get_account_uid (EM_FILTER_RULE (rule)));
		if (account && *account) {
			ESource *esource;

			esource = e_source_registry_ref_source (registry, account);
			if (esource) {
				g_free (account);
				account = e_source_dup_display_name (esource);
				g_object_unref (esource);
			}
		} else {
			g_free (account);
			account = NULL;
		}

		if (description->len)
			g_string_append_c (description, '\n');

		rule_name = g_strdup_printf ("%c%s%c",
			E_FILTER_ELEMENT_DESCRIPTION_VALUE_START,
			rule->name,
			E_FILTER_ELEMENT_DESCRIPTION_VALUE_END);

		if (account) {
			/* Translators: The first '%s' is replaced with the rule name;
			   the second '%s' with 'enabled' or 'disabled' word;
			   the third '%s' is replaced with the account name */
			g_string_append_printf (description, _("%s (%s, for account %s)"), rule_name, rule->enabled ? _("enabled") : _("disabled"), account);
		} else {
			/* Translators: The first '%s' is replaced with the rule name;
			   the second '%s' with 'enabled' or 'disabled' word */
			g_string_append_printf (description, _("%s (%s, for any account)"), rule_name, rule->enabled ? _("enabled") : _("disabled"));
		}

		g_string_append_c (description, '\n');

		g_free (rule_name);
		g_free (account);

		g_string_append (description, "   ");

		switch (rule->grouping) {
		case E_FILTER_GROUP_ALL:
			g_string_append (description, _("If all the following conditions are met"));
			break;
		case E_FILTER_GROUP_ANY:
			g_string_append (description, _("If any of the following conditions are met"));
			break;
		}
		g_string_append_c (description, '\n');

		for (link = rule->parts; link; link = g_list_next (link)) {
			EFilterPart *part = link->data;

			if (!part)
				continue;

			g_string_append (description, "      ");
			emfe_describe_part (part, description, session);
			g_string_append_c (description, '\n');
		}

		g_string_append (description, "   ");
		g_string_append (description, _("Then"));
		g_string_append_c (description, '\n');

		for (link = em_filter_rule_get_actions (EM_FILTER_RULE (rule)); link; link = g_list_next (link)) {
			EFilterPart *part = link->data;

			if (!part)
				continue;

			g_string_append (description, "      ");
			emfe_describe_part (part, description, session);
			g_string_append_c (description, '\n');
		}
	}

	html = camel_text_to_html (description->str, CAMEL_MIME_FILTER_TOHTML_CONVERT_NL | CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES, 0);
	g_string_free (description, TRUE);

	description = e_str_replace_string (html, "&#1;", "<b>"); /* E_FILTER_ELEMENT_DESCRIPTION_VALUE_START */
	g_string_prepend (description, "<div style=\"white-space: nowrap;\">");
	g_string_append (description, "</div>");
	g_free (html);
	html = g_string_free (description, FALSE);

	#define replace_in_html(_find, _replace) \
		description = e_str_replace_string (html, _find, _replace); \
		g_free (html); \
		html = g_string_free (description, FALSE);

	replace_in_html ("&#2;", "</b>"); /* E_FILTER_ELEMENT_DESCRIPTION_VALUE_END */

	if (strstr (html, "&#3;") && strstr (html, "&#4;")) {
		replace_in_html ("&#3;", "<span style=\"background-color:"); /* E_FILTER_ELEMENT_DESCRIPTION_COLOR_START */
		replace_in_html ("&#4;", ";\">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>"); /* E_FILTER_ELEMENT_DESCRIPTION_COLOR_END */
	}

	#undef replace_in_html

	emfe_show_html (GTK_WINDOW (fe), html);

	g_clear_object (&session);
	g_free (html);
}

static void
emfe_update_describe_sensitive (GtkTreeModel *model,
				GtkWidget *button)
{
	GtkTreeIter iter;

	gtk_widget_set_sensitive (button, gtk_tree_model_get_iter_first (model, &iter));
}

static void
emfe_rules_model_row_inserted_cb (GtkTreeModel *tree_model,
				  GtkTreePath *path,
				  GtkTreeIter *iter,
				  gpointer user_data)
{
	emfe_update_describe_sensitive (tree_model, user_data);
}

static void
emfe_rules_model_row_deleted_cb (GtkTreeModel *tree_model,
				 GtkTreePath *path,
				 gpointer user_data)
{
	emfe_update_describe_sensitive (tree_model, user_data);
}

static EFilterRule *
filter_editor_create_rule (ERuleEditor *rule_editor)
{
	EFilterRule *rule;
	EFilterPart *part;

	/* create a rule with 1 part & 1 action in it */
	rule = (EFilterRule *) em_filter_rule_new ();
	part = e_rule_context_next_part (rule_editor->context, NULL);
	e_filter_rule_add_part (rule, e_filter_part_clone (part));
	part = em_filter_context_next_action (
		(EMFilterContext *) rule_editor->context, NULL);
	em_filter_rule_add_action (
		(EMFilterRule *) rule, e_filter_part_clone (part));

	return rule;
}

static void
em_filter_editor_class_init (EMFilterEditorClass *class)
{
	ERuleEditorClass *rule_editor_class;

	rule_editor_class = E_RULE_EDITOR_CLASS (class);
	rule_editor_class->create_rule = filter_editor_create_rule;
}

static void
em_filter_editor_init (EMFilterEditor *filter_editor)
{
	gtk_window_set_default_size (GTK_WINDOW (filter_editor), 400, 650);

	e_restore_window (
		GTK_WINDOW (filter_editor),
		"/org/gnome/evolution/mail/filter-window/",
		E_RESTORE_WINDOW_SIZE);
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

	g_slist_foreach (sources, (GFunc) g_free, NULL);
	g_slist_free (sources);
}

static void
select_source (GtkComboBox *combobox,
               EMFilterEditor *fe)
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

	e_rule_editor_set_source ((ERuleEditor *) fe, source);
}

void
em_filter_editor_construct (EMFilterEditor *fe,
                            EMFilterContext *fc,
                            GtkBuilder *builder,
                            const EMFilterSource *source_names)
{
	GtkWidget *combobox, *action_area, *button, *tree_view;
	gint i;
	GtkTreeViewColumn *column;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkListStore *store;
	GSList *sources = NULL;

	combobox = e_builder_get_widget (builder, "filter_source_combobox");
	store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combobox)));
	gtk_list_store_clear (store);

	for (i = 0; source_names[i].source; i++) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			0, source_names[i].name,
			-1);
		sources = g_slist_append (sources, g_strdup (source_names[i].source));
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
	g_signal_connect (
		combobox, "changed",
		G_CALLBACK (select_source), fe);
	g_object_set_data_full (G_OBJECT (combobox), "sources", sources, free_sources);
	gtk_widget_show (combobox);

	e_rule_editor_construct (
		(ERuleEditor *) fe, (ERuleContext *) fc,
		builder, source_names[0].source, _("_Filter Rules"));

	/* Show the Enabled column, we support it here */
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (E_RULE_EDITOR (fe)->list), 0);
	gtk_tree_view_column_set_visible (column, TRUE);

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (fe));
	button = gtk_button_new_with_mnemonic (_("De_scribe Filters…"));
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (action_area), button, FALSE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX (action_area), button, 0);

	if (GTK_IS_BUTTON_BOX (action_area))
		gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (action_area), button, TRUE);

	g_signal_connect (button, "clicked", G_CALLBACK (emfe_describe_filters_cb), fe);

	tree_view = e_builder_get_widget (builder, "rule_tree_view");
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

	g_signal_connect_object (model, "row-inserted", G_CALLBACK (emfe_rules_model_row_inserted_cb), button, 0);
	g_signal_connect_object (model, "row-deleted", G_CALLBACK (emfe_rules_model_row_deleted_cb), button, 0);

	emfe_update_describe_sensitive (model, button);
}
