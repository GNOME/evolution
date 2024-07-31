/*
 *
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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

#include "e-mail-folder-create-dialog.h"
#include "em-folder-tree.h"
#include "em-folder-utils.h"
#include "em-utils.h"

#include "em-folder-selector.h"

#define d(x)

#define DEFAULT_BUTTON_LABEL N_("_OK")

struct _EMFolderSelectorPrivate {
	EMFolderTreeModel *model;
	GtkWidget *alert_bar;
	GtkWidget *activity_bar;
	GtkWidget *caption_label;
	GtkWidget *content_area;
	GtkWidget *tree_view_frame;
	GtkWidget *folder_tree_view;
	GtkWidget *search_tree_view;
	gchar *search_text; /* case-folded search text */

	gchar *selected_uri;

	gboolean can_create;
	gboolean can_none;
	gchar *caption;
	gchar *default_button_label;
};

enum {
	PROP_0,
	PROP_CAN_CREATE,
	PROP_CAN_NONE,
	PROP_CAPTION,
	PROP_DEFAULT_BUTTON_LABEL,
	PROP_MODEL
};

enum {
	FOLDER_SELECTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void	em_folder_selector_alert_sink_init
					(EAlertSinkInterface *interface);

G_DEFINE_TYPE_WITH_CODE (EMFolderSelector, em_folder_selector, GTK_TYPE_DIALOG,
	G_ADD_PRIVATE (EMFolderSelector)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, em_folder_selector_alert_sink_init))

enum {
	FILTER_COL_STRING_DISPLAY_NAME,
	FILTER_COL_OBJECT_STORE,
	FILTER_COL_STRING_FOLDER_NAME,
	FILTER_COL_STRING_CASEFOLDED_NAME,
	FILTER_COL_STRING_ICON_NAME,
	FILTER_COL_GICON_CUSTOM_ICON,
	N_FILTER_COLS
};

static void
folder_selector_search_row_activated_cb (GtkTreeView *tree_view,
					 GtkTreePath *path,
					 GtkTreeViewColumn *column,
					 gpointer user_data)
{
	EMFolderSelector *selector = user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelStore *store;
	gchar *folder_name;

	model = gtk_tree_view_get_model (tree_view);

	if (!gtk_tree_model_get_iter (model, &iter, path))
		return;

	gtk_tree_model_get (model, &iter,
		FILTER_COL_OBJECT_STORE, &store,
		FILTER_COL_STRING_FOLDER_NAME, &folder_name,
		-1);

	em_folder_selector_set_selected (selector, store, folder_name);

	g_clear_object (&store);
	g_free (folder_name);

	gtk_dialog_response (GTK_DIALOG (selector), GTK_RESPONSE_OK);
}

static void
folder_selector_search_selection_changed_cb (GtkTreeSelection *selection,
					     gpointer user_data)
{
	EMFolderSelector *selector = user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelStore *store;
	gchar *folder_name;

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		g_signal_emit (selector, signals[FOLDER_SELECTED], 0, NULL, NULL);
		return;
	}

	gtk_tree_model_get (model, &iter,
		FILTER_COL_OBJECT_STORE, &store,
		FILTER_COL_STRING_FOLDER_NAME, &folder_name,
		-1);

	em_folder_selector_set_selected (selector, store, folder_name);

	g_clear_object (&store);
	g_free (folder_name);
}

static void
folder_selector_render_icon (GtkTreeViewColumn *column,
			     GtkCellRenderer *renderer,
			     GtkTreeModel *model,
			     GtkTreeIter *iter,
			     gpointer user_data)
{
	GIcon *icon, *custom_icon = NULL;
	gchar *icon_name = NULL;

	gtk_tree_model_get (model, iter,
		FILTER_COL_STRING_ICON_NAME, &icon_name,
		FILTER_COL_GICON_CUSTOM_ICON, &custom_icon,
		-1);

	if (!icon_name && !custom_icon)
		return;

	if (custom_icon)
		icon = g_object_ref (custom_icon);
	else
		icon = g_themed_icon_new (icon_name);

	g_object_set (renderer, "gicon", icon, NULL);

	g_clear_object (&custom_icon);
	g_object_unref (icon);
	g_free (icon_name);
}

static gboolean
folder_selector_filter_model_cb (GtkTreeModel *tree_model,
				 GtkTreeIter *iter,
				 gpointer user_data)
{
	EMFolderSelector *selector = user_data;
	gchar *casefolded;
	gboolean match;

	/* If there's no search string let everything through. */
	if (!selector->priv->search_text)
		return TRUE;

	gtk_tree_model_get (tree_model, iter,
		FILTER_COL_STRING_CASEFOLDED_NAME, &casefolded,
		-1);

	match = (casefolded != NULL) && (*casefolded != '\0') &&
		(strstr (casefolded, selector->priv->search_text) != NULL);

	g_free (casefolded);

	return match;
}

