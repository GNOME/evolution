/*
 * e-book-shell-content.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-book-shell-content.h"

#include <glib/gi18n.h>

#include "shell/e-shell-utils.h"
#include "e-book-shell-view.h"

#define E_BOOK_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_BOOK_SHELL_CONTENT, EBookShellContentPrivate))

struct _EBookShellContentPrivate {
	GtkWidget *paned;
	GtkWidget *notebook;
	GtkWidget *preview_pane;

	GtkOrientation orientation;

	gboolean preview_show_maps;
	guint preview_visible : 1;
};

enum {
	PROP_0,
	PROP_CURRENT_VIEW,
	PROP_ORIENTATION,
	PROP_PREVIEW_CONTACT,
	PROP_PREVIEW_VISIBLE,
	PROP_PREVIEW_SHOW_MAPS
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EBookShellContent,
	e_book_shell_content,
	E_TYPE_SHELL_CONTENT,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		GTK_TYPE_ORIENTABLE, NULL))

static void
book_shell_content_send_message_cb (EBookShellContent *book_shell_content,
                                    EDestination *destination,
                                    EABContactDisplay *display)
{
	EShell *shell;
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;
	GSList node = { destination, NULL };

	shell_content = E_SHELL_CONTENT (book_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	eab_send_as_to (shell, &node);
}

static void
book_shell_content_restore_state_cb (EShellWindow *shell_window,
                                     EShellView *shell_view,
                                     EShellContent *shell_content)
{
	EBookShellContentPrivate *priv;
	GSettings *settings;

	priv = E_BOOK_SHELL_CONTENT_GET_PRIVATE (shell_content);

	/* Bind GObject properties to GSettings keys. */

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");

	if (e_shell_window_is_main_instance (shell_window)) {
		g_settings_bind (
			settings, "hpane-position",
			priv->paned, "hposition",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "vpane-position",
			priv->paned, "vposition",
			G_SETTINGS_BIND_DEFAULT);
	} else {
		g_settings_bind (
			settings, "hpane-position-sub",
			priv->paned, "hposition",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "vpane-position-sub",
			priv->paned, "vposition",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);
	}

	g_object_unref (settings);
}

static GtkOrientation
book_shell_content_get_orientation (EBookShellContent *book_shell_content)
{
	return book_shell_content->priv->orientation;
}

static void
book_shell_content_set_orientation (EBookShellContent *book_shell_content,
                                    GtkOrientation orientation)
{
	if (book_shell_content->priv->orientation == orientation)
		return;

	book_shell_content->priv->orientation = orientation;

	g_object_notify (G_OBJECT (book_shell_content), "orientation");
}

static void
book_shell_content_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_VIEW:
			e_book_shell_content_set_current_view (
				E_BOOK_SHELL_CONTENT (object),
				g_value_get_object (value));
			return;

		case PROP_ORIENTATION:
			book_shell_content_set_orientation (
				E_BOOK_SHELL_CONTENT (object),
				g_value_get_enum (value));
			return;

		case PROP_PREVIEW_CONTACT:
			e_book_shell_content_set_preview_contact (
				E_BOOK_SHELL_CONTENT (object),
				g_value_get_object (value));
			return;

		case PROP_PREVIEW_VISIBLE:
			e_book_shell_content_set_preview_visible (
				E_BOOK_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_PREVIEW_SHOW_MAPS:
			e_book_shell_content_set_preview_show_maps (
				E_BOOK_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
book_shell_content_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_VIEW:
			g_value_set_object (
				value,
				e_book_shell_content_get_current_view (
				E_BOOK_SHELL_CONTENT (object)));
			return;

		case PROP_ORIENTATION:
			g_value_set_enum (
				value,
				book_shell_content_get_orientation (
				E_BOOK_SHELL_CONTENT (object)));
			return;

		case PROP_PREVIEW_CONTACT:
			g_value_set_object (
				value,
				e_book_shell_content_get_preview_contact (
				E_BOOK_SHELL_CONTENT (object)));
			return;

		case PROP_PREVIEW_VISIBLE:
			g_value_set_boolean (
				value,
				e_book_shell_content_get_preview_visible (
				E_BOOK_SHELL_CONTENT (object)));
			return;

		case PROP_PREVIEW_SHOW_MAPS:
			g_value_set_boolean (
				value,
				e_book_shell_content_get_preview_show_maps (
				E_BOOK_SHELL_CONTENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
book_shell_content_dispose (GObject *object)
{
	EBookShellContentPrivate *priv;

	priv = E_BOOK_SHELL_CONTENT_GET_PRIVATE (object);

	if (priv->paned != NULL) {
		g_object_unref (priv->paned);
		priv->paned = NULL;
	}

	if (priv->notebook != NULL) {
		g_object_unref (priv->notebook);
		priv->notebook = NULL;
	}

	if (priv->preview_pane != NULL) {
		g_object_unref (priv->preview_pane);
		priv->preview_pane = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_book_shell_content_parent_class)->dispose (object);
}

static void
book_shell_content_constructed (GObject *object)
{
	EBookShellContentPrivate *priv;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellTaskbar *shell_taskbar;
	GtkWidget *container;
	GtkWidget *widget;

	priv = E_BOOK_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_book_shell_content_parent_class)->constructed (object);

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);

	container = GTK_WIDGET (object);

	widget = e_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		object, "orientation",
		widget, "orientation",
		G_BINDING_SYNC_CREATE);

	container = widget;

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	priv->notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = eab_contact_display_new ();
	eab_contact_display_set_mode (
		EAB_CONTACT_DISPLAY (widget),
		EAB_CONTACT_DISPLAY_RENDER_NORMAL);

	eab_contact_display_set_show_maps (
		EAB_CONTACT_DISPLAY (widget),
		priv->preview_show_maps);

	e_binding_bind_property (
		object, "preview-show-maps",
		widget, "show-maps",
		G_BINDING_SYNC_CREATE);

	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "send-message",
		G_CALLBACK (book_shell_content_send_message_cb), object);

	g_signal_connect_swapped (
		widget, "status-message",
		G_CALLBACK (e_shell_taskbar_set_message),
		shell_taskbar);

	widget = e_preview_pane_new (E_WEB_VIEW (widget));
	gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, FALSE);
	priv->preview_pane = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		object, "preview-visible",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	/* Restore pane positions from the last session once
	 * the shell view is fully initialized and visible. */
	g_signal_connect (
		shell_window, "shell-view-created::addressbook",
		G_CALLBACK (book_shell_content_restore_state_cb),
		shell_content);
}

