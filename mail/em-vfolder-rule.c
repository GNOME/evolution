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
#include <e-util/e-alert.h>
#include <e-util/e-util-private.h>

#include <libemail-engine/e-mail-folder-utils.h>

#include "em-folder-selector.h"
#include "em-folder-tree.h"
#include "em-utils.h"
#include "em-vfolder-context.h"
#include "em-vfolder-rule.h"

#define EM_VFOLDER_RULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_VFOLDER_RULE, EMVFolderRulePrivate))

#define EM_VFOLDER_RULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_VFOLDER_RULE, EMVFolderRulePrivate))

struct _EMVFolderRulePrivate {
	EMailSession *session;
};

enum {
	PROP_0,
	PROP_SESSION
};

static gint validate (EFilterRule *, EAlert **alert);
static gint vfolder_eq (EFilterRule *fr, EFilterRule *cm);
static xmlNodePtr xml_encode (EFilterRule *);
static gint xml_decode (EFilterRule *, xmlNodePtr, ERuleContext *f);
static void rule_copy (EFilterRule *dest, EFilterRule *src);
static GtkWidget *get_widget (EFilterRule *fr, ERuleContext *f);

/* DO NOT internationalise these strings */
static const gchar *with_names[] = {
	"specific",
	"local_remote_active",
	"remote_active",
	"local"
};

G_DEFINE_TYPE (
	EMVFolderRule,
	em_vfolder_rule,
	E_TYPE_FILTER_RULE)

static void
vfolder_rule_set_session (EMVFolderRule *rule,
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
vfolder_rule_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			vfolder_rule_set_session (
				EM_VFOLDER_RULE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
vfolder_rule_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				em_vfolder_rule_get_session (
				EM_VFOLDER_RULE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
vfolder_rule_dispose (GObject *object)
{
	EMVFolderRulePrivate *priv;

	priv = EM_VFOLDER_RULE_GET_PRIVATE (object);

	if (priv->session != NULL) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_vfolder_rule_parent_class)->dispose (object);
}

static void
vfolder_rule_finalize (GObject *object)
{
	EMVFolderRule *rule = EM_VFOLDER_RULE (object);
	gchar *uri;

	while ((uri = g_queue_pop_head (&rule->sources)) != NULL)
		g_free (uri);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_vfolder_rule_parent_class)->finalize (object);
}

static void
em_vfolder_rule_class_init (EMVFolderRuleClass *class)
{
	GObjectClass *object_class;
	EFilterRuleClass *filter_rule_class;

	g_type_class_add_private (class, sizeof (EMVFolderRulePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = vfolder_rule_set_property;
	object_class->get_property = vfolder_rule_get_property;
	object_class->dispose = vfolder_rule_dispose;
	object_class->finalize = vfolder_rule_finalize;

	filter_rule_class = E_FILTER_RULE_CLASS (class);
	filter_rule_class->validate = validate;
	filter_rule_class->eq = vfolder_eq;
	filter_rule_class->xml_encode = xml_encode;
	filter_rule_class->xml_decode = xml_decode;
	filter_rule_class->copy = rule_copy;
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
em_vfolder_rule_init (EMVFolderRule *rule)
{
	rule->priv = EM_VFOLDER_RULE_GET_PRIVATE (rule);
	rule->with = EM_VFOLDER_RULE_WITH_SPECIFIC;
	rule->rule.source = g_strdup ("incoming");
}

EFilterRule *
em_vfolder_rule_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		EM_TYPE_VFOLDER_RULE, "session", session, NULL);
}

EMailSession *
em_vfolder_rule_get_session (EMVFolderRule *rule)
{
	g_return_val_if_fail (EM_IS_VFOLDER_RULE (rule), NULL);

	return rule->priv->session;
}

void
em_vfolder_rule_add_source (EMVFolderRule *rule,
                            const gchar *uri)
{
	g_return_if_fail (EM_IS_VFOLDER_RULE (rule));
	g_return_if_fail (uri);

	g_queue_push_tail (&rule->sources, g_strdup (uri));

	e_filter_rule_emit_changed (E_FILTER_RULE (rule));
}

const gchar *
em_vfolder_rule_find_source (EMVFolderRule *rule,
                             const gchar *uri)
{
	GList *link;

	g_return_val_if_fail (EM_IS_VFOLDER_RULE (rule), NULL);

	/* only does a simple string or address comparison, should
	 * probably do a decoded url comparison */
	link = g_queue_find_custom (
		&rule->sources, uri, (GCompareFunc) strcmp);

	return (link != NULL) ? link->data : NULL;
}

void
em_vfolder_rule_remove_source (EMVFolderRule *rule,
                               const gchar *uri)
{
	gchar *found;

	g_return_if_fail (EM_IS_VFOLDER_RULE (rule));

	found =(gchar *) em_vfolder_rule_find_source (rule, uri);
	if (found != NULL) {
		g_queue_remove (&rule->sources, found);
		g_free (found);
		e_filter_rule_emit_changed (E_FILTER_RULE (rule));
	}
}

const gchar *
em_vfolder_rule_next_source (EMVFolderRule *rule,
                             const gchar *last)
{
	GList *link;

	if (last == NULL) {
		link = g_queue_peek_head_link (&rule->sources);
	} else {
		link = g_queue_find (&rule->sources, last);
		if (link == NULL)
			link = g_queue_peek_head_link (&rule->sources);
		else
			link = g_list_next (link);
	}

	return (link != NULL) ? link->data : NULL;
}

static gint
validate (EFilterRule *fr,
          EAlert **alert)
{
	g_return_val_if_fail (fr != NULL, 0);
	g_warn_if_fail (alert == NULL || *alert == NULL);

	if (!fr->name || !*fr->name) {
		if (alert)
			*alert = e_alert_new ("mail:no-name-vfolder", NULL);
		return 0;
	}

	/* We have to have at least one source set in the "specific" case.
	 * Do not translate this string! */
	if (((EMVFolderRule *) fr)->with == EM_VFOLDER_RULE_WITH_SPECIFIC &&
		g_queue_is_empty (&((EMVFolderRule *) fr)->sources)) {
		if (alert)
			*alert = e_alert_new ("mail:vfolder-no-source", NULL);
		return 0;
	}

	return E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->validate (fr, alert);
}

static gint
queue_eq (GQueue *queue_a,
          GQueue *queue_b)
{
	GList *link_a;
	GList *link_b;
	gint truth = TRUE;

	link_a = g_queue_peek_head_link (queue_a);
	link_b = g_queue_peek_head_link (queue_b);

	while (truth && link_a != NULL && link_b != NULL) {
		gchar *uri_a = link_a->data;
		gchar *uri_b = link_b->data;

		truth = (strcmp (uri_a, uri_b)== 0);

		link_a = g_list_next (link_a);
		link_b = g_list_next (link_b);
	}

	return truth && link_a == NULL && link_b == NULL;
}

static gint
vfolder_eq (EFilterRule *fr,
            EFilterRule *cm)
{
	return E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->eq (fr, cm)
		&& queue_eq (
			&((EMVFolderRule *) fr)->sources,
			&((EMVFolderRule *) cm)->sources);
}

static xmlNodePtr
xml_encode (EFilterRule *fr)
{
	EMVFolderRule *vr =(EMVFolderRule *) fr;
	xmlNodePtr node, set, work;
	GList *head, *link;

	node = E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->xml_encode (fr);
	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (vr->with < G_N_ELEMENTS (with_names), NULL);

	set = xmlNewNode(NULL, (const guchar *)"sources");
	xmlAddChild (node, set);
	xmlSetProp(set, (const guchar *)"with", (guchar *)with_names[vr->with]);

	head = g_queue_peek_head_link (&vr->sources);
	for (link = head; link != NULL; link = g_list_next (link)) {
		const gchar *uri = link->data;

		work = xmlNewNode (NULL, (const guchar *) "folder");
		xmlSetProp (work, (const guchar *) "uri", (guchar *) uri);
		xmlAddChild (set, work);
	}

	return node;
}

static void
set_with (EMVFolderRule *vr,
          const gchar *name)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (with_names); i++) {
		if (!strcmp (name, with_names[i])) {
			vr->with = i;
			return;
		}
	}

	vr->with = 0;
}

