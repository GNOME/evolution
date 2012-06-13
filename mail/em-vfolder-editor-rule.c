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

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <shell/e-shell.h>

#include <e-util/e-util.h>
#include <libevolution-utils/e-alert.h>
#include <e-util/e-util-private.h>

#include <libemail-engine/e-mail-folder-utils.h>

#include "em-folder-selector.h"
#include "em-folder-tree.h"
#include "em-utils.h"
#include "em-vfolder-editor-context.h"
#include "em-vfolder-editor-rule.h"

#define EM_VFOLDER_EDITOR_RULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_VFOLDER_EDITOR_RULE, EMVFolderEditorRulePrivate))

#define EM_VFOLDER_EDITOR_RULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_VFOLDER_EDITOR_RULE, EMVFolderEditorRulePrivate))

struct _EMVFolderEditorRulePrivate {
	EMailSession *session;
};

enum {
	PROP_0,
	PROP_SESSION
};

static GtkWidget *get_widget (EFilterRule *fr, ERuleContext *f);

G_DEFINE_TYPE (
	EMVFolderEditorRule,
	em_vfolder_editor_rule,
	EM_TYPE_VFOLDER_RULE)

static void
vfolder_editor_rule_set_session (EMVFolderEditorRule *rule,
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
	g_return_if_fail (rule->priv->session == NULL);

	rule->priv->session = g_object_ref (session);
}

static void
vfolder_editor_rule_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			vfolder_editor_rule_set_session (
				EM_VFOLDER_EDITOR_RULE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
vfolder_editor_rule_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				em_vfolder_editor_rule_get_session (
				EM_VFOLDER_EDITOR_RULE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
vfolder_editor_rule_dispose (GObject *object)
{
	EMVFolderEditorRulePrivate *priv;

	priv = EM_VFOLDER_EDITOR_RULE_GET_PRIVATE (object);
	if (priv->session != NULL) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_vfolder_editor_rule_parent_class)->dispose (object);
}

static void
vfolder_editor_rule_finalize (GObject *object)
{
	/* EMVFolderEditorRule *rule = EM_VFOLDER_EDITOR_RULE (object); */

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_vfolder_editor_rule_parent_class)->finalize (object);
}

