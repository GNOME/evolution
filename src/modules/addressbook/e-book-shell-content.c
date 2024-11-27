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

#include "evolution-config.h"

#include "e-book-shell-content.h"

#include <glib/gi18n.h>

#include "shell/e-shell-utils.h"
#include "addressbook/gui/widgets/gal-view-minicard.h"
#include "e-book-shell-view-private.h"
#include "e-book-shell-view.h"

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

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EBookShellContent, e_book_shell_content, E_TYPE_SHELL_CONTENT, 0,
	G_ADD_PRIVATE_DYNAMIC (EBookShellContent)
	G_IMPLEMENT_INTERFACE_DYNAMIC (GTK_TYPE_ORIENTABLE, NULL))

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
	EBookShellContent *self = E_BOOK_SHELL_CONTENT (shell_content);
	GSettings *settings;

	/* Bind GObject properties to GSettings keys. */

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");

	if (e_shell_window_is_main_instance (shell_window)) {
		g_settings_bind (
			settings, "hpane-position",
			self->priv->paned, "hposition",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "vpane-position",
			self->priv->paned, "vposition",
			G_SETTINGS_BIND_DEFAULT);
	} else {
		g_settings_bind (
			settings, "hpane-position-sub",
			self->priv->paned, "hposition",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "vpane-position-sub",
			self->priv->paned, "vposition",
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
	EBookShellContent *self = E_BOOK_SHELL_CONTENT (object);

	g_clear_object (&self->priv->paned);
	g_clear_object (&self->priv->notebook);
	g_clear_object (&self->priv->preview_pane);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_book_shell_content_parent_class)->dispose (object);
}

static void
book_shell_content_constructed (GObject *object)
{
	EBookShellContent *self = E_BOOK_SHELL_CONTENT (object);
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellTaskbar *shell_taskbar;
	GtkWidget *container;
	GtkWidget *widget;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_book_shell_content_parent_class)->constructed (object);

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);

	container = GTK_WIDGET (object);

	widget = e_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_container_add (GTK_CONTAINER (container), widget);
	self->priv->paned = g_object_ref (widget);
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
	self->priv->notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = eab_contact_display_new ();
	eab_contact_display_set_mode (
		EAB_CONTACT_DISPLAY (widget),
		EAB_CONTACT_DISPLAY_RENDER_NORMAL);

	eab_contact_display_set_show_maps (
		EAB_CONTACT_DISPLAY (widget),
		self->priv->preview_show_maps);

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
	self->priv->preview_pane = g_object_ref (widget);
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
e_book_shell_content_got_selected_contacts_cb (GObject *source_object,
					       GAsyncResult *result,
					       gpointer user_data)
{
	EShellContent *shell_content = user_data;
	GPtrArray *contacts;
	GError *error = NULL;

	contacts = e_addressbook_view_dup_selected_contacts_finish (E_ADDRESSBOOK_VIEW (source_object), result, &error);

	if (contacts) {
		e_shell_view_update_actions (e_shell_content_get_shell_view (shell_content));
		g_ptr_array_unref (contacts);
	} else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_message ("%s: Failed to retrieve selected contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	g_object_unref (shell_content);
}

static guint32
book_shell_content_check_state (EShellContent *shell_content)
{
	EBookShellContent *book_shell_content;
	EAddressbookView *view;
	GtkNotebook *notebook;
	gboolean has_email = FALSE;
	gboolean is_contact_list = FALSE;
	guint32 state = 0;
	guint n_selected;

	book_shell_content = E_BOOK_SHELL_CONTENT (shell_content);

	/* This function may be triggered at startup before any address
	 * book views are added.  Check for that and return silently. */
	notebook = GTK_NOTEBOOK (book_shell_content->priv->notebook);
	if (gtk_notebook_get_n_pages (notebook) == 0)
		return 0;

	view = e_book_shell_content_get_current_view (book_shell_content);
	n_selected = e_addressbook_view_get_n_selected (view);

	if (n_selected > 0) {
		GPtrArray *contacts;

		contacts = e_addressbook_view_peek_selected_contacts (view);

		if (contacts) {
			guint ii;

			has_email = contacts->len > 0;
			is_contact_list = contacts->len > 0;

			for (ii = 0; ii < contacts->len && (has_email || is_contact_list); ii++) {
				EContact *contact = g_ptr_array_index (contacts, ii);
				GList *email_list;

				email_list = e_contact_get (contact, E_CONTACT_EMAIL);
				has_email &= (email_list != NULL);
				g_list_free_full (email_list, g_free);

				is_contact_list &= (e_contact_get (contact, E_CONTACT_IS_LIST) != NULL);
			}

			g_ptr_array_unref (contacts);
		} else {
			/* Need to update actions after all the selected contacts are available */
			e_addressbook_view_dup_selected_contacts (view, NULL, e_book_shell_content_got_selected_contacts_cb, g_object_ref (shell_content));
		}
	}

	if (n_selected == 1)
		state |= E_BOOK_SHELL_CONTENT_SELECTION_SINGLE;
	if (n_selected > 1)
		state |= E_BOOK_SHELL_CONTENT_SELECTION_MULTIPLE;
	if (n_selected > 0 && has_email)
		state |= E_BOOK_SHELL_CONTENT_SELECTION_HAS_EMAIL;
	if (n_selected == 1 && is_contact_list)
		state |= E_BOOK_SHELL_CONTENT_SELECTION_IS_CONTACT_LIST;
	if (e_addressbook_view_can_stop (view))
		state |= E_BOOK_SHELL_CONTENT_SOURCE_IS_BUSY;
	if (e_addressbook_view_get_editable (view))
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
	book_shell_content->priv = e_book_shell_content_get_instance_private (book_shell_content);

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
		GalViewInstance *view_instance;
		GalView *gl_view;
		EActionComboBox *combo_box;
		EUIAction *action;
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
		e_ui_action_set_state (action, g_variant_new_int32 (search_id));

		e_shell_searchbar_set_search_text (searchbar, search_text);

		e_shell_view_set_search_rule (shell_view, advanced_search);

		g_free (search_text);

		if (advanced_search)
			g_object_unref (advanced_search);

		e_book_shell_view_enable_searching (book_shell_view);

		view_instance = e_addressbook_view_get_view_instance (addressbook_view);
		gl_view = gal_view_instance_get_current_view (view_instance);

		action = ACTION (CONTACT_CARDS_SORT_BY_MENU);
		e_ui_action_set_visible (action, GAL_IS_VIEW_MINICARD (gl_view));

		if (GAL_IS_VIEW_MINICARD (gl_view)) {
			action = ACTION (CONTACT_CARDS_SORT_BY_FILE_AS);
			e_ui_action_set_state (action, g_variant_new_int32 (gal_view_minicard_get_sort_by (GAL_VIEW_MINICARD (gl_view))));
		}
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

	if (preview_visible && book_shell_content->priv->preview_pane)
		e_web_view_update_actions (e_preview_pane_get_web_view (E_PREVIEW_PANE (book_shell_content->priv->preview_pane)));

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
