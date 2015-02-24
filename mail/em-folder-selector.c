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

#include "em-folder-selector.h"

#include <config.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

#include "e-mail-folder-create-dialog.h"
#include "em-folder-tree.h"
#include "em-folder-utils.h"
#include "em-utils.h"

#define d(x)

#define EM_FOLDER_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FOLDER_SELECTOR, EMFolderSelectorPrivate))

#define DEFAULT_BUTTON_LABEL N_("_OK")

struct _EMFolderSelectorPrivate {
	EMFolderTreeModel *model;
	GtkWidget *alert_bar;
	GtkWidget *activity_bar;
	GtkWidget *caption_label;
	GtkWidget *content_area;
	GtkWidget *tree_view_frame;

	gchar *selected_uri;

	gboolean can_create;
	gchar *caption;
	gchar *default_button_label;
};

enum {
	PROP_0,
	PROP_CAN_CREATE,
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

G_DEFINE_TYPE_WITH_CODE (
	EMFolderSelector,
	em_folder_selector,
	GTK_TYPE_DIALOG,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_ALERT_SINK,
		em_folder_selector_alert_sink_init))

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
                               GtkAction *action,
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
	EMFolderSelectorPrivate *priv;

	priv = EM_FOLDER_SELECTOR_GET_PRIVATE (object);

	if (priv->model && priv->model != em_folder_tree_model_get_default ())
		em_folder_tree_model_remove_all_stores (priv->model);

	g_clear_object (&priv->model);
	g_clear_object (&priv->alert_bar);
	g_clear_object (&priv->activity_bar);
	g_clear_object (&priv->caption_label);
	g_clear_object (&priv->content_area);
	g_clear_object (&priv->tree_view_frame);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_folder_selector_parent_class)->dispose (object);
}

static void
folder_selector_finalize (GObject *object)
{
	EMFolderSelectorPrivate *priv;

	priv = EM_FOLDER_SELECTOR_GET_PRIVATE (object);

	g_free (priv->selected_uri);
	g_free (priv->caption);
	g_free (priv->default_button_label);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_folder_selector_parent_class)->finalize (object);
}

static void
folder_selector_constructed (GObject *object)
{
	EMFolderSelector *selector;
	EMailSession *session;
	EMFolderTreeModel *model;
	GtkAction *action;
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

	gtk_dialog_add_buttons (
		GTK_DIALOG (selector),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		selector->priv->default_button_label, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (selector), GTK_RESPONSE_OK, FALSE);
	gtk_dialog_set_default_response (
		GTK_DIALOG (selector), GTK_RESPONSE_OK);

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
	gtk_action_set_tooltip (action, _("Create a new folder"));

	action = e_tree_view_frame_lookup_toolbar_action (
		E_TREE_VIEW_FRAME (selector->priv->tree_view_frame),
		E_TREE_VIEW_FRAME_ACTION_REMOVE);
	gtk_action_set_visible (action, FALSE);
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
	EMFolderSelectorPrivate *priv;
	EAlertBar *alert_bar;
	GtkWidget *dialog;
	GtkWindow *parent;

	priv = EM_FOLDER_SELECTOR_GET_PRIVATE (alert_sink);

	switch (e_alert_get_message_type (alert)) {
		case GTK_MESSAGE_INFO:
		case GTK_MESSAGE_WARNING:
		case GTK_MESSAGE_ERROR:
			alert_bar = E_ALERT_BAR (priv->alert_bar);
			e_alert_bar_add_alert (alert_bar, alert);
			break;

		default:
			parent = GTK_WINDOW (alert_sink);
			dialog = e_alert_dialog_new (parent, alert);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			break;
	}
}

static void
em_folder_selector_class_init (EMFolderSelectorClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMFolderSelectorPrivate));

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
	selector->priv = EM_FOLDER_SELECTOR_GET_PRIVATE (selector);

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
	ETreeViewFrame *tree_view_frame;
	GtkTreeView *tree_view;

	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), NULL);

	tree_view_frame = E_TREE_VIEW_FRAME (selector->priv->tree_view_frame);
	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	return EM_FOLDER_TREE (tree_view);
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

	folder_tree = em_folder_selector_get_folder_tree (selector);

	if (em_folder_tree_store_root_selected (folder_tree, out_store)) {
		if (out_folder_name != NULL)
			*out_folder_name = NULL;
		return TRUE;
	}

	return em_folder_tree_get_selected (
		folder_tree, out_store, out_folder_name);
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
	EMFolderTree *folder_tree;
	gchar *uri;

	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), NULL);

	folder_tree = em_folder_selector_get_folder_tree (selector);
	uri = em_folder_tree_get_selected_uri (folder_tree);

	if (uri == NULL)
		return NULL;

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