static void
em_vfolder_editor_rule_class_init (EMVFolderEditorRuleClass *class)
{
	GObjectClass *object_class;
	EFilterRuleClass *filter_rule_class;

	g_type_class_add_private (class, sizeof (EMVFolderEditorRulePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = vfolder_editor_rule_set_property;
	object_class->get_property = vfolder_editor_rule_get_property;
	object_class->dispose = vfolder_editor_rule_dispose;
	object_class->finalize = vfolder_editor_rule_finalize;

	filter_rule_class = E_FILTER_RULE_CLASS (class);
	filter_rule_class->get_widget = get_widget;

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
em_vfolder_editor_rule_init (EMVFolderEditorRule *rule)
{
	rule->priv = EM_VFOLDER_EDITOR_RULE_GET_PRIVATE (rule);
}

EFilterRule *
em_vfolder_editor_rule_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		EM_TYPE_VFOLDER_EDITOR_RULE, "session", session, NULL);
}

EMailSession *
em_vfolder_editor_rule_get_session (EMVFolderEditorRule *rule)
{
	g_return_val_if_fail (EM_IS_VFOLDER_RULE (rule), NULL);

	return rule->priv->session;
}

enum {
	BUTTON_ADD,
	BUTTON_REMOVE,
	BUTTON_LAST
};

struct _source_data {
	ERuleContext *rc;
	EMVFolderRule *vr;
	const gchar *current;
	GtkListStore *model;
	GtkTreeView *list;
	GtkWidget *source_selector;
	GtkButton *buttons[BUTTON_LAST];
};

static void source_add (GtkWidget *widget, struct _source_data *data);
static void source_remove (GtkWidget *widget, struct _source_data *data);

static struct {
	const gchar *name;
	GCallback func;
} edit_buttons[] = {
	{ "source_add",    G_CALLBACK(source_add)   },
	{ "source_remove", G_CALLBACK(source_remove)},
};

static void
set_sensitive (struct _source_data *data)
{
	gtk_widget_set_sensitive (
		GTK_WIDGET (data->buttons[BUTTON_ADD]), TRUE);
	gtk_widget_set_sensitive (
		GTK_WIDGET (data->buttons[BUTTON_REMOVE]),
		data->current != NULL);
}

static void
select_source (GtkWidget *list,
               struct _source_data *data)
{
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	GtkTreeIter iter;

	gtk_tree_view_get_cursor (data->list, &path, &column);
	if (path && gtk_tree_model_get_iter (GTK_TREE_MODEL (data->model), &iter, path))
		gtk_tree_model_get (GTK_TREE_MODEL (data->model), &iter, 0, &data->current, -1);
	else
		data->current = NULL;
	gtk_tree_path_free (path);

	set_sensitive (data);
}

static void
select_source_with_changed (GtkWidget *widget,
                            struct _source_data *data)
{
	em_vfolder_rule_with_t with = 0;
	GSList *group = NULL;
	gint i = 0;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		return;

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	for (i = 0; i< g_slist_length (group); i++) {
		if (g_slist_nth_data (group, with = i) == widget)
			break;
	}

	if (with > EM_VFOLDER_RULE_WITH_LOCAL )
		with = 0;

	gtk_widget_set_sensitive (data->source_selector, !with );

	data->vr->with = with;
}

static void
vfr_folder_response (EMFolderSelector *selector,
                     gint button,
                     struct _source_data *data)
{
	EMFolderTreeModel *model;
	EMailSession *session;
	const gchar *uri;

	model = em_folder_selector_get_model (selector);
	session = em_folder_tree_model_get_session (model);

	uri = em_folder_selector_get_selected_uri (selector);

	if (button == GTK_RESPONSE_OK && uri != NULL) {
		GtkTreeSelection *selection;
		GtkTreeIter iter;
		gchar *markup;

		g_queue_push_tail (&data->vr->sources, g_strdup (uri));

		markup = e_mail_folder_uri_to_markup (
			CAMEL_SESSION (session), uri, NULL);

		gtk_list_store_append (data->model, &iter);
		gtk_list_store_set (data->model, &iter, 0, markup, 1, uri, -1);
		selection = gtk_tree_view_get_selection (data->list);
		gtk_tree_selection_select_iter (selection, &iter);
		data->current = uri;

		g_free (markup);

		set_sensitive (data);
	}

	gtk_widget_destroy (GTK_WIDGET (selector));
}

static void
source_add (GtkWidget *widget,
            struct _source_data *data)
{
	EMFolderTree *folder_tree;
	EMFolderTreeModel *model;
	GtkWidget *dialog;
	gpointer parent;

	parent = gtk_widget_get_toplevel (widget);
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	model = em_folder_tree_model_get_default ();

	dialog = em_folder_selector_new (
		parent, model,
		EM_FOLDER_SELECTOR_CAN_CREATE,
		_("Add Folder"), NULL, _("_Add"));

	folder_tree = em_folder_selector_get_folder_tree (
		EM_FOLDER_SELECTOR (dialog));

	em_folder_tree_set_excluded (folder_tree, EMFT_EXCLUDE_NOSELECT);

	g_signal_connect (
		dialog, "response",
		G_CALLBACK (vfr_folder_response), data);

	gtk_widget_show (dialog);
}

static void
source_remove (GtkWidget *widget,
               struct _source_data *data)
{
	GtkTreeSelection *selection;
	const gchar *source;
	GtkTreePath *path;
	GtkTreeIter iter;
	gint index = 0;
	gint n;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data->list));

	source = NULL;
	while ((source = em_vfolder_rule_next_source (data->vr, source))) {
		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, index);

		if (gtk_tree_selection_path_is_selected (selection, path)) {
			gtk_tree_model_get_iter (
				GTK_TREE_MODEL (data->model), &iter, path);

			em_vfolder_rule_remove_source (data->vr, source);
			gtk_list_store_remove (data->model, &iter);
			gtk_tree_path_free (path);

			/* now select the next rule */
			n = gtk_tree_model_iter_n_children (
				GTK_TREE_MODEL (data->model), NULL);
			index = index >= n ? n - 1 : index;

			if (index >= 0) {
				path = gtk_tree_path_new ();
				gtk_tree_path_append_index (path, index);
				gtk_tree_model_get_iter (
					GTK_TREE_MODEL (data->model),
					&iter, path);
				gtk_tree_path_free (path);

				gtk_tree_selection_select_iter (
					selection, &iter);
				gtk_tree_model_get (
					GTK_TREE_MODEL (data->model), &iter,
					0, &data->current, -1);
			} else {
				data->current = NULL;
			}

			break;
		}

		index++;
		gtk_tree_path_free (path);
	}

	set_sensitive (data);
}

