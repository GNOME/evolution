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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include "e-contact-list-editor.h"
#include <e-util/e-util-private.h>
#include <e-util/e-alert-dialog.h>
#include <e-util/e-selection.h>
#include <e-util/gtk-compat.h>
#include "shell/e-shell.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <camel/camel.h>
#include <libedataserverui/e-book-auth-util.h>
#include <libedataserverui/e-source-combo-box.h>

#include "e-util/e-util.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/util/eab-book-util.h"

#include "eab-editor.h"
#include "e-contact-editor.h"
#include "e-contact-list-model.h"
#include "eab-contact-merging.h"

#define E_CONTACT_LIST_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CONTACT_LIST_EDITOR, EContactListEditorPrivate))

#define CONTACT_LIST_EDITOR_WIDGET(editor, name) \
	(e_builder_get_widget \
	(E_CONTACT_LIST_EDITOR_GET_PRIVATE (editor)->builder, name))

/* More macros, less typos. */
#define CONTACT_LIST_EDITOR_WIDGET_ADD_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "add-button")
#define CONTACT_LIST_EDITOR_WIDGET_CHECK_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "check-button")
#define CONTACT_LIST_EDITOR_WIDGET_DIALOG(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "dialog")
#define CONTACT_LIST_EDITOR_WIDGET_EMAIL_ENTRY(editor) \
	E_CONTACT_LIST_EDITOR_GET_PRIVATE (editor)->email_entry
#define CONTACT_LIST_EDITOR_WIDGET_LIST_NAME_ENTRY(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "list-name-entry")
#define CONTACT_LIST_EDITOR_WIDGET_MEMBERS_VBOX(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "members-vbox")
#define CONTACT_LIST_EDITOR_WIDGET_OK_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "ok-button")
#define CONTACT_LIST_EDITOR_WIDGET_REMOVE_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "remove-button")
#define CONTACT_LIST_EDITOR_WIDGET_SOURCE_MENU(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "source-combo-box")
#define CONTACT_LIST_EDITOR_WIDGET_TREE_VIEW(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "tree-view")

/* Shorthand, requires a variable named "editor". */
#define WIDGET(name)	(CONTACT_LIST_EDITOR_WIDGET_##name (editor))

#define TOPLEVEL_KEY	(g_type_name (E_TYPE_CONTACT_LIST_EDITOR))

enum {
	PROP_0,
	PROP_BOOK,
	PROP_CONTACT,
	PROP_IS_NEW_LIST,
	PROP_EDITABLE
};

typedef struct {
	EContactListEditor *editor;
	gboolean should_close;
} EditorCloseStruct;

struct _EContactListEditorPrivate {

	EBook *book;
	EContact *contact;

	GtkBuilder *builder;
	GtkTreeModel *model;
	ENameSelector *name_selector;

	/* This is kept here because the builder has an old widget
	 * which was changed with this one. */
	ENameSelectorEntry *email_entry;

	/* Whether we are editing a new contact or an existing one. */
	guint is_new_list : 1;

	/* Whether the contact has been changed since bringing up the
	 * contact editor. */
	guint changed : 1;

	/* Whether the contact editor will accept modifications. */
	guint editable : 1;

	/* Whether the target book accepts storing of contact lists. */
	guint allows_contact_lists : 1;

	/* Whether an async wombat call is in progress. */
	guint in_async_call : 1;
};

static gpointer parent_class;

static EContactListEditor *
contact_list_editor_extract (GtkWidget *widget)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (widget);
	return g_object_get_data (G_OBJECT (toplevel), TOPLEVEL_KEY);
}

static void
contact_list_editor_scroll_to_end (EContactListEditor *editor)
{
	GtkTreeView *view;
	GtkTreePath *path;
	gint n_rows;

	view = GTK_TREE_VIEW (WIDGET (TREE_VIEW));
	n_rows = gtk_tree_model_iter_n_children (editor->priv->model, NULL);

	path = gtk_tree_path_new_from_indices (n_rows - 1, -1);
	gtk_tree_view_scroll_to_cell (view, path, NULL, FALSE, 0., 0.);
	gtk_tree_view_set_cursor (view, path, NULL, FALSE);
	gtk_tree_path_free (path);
}

static void
contact_list_editor_update (EContactListEditor *editor)
{
	EContactListEditorPrivate *priv = editor->priv;

	gtk_widget_set_sensitive (
		WIDGET (OK_BUTTON),
		eab_editor_is_valid (EAB_EDITOR (editor)) &&
		priv->allows_contact_lists);

	gtk_widget_set_sensitive (
		WIDGET (SOURCE_MENU), priv->is_new_list);
}

static void
contact_list_editor_notify_cb (EContactListEditor *editor,
                               GParamSpec *pspec)
{
	EContactListEditorPrivate *priv = editor->priv;
	gboolean sensitive;

	sensitive = priv->editable && priv->allows_contact_lists;

	gtk_widget_set_sensitive (WIDGET (LIST_NAME_ENTRY), sensitive);
	gtk_widget_set_sensitive (WIDGET (MEMBERS_VBOX), sensitive);
}

static void
contact_list_editor_add_email (EContactListEditor *editor)
{
	EContactListEditorPrivate *priv = editor->priv;
	EContactListModel *model;
	GtkEntry *entry;
	const gchar *text;

	entry = GTK_ENTRY (WIDGET (EMAIL_ENTRY));
	model = E_CONTACT_LIST_MODEL (priv->model);

	text = gtk_entry_get_text (entry);
	if (text != NULL && *text != '\0') {
		e_contact_list_model_add_email (model, text);
		contact_list_editor_scroll_to_end (editor);
		priv->changed = TRUE;
	}

	gtk_entry_set_text (entry, "");
	contact_list_editor_update (editor);
}