static gboolean
folder_selector_traverse_model_cb (GtkTreeModel *model,
				   GtkTreePath *path,
				   GtkTreeIter *iter,
				   gpointer user_data)
{
	GtkListStore *list_store = user_data;
	CamelStore *store = NULL;
	GIcon *icon_obj = NULL;
	guint folder_flags = 0;
	gchar *display_name = NULL;
	gchar *full_name = NULL;
	gchar *icon_name = NULL;
	gboolean is_folder = FALSE;

	gtk_tree_model_get (model, iter,
		COL_BOOL_IS_FOLDER, &is_folder,
		COL_UINT_FLAGS, &folder_flags,
		-1);

	if (is_folder && !(folder_flags & CAMEL_FOLDER_NOSELECT)) {
		gtk_tree_model_get (model, iter,
			COL_STRING_DISPLAY_NAME, &display_name,
			COL_OBJECT_CAMEL_STORE, &store,
			COL_STRING_FULL_NAME, &full_name,
			COL_STRING_ICON_NAME, &icon_name,
			COL_GICON_CUSTOM_ICON, &icon_obj,
			-1);

		if (display_name && store && full_name) {
			GtkTreeIter added;
			gboolean is_path;
			gchar *casefolded;
			gchar *tmp;

			is_path = strchr (full_name, '/') != NULL;
			tmp = g_strdup_printf ("%s : %s", camel_service_get_display_name (CAMEL_SERVICE (store)),
				is_path ? full_name : display_name);
			casefolded = g_utf8_casefold (is_path ? full_name : display_name, -1);

			gtk_list_store_append (list_store, &added);
			gtk_list_store_set (list_store, &added,
				FILTER_COL_STRING_DISPLAY_NAME, tmp,
				FILTER_COL_OBJECT_STORE, store,
				FILTER_COL_STRING_FOLDER_NAME, full_name,
				FILTER_COL_STRING_CASEFOLDED_NAME, casefolded,
				FILTER_COL_STRING_ICON_NAME, icon_name,
				FILTER_COL_GICON_CUSTOM_ICON, icon_obj,
				-1);

			g_free (casefolded);
			g_free (tmp);
		}
	}

	g_clear_object (&store);
	g_clear_object (&icon_obj);
	g_free (display_name);
	g_free (full_name);
	g_free (icon_name);

	return FALSE;
}

static void
folder_selector_search_changed_cb (GtkSearchEntry *search_entry,
				   gpointer user_data)
{
	EMFolderSelector *selector = user_data;
	gchar *search_casefolded;

	search_casefolded = g_utf8_casefold (gtk_entry_get_text (GTK_ENTRY (search_entry)), -1);

	if (g_strcmp0 (search_casefolded, selector->priv->search_text ? selector->priv->search_text : "") == 0) {
		g_free (search_casefolded);
		return;
	}

	g_clear_pointer (&selector->priv->search_text, g_free);
	if (search_casefolded && *search_casefolded)
		selector->priv->search_text = search_casefolded;
	else
		g_free (search_casefolded);

	if (selector->priv->search_text) {
		GtkTreeModel *model;

		if (!selector->priv->search_tree_view) {
			GtkCellRenderer *renderer;
			GtkListStore *list_store;
			GtkTreeModel *filter_model;
			GtkTreeView *tree_view;
			GtkTreeSelection *selection;
			GtkTreeViewColumn *column;

			list_store = gtk_list_store_new (N_FILTER_COLS,
				G_TYPE_STRING, /* FILTER_COL_STRING_DISPLAY_NAME */
				CAMEL_TYPE_STORE, /* FILTER_COL_OBJECT_STORE */
				G_TYPE_STRING, /* FILTER_COL_STRING_FOLDER_NAME */
				G_TYPE_STRING, /* FILTER_COL_STRING_CASEFOLDED_NAME */
				G_TYPE_STRING, /* FILTER_COL_STRING_ICON_NAME */
				G_TYPE_ICON); /* FILTER_COL_GICON_CUSTOM_ICON */

			model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector->priv->folder_tree_view));
			gtk_tree_model_foreach (model, folder_selector_traverse_model_cb, list_store);

			filter_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (list_store), NULL);
			gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter_model),
				folder_selector_filter_model_cb, selector, NULL);
			selector->priv->search_tree_view = g_object_ref_sink (gtk_tree_view_new_with_model (filter_model));
			tree_view = GTK_TREE_VIEW (selector->priv->search_tree_view);
			gtk_tree_view_set_search_column (tree_view, FILTER_COL_STRING_DISPLAY_NAME);
			gtk_tree_view_set_headers_visible (tree_view, FALSE);
			g_object_unref (filter_model);
			g_object_unref (list_store);

			column = gtk_tree_view_column_new ();
			gtk_tree_view_column_set_expand (column, TRUE);
			gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
			gtk_tree_view_append_column (tree_view, column);

			renderer = gtk_cell_renderer_pixbuf_new ();
			gtk_tree_view_column_pack_start (column, renderer, FALSE);
			gtk_tree_view_column_set_cell_data_func (column, renderer, folder_selector_render_icon, NULL, NULL);

			renderer = gtk_cell_renderer_text_new ();
			gtk_tree_view_column_pack_start (column, renderer, TRUE);
			gtk_tree_view_column_add_attribute (column, renderer, "text", FILTER_COL_STRING_DISPLAY_NAME);
			g_object_set (renderer, "editable", FALSE, NULL);

			g_signal_connect (tree_view, "row-activated",
				G_CALLBACK (folder_selector_search_row_activated_cb), selector);

			selection = gtk_tree_view_get_selection (tree_view);
			g_signal_connect_object (selection, "changed",
				G_CALLBACK (folder_selector_search_selection_changed_cb), selector, 0);
		}

		e_tree_view_frame_set_tree_view (E_TREE_VIEW_FRAME (selector->priv->tree_view_frame),
			GTK_TREE_VIEW (selector->priv->search_tree_view));

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector->priv->search_tree_view));
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (model));
	} else {
		e_tree_view_frame_set_tree_view (E_TREE_VIEW_FRAME (selector->priv->tree_view_frame),
			GTK_TREE_VIEW (selector->priv->folder_tree_view));
	}
}

