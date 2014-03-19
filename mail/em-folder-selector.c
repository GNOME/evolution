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

#include "em-folder-tree.h"
#include "em-folder-utils.h"
#include "em-utils.h"

#define d(x)

#define EM_FOLDER_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FOLDER_SELECTOR, EMFolderSelectorPrivate))

/* Dialog response code. */
#define EM_FOLDER_SELECTOR_RESPONSE_NEW 1

#define DEFAULT_BUTTON_LABEL N_("_OK")

struct _EMFolderSelectorPrivate {
	EMFolderTree *folder_tree;  /* not referenced */
	EMFolderTreeModel *model;
	GtkWidget *alert_bar;
	GtkWidget *caption_label;

	GtkEntry *name_entry;
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

	g_clear_object (&priv->model);
	g_clear_object (&priv->alert_bar);
	g_clear_object (&priv->caption_label);

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
folder_selector_response (GtkDialog *dialog,
                          gint response_id)
{
	EMFolderSelectorPrivate *priv;

	/* Do not chain up.  GtkDialog does not implement this method. */

	priv = EM_FOLDER_SELECTOR_GET_PRIVATE (dialog);

	if (response_id == EM_FOLDER_SELECTOR_RESPONSE_NEW) {
		EMailSession *session;
		const gchar *uri;

		g_object_set_data (
			G_OBJECT (priv->folder_tree),
			"select", GUINT_TO_POINTER (1));

		session = em_folder_tree_get_session (priv->folder_tree);

		uri = em_folder_selector_get_selected_uri (
			EM_FOLDER_SELECTOR (dialog));

		em_folder_utils_create_folder (
			GTK_WINDOW (dialog), session,
			priv->folder_tree, uri);

		g_signal_stop_emission_by_name (dialog, "response");
	}
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
	GtkDialogClass *dialog_class;

	g_type_class_add_private (class, sizeof (EMFolderSelectorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = folder_selector_set_property;
	object_class->get_property = folder_selector_get_property;
	object_class->dispose = folder_selector_dispose;
	object_class->finalize = folder_selector_finalize;

	dialog_class = GTK_DIALOG_CLASS (class);
	dialog_class->response = folder_selector_response;

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

static void
folder_selector_create_name_changed (GtkEntry *entry,
                                     EMFolderSelector *selector)
{
	EMFolderTree *folder_tree;
	gchar *path;
	const gchar *text = NULL;
	gboolean active;

	if (gtk_entry_get_text_length (selector->priv->name_entry) > 0)
		text = gtk_entry_get_text (selector->priv->name_entry);

	folder_tree = em_folder_selector_get_folder_tree (selector);

	path = em_folder_tree_get_selected_uri (folder_tree);
	active = text && path && !strchr (text, '/');
	g_free (path);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (selector), GTK_RESPONSE_OK, active);
}

static void
folder_selected_cb (EMFolderTree *emft,
                    CamelStore *store,
                    const gchar *folder_name,
                    CamelFolderInfoFlags flags,
                    EMFolderSelector *selector)
{
	if (selector->priv->name_entry != NULL) {
		folder_selector_create_name_changed (
			selector->priv->name_entry, selector);
	} else {
		gtk_dialog_set_response_sensitive (
			GTK_DIALOG (selector), GTK_RESPONSE_OK, TRUE);
	}
}

static void
folder_activated_cb (EMFolderTree *emft,
                     CamelStore *store,
                     const gchar *folder_name,
                     EMFolderSelector *selector)
{
	gtk_dialog_response (GTK_DIALOG (selector), GTK_RESPONSE_OK);
}

static GtkWidget *
folder_selector_construct (EMFolderSelector *selector)
{
	EMailSession *session;
	EMFolderTreeModel *model;
	GtkWidget *content_area;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *vbox;

	model = em_folder_selector_get_model (selector);
	session = em_folder_tree_model_get_session (model);

	gtk_window_set_default_size (GTK_WINDOW (selector), 400, 500);
	gtk_container_set_border_width (GTK_CONTAINER (selector), 5);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (selector));

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	container = vbox;

	gtk_dialog_add_buttons (
		GTK_DIALOG (selector),
		_("_New"), EM_FOLDER_SELECTOR_RESPONSE_NEW,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		selector->priv->default_button_label, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (selector), GTK_RESPONSE_OK, FALSE);
	gtk_dialog_set_default_response (
		GTK_DIALOG (selector), GTK_RESPONSE_OK);

	widget = gtk_dialog_get_widget_for_response (
		GTK_DIALOG (selector), EM_FOLDER_SELECTOR_RESPONSE_NEW);

	g_object_bind_property (
		selector, "can-create",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	widget = gtk_dialog_get_widget_for_response (
		GTK_DIALOG (selector), GTK_RESPONSE_OK);

	/* No need to synchronize properties. */
	g_object_bind_property (
		selector, "default-button-label",
		widget, "label",
		G_BINDING_DEFAULT);

	widget = e_alert_bar_new ();
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	selector->priv->alert_bar = g_object_ref (widget);
	/* EAlertBar controls its own visibility. */

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_end (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = em_folder_tree_new_with_model (
		session, E_ALERT_SINK (selector), model);
	emu_restore_folder_tree_state (EM_FOLDER_TREE (widget));
	gtk_container_add (GTK_CONTAINER (container), widget);
	selector->priv->folder_tree = EM_FOLDER_TREE (widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "folder-selected",
		G_CALLBACK (folder_selected_cb), selector);
	g_signal_connect (
		widget, "folder-activated",
		G_CALLBACK (folder_activated_cb), selector);

	container = vbox;

	/* This can be made visible by setting the "caption" property. */
	widget = gtk_label_new (NULL);
	gtk_widget_set_margin_top (widget, 6);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, TRUE, 0);
	selector->priv->caption_label = g_object_ref (widget);
	gtk_widget_hide (widget);

	g_object_bind_property (
		selector, "caption",
		widget, "label",
		G_BINDING_DEFAULT);

	gtk_widget_grab_focus (GTK_WIDGET (selector->priv->folder_tree));

	return vbox;
}

GtkWidget *
em_folder_selector_new (GtkWindow *parent,
                        EMFolderTreeModel *model)
{
	EMFolderSelector *selector;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);

	selector = g_object_new (
		EM_TYPE_FOLDER_SELECTOR,
		"transient-for", parent,
		"model", model, NULL);
	folder_selector_construct (selector);

	return GTK_WIDGET (selector);
}

static void
folder_selector_create_name_activate (GtkEntry *entry,
                                      EMFolderSelector *selector)
{
	if (gtk_entry_get_text_length (selector->priv->name_entry) > 0) {
		EMFolderTree *folder_tree;
		gchar *path;
		const gchar *text;
		gboolean emit_response;

		text = gtk_entry_get_text (selector->priv->name_entry);

		folder_tree = em_folder_selector_get_folder_tree (selector);
		path = em_folder_tree_get_selected_uri (folder_tree);

		emit_response =
			(path != NULL) &&
			(text != NULL) &&
			(strchr (text, '/') == NULL);

		if (emit_response) {
			g_signal_emit_by_name (
				selector, "response", GTK_RESPONSE_OK);
		}

		g_free (path);
	}
}

GtkWidget *
em_folder_selector_create_new (GtkWindow *parent,
                               EMFolderTreeModel *model)
{
	EMFolderSelector *selector;
	EMFolderTree *folder_tree;
	GtkWidget *container;
	GtkWidget *widget;
	GtkLabel *label;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);

	selector = g_object_new (
		EM_TYPE_FOLDER_SELECTOR,
		"transient-for", parent,
		"model", model,
		"default-button-label", _("C_reate"), NULL);

	container = folder_selector_construct (selector);

	folder_tree = em_folder_selector_get_folder_tree (selector);
	em_folder_tree_set_excluded (folder_tree, EMFT_EXCLUDE_NOINFERIORS);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("Folder _name:"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	selector->priv->name_entry = GTK_ENTRY (widget);
	gtk_widget_grab_focus (widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "changed",
		G_CALLBACK (folder_selector_create_name_changed), selector);
	g_signal_connect (
		widget, "activate",
		G_CALLBACK (folder_selector_create_name_activate), selector);

	return GTK_WIDGET (selector);
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

EMFolderTree *
em_folder_selector_get_folder_tree (EMFolderSelector *selector)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (selector), NULL);

	return selector->priv->folder_tree;
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

	if (selector->priv->name_entry != NULL) {
		const gchar *name;
		gchar *temp_uri, *escaped_name;

		name = gtk_entry_get_text (selector->priv->name_entry);
		escaped_name = g_uri_escape_string (name, NULL, TRUE);
		temp_uri = g_strconcat (uri, "/", escaped_name, NULL);

		g_free (escaped_name);
		g_free (uri);
		uri = temp_uri;
	}

	g_free (selector->priv->selected_uri);
	selector->priv->selected_uri = uri;  /* takes ownership */

	return uri;
}