static void
book_shell_content_check_state_foreach (gint row,
                                        gpointer user_data)
{
	EContact *contact;

	struct {
		EAddressbookModel *model;
		GList *list;
	} *foreach_data = user_data;

	contact = e_addressbook_model_get_contact (foreach_data->model, row);
	g_return_if_fail (E_IS_CONTACT (contact));

	foreach_data->list = g_list_prepend (foreach_data->list, contact);
}

static guint32
book_shell_content_check_state (EShellContent *shell_content)
{
	EBookShellContent *book_shell_content;
	ESelectionModel *selection_model;
	EAddressbookModel *model;
	EAddressbookView *view;
	GtkNotebook *notebook;
	gboolean has_email = TRUE;
	gboolean is_contact_list = TRUE;
	guint32 state = 0;
	gint n_selected;

	struct {
		EAddressbookModel *model;
		GList *list;
	} foreach_data;

	book_shell_content = E_BOOK_SHELL_CONTENT (shell_content);

	/* This function may be triggered at startup before any address
	 * book views are added.  Check for that and return silently. */
	notebook = GTK_NOTEBOOK (book_shell_content->priv->notebook);
	if (gtk_notebook_get_n_pages (notebook) == 0)
		return 0;

	view = e_book_shell_content_get_current_view (book_shell_content);
	model = e_addressbook_view_get_model (view);

	selection_model = e_addressbook_view_get_selection_model (view);
	n_selected = (selection_model != NULL) ?
		e_selection_model_selected_count (selection_model) : 0;

	foreach_data.model = model;
	foreach_data.list = NULL;

	if (selection_model != NULL)
		e_selection_model_foreach (
			selection_model, (EForeachFunc)
			book_shell_content_check_state_foreach,
			&foreach_data);

	while (foreach_data.list != NULL) {
		EContact *contact = E_CONTACT (foreach_data.list->data);
		GList *email_list;

		email_list = e_contact_get (contact, E_CONTACT_EMAIL);
		has_email &= (email_list != NULL);
		g_list_foreach (email_list, (GFunc) g_free, NULL);
		g_list_free (email_list);

		is_contact_list &=
			(e_contact_get (contact, E_CONTACT_IS_LIST) != NULL);

		g_object_unref (contact);

		foreach_data.list = g_list_delete_link (
			foreach_data.list, foreach_data.list);
	}

	if (n_selected == 1)
		state |= E_BOOK_SHELL_CONTENT_SELECTION_SINGLE;
	if (n_selected > 1)
		state |= E_BOOK_SHELL_CONTENT_SELECTION_MULTIPLE;
	if (n_selected > 0 && has_email)
		state |= E_BOOK_SHELL_CONTENT_SELECTION_HAS_EMAIL;
	if (n_selected == 1 && is_contact_list)
		state |= E_BOOK_SHELL_CONTENT_SELECTION_IS_CONTACT_LIST;
	if (e_addressbook_model_can_stop (model))
		state |= E_BOOK_SHELL_CONTENT_SOURCE_IS_BUSY;
	if (e_addressbook_model_get_editable (model))
		state |= E_BOOK_SHELL_CONTENT_SOURCE_IS_EDITABLE;

	return state;
}