static void
folder_selector_stop_search_cb (GtkSearchEntry *search_entry,
				gpointer user_data)
{
	EMFolderSelector *selector = user_data;

	if (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (search_entry)), "") == 0)
		gtk_dialog_response (GTK_DIALOG (selector), GTK_RESPONSE_CANCEL);
	else
		gtk_entry_set_text (GTK_ENTRY (search_entry), "");
}

static void
folder_selector_selected_cb (EMFolderTree *emft,
                             CamelStore *store,
                             const gchar *folder_name,
                             CamelFolderInfoFlags flags,
                             EMFolderSelector *selector)
{
	g_signal_emit (
		selector, signals[FOLDER_SELECTED], 0, store, folder_name);
}

static void
folder_selector_activated_cb (EMFolderTree *emft,
                              CamelStore *store,
                              const gchar *folder_name,
                              EMFolderSelector *selector)
{
	gtk_dialog_response (GTK_DIALOG (selector), GTK_RESPONSE_OK);
}

static void
folder_selector_folder_created_cb (EMailFolderCreateDialog *dialog,
                                   CamelStore *store,
                                   const gchar *folder_name,
                                   GWeakRef *folder_tree_weak_ref)
{
	EMFolderTree *folder_tree;

	folder_tree = g_weak_ref_get (folder_tree_weak_ref);

	if (folder_tree != NULL) {
		gchar *folder_uri;

		/* Select the newly created folder. */
		folder_uri = e_mail_folder_uri_build (store, folder_name);
		em_folder_tree_set_selected (folder_tree, folder_uri, TRUE);
		g_free (folder_uri);

		g_object_unref (folder_tree);
	}
}

static void
folder_selector_action_add_cb (ETreeViewFrame *tree_view_frame,
			       EUIAction *action,
			       EMFolderSelector *selector)
{
	GtkWidget *new_dialog;
	EMailSession *session;
	EMFolderTree *folder_tree;
	const gchar *initial_uri;

	folder_tree = em_folder_selector_get_folder_tree (selector);
	session = em_folder_tree_get_session (folder_tree);

	new_dialog = e_mail_folder_create_dialog_new (
		GTK_WINDOW (selector),
		E_MAIL_UI_SESSION (session));

	gtk_window_set_modal (GTK_WINDOW (new_dialog), TRUE);

	g_signal_connect_data (
		new_dialog, "folder-created",
		G_CALLBACK (folder_selector_folder_created_cb),
		e_weak_ref_new (folder_tree),
		(GClosureNotify) e_weak_ref_free, 0);

	initial_uri = em_folder_selector_get_selected_uri (selector);

	folder_tree = em_folder_selector_get_folder_tree (
		EM_FOLDER_SELECTOR (new_dialog));

	em_folder_tree_set_selected (folder_tree, initial_uri, FALSE);

	gtk_widget_show (new_dialog);
}