static void
contact_list_editor_book_loaded_cb (ESource *source,
                                    GAsyncResult *result,
                                    EContactListEditor *editor)
{
	EContactListEditorPrivate *priv = editor->priv;
	EContactStore *contact_store;
	ENameSelectorEntry *entry;
	EBook *book;
	GError *error = NULL;

	book = e_load_book_source_finish (source, result, &error);

	if (error != NULL) {
		GtkWindow *parent;

		parent = eab_editor_get_window (EAB_EDITOR (editor));
		eab_load_error_dialog (GTK_WIDGET (parent), source, error);

		e_source_combo_box_set_active (
			E_SOURCE_COMBO_BOX (WIDGET (SOURCE_MENU)),
			e_book_get_source (priv->book));

		g_error_free (error);
		goto exit;
	}

	g_return_if_fail (E_IS_BOOK (book));

	entry = E_NAME_SELECTOR_ENTRY (WIDGET (EMAIL_ENTRY));
	contact_store = e_name_selector_entry_peek_contact_store (entry);
	e_contact_store_add_book (contact_store, book);
	e_contact_list_editor_set_book (editor, book);

	g_object_unref (book);

exit:
	g_object_unref (editor);
}

static gboolean
contact_list_editor_contact_exists (EContactListModel *model,
                                    const gchar *email)
{
	const gchar *tag = "addressbook:ask-list-add-exists";

	if (!e_contact_list_model_has_email (model, email))
		return FALSE;

	return (e_alert_run_dialog_for_args (e_shell_get_active_window (NULL),
					     tag, email, NULL) != GTK_RESPONSE_YES);
}

static void
contact_list_editor_list_added_cb (EBook *book,
                                   const GError *error,
                                   const gchar *id,
                                   EditorCloseStruct *ecs)
{
	EContactListEditor *editor = ecs->editor;
	EContactListEditorPrivate *priv = editor->priv;
	gboolean should_close = ecs->should_close;

	gtk_widget_set_sensitive (WIDGET (DIALOG), TRUE);
	priv->in_async_call = FALSE;

	e_contact_set (priv->contact, E_CONTACT_UID, (gchar *) id);

	eab_editor_contact_added (
		EAB_EDITOR (editor), error, priv->contact);

	if (!error) {
		priv->is_new_list = FALSE;

		if (should_close)
			eab_editor_close (EAB_EDITOR (editor));
		else
			contact_list_editor_update (editor);
	}

	g_object_unref (editor);
	g_free (ecs);
}

static void
contact_list_editor_list_modified_cb (EBook *book,
                                      const GError *error,
                                      EditorCloseStruct *ecs)
{
	EContactListEditor *editor = ecs->editor;
	EContactListEditorPrivate *priv = editor->priv;
	gboolean should_close = ecs->should_close;

	gtk_widget_set_sensitive (WIDGET (DIALOG), TRUE);
	priv->in_async_call = FALSE;

	eab_editor_contact_modified (
		EAB_EDITOR (editor), error, priv->contact);

	if (!error) {
		if (should_close)
			eab_editor_close (EAB_EDITOR (editor));
	}

	g_object_unref (editor);
	g_free (ecs);
}

static void
contact_list_editor_render_destination (GtkTreeViewColumn *column,
                                        GtkCellRenderer *renderer,
                                        GtkTreeModel *model,
                                        GtkTreeIter *iter)
{
	/* XXX Would be nice if EDestination had a text property
	 *     that we could just bind the GtkCellRenderer to. */

	EDestination *destination;
	const gchar *textrep;

	gtk_tree_model_get (model, iter, 0, &destination, -1);
	textrep = e_destination_get_textrep (destination, TRUE);
	g_object_set (renderer, "text", textrep, NULL);
	g_object_unref (destination);
}

static void
contact_list_editor_row_deleted_cb (GtkTreeModel *model,
                                    GtkTreePath *path,
                                    EContactListEditor *editor)
{
	gint n_children;

	n_children = gtk_tree_model_iter_n_children (model, NULL);
	gtk_widget_set_sensitive (WIDGET (REMOVE_BUTTON), n_children > 0);
}

static void
contact_list_editor_row_inserted_cb (GtkTreeModel *model,
                                     GtkTreePath *path,
                                     GtkTreeIter *iter,
                                     EContactListEditor *editor)
{
	gtk_widget_set_sensitive (WIDGET (REMOVE_BUTTON), TRUE);
}

/*********************** Autoconnected Signal Handlers ***********************/

void
contact_list_editor_add_button_clicked_cb (GtkWidget *widget);

void
contact_list_editor_add_button_clicked_cb (GtkWidget *widget)
{
	EContactListEditor *editor;

	editor = contact_list_editor_extract (widget);

	contact_list_editor_add_email (editor);
}

void
contact_list_editor_cancel_button_clicked_cb (GtkWidget *widget);

void
contact_list_editor_cancel_button_clicked_cb (GtkWidget *widget)
{
	EContactListEditor *editor;
	GtkWindow *window;

	editor = contact_list_editor_extract (widget);
	window = GTK_WINDOW (WIDGET (DIALOG));

	eab_editor_prompt_to_save_changes (EAB_EDITOR (editor), window);
}

void
contact_list_editor_check_button_toggled_cb (GtkWidget *widget);

void
contact_list_editor_check_button_toggled_cb (GtkWidget *widget)
{
	EContactListEditor *editor;

	editor = contact_list_editor_extract (widget);

	editor->priv->changed = TRUE;
	contact_list_editor_update (editor);
}