static GtkWidget *
get_widget (EFilterRule *fr,
            ERuleContext *rc)
{
	EMVFolderRule *vr =(EMVFolderRule *) fr;
	EMailSession *session;
	GtkWidget *widget, *frame;
	struct _source_data *data;
	GtkRadioButton *rb;
	const gchar *source;
	GtkTreeIter iter;
	GtkBuilder *builder;
	GObject *object;
	gint i;

	widget = E_FILTER_RULE_CLASS (em_vfolder_editor_rule_parent_class)->
		get_widget (fr, rc);

	data = g_malloc0 (sizeof (*data));
	data->rc = rc;
	data->vr = vr;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-dialogs.ui");

	frame = e_builder_get_widget(builder, "vfolder_source_frame");

	g_object_set_data_full((GObject *)frame, "data", data, g_free);

	for (i = 0; i < BUTTON_LAST; i++) {
		data->buttons[i] =(GtkButton *)
			e_builder_get_widget (builder, edit_buttons[i].name);
		g_signal_connect (
			data->buttons[i], "clicked",
			edit_buttons[i].func, data);
	}

	object = gtk_builder_get_object (builder, "source_list");
	data->list = GTK_TREE_VIEW (object);
	object = gtk_builder_get_object (builder, "source_model");
	data->model = GTK_LIST_STORE (object);

	session = em_vfolder_editor_context_get_session (EM_VFOLDER_EDITOR_CONTEXT (rc));

	source = NULL;
	while ((source = em_vfolder_rule_next_source (vr, source))) {
		gchar *markup;

		markup = e_mail_folder_uri_to_markup (
			CAMEL_SESSION (session), source, NULL);

		gtk_list_store_append (data->model, &iter);
		gtk_list_store_set (data->model, &iter, 0, markup, 1, source, -1);
		g_free (markup);
	}

	g_signal_connect (
		data->list, "cursor-changed",
		G_CALLBACK (select_source), data);

	rb = (GtkRadioButton *)e_builder_get_widget (builder, "local_rb");
	g_signal_connect (
		rb, "toggled",
		G_CALLBACK (select_source_with_changed), data);

	rb = (GtkRadioButton *)e_builder_get_widget (builder, "remote_rb");
	g_signal_connect (
		rb, "toggled",
		G_CALLBACK (select_source_with_changed), data);

	rb = (GtkRadioButton *)e_builder_get_widget (builder, "local_and_remote_rb");
	g_signal_connect (
		rb, "toggled",
		G_CALLBACK (select_source_with_changed), data);

	rb = (GtkRadioButton *) e_builder_get_widget (builder, "specific_rb");
	g_signal_connect (
		rb, "toggled",
		G_CALLBACK (select_source_with_changed), data);

	data->source_selector = (GtkWidget *)
		e_builder_get_widget (builder, "source_selector");

	rb = g_slist_nth_data (gtk_radio_button_get_group (rb), vr->with);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb), TRUE);
	g_signal_emit_by_name (rb, "toggled");

	set_sensitive (data);

	gtk_box_pack_start (GTK_BOX (widget), frame, TRUE, TRUE, 3);

	g_object_unref (builder);

	return widget;
}