static gint
xml_decode (EFilterRule *fr,
            xmlNodePtr node,
            ERuleContext *f)
{
	xmlNodePtr set, work;
	gint result;
	EMVFolderRule *vr =(EMVFolderRule *) fr;
	gchar *tmp;

	result = E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->
		xml_decode (fr, node, f);
	if (result != 0)
		return result;

	/* handle old format file, vfolder source is in filterrule */
	if (strcmp(fr->source, "incoming")!= 0) {
		set_with (vr, fr->source);
		g_free (fr->source);
		fr->source = g_strdup("incoming");
	}

	set = node->children;
	while (set) {
		if (!strcmp((gchar *)set->name, "sources")) {
			tmp = (gchar *)xmlGetProp(set, (const guchar *)"with");
			if (tmp) {
				set_with (vr, tmp);
				xmlFree (tmp);
			}
			work = set->children;
			while (work) {
				if (!strcmp((gchar *)work->name, "folder")) {
					tmp = (gchar *)xmlGetProp(work, (const guchar *)"uri");
					if (tmp) {
						g_queue_push_tail (&vr->sources, g_strdup (tmp));
						xmlFree (tmp);
					}
				}
				work = work->next;
			}
		}
		set = set->next;
	}
	return 0;
}

static void
rule_copy (EFilterRule *dest,
           EFilterRule *src)
{
	EMVFolderRule *vdest, *vsrc;
	GList *head, *link;
	gchar *uri;

	vdest =(EMVFolderRule *) dest;
	vsrc =(EMVFolderRule *) src;

	while ((uri = g_queue_pop_head (&vdest->sources)) != NULL)
		g_free (uri);

	head = g_queue_peek_head_link (&vsrc->sources);
	for (link = head; link != NULL; link = g_list_next (link)) {
		const gchar *uri = link->data;
		g_queue_push_tail (&vdest->sources, g_strdup (uri));
	}

	vdest->with = vsrc->with;

	E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->copy (dest, src);
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
	gtk_tree_model_get_iter (GTK_TREE_MODEL (data->model), &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (GTK_TREE_MODEL (data->model), &iter, 0, &data->current, -1);

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

	widget = E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->
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

	session = em_vfolder_context_get_session (EM_VFOLDER_CONTEXT (rc));

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