gboolean
contact_list_editor_delete_event_cb (GtkWidget *widget,
                                     GdkEvent *event);

gboolean
contact_list_editor_delete_event_cb (GtkWidget *widget,
                                     GdkEvent *event)
{
	EContactListEditor *editor;
	GtkWindow *window;

	editor = contact_list_editor_extract (widget);
	window = GTK_WINDOW (WIDGET (DIALOG));

	/* If we're in an async call, don't allow the dialog to close. */
	if (!editor->priv->in_async_call)
		eab_editor_prompt_to_save_changes (
			EAB_EDITOR (editor), window);

	return TRUE;
}

void
contact_list_editor_drag_data_received_cb (GtkWidget *widget,
                                           GdkDragContext *context,
                                             gint x, gint y,
                                             GtkSelectionData *selection_data,
                                             guint info,
                                             guint time);

void
contact_list_editor_drag_data_received_cb (GtkWidget *widget,
                                           GdkDragContext *context,
                                           gint x, gint y,
                                           GtkSelectionData *selection_data,
                                           guint info,
                                           guint time)
{
	CamelInternetAddress *address;
	EContactListEditor *editor;
	EContactListModel *model;
	gboolean changed = FALSE;
	gboolean handled = FALSE;
	const guchar *data;
	GList *list, *iter;
	GdkAtom target;
	gint n_addresses = 0;
	gchar *text;

	editor = contact_list_editor_extract (widget);
	model = E_CONTACT_LIST_MODEL (editor->priv->model);

	target = gtk_selection_data_get_target (selection_data);

	/* Sanity check the selection target. */

	if (gtk_targets_include_text (&target, 1))
		goto handle_text;

	if (!e_targets_include_directory (&target, 1))
		goto exit;

	data = gtk_selection_data_get_data (selection_data);
	list = eab_contact_list_from_string ((gchar *) data);

	if (list != NULL)
		handled = TRUE;

	for (iter = list; iter != NULL; iter = iter->next) {
		EContact *contact = iter->data;
		const gchar *email;

		if (e_contact_get (contact, E_CONTACT_IS_LIST))
			continue;

		email = e_contact_get (contact, E_CONTACT_EMAIL_1);
		if (email == NULL) {
			g_warning (
				"Contact with no email-ids listed "
				"can't be added to a Contact-List");
			continue;
		}

		if (!contact_list_editor_contact_exists (model, email)) {
			/* Hard-wired for default e-mail */
			e_contact_list_model_add_contact (model, contact, 0);
			changed = TRUE;
		}
	}

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	contact_list_editor_scroll_to_end (editor);

	if (changed) {
		editor->priv->changed = TRUE;
		contact_list_editor_update (editor);
	}

	goto exit;

handle_text:

	address = camel_internet_address_new ();
	text = (gchar *) gtk_selection_data_get_text (selection_data);

	/* See if Camel can parse a valid email address from the text. */
	if (text != NULL && *text != '\0') {
		camel_url_decode (text);
		if (g_ascii_strncasecmp (text, "mailto:", 7) == 0)
			n_addresses = camel_address_decode (
				CAMEL_ADDRESS (address), text + 7);
		else
			n_addresses = camel_address_decode (
				CAMEL_ADDRESS (address), text);
	}

	if (n_addresses == 1) {
		g_free (text);

		text = camel_address_format (CAMEL_ADDRESS (address));
		e_contact_list_model_add_email (model, text);

		contact_list_editor_scroll_to_end (editor);
		editor->priv->changed = TRUE;

		contact_list_editor_update (editor);
		handled = TRUE;
	}

	g_free (text);

exit:
	gtk_drag_finish (context, handled, FALSE, time);
}

void
contact_list_editor_email_entry_activate_cb (GtkWidget *widget);

void
contact_list_editor_email_entry_activate_cb (GtkWidget *widget)
{
	EContactListEditor *editor;

	editor = contact_list_editor_extract (widget);

	contact_list_editor_add_email (editor);
}

void
contact_list_editor_email_entry_changed_cb (GtkWidget *widget);

void
contact_list_editor_email_entry_changed_cb (GtkWidget *widget)
{
	EContactListEditor *editor;
	const gchar *text;
	gboolean sensitive;

	editor = contact_list_editor_extract (widget);
	text = gtk_entry_get_text (GTK_ENTRY (widget));

	sensitive = (text != NULL && *text != '\0');
	gtk_widget_set_sensitive (WIDGET (ADD_BUTTON), sensitive);
}

gboolean
contact_list_editor_email_entry_key_press_event_cb (GtkWidget *widget,
                                                    GdkEventKey *event);

gboolean
contact_list_editor_email_entry_key_press_event_cb (GtkWidget *widget,
                                                    GdkEventKey *event)
{
	EContactListEditor *editor;
	gboolean can_comma = FALSE;

	editor = contact_list_editor_extract (widget);

	if (event->keyval == GDK_comma) {
		GtkEntry *entry;
		gint cpos = -1;

		entry = GTK_ENTRY (WIDGET (EMAIL_ENTRY));
		g_object_get (G_OBJECT (entry), "cursor-position", &cpos, NULL);

		/* not the first letter */
		if (cpos > 0) {
			const gchar *text;
			gint quotes = 0, i;

			text = gtk_entry_get_text (entry);

			for (i = 0; text && text[i] && i < cpos; i++) {
				if (text[i] == '\"')
					quotes++;
			}

			/* even count of quotes */
			can_comma = (quotes & 1) == 0;
		}
	}

	if (can_comma || event->keyval == GDK_Return) {
		g_signal_emit_by_name (widget, "activate", 0);
		contact_list_editor_add_email (editor);

		return TRUE;
	}

	return FALSE;
}