static void
folder_selector_set_model (EMFolderSelector *selector,
                           EMFolderTreeModel *model)
{
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (selector->priv->model == NULL);

	selector->priv->model = g_object_ref (model);
}

static void
folder_selector_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_CREATE:
			em_folder_selector_set_can_create (
				EM_FOLDER_SELECTOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_CAN_NONE:
			em_folder_selector_set_can_none (
				EM_FOLDER_SELECTOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_CAPTION:
			em_folder_selector_set_caption (
				EM_FOLDER_SELECTOR (object),
				g_value_get_string (value));
			return;

		case PROP_DEFAULT_BUTTON_LABEL:
			em_folder_selector_set_default_button_label (
				EM_FOLDER_SELECTOR (object),
				g_value_get_string (value));
			return;

		case PROP_MODEL:
			folder_selector_set_model (
				EM_FOLDER_SELECTOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
folder_selector_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_CREATE:
			g_value_set_boolean (
				value,
				em_folder_selector_get_can_create (
				EM_FOLDER_SELECTOR (object)));
			return;

		case PROP_CAN_NONE:
			g_value_set_boolean (
				value,
				em_folder_selector_get_can_none (
				EM_FOLDER_SELECTOR (object)));
			return;

		case PROP_CAPTION:
			g_value_set_string (
				value,
				em_folder_selector_get_caption (
				EM_FOLDER_SELECTOR (object)));
			return;

		case PROP_DEFAULT_BUTTON_LABEL:
			g_value_set_string (
				value,
				em_folder_selector_get_default_button_label (
				EM_FOLDER_SELECTOR (object)));
			return;

		case PROP_MODEL:
			g_value_set_object (
				value,
				em_folder_selector_get_model (
				EM_FOLDER_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
folder_selector_dispose (GObject *object)
{
	EMFolderSelector *self = EM_FOLDER_SELECTOR (object);

	if (self->priv->model && self->priv->model != em_folder_tree_model_get_default ())
		em_folder_tree_model_remove_all_stores (self->priv->model);

	g_clear_object (&self->priv->model);
	g_clear_object (&self->priv->alert_bar);
	g_clear_object (&self->priv->activity_bar);
	g_clear_object (&self->priv->caption_label);
	g_clear_object (&self->priv->content_area);
	g_clear_object (&self->priv->tree_view_frame);
	g_clear_object (&self->priv->folder_tree_view);
	g_clear_object (&self->priv->search_tree_view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_folder_selector_parent_class)->dispose (object);
}

static void
folder_selector_finalize (GObject *object)
{
	EMFolderSelector *self = EM_FOLDER_SELECTOR (object);

	g_free (self->priv->selected_uri);
	g_free (self->priv->caption);
	g_free (self->priv->default_button_label);
	g_free (self->priv->search_text);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_folder_selector_parent_class)->finalize (object);
}

static void
folder_selector_constructed (GObject *object)
{
	EMFolderSelector *selector;
	EMailSession *session;
	EMFolderTreeModel *model;
	EUIAction *action;
	GtkWidget *content_area;
	GtkWidget *container;
	GtkWidget *widget;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (em_folder_selector_parent_class)->constructed (object);

	selector = EM_FOLDER_SELECTOR (object);
	model = em_folder_selector_get_model (selector);
	session = em_folder_tree_model_get_session (model);

	gtk_window_set_default_size (GTK_WINDOW (selector), 400, 500);
	gtk_container_set_border_width (GTK_CONTAINER (selector), 5);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (selector));

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 0);
	selector->priv->content_area = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_search_entry_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_entry_set_placeholder_text (GTK_ENTRY (widget), _("Search for folder…"));
	gtk_widget_show (widget);

	g_signal_connect (widget, "search-changed",
		G_CALLBACK (folder_selector_search_changed_cb), selector);
	g_signal_connect (widget, "stop-search",
		G_CALLBACK (folder_selector_stop_search_cb), selector);

	gtk_dialog_add_buttons (
		GTK_DIALOG (selector),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_None"), GTK_RESPONSE_NO,
		selector->priv->default_button_label, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (selector), GTK_RESPONSE_OK, FALSE);
	gtk_dialog_set_default_response (
		GTK_DIALOG (selector), GTK_RESPONSE_OK);

	widget = gtk_dialog_get_widget_for_response (GTK_DIALOG (selector), GTK_RESPONSE_NO);

	e_binding_bind_property (
		selector, "can-none",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	widget = gtk_dialog_get_widget_for_response (
		GTK_DIALOG (selector), GTK_RESPONSE_OK);

	/* No need to synchronize properties. */
	e_binding_bind_property (
		selector, "default-button-label",
		widget, "label",
		G_BINDING_DEFAULT);

	widget = e_alert_bar_new ();
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	selector->priv->alert_bar = g_object_ref (widget);
	/* EAlertBar controls its own visibility. */

	widget = e_activity_bar_new ();
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	selector->priv->activity_bar = g_object_ref (widget);
	/* EActivityBar controls its own visibility. */

	widget = e_tree_view_frame_new ();
	gtk_box_pack_end (GTK_BOX (container), widget, TRUE, TRUE, 0);
	selector->priv->tree_view_frame = g_object_ref (widget);
	gtk_widget_set_size_request (widget, -1, 240);
	gtk_widget_show (widget);

	g_signal_connect (
		widget,
		"toolbar-action-activate::"
		E_TREE_VIEW_FRAME_ACTION_ADD,
		G_CALLBACK (folder_selector_action_add_cb),
		selector);

	e_binding_bind_property (
		selector, "can-create",
		widget, "toolbar-visible",
		G_BINDING_SYNC_CREATE);

	container = widget;

	widget = em_folder_tree_new_with_model (
		session, E_ALERT_SINK (selector), model);
	emu_restore_folder_tree_state (EM_FOLDER_TREE (widget));
	e_tree_view_frame_set_tree_view (
		E_TREE_VIEW_FRAME (container),
		GTK_TREE_VIEW (widget));
	gtk_widget_grab_focus (widget);
	gtk_widget_show (widget);

	selector->priv->folder_tree_view = g_object_ref (widget);

	g_signal_connect (
		widget, "folder-selected",
		G_CALLBACK (folder_selector_selected_cb), selector);
	g_signal_connect (
		widget, "folder-activated",
		G_CALLBACK (folder_selector_activated_cb), selector);

	container = selector->priv->content_area;

	/* This can be made visible by setting the "caption" property. */
	widget = gtk_label_new (NULL);
	gtk_widget_set_margin_top (widget, 6);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, TRUE, 0);
	selector->priv->caption_label = g_object_ref (widget);
	gtk_widget_hide (widget);

	e_binding_bind_property (
		selector, "caption",
		widget, "label",
		G_BINDING_DEFAULT);

	action = e_tree_view_frame_lookup_toolbar_action (
		E_TREE_VIEW_FRAME (selector->priv->tree_view_frame),
		E_TREE_VIEW_FRAME_ACTION_ADD);
	e_ui_action_set_tooltip (action, _("Create a new folder"));

	action = e_tree_view_frame_lookup_toolbar_action (
		E_TREE_VIEW_FRAME (selector->priv->tree_view_frame),
		E_TREE_VIEW_FRAME_ACTION_REMOVE);
	e_ui_action_set_visible (action, FALSE);
}

static void
folder_selector_folder_selected (EMFolderSelector *selector,
                                 CamelStore *store,
                                 const gchar *folder_name)
{
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (selector), GTK_RESPONSE_OK,
		(store != NULL) && (folder_name != NULL));
}

static void
folder_selector_submit_alert (EAlertSink *alert_sink,
                              EAlert *alert)
{
	EMFolderSelector *self = EM_FOLDER_SELECTOR (alert_sink);

	e_alert_bar_submit_alert (E_ALERT_BAR (self->priv->alert_bar), alert);
}

static void
em_folder_selector_class_init (EMFolderSelectorClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = folder_selector_set_property;
	object_class->get_property = folder_selector_get_property;
	object_class->dispose = folder_selector_dispose;
	object_class->finalize = folder_selector_finalize;
	object_class->constructed = folder_selector_constructed;

	class->folder_selected = folder_selector_folder_selected;

	g_object_class_install_property (
		object_class,
		PROP_CAN_CREATE,
		g_param_spec_boolean (
			"can-create",
			"Can Create",
			"Allow the user to create a new folder "
			"before making a final selection",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_CAN_NONE,
		g_param_spec_boolean (
			"can-none",
			"Can None",
			"Whether can show 'None' button, to be able to unselect folder",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_CAPTION,
		g_param_spec_string (
			"caption",
			"Caption",
			"Brief description above folder tree",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_BUTTON_LABEL,
		g_param_spec_string (
			"default-button-label",
			"Default Button Label",
			"Label for the dialog's default button",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			NULL,
			NULL,
			EM_TYPE_FOLDER_TREE_MODEL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[FOLDER_SELECTED] = g_signal_new (
		"folder-selected",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMFolderSelectorClass, folder_selected),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		CAMEL_TYPE_STORE,
		G_TYPE_STRING);
}

static void
em_folder_selector_alert_sink_init (EAlertSinkInterface *interface)
{
	interface->submit_alert = folder_selector_submit_alert;
}

static void
em_folder_selector_init (EMFolderSelector *selector)
{
	selector->priv = em_folder_selector_get_instance_private (selector);

	selector->priv->default_button_label =
		g_strdup (gettext (DEFAULT_BUTTON_LABEL));
}

GtkWidget *
em_folder_selector_new (GtkWindow *parent,
                        EMFolderTreeModel *model)
{
	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);

	return g_object_new (
		EM_TYPE_FOLDER_SELECTOR,
		"transient-for", parent,
		"use-header-bar", e_util_get_use_header_bar (),
		"model", model, NULL);
}

/**
 * em_folder_selector_get_can_create:
 * @selector: an #EMFolderSelector
 *
 * Returns whether the user can create a new folder before making a final
 * selection.  When %TRUE, the action area of the dialog will show a "New"
 * button.
 *
 * Returns: whether folder creation is allowed
 **/
gboolean
em_folder_selector_get_can_create (EMFolderSelector *selector)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), FALSE);

	return selector->priv->can_create;
}

/**
 * em_folder_selector_set_can_create:
 * @selector: an #EMFolderSelector
 * @can_create: whether folder creation is allowed
 *
 * Sets whether the user can create a new folder before making a final
 * selection.  When %TRUE, the action area of the dialog will show a "New"
 * button.
 **/
void
em_folder_selector_set_can_create (EMFolderSelector *selector,
                                   gboolean can_create)
{
	g_return_if_fail (EM_IS_FOLDER_SELECTOR (selector));

	if (can_create == selector->priv->can_create)
		return;

	selector->priv->can_create = can_create;

	g_object_notify (G_OBJECT (selector), "can-create");
}

/**
 * em_folder_selector_get_can_none:
 * @selector: an #EMFolderSelector
 *
 * Returns whether the user can unselect folder by using a 'None' button.
 *
 * Returns: whether can unselect folder
 *
 * Since: 3.36
 **/
gboolean
em_folder_selector_get_can_none (EMFolderSelector *selector)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), FALSE);

	return selector->priv->can_none;
}