static void
book_shell_content_focus_search_results (EShellContent *shell_content)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *view;

	book_shell_content = E_BOOK_SHELL_CONTENT (shell_content);
	view = e_book_shell_content_get_current_view (book_shell_content);

	gtk_widget_grab_focus (GTK_WIDGET (view));
}

static void
e_book_shell_content_class_init (EBookShellContentClass *class)
{
	GObjectClass *object_class;
	EShellContentClass *shell_content_class;

	g_type_class_add_private (class, sizeof (EBookShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = book_shell_content_set_property;
	object_class->get_property = book_shell_content_get_property;
	object_class->dispose = book_shell_content_dispose;
	object_class->constructed = book_shell_content_constructed;

	shell_content_class = E_SHELL_CONTENT_CLASS (class);
	shell_content_class->check_state = book_shell_content_check_state;
	shell_content_class->focus_search_results =
		book_shell_content_focus_search_results;

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_VIEW,
		g_param_spec_object (
			"current-view",
			"Current View",
			"The currently selected address book view",
			E_TYPE_ADDRESSBOOK_VIEW,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_CONTACT,
		g_param_spec_object (
			"preview-contact",
			"Previewed Contact",
			"The contact being shown in the preview pane",
			E_TYPE_CONTACT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_VISIBLE,
		g_param_spec_boolean (
			"preview-visible",
			"Preview is Visible",
			"Whether the preview pane is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_override_property (
		object_class, PROP_ORIENTATION, "orientation");

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_SHOW_MAPS,
		g_param_spec_boolean (
			"preview-show-maps",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));
}

static void
e_book_shell_content_class_finalize (EBookShellContentClass *class)
{
}

static void
e_book_shell_content_init (EBookShellContent *book_shell_content)
{
	book_shell_content->priv =
		E_BOOK_SHELL_CONTENT_GET_PRIVATE (book_shell_content);

	/* Postpone widget construction until we have a shell view. */
}

void
e_book_shell_content_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_book_shell_content_register_type (type_module);
}

GtkWidget *
e_book_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_BOOK_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

void
e_book_shell_content_insert_view (EBookShellContent *book_shell_content,
                                  EAddressbookView *addressbook_view)
{
	GtkNotebook *notebook;
	GtkWidget *child;

	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (addressbook_view));

	notebook = GTK_NOTEBOOK (book_shell_content->priv->notebook);
	child = GTK_WIDGET (addressbook_view);
	gtk_notebook_append_page (notebook, child, NULL);
}

void
e_book_shell_content_remove_view (EBookShellContent *book_shell_content,
                                  EAddressbookView *addressbook_view)
{
	GtkNotebook *notebook;
	GtkWidget *child;
	gint page_num;

	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (addressbook_view));

	notebook = GTK_NOTEBOOK (book_shell_content->priv->notebook);
	child = GTK_WIDGET (addressbook_view);
	page_num = gtk_notebook_page_num (notebook, child);
	g_return_if_fail (page_num >= 0);

	gtk_notebook_remove_page (notebook, page_num);
}

EAddressbookView *
e_book_shell_content_get_current_view (EBookShellContent *book_shell_content)
{
	GtkNotebook *notebook;
	GtkWidget *widget;
	gint page_num;

	g_return_val_if_fail (
		E_IS_BOOK_SHELL_CONTENT (book_shell_content), NULL);

	notebook = GTK_NOTEBOOK (book_shell_content->priv->notebook);
	page_num = gtk_notebook_get_current_page (notebook);
	widget = gtk_notebook_get_nth_page (notebook, page_num);
	g_return_val_if_fail (widget != NULL, NULL);

	return E_ADDRESSBOOK_VIEW (widget);
}