void
contact_list_editor_email_entry_updated_cb (GtkWidget *widget,
                                            EDestination *destination);

void
contact_list_editor_email_entry_updated_cb (GtkWidget *widget,
                                            EDestination *destination)
{
	EContactListEditor *editor;
	ENameSelectorEntry *entry;
	EContactListModel *model;
	EDestinationStore *store;
	gchar *email;

	editor = contact_list_editor_extract (widget);

	entry = E_NAME_SELECTOR_ENTRY (widget);
	model = E_CONTACT_LIST_MODEL (editor->priv->model);

	email = g_strdup (e_destination_get_textrep (destination, TRUE));
	store = e_name_selector_entry_peek_destination_store (entry);
	e_destination_store_remove_destination (store, destination);
	gtk_entry_set_text (GTK_ENTRY (WIDGET (EMAIL_ENTRY)), "");

	if (email && *email) {
		e_contact_list_model_add_email (model, email);
		contact_list_editor_scroll_to_end (editor);
		editor->priv->changed = TRUE;
	}

	g_free (email);
	contact_list_editor_update (editor);
}

void
contact_list_editor_list_name_entry_changed_cb (GtkWidget *widget);

void
contact_list_editor_list_name_entry_changed_cb (GtkWidget *widget)
{
	EContactListEditor *editor;
	const gchar *title;

	editor = contact_list_editor_extract (widget);

	title = gtk_entry_get_text (GTK_ENTRY (widget));

	if (title == NULL || *title == '\0')
		title = _("Contact List Editor");

	gtk_window_set_title (GTK_WINDOW (WIDGET (DIALOG)), title);

	editor->priv->changed = TRUE;
	contact_list_editor_update (editor);
}

void
contact_list_editor_ok_button_clicked_cb (GtkWidget *widget);

void
contact_list_editor_ok_button_clicked_cb (GtkWidget *widget)
{
	EContactListEditor *editor;
	gboolean save_contact;

	editor = contact_list_editor_extract (widget);

	save_contact =
		editor->priv->editable &&
		editor->priv->allows_contact_lists;

	if (save_contact)
		eab_editor_save_contact (EAB_EDITOR (editor), TRUE);
	else
		eab_editor_close (EAB_EDITOR (editor));
}

void
contact_list_editor_remove_button_clicked_cb (GtkWidget *widget);

void
contact_list_editor_remove_button_clicked_cb (GtkWidget *widget)
{
	EContactListEditor *editor;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeView *view;
	GList *list, *iter;

	editor = contact_list_editor_extract (widget);

	view = GTK_TREE_VIEW (WIDGET (TREE_VIEW));
	selection = gtk_tree_view_get_selection (view);
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	/* Convert the GtkTreePaths to GtkTreeRowReferences. */
	for (iter = list; iter != NULL; iter = iter->next) {
		GtkTreePath *path = iter->data;

		iter->data = gtk_tree_row_reference_new (model, path);
		gtk_tree_path_free (path);
	}

	/* Delete each row in the list. */
	for (iter = list; iter != NULL; iter = iter->next) {
		GtkTreeRowReference *reference = iter->data;
		GtkTreePath *path;
		GtkTreeIter iter;
		gboolean valid;

		path = gtk_tree_row_reference_get_path (reference);
		valid = gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_path_free (path);
		g_assert (valid);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		gtk_tree_row_reference_free (reference);
	}

	g_list_free (list);

	editor->priv->changed = TRUE;
	contact_list_editor_update (editor);
}

void
contact_list_editor_select_button_clicked_cb (GtkWidget *widget);

void
contact_list_editor_select_button_clicked_cb (GtkWidget *widget)
{
	EContactListEditor *editor;
	EContactListModel *model;
	ENameSelectorDialog *dialog;
	EDestinationStore *store;
	GList *list, *iter;
	GtkWindow *window;

	editor = contact_list_editor_extract (widget);

	model = E_CONTACT_LIST_MODEL (editor->priv->model);
	dialog = e_name_selector_peek_dialog (editor->priv->name_selector);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Contact List Members"));

	/* We need to empty out the destination store, since we copy its
	 * contents every time.  This sucks, we should really be wired
	 * directly to the EDestinationStore that the name selector uses
	 * in true MVC fashion. */

	e_name_selector_model_peek_section (
		e_name_selector_peek_model (editor->priv->name_selector),
		"Members", NULL, &store);

	list = e_destination_store_list_destinations (store);

	for (iter = list; iter != NULL; iter = iter->next)
		e_destination_store_remove_destination (store, iter->data);

	g_list_free (list);

	window = eab_editor_get_window (EAB_EDITOR (editor));
	e_name_selector_show_dialog (
		editor->priv->name_selector, GTK_WIDGET (window));
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_hide (GTK_WIDGET (dialog));

	list = e_destination_store_list_destinations (store);

	for (iter = list; iter != NULL; iter = iter->next) {
		EDestination *destination = iter->data;
		const gchar *email = e_destination_get_email (destination);

		if (email == NULL)
			continue;

		if (!contact_list_editor_contact_exists (model, email))
			e_contact_list_model_add_destination (
				model, destination);
	}

	g_list_free (list);

	editor->priv->changed = TRUE;
	contact_list_editor_update (editor);
}

void
contact_list_editor_source_menu_changed_cb (GtkWidget *widget);