/**
 * em_folder_selector_set_can_none:
 * @selector: an #EMFolderSelector
 * @can_none: whether can unselect folder
 *
 * Sets whether the user can unselect folder using a 'None' button.
 *
 * Since: 3.36
 **/
void
em_folder_selector_set_can_none (EMFolderSelector *selector,
				 gboolean can_none)
{
	g_return_if_fail (EM_IS_FOLDER_SELECTOR (selector));

	if (can_none == selector->priv->can_none)
		return;

	selector->priv->can_none = can_none;

	g_object_notify (G_OBJECT (selector), "can-none");
}

/**
 * em_folder_selector_get_caption:
 * @selector: an #EMFolderSelector
 *
 * Returns the folder tree caption, which is an optional brief message
 * instructing the user what to do.  If no caption has been set, the
 * function returns %NULL.
 *
 * Returns: the folder tree caption, or %NULL
 **/
const gchar *
em_folder_selector_get_caption (EMFolderSelector *selector)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), NULL);

	return selector->priv->caption;
}

/**
 * em_folder_selector_set_caption:
 * @selector: an #EMFolderSelector
 * @caption: the folder tree caption, or %NULL
 *
 * Sets the folder tree caption, which is an optional brief message
 * instructing the user what to do.  If @caption is %NULL or empty,
 * the label widget is hidden so as not to waste vertical space.
 **/