void
e_book_shell_content_set_current_view (EBookShellContent *book_shell_content,
                                       EAddressbookView *addressbook_view)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	EShellSearchbar *searchbar;
	EBookShellView *book_shell_view;
	GtkNotebook *notebook;
	GtkWidget *child;
	gint page_num, old_page_num;

	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (addressbook_view));

	shell_content = E_SHELL_CONTENT (book_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);

	book_shell_view = E_BOOK_SHELL_VIEW (shell_view);
	searchbar = e_book_shell_content_get_searchbar (book_shell_content);

	notebook = GTK_NOTEBOOK (book_shell_content->priv->notebook);
	child = GTK_WIDGET (addressbook_view);
	page_num = gtk_notebook_page_num (notebook, child);
	g_return_if_fail (page_num >= 0);

	old_page_num = gtk_notebook_get_current_page (notebook);
	gtk_notebook_set_current_page (notebook, page_num);

	if (old_page_num != page_num) {
		EActionComboBox *combo_box;
		GtkRadioAction *action;
		gint filter_id = 0, search_id = 0;
		gchar *search_text = NULL;
		EFilterRule *advanced_search = NULL;

		e_book_shell_view_disable_searching (book_shell_view);

		e_addressbook_view_get_search (
			addressbook_view, &filter_id, &search_id,
			&search_text, &advanced_search);

		combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);
		e_action_combo_box_set_current_value (combo_box, filter_id);

		action = e_shell_searchbar_get_search_option (searchbar);
		gtk_radio_action_set_current_value (action, search_id);

		e_shell_searchbar_set_search_text (searchbar, search_text);

		e_shell_view_set_search_rule (shell_view, advanced_search);

		g_free (search_text);

		if (advanced_search)
			g_object_unref (advanced_search);

		e_book_shell_view_enable_searching (book_shell_view);
	}

	g_object_notify (G_OBJECT (book_shell_content), "current-view");
}

EContact *
e_book_shell_content_get_preview_contact (EBookShellContent *book_shell_content)
{
	EPreviewPane *preview_pane;
	EABContactDisplay *display;
	EWebView *web_view;

	g_return_val_if_fail (
		E_IS_BOOK_SHELL_CONTENT (book_shell_content), NULL);

	preview_pane = E_PREVIEW_PANE (book_shell_content->priv->preview_pane);
	web_view = e_preview_pane_get_web_view (preview_pane);
	display = EAB_CONTACT_DISPLAY (web_view);

	return eab_contact_display_get_contact (display);
}

void
e_book_shell_content_set_preview_contact (EBookShellContent *book_shell_content,
                                          EContact *preview_contact)
{
	EPreviewPane *preview_pane;
	EABContactDisplay *display;
	EWebView *web_view;

	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));

	preview_pane = E_PREVIEW_PANE (book_shell_content->priv->preview_pane);
	web_view = e_preview_pane_get_web_view (preview_pane);
	display = EAB_CONTACT_DISPLAY (web_view);

	eab_contact_display_set_contact (display, preview_contact);
	g_object_notify (G_OBJECT (book_shell_content), "preview-contact");
}

EPreviewPane *
e_book_shell_content_get_preview_pane (EBookShellContent *book_shell_content)
{
	g_return_val_if_fail (
		E_IS_BOOK_SHELL_CONTENT (book_shell_content), NULL);

	return E_PREVIEW_PANE (book_shell_content->priv->preview_pane);
}

gboolean
e_book_shell_content_get_preview_visible (EBookShellContent *book_shell_content)
{
	g_return_val_if_fail (
		E_IS_BOOK_SHELL_CONTENT (book_shell_content), FALSE);

	return book_shell_content->priv->preview_visible;
}

void
e_book_shell_content_set_preview_visible (EBookShellContent *book_shell_content,
                                          gboolean preview_visible)
{
	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));

	if (book_shell_content->priv->preview_visible == preview_visible)
		return;

	book_shell_content->priv->preview_visible = preview_visible;

	g_object_notify (G_OBJECT (book_shell_content), "preview-visible");
}

gboolean
e_book_shell_content_get_preview_show_maps (EBookShellContent *book_shell_content)
{
	g_return_val_if_fail (
		E_IS_BOOK_SHELL_CONTENT (book_shell_content), FALSE);

	return book_shell_content->priv->preview_show_maps;
}

void
e_book_shell_content_set_preview_show_maps (EBookShellContent *book_shell_content,
                                            gboolean show_maps)
{
	g_return_if_fail (E_IS_BOOK_SHELL_CONTENT (book_shell_content));

	if (book_shell_content->priv->preview_show_maps == show_maps)
		return;

	book_shell_content->priv->preview_show_maps = show_maps;

	g_object_notify (G_OBJECT (book_shell_content), "preview-show-maps");
}

EShellSearchbar *
e_book_shell_content_get_searchbar (EBookShellContent *book_shell_content)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	GtkWidget *widget;

	g_return_val_if_fail (
		E_IS_BOOK_SHELL_CONTENT (book_shell_content), NULL);

	shell_content = E_SHELL_CONTENT (book_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	widget = e_shell_view_get_searchbar (shell_view);

	return E_SHELL_SEARCHBAR (widget);
}