void
contact_list_editor_source_menu_changed_cb (GtkWidget *widget)
{
	EContactListEditor *editor;
	GtkWindow *parent;
	ESource *source;

	editor = contact_list_editor_extract (widget);
	source = e_source_combo_box_get_active (E_SOURCE_COMBO_BOX (widget));

	if (e_source_equal (e_book_get_source (editor->priv->book), source))
		return;

	parent = eab_editor_get_window (EAB_EDITOR (editor));

	e_load_book_source_async (
		source, parent, NULL, (GAsyncReadyCallback)
		contact_list_editor_book_loaded_cb,
		g_object_ref (editor));
}

gboolean
contact_list_editor_tree_view_key_press_event_cb (GtkWidget *widget,
                                                  GdkEventKey *event);
gboolean
contact_list_editor_tree_view_key_press_event_cb (GtkWidget *widget,
                                                  GdkEventKey *event)
{
	EContactListEditor *editor;

	editor = contact_list_editor_extract (widget);

	if (event->keyval == GDK_Delete) {
		g_signal_emit_by_name (WIDGET (REMOVE_BUTTON), "clicked");
		return TRUE;
	}

	return FALSE;
}

/******************** GtkBuilder Custom Widgets Functions ********************/

static gpointer
contact_editor_fudge_new (EBook *book,
                          EContact *contact,
                          gboolean is_new,
                          gboolean editable)
{
	EShell *shell = e_shell_get_default ();

	/* XXX Putting this function signature in libedataserverui
	 *     was a terrible idea.  Now we're stuck with it. */

	return e_contact_editor_new (
		shell, book, contact, is_new, editable);
}

static gpointer
contact_list_editor_fudge_new (EBook *book,
                               EContact *contact,
                               gboolean is_new,
                               gboolean editable)
{
	EShell *shell = e_shell_get_default ();

	/* XXX Putting this function signature in libedataserverui
	 *     was a terrible idea.  Now we're stuck with it. */

	return e_contact_list_editor_new (
		shell, book, contact, is_new, editable);
}

static void
setup_custom_widgets (EContactListEditor *editor)
{
	const gchar *key = "/apps/evolution/addressbook/sources";
	GtkWidget *combo_box;
	GConfClient *client;
	ESourceList *source_list;
	ENameSelectorEntry *name_selector_entry;
	ENameSelector *name_selector;
	GtkWidget *old, *parent;
	EContactListEditorPrivate *priv;
	guint ba = 0, la = 0, ra = 0, ta = 0, xo = 0, xp = 0, yo = 0, yp = 0;

	g_return_if_fail (editor != NULL);

	priv = E_CONTACT_LIST_EDITOR_GET_PRIVATE (editor);

	combo_box = WIDGET (SOURCE_MENU);
	client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (client, key);
	g_object_set (G_OBJECT (combo_box), "source-list", source_list, NULL);
	g_object_unref (source_list);
	g_object_unref (client);

	g_signal_connect (
		combo_box, "changed", G_CALLBACK (
		contact_list_editor_source_menu_changed_cb), NULL);

	old = CONTACT_LIST_EDITOR_WIDGET (editor, "email-entry");
	g_return_if_fail (old != NULL);

	name_selector = e_name_selector_new ();

	e_name_selector_model_add_section (
		e_name_selector_peek_model (name_selector),
		"Members", _("_Members"), NULL);

	name_selector_entry = e_name_selector_peek_section_entry (
		name_selector, "Members");

	gtk_widget_set_name (
		GTK_WIDGET (name_selector_entry),
		gtk_widget_get_name (old));
	parent = gtk_widget_get_parent (old);

	gtk_container_child_get (GTK_CONTAINER (parent), old,
		"bottom-attach", &ba,
		"left-attach", &la,
		"right-attach", &ra,
		"top-attach", &ta,
		"x-options", &xo,
		"x-padding", &xp,
		"y-options", &yo,
		"y-padding", &yp,
		NULL);

	/* only hide it... */
	gtk_widget_hide (old);

	/* ... and place the new name selector to the
	 * exact place as is the old one in UI file */
	gtk_widget_show (GTK_WIDGET (name_selector_entry));
	gtk_table_attach (
		GTK_TABLE (parent), GTK_WIDGET (name_selector_entry),
		la, ra, ta, ba, xo, yo, xp, yp);
	priv->email_entry = name_selector_entry;

	e_name_selector_entry_set_contact_editor_func (
		name_selector_entry, contact_editor_fudge_new);
	e_name_selector_entry_set_contact_list_editor_func (
		name_selector_entry, contact_list_editor_fudge_new);

	g_signal_connect (
		name_selector_entry, "activate", G_CALLBACK (
		contact_list_editor_email_entry_activate_cb), NULL);
	g_signal_connect (
		name_selector_entry, "changed", G_CALLBACK (
		contact_list_editor_email_entry_changed_cb), NULL);
	g_signal_connect (
		name_selector_entry, "key-press-event", G_CALLBACK (
		contact_list_editor_email_entry_key_press_event_cb), NULL);
	g_signal_connect (
		name_selector_entry, "updated", G_CALLBACK (
		contact_list_editor_email_entry_updated_cb), NULL);
}

/***************************** GObject Callbacks *****************************/

static GObject *
contact_list_editor_constructor (GType type,
                                 guint n_construct_properties,
                                 GObjectConstructParam *construct_properties)
{
	GObject *object;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	contact_list_editor_update (E_CONTACT_LIST_EDITOR (object));

	return object;
}