void
em_folder_selector_set_caption (EMFolderSelector *selector,
                                const gchar *caption)
{
	gboolean visible;

	g_return_if_fail (EM_IS_FOLDER_SELECTOR (selector));

	if (g_strcmp0 (caption, selector->priv->caption) == 0)
		return;

	g_free (selector->priv->caption);
	selector->priv->caption = e_util_strdup_strip (caption);

	visible = (selector->priv->caption != NULL);
	gtk_widget_set_visible (selector->priv->caption_label, visible);

	g_object_notify (G_OBJECT (selector), "caption");
}

/**
 * em_folder_selector_get_default_button_label:
 * @selector: an #EMFolderSelector
 *
 * Returns the label for the dialog's default button, which triggers a
 * #GTK_RESPONSE_OK response ID.
 *
 * Returns: the label for the default button
 **/
const gchar *
em_folder_selector_get_default_button_label (EMFolderSelector *selector)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), NULL);

	return selector->priv->default_button_label;
}

/**
 * em_folder_selector_set_default_button_label:
 * @selector: an #EMFolderSelector
 * @button_label: the label for the default button, or %NULL
 *
 * Sets the label for the dialog's default button, which triggers a
 * #GTK_RESPONSE_OK response ID.  If @button_label is %NULL, the default
 * button's label is reset to "OK".
 **/
void
em_folder_selector_set_default_button_label (EMFolderSelector *selector,
                                             const gchar *button_label)
{
	g_return_if_fail (EM_IS_FOLDER_SELECTOR (selector));

	if (button_label == NULL)
		button_label = gettext (DEFAULT_BUTTON_LABEL);

	if (g_strcmp0 (button_label, selector->priv->default_button_label) == 0)
		return;

	g_free (selector->priv->default_button_label);
	selector->priv->default_button_label = g_strdup (button_label);

	g_object_notify (G_OBJECT (selector), "default-button-label");
}

EMFolderTreeModel *
em_folder_selector_get_model (EMFolderSelector *selector)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), NULL);

	return selector->priv->model;
}

/**
 * em_folder_selector_get_content_area:
 * @selector: an #EMFolderSelector
 *
 * Returns the #GtkBox widget containing the dialog's content, including
 * the #EMFolderTree widget.  This is intended to help extend the dialog
 * with additional widgets.
 *
 * Note, the returned #GtkBox is a child of the #GtkBox returned by
 * gtk_dialog_get_content_area(), but with a properly set border width
 * and 6 pixel spacing.
 *
 * Returns: a #GtkBox widget
 **/
GtkWidget *
em_folder_selector_get_content_area (EMFolderSelector *selector)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), NULL);

	return selector->priv->content_area;
}

EMFolderTree *
em_folder_selector_get_folder_tree (EMFolderSelector *selector)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), NULL);

	return EM_FOLDER_TREE (selector->priv->folder_tree_view);
}

/**
 * em_folder_selector_get_selected:
 * @selector: an #EMFolderSelector
 * @out_store: return location for a #CamelStore, or %NULL
 * @out_folder_name: return location for a folder name string, or %NULL
 *
 * Sets @out_store and @out_folder_name to the currently selected folder
 * in the @selector dialog and returns %TRUE.  If only a #CamelStore row
 * is selected, the function returns the #CamelStore through @out_store,
 * sets @out_folder_name to %NULL and returns %TRUE.
 *
 * If the dialog has no selection, the function leaves @out_store and
 * @out_folder_name unset and returns %FALSE.
 *
 * Unreference the returned #CamelStore with g_object_unref() and free
 * the returned folder name with g_free() when finished with them.
 *
 * Returns: whether a row is selected in the @selector dialog
 **/
gboolean
em_folder_selector_get_selected (EMFolderSelector *selector,
                                 CamelStore **out_store,
                                 gchar **out_folder_name)
{
	EMFolderTree *folder_tree;

	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), FALSE);

	if (selector->priv->search_text) {
		GtkTreeView *tree_view = GTK_TREE_VIEW (selector->priv->search_tree_view);
		GtkTreeModel *model = NULL;
		GtkTreeIter iter;
		CamelStore *store = NULL;
		gchar *folder_name = NULL;

		if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection (tree_view), &model, &iter))
			return FALSE;

		gtk_tree_model_get (model, &iter,
			FILTER_COL_OBJECT_STORE, &store,
			FILTER_COL_STRING_FOLDER_NAME, &folder_name,
			-1);

		if (!store || !folder_name) {
			g_clear_object (&store);
			g_free (folder_name);
			return FALSE;
		}

		if (out_store)
			*out_store = store;
		else
			g_object_unref (store);

		if (out_folder_name)
			*out_folder_name = folder_name;
		else
			g_free (folder_name);

		return TRUE;
	} else {
		folder_tree = em_folder_selector_get_folder_tree (selector);

		if (em_folder_tree_store_root_selected (folder_tree, out_store)) {
			if (out_folder_name != NULL)
				*out_folder_name = NULL;
			return TRUE;
		}

		return em_folder_tree_get_selected (folder_tree, out_store, out_folder_name);
	}
}

/**
 * em_folder_selector_set_selected:
 * @selector: an #EMFolderSelector
 * @store: a #CamelStore
 * @folder_name: a folder name
 *
 * Selects the folder given by @store and @folder_name in the @selector
 * dialog.
 **/
void
em_folder_selector_set_selected (EMFolderSelector *selector,
                                 CamelStore *store,
                                 const gchar *folder_name)
{
	EMFolderTree *folder_tree;
	gchar *folder_uri;

	g_return_if_fail (EM_IS_FOLDER_SELECTOR (selector));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_name != NULL);

	folder_tree = em_folder_selector_get_folder_tree (selector);
	folder_uri = e_mail_folder_uri_build (store, folder_name);

	em_folder_tree_set_selected (folder_tree, folder_uri, FALSE);

	g_free (folder_uri);
}