static void
contact_list_editor_set_property (GObject *object,
                                  guint property_id,
				  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BOOK:
			e_contact_list_editor_set_book (
				E_CONTACT_LIST_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_CONTACT:
			e_contact_list_editor_set_contact (
				E_CONTACT_LIST_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_IS_NEW_LIST:
			e_contact_list_editor_set_is_new_list (
				E_CONTACT_LIST_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_EDITABLE:
			e_contact_list_editor_set_editable (
				E_CONTACT_LIST_EDITOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
contact_list_editor_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BOOK:
			g_value_set_object (
				value,
				e_contact_list_editor_get_book (
				E_CONTACT_LIST_EDITOR (object)));
			return;

		case PROP_CONTACT:
			g_value_set_object (
				value,
				e_contact_list_editor_get_contact (
				E_CONTACT_LIST_EDITOR (object)));
			return;

		case PROP_IS_NEW_LIST:
			g_value_set_boolean (
				value,
				e_contact_list_editor_get_is_new_list (
				E_CONTACT_LIST_EDITOR (object)));
			return;

		case PROP_EDITABLE:
			g_value_set_boolean (
				value,
				e_contact_list_editor_get_editable (
				E_CONTACT_LIST_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
contact_list_editor_dispose (GObject *object)
{
	EContactListEditor *editor = E_CONTACT_LIST_EDITOR (object);
	EContactListEditorPrivate *priv = editor->priv;

	if (priv->name_selector) {
		g_object_unref (priv->name_selector);
		priv->name_selector = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

/**************************** EABEditor Callbacks ****************************/

static void
contact_list_editor_show (EABEditor *editor)
{
	gtk_widget_show (WIDGET (DIALOG));
}

static void
contact_list_editor_close (EABEditor *editor)
{
	gtk_widget_destroy (WIDGET (DIALOG));
	eab_editor_closed (editor);
}

static void
contact_list_editor_raise (EABEditor *editor)
{
	GdkWindow *window;

	window = gtk_widget_get_window (WIDGET (DIALOG));
	gdk_window_raise (window);
}

static void
contact_list_editor_save_contact (EABEditor *eab_editor,
                                  gboolean should_close)
{
	EContactListEditor *editor = E_CONTACT_LIST_EDITOR (eab_editor);
	EContactListEditorPrivate *priv = editor->priv;
	EditorCloseStruct *ecs;
	EContact *contact;

	contact = e_contact_list_editor_get_contact (editor);

	if (priv->book == NULL)
		return;

	ecs = g_new (EditorCloseStruct, 1);
	ecs->editor = g_object_ref (editor);
	ecs->should_close = should_close;

	gtk_widget_set_sensitive (WIDGET (DIALOG), FALSE);
	priv->in_async_call = TRUE;

	if (priv->is_new_list)
		eab_merging_book_add_contact (
			priv->book, contact, (EBookIdAsyncCallback)
			contact_list_editor_list_added_cb, ecs);
	else
		eab_merging_book_commit_contact (
			priv->book, contact, (EBookAsyncCallback)
			contact_list_editor_list_modified_cb, ecs);

	priv->changed = FALSE;
}

static gboolean
contact_list_editor_is_valid (EABEditor *editor)
{
	GtkEditable *editable;
	gboolean valid;
	gchar *chars;

	editable = GTK_EDITABLE (WIDGET (LIST_NAME_ENTRY));
	chars = gtk_editable_get_chars (editable, 0, -1);
	valid = (chars != NULL && *chars != '\0');
	g_free (chars);

	return valid;
}

static gboolean
contact_list_editor_is_changed (EABEditor *editor)
{
	return E_CONTACT_LIST_EDITOR_GET_PRIVATE (editor)->changed;
}

static GtkWindow *
contact_list_editor_get_window (EABEditor *editor)
{
	return GTK_WINDOW (WIDGET (DIALOG));
}

static void
contact_list_editor_contact_added (EABEditor *editor,
                                   const GError *error,
                                   EContact *contact)
{
	if (!error)
		return;

	if (g_error_matches (error, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED))
		return;

	eab_error_dialog (_("Error adding list"), error);
}

static void
contact_list_editor_contact_modified (EABEditor *editor,
                                      const GError *error,
                                      EContact *contact)
{
	if (!error)
		return;

	if (g_error_matches (error, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED))
		return;

	eab_error_dialog (_("Error modifying list"), error);
}

static void
contact_list_editor_contact_deleted (EABEditor *editor,
                                     const GError *error,
                                     EContact *contact)
{
	if (!error)
		return;

	if (g_error_matches (error, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED))
		return;

	eab_error_dialog (_("Error removing list"), error);
}

static void
contact_list_editor_closed (EABEditor *editor)
{
	g_object_unref (editor);
}

/****************************** GType Callbacks ******************************/

static void
contact_list_editor_class_init (EContactListEditorClass *class)
{
	GObjectClass *object_class;
	EABEditorClass *editor_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EContactListEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = contact_list_editor_constructor;
	object_class->set_property = contact_list_editor_set_property;
	object_class->get_property = contact_list_editor_get_property;
	object_class->dispose = contact_list_editor_dispose;

	editor_class = EAB_EDITOR_CLASS (class);
	editor_class->show = contact_list_editor_show;
	editor_class->close = contact_list_editor_close;
	editor_class->raise = contact_list_editor_raise;
	editor_class->save_contact = contact_list_editor_save_contact;
	editor_class->is_valid = contact_list_editor_is_valid;
	editor_class->is_changed = contact_list_editor_is_changed;
	editor_class->get_window = contact_list_editor_get_window;
	editor_class->contact_added = contact_list_editor_contact_added;
	editor_class->contact_modified = contact_list_editor_contact_modified;
	editor_class->contact_deleted = contact_list_editor_contact_deleted;
	editor_class->editor_closed = contact_list_editor_closed;

	g_object_class_install_property (
		object_class,
		PROP_BOOK,
		g_param_spec_object (
			"book",
			"Book",
			NULL,
			E_TYPE_BOOK,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CONTACT,
		g_param_spec_object (
			"contact",
			"Contact",
			NULL,
			E_TYPE_CONTACT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_IS_NEW_LIST,
		g_param_spec_boolean (
			"is_new_list",
			"Is New List",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_EDITABLE,
		g_param_spec_boolean (
			"editable",
			"Editable",
			NULL,
			FALSE,
			G_PARAM_READWRITE));
}

static void
contact_list_editor_init (EContactListEditor *editor)
{
	EContactListEditorPrivate *priv;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeView *view;

	priv = E_CONTACT_LIST_EDITOR_GET_PRIVATE (editor);

	priv->editable = TRUE;
	priv->allows_contact_lists = TRUE;

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (priv->builder, "contact-list-editor.ui");
	gtk_builder_connect_signals (priv->builder, NULL);

	/* Embed a pointer to the EContactListEditor in the top-level
	 * widget.  Signal handlers can then access the pointer from any
	 * child widget by calling contact_list_editor_extract(widget). */
	g_object_set_data (G_OBJECT (WIDGET (DIALOG)), TOPLEVEL_KEY, editor);

	view = GTK_TREE_VIEW (WIDGET (TREE_VIEW));
	priv->model = e_contact_list_model_new ();
	gtk_tree_view_set_model (view, priv->model);

	gtk_tree_selection_set_mode (
		gtk_tree_view_get_selection (view), GTK_SELECTION_MULTIPLE);

	gtk_tree_view_enable_model_drag_dest (view, NULL, 0, GDK_ACTION_LINK);
	e_drag_dest_add_directory_targets (WIDGET (TREE_VIEW));
	gtk_drag_dest_add_text_targets (WIDGET (TREE_VIEW));

	g_signal_connect (
		priv->model, "row-deleted",
		G_CALLBACK (contact_list_editor_row_deleted_cb), editor);
	g_signal_connect (
		priv->model, "row-inserted",
		G_CALLBACK (contact_list_editor_row_inserted_cb), editor);

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_append_column (view, column);

	gtk_tree_view_column_set_cell_data_func (
		column, renderer, (GtkTreeCellDataFunc)
		contact_list_editor_render_destination, NULL, NULL);

	priv->name_selector = e_name_selector_new ();

	e_name_selector_model_add_section (
		e_name_selector_peek_model (priv->name_selector),
		"Members", _("_Members"), NULL);

	g_signal_connect (
		editor, "notify::book",
		G_CALLBACK (contact_list_editor_notify_cb), NULL);
	g_signal_connect (
		editor, "notify::editable",
		G_CALLBACK (contact_list_editor_notify_cb), NULL);

	gtk_widget_show_all (WIDGET (DIALOG));

	setup_custom_widgets (editor);

	editor->priv = priv;
}

/***************************** Public Interface ******************************/

GType
e_contact_list_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
		type = g_type_register_static_simple (
			EAB_TYPE_EDITOR,
			"EContactListEditor",
			sizeof (EContactListEditorClass),
			(GClassInitFunc) contact_list_editor_class_init,
			sizeof (EContactListEditor),
			(GInstanceInitFunc) contact_list_editor_init, 0);

	return type;
}

EABEditor *
e_contact_list_editor_new (EShell *shell,
                           EBook *book,
                           EContact *list_contact,
                           gboolean is_new_list,
                           gboolean editable)
{
	EABEditor *editor;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	editor = g_object_new (
		E_TYPE_CONTACT_LIST_EDITOR,
		"shell", shell, NULL);

	g_object_set (editor,
		      "book", book,
		      "contact", list_contact,
		      "is_new_list", is_new_list,
		      "editable", editable,
		      NULL);

	return editor;
}

EBook *
e_contact_list_editor_get_book (EContactListEditor *editor)
{
	g_return_val_if_fail (E_IS_CONTACT_LIST_EDITOR (editor), NULL);

	return editor->priv->book;
}

void
e_contact_list_editor_set_book (EContactListEditor *editor,
                                EBook *book)
{
	g_return_if_fail (E_IS_CONTACT_LIST_EDITOR (editor));
	g_return_if_fail (E_IS_BOOK (book));

	if (editor->priv->book != NULL)
		g_object_unref (editor->priv->book);
	editor->priv->book = g_object_ref (book);

	editor->priv->allows_contact_lists =
		e_book_check_static_capability (
		editor->priv->book, "contact-lists");

	contact_list_editor_update (editor);

	g_object_notify (G_OBJECT (editor), "book");
}

EContact *
e_contact_list_editor_get_contact (EContactListEditor *editor)
{
	GtkTreeModel *model;
	EContact *contact;
	GtkTreeIter iter;
	gboolean iter_valid;
	const gchar *text;
	GSList *attrs = NULL, *a;

	g_return_val_if_fail (E_IS_CONTACT_LIST_EDITOR (editor), NULL);

	model = editor->priv->model;
	contact = editor->priv->contact;

	if (contact == NULL)
		return NULL;

	text = gtk_entry_get_text (GTK_ENTRY (WIDGET (LIST_NAME_ENTRY)));
	if (text != NULL && *text != '\0') {
		e_contact_set (contact, E_CONTACT_FILE_AS, (gpointer) text);
		e_contact_set (contact, E_CONTACT_FULL_NAME, (gpointer) text);
	}

	e_contact_set (contact, E_CONTACT_LOGO, NULL);
	e_contact_set (contact, E_CONTACT_IS_LIST, GINT_TO_POINTER (TRUE));

	e_contact_set (
		contact, E_CONTACT_LIST_SHOW_ADDRESSES,
		GINT_TO_POINTER (!gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (WIDGET (CHECK_BUTTON)))));

	e_vcard_remove_attributes (E_VCARD (contact), "", EVC_EMAIL);

	iter_valid = gtk_tree_model_get_iter_first (model, &iter);

	while (iter_valid) {
		EDestination *dest;
		EVCardAttribute *attr;

		gtk_tree_model_get (model, &iter, 0, &dest, -1);
		attr = e_vcard_attribute_new (NULL, EVC_EMAIL);
		attrs = g_slist_prepend (attrs, attr);
		e_destination_export_to_vcard_attribute (dest, attr);
		g_object_unref (dest);

		iter_valid = gtk_tree_model_iter_next (model, &iter);
	}

	/* Put it in reverse order because e_vcard_add_attribute also uses prepend,
	   but we want to keep order of mails there. Hopefully noone will change
	   the behaviour of the e_vcard_add_attribute. */
	for (a = attrs; a; a = a->next) {
		e_vcard_add_attribute (E_VCARD (contact), a->data);
	}

	return contact;
}

/* Helper for e_contact_list_editor_set_contact() */
static void
contact_list_editor_add_destination (EVCardAttribute *attr,
                                     EContactListEditor *editor)
{
	EDestination *destination;
	gchar *contact_uid = NULL;
	gint email_num = -1;
	GList *list, *iter;
	GList *values;
	gchar *value;

	destination = e_destination_new ();

	list = e_vcard_attribute_get_params (attr);
	for (iter = list; iter; iter = iter->next) {
		EVCardAttributeParam *param = iter->data;
		const gchar *param_name;
		gpointer param_data;

		values = e_vcard_attribute_param_get_values (param);
		if (values == NULL)
			continue;

		param_name = e_vcard_attribute_param_get_name (param);
		param_data = values->data;

		if (!g_ascii_strcasecmp (param_name, EVC_X_DEST_CONTACT_UID))
			contact_uid = param_data;
		else if (!g_ascii_strcasecmp (param_name, EVC_X_DEST_EMAIL_NUM))
			email_num = atoi (param_data);
		else if (!g_ascii_strcasecmp (param_name, EVC_X_DEST_HTML_MAIL))
			e_destination_set_html_mail_pref (
				destination,
				!g_ascii_strcasecmp (param_data, "true"));
	}

	value = e_vcard_attribute_get_value (attr);
	if (value)
		e_destination_set_raw (destination, value);
	g_free (value);

	if (contact_uid != NULL)
		e_destination_set_contact_uid (
			destination, contact_uid, email_num);

	e_contact_list_model_add_destination (
		E_CONTACT_LIST_MODEL (editor->priv->model), destination);

	e_vcard_attribute_free (attr);
}

void
e_contact_list_editor_set_contact (EContactListEditor *editor,
                                   EContact *contact)
{
	EContactListEditorPrivate *priv;

	g_return_if_fail (E_IS_CONTACT_LIST_EDITOR (editor));
	g_return_if_fail (E_IS_CONTACT (contact));

	priv = editor->priv;

	if (priv->contact != NULL)
		g_object_unref (priv->contact);
	priv->contact = e_contact_duplicate (contact);

	if (priv->contact != NULL) {
		const gchar *file_as;
		gboolean show_addresses;
		GList *email_list;

		file_as = e_contact_get_const (
			priv->contact, E_CONTACT_FILE_AS);
		email_list = e_contact_get_attributes (
			priv->contact, E_CONTACT_EMAIL);
		show_addresses = GPOINTER_TO_INT (e_contact_get (
			priv->contact, E_CONTACT_LIST_SHOW_ADDRESSES));

		if (file_as == NULL)
			file_as = "";

		gtk_entry_set_text (
			GTK_ENTRY (WIDGET (LIST_NAME_ENTRY)), file_as);

		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (WIDGET (CHECK_BUTTON)),
			!show_addresses);

		e_contact_list_model_remove_all (
			E_CONTACT_LIST_MODEL (priv->model));

		g_list_foreach (
			email_list, (GFunc)
			contact_list_editor_add_destination, editor);
		g_list_free (email_list);
	}

	if (priv->book != NULL) {
		e_source_combo_box_set_active (
			E_SOURCE_COMBO_BOX (WIDGET (SOURCE_MENU)),
			e_book_get_source (priv->book));
		gtk_widget_set_sensitive (
			WIDGET (SOURCE_MENU), priv->is_new_list);
	}

	priv->changed = FALSE;
	contact_list_editor_update (editor);

	g_object_notify (G_OBJECT (editor), "contact");
}

gboolean
e_contact_list_editor_get_is_new_list (EContactListEditor *editor)
{
	g_return_val_if_fail (E_IS_CONTACT_LIST_EDITOR (editor), FALSE);

	return editor->priv->is_new_list;
}

void
e_contact_list_editor_set_is_new_list (EContactListEditor *editor,
                                       gboolean is_new_list)
{

	g_return_if_fail (E_IS_CONTACT_LIST_EDITOR (editor));

	editor->priv->is_new_list = is_new_list;
	contact_list_editor_update (editor);

	g_object_notify (G_OBJECT (editor), "is_new_list");
}

gboolean
e_contact_list_editor_get_editable (EContactListEditor *editor)
{
	g_return_val_if_fail (E_IS_CONTACT_LIST_EDITOR (editor), FALSE);

	return editor->priv->editable;
}

void
e_contact_list_editor_set_editable (EContactListEditor *editor,
                                    gboolean editable)
{
	g_return_if_fail (E_IS_CONTACT_LIST_EDITOR (editor));

	editor->priv->editable = editable;
	contact_list_editor_update (editor);

	g_object_notify (G_OBJECT (editor), "editable");
}