const gchar *
em_folder_selector_get_selected_uri (EMFolderSelector *selector)
{
	CamelStore *store = NULL;
	gchar *folder_name = NULL;
	gchar *uri;

	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), NULL);

	if (!em_folder_selector_get_selected (selector, &store, &folder_name))
		return NULL;

	uri = e_mail_folder_uri_build (store, folder_name);

	g_object_unref (store);
	g_free (folder_name);

	g_free (selector->priv->selected_uri);
	selector->priv->selected_uri = uri;  /* takes ownership */

	return uri;
}

/**
 * em_folder_selector_new_activity:
 * @selector: an #EMFolderSelector
 *
 * Returns a new #EActivity configured to display status and error messages
 * directly in the @selector dialog.
 *
 * Returns: an #EActivity
 **/
EActivity *
em_folder_selector_new_activity (EMFolderSelector *selector)
{
	EActivity *activity;
	EActivityBar *activity_bar;
	EAlertSink *alert_sink;
	GCancellable *cancellable;

	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), NULL);

	activity = e_activity_new ();

	alert_sink = E_ALERT_SINK (selector);
	e_activity_set_alert_sink (activity, alert_sink);

	cancellable = camel_operation_new ();
	e_activity_set_cancellable (activity, cancellable);
	g_object_unref (cancellable);

	activity_bar = E_ACTIVITY_BAR (selector->priv->activity_bar);
	e_activity_bar_set_activity (activity_bar, activity);

	return activity;
}

void
em_folder_selector_maybe_collapse_archive_folders (EMFolderSelector *selector)
{
	EMFolderTreeModel *model;
	EMailSession *mail_session;
	ESourceRegistry *registry;
	CamelSession *session;
	GSettings *settings;
	GList *services, *link;
	GHashTable *archives;
	gchar *local_archive_folder;

	g_return_if_fail (EM_IS_FOLDER_SELECTOR (selector));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	if (!g_settings_get_boolean (settings, "collapse-archive-folders-in-selectors")) {
		g_object_unref (settings);
		return;
	}
	local_archive_folder = g_settings_get_string (settings, "local-archive-folder");
	g_object_unref (settings);

	model = em_folder_selector_get_model (selector);
	mail_session = em_folder_tree_model_get_session (model);
	registry = e_mail_session_get_registry (mail_session);
	session = CAMEL_SESSION (mail_session);

	archives = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	if (local_archive_folder && *local_archive_folder) {
		g_hash_table_insert (archives, local_archive_folder, NULL);
	} else {
		g_free (local_archive_folder);
	}

	services = camel_session_list_services (session);
	for (link = services; link; link = g_list_next (link)) {
		CamelService *service = link->data;

		if (CAMEL_IS_STORE (service)) {
			ESource *source;

			source = e_source_registry_ref_source (registry, camel_service_get_uid (service));
			if (source && e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT)) {
				ESourceMailAccount *account_ext;
				gchar *archive_folder;

				account_ext = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);

				archive_folder = e_source_mail_account_dup_archive_folder (account_ext);
				if (archive_folder && *archive_folder) {
					g_hash_table_insert (archives, archive_folder, NULL);
				} else {
					g_free (archive_folder);
				}
			}

			g_clear_object (&source);
		}
	}

	g_list_free_full (services, g_object_unref);

	if (g_hash_table_size (archives)) {
		GtkTreeView *tree_view;
		GHashTableIter iter;
		gpointer key;

		tree_view = GTK_TREE_VIEW (em_folder_selector_get_folder_tree (selector));

		g_hash_table_iter_init (&iter, archives);

		while (g_hash_table_iter_next (&iter, &key, NULL)) {
			const gchar *folder_uri = key;
			CamelStore *store = NULL;
			gchar *folder_name = NULL;

			if (folder_uri && *folder_uri &&
			    e_mail_folder_uri_parse (session, folder_uri, &store, &folder_name, NULL)) {
				GtkTreeRowReference *row;

				row = em_folder_tree_model_get_row_reference (model, store, folder_name);
				if (row) {
					GtkTreePath *path;

					path = gtk_tree_row_reference_get_path (row);
					gtk_tree_view_collapse_row (tree_view, path);
					gtk_tree_path_free (path);
				}

				g_clear_object (&store);
				g_free (folder_name);
			}
		}
	}

	g_hash_table_destroy (archives);
}
