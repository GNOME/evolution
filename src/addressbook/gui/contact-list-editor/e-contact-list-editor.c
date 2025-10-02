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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-contact-list-editor.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <camel/camel.h>

#include "shell/e-shell.h"

#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/util/eab-book-util.h"

#include "addressbook/gui/contact-editor/eab-editor.h"
#include "addressbook/gui/contact-editor/e-contact-editor.h"
#include "e-contact-list-model.h"
#include "eab-contact-merging.h"

#define CONTACT_LIST_EDITOR_WIDGET(editor, name) \
	(e_builder_get_widget \
	(E_CONTACT_LIST_EDITOR (editor)->priv->builder, name))

/* More macros, less typos. */
#define CONTACT_LIST_EDITOR_WIDGET_ADD_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "add-button")
#define CONTACT_LIST_EDITOR_WIDGET_CHECK_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "check-button")
#define CONTACT_LIST_EDITOR_WIDGET_CLIENT_COMBO_BOX(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "client-combo-box")
#define CONTACT_LIST_EDITOR_WIDGET_DIALOG(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "dialog")
#define CONTACT_LIST_EDITOR_WIDGET_EMAIL_ENTRY(editor) \
	editor->priv->email_entry
#define CONTACT_LIST_EDITOR_WIDGET_LIST_NAME_ENTRY(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "list-name-entry")
#define CONTACT_LIST_EDITOR_WIDGET_MEMBERS_VBOX(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "members-vbox")
#define CONTACT_LIST_EDITOR_WIDGET_OK_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "ok-button")
#define CONTACT_LIST_EDITOR_WIDGET_REMOVE_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "remove-button")
#define CONTACT_LIST_EDITOR_WIDGET_TREE_VIEW(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "tree-view")
#define CONTACT_LIST_EDITOR_WIDGET_TOP_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "top-button")
#define CONTACT_LIST_EDITOR_WIDGET_UP_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "up-button")
#define CONTACT_LIST_EDITOR_WIDGET_DOWN_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "down-button")
#define CONTACT_LIST_EDITOR_WIDGET_BOTTOM_BUTTON(editor) \
	CONTACT_LIST_EDITOR_WIDGET ((editor), "bottom-button")

/* Shorthand, requires a variable named "editor". */
#define WIDGET(name)	(CONTACT_LIST_EDITOR_WIDGET_##name (editor))

#define TOPLEVEL_KEY	(g_type_name (E_TYPE_CONTACT_LIST_EDITOR))

enum {
	PROP_0,
	PROP_CLIENT,
	PROP_CONTACT,
	PROP_IS_NEW_LIST,
	PROP_EDITABLE
};

typedef struct {
	EContactListEditor *editor;
	gboolean should_close;
} EditorCloseStruct;

struct _EContactListEditorPrivate {

	EBookClient *book_client;
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

typedef struct {
	EContactListEditor *editor;
	ESource *source;
} ConnectClosure;

G_DEFINE_TYPE_WITH_PRIVATE (EContactListEditor, e_contact_list_editor, EAB_TYPE_EDITOR)

static void
editor_close_struct_free (EditorCloseStruct *ecs)
{
	if (ecs) {
		g_clear_object (&ecs->editor);
		g_slice_free (EditorCloseStruct, ecs);
	}
}

static void
connect_closure_free (ConnectClosure *connect_closure)
{
	if (connect_closure->editor != NULL)
		g_object_unref (connect_closure->editor);

	if (connect_closure->source != NULL)
		g_object_unref (connect_closure->source);

	g_slice_free (ConnectClosure, connect_closure);
}

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
		WIDGET (CLIENT_COMBO_BOX), priv->is_new_list);
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

static gboolean
contact_list_editor_add_destination (GtkWidget *widget,
                                     EDestination *dest)
{
	EContactListEditor *editor = contact_list_editor_extract (widget);
	EContactListModel *model = E_CONTACT_LIST_MODEL (editor->priv->model);
	GtkTreeView *treeview = GTK_TREE_VIEW (WIDGET (TREE_VIEW));
	GtkTreePath *path;
	gboolean ignore_conflicts = TRUE;

	if (e_destination_is_evolution_list (dest)) {
		const gchar *id = e_destination_get_contact_uid (dest);
		const gchar *name = e_destination_get_name (dest);

		if (e_contact_list_model_has_uid (model, id)) {
			gint response;

			response = e_alert_run_dialog_for_args (
				GTK_WINDOW (WIDGET (DIALOG)),
				"addressbook:ask-list-add-list-exists",
				name, NULL);
			if (response != GTK_RESPONSE_YES)
				return FALSE;
		} else {
			const GList *l_dests, *l_dest;
			gint reply;

			/* Check the new list mail-by-mail for conflicts and
			 * eventually ask user what to do with all conflicts. */
			l_dests = e_destination_list_get_dests (dest);
			for (l_dest = l_dests; l_dest; l_dest = l_dest->next) {
				if (e_contact_list_model_has_email (model, e_destination_get_email (l_dest->data))) {
					reply = e_alert_run_dialog_for_args (
						GTK_WINDOW (WIDGET (DIALOG)),
						"addressbook:ask-list-add-some-mails-exist", NULL);
					if (reply == GTK_RESPONSE_YES) {
						ignore_conflicts = TRUE;
						break;
					} else if (reply == GTK_RESPONSE_NO) {
						ignore_conflicts = FALSE;
						break;
					} else {
						return FALSE;
					}
				}
			}
		}

	} else {
		const gchar *email = e_destination_get_email (dest);
		const gchar *tag = "addressbook:ask-list-add-exists";

		if (e_contact_list_model_has_email (model, email) &&
		    (e_alert_run_dialog_for_args (GTK_WINDOW (WIDGET (DIALOG)), tag, email, NULL) != GTK_RESPONSE_YES))
			return FALSE;
	}

	/* always add to the root level */
	path = e_contact_list_model_add_destination (
		model, dest, NULL, ignore_conflicts);
	if (path) {
		contact_list_editor_scroll_to_end (editor);
		gtk_tree_view_expand_to_path (treeview, path);
		gtk_tree_path_free (path);

		return TRUE;
	}

	return FALSE;
}

static void
contact_list_editor_add_email (EContactListEditor *editor,
                               const gchar *email)
{
	CamelInternetAddress *addr;
	EContactListEditorPrivate *priv = editor->priv;
	EDestination *dest = NULL;
	gint addr_length;

	addr = camel_internet_address_new ();
	addr_length = camel_address_unformat (CAMEL_ADDRESS (addr), email);
	if (addr_length >= 1) {
		gint ii;

		for (ii = 0; ii < addr_length; ii++) {
			const gchar *name = NULL, *mail = NULL;

			if (camel_internet_address_get (addr, ii, &name, &mail) &&
			    (name || mail)) {
				dest = e_destination_new ();
				if (mail)
					e_destination_set_email (dest, mail);
				if (name)
					e_destination_set_name (dest, name);

				priv->changed = contact_list_editor_add_destination (WIDGET (DIALOG), dest)
						|| priv->changed;
			}
		}
	} else {
		dest = e_destination_new ();
		e_destination_set_email (dest, email);

		priv->changed = contact_list_editor_add_destination (WIDGET (DIALOG), dest)
				|| priv->changed;
	}
	g_object_unref (addr);

	contact_list_editor_update (editor);
}

static void
contact_list_editor_get_client_cb (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	ConnectClosure *closure = user_data;
	EContactListEditor *editor = closure->editor;
	EClientComboBox *combo_box;
	EContactStore *contact_store;
	ENameSelectorEntry *entry;
	EClient *client;
	EBookClient *book_client;
	GError *error = NULL;

	combo_box = E_CLIENT_COMBO_BOX (source_object);

	client = e_client_combo_box_get_client_finish (
		combo_box, result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		GtkWindow *parent;

		parent = eab_editor_get_window (EAB_EDITOR (editor));

		eab_load_error_dialog (
			GTK_WIDGET (parent), NULL,
			closure->source, error);

		e_source_combo_box_set_active (
			E_SOURCE_COMBO_BOX (combo_box),
			e_client_get_source (E_CLIENT (editor->priv->book_client)));

		g_error_free (error);
		goto exit;
	}

	book_client = E_BOOK_CLIENT (client);

	entry = E_NAME_SELECTOR_ENTRY (WIDGET (EMAIL_ENTRY));
	contact_store = e_name_selector_entry_peek_contact_store (entry);
	e_contact_store_add_client (contact_store, book_client);
	e_contact_list_editor_set_client (editor, book_client);

	g_object_unref (client);

exit:
	connect_closure_free (closure);
}

static void
contact_list_editor_list_added_cb (EBookClient *book_client,
                                   const GError *error,
                                   const gchar *id,
                                   gpointer closure)
{
	EditorCloseStruct *ecs = closure;
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

	editor_close_struct_free (ecs);
}

static void
contact_list_editor_list_modified_cb (EBookClient *book_client,
                                      const GError *error,
                                      gpointer closure)
{
	EditorCloseStruct *ecs = closure;
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

	editor_close_struct_free (ecs);
}

static void
contact_list_editor_render_destination (GtkTreeViewColumn *column,
                                        GtkCellRenderer *renderer,
                                        GtkTreeModel *model,
                                        GtkTreeIter *iter)
{
	/* XXX Would be nice if EDestination had a text property
	 *     that we could just bind the GtkCellRenderer to. */

	EDestination *destination = NULL;
	gchar *name = NULL, *email = NULL;
	const gchar *textrep;
	gchar *out;

	g_return_if_fail (GTK_IS_TREE_MODEL (model));

	gtk_tree_model_get (model, iter, 0, &destination, -1);
	g_return_if_fail (destination && E_IS_DESTINATION (destination));

	textrep = e_destination_get_textrep (destination, TRUE);
	if (eab_parse_qp_email (textrep, &name, &email)) {
		if (e_destination_is_evolution_list (destination)) {
			g_object_set (renderer, "text", name, NULL);
		} else {
			out = g_strdup_printf ("%s <%s>", name, email);
			g_object_set (renderer, "text", out, NULL);
			g_free (out);
		}
		g_free (email);
		g_free (name);
	} else {
		g_object_set (renderer, "text", textrep, NULL);
	}

	g_object_unref (destination);
}

static void
contact_list_editor_selection_changed_cb (GtkTreeSelection *selection,
                                          gpointer user_data)
{
	EContactListEditor *editor = user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *first_item;
	GList *selected;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (WIDGET (TREE_VIEW)));

	/* Is selected anything at all? */
	if (gtk_tree_selection_count_selected_rows (selection) == 0) {
		gtk_widget_set_sensitive (WIDGET (TOP_BUTTON), FALSE);
		gtk_widget_set_sensitive (WIDGET (UP_BUTTON), FALSE);
		gtk_widget_set_sensitive (WIDGET (DOWN_BUTTON), FALSE);
		gtk_widget_set_sensitive (WIDGET (BOTTOM_BUTTON), FALSE);
		gtk_widget_set_sensitive (WIDGET (REMOVE_BUTTON), FALSE);
		return;
	}

	gtk_widget_set_sensitive (WIDGET (REMOVE_BUTTON), TRUE);

	/* Item before selected item exists => enable Top/Up buttons */
	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	/* Don't update path in the list! */
	first_item = gtk_tree_path_copy (selected->data);
	if (gtk_tree_path_prev (first_item)) {
		gtk_widget_set_sensitive (WIDGET (TOP_BUTTON), TRUE);
		gtk_widget_set_sensitive (WIDGET (UP_BUTTON), TRUE);
	} else {
		gtk_widget_set_sensitive (WIDGET (TOP_BUTTON), FALSE);
		gtk_widget_set_sensitive (WIDGET (UP_BUTTON), FALSE);
	}

	/* Item below last selected exists => enable Down/Bottom buttons */
	if (gtk_tree_model_get_iter (model, &iter, g_list_last (selected)->data) &&
	    gtk_tree_model_iter_next (model, &iter)) {
		gtk_widget_set_sensitive (WIDGET (DOWN_BUTTON), TRUE);
		gtk_widget_set_sensitive (WIDGET (BOTTOM_BUTTON), TRUE);
	} else {
		gtk_widget_set_sensitive (WIDGET (DOWN_BUTTON), FALSE);
		gtk_widget_set_sensitive (WIDGET (BOTTOM_BUTTON), FALSE);
	}

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);
	gtk_tree_path_free (first_item);
}

static void
contact_list_editor_add_from_email_entry (EContactListEditor *editor,
                                          ENameSelectorEntry *entry)
{
	EDestinationStore *store;
	GList *dests, *diter;
	gboolean added = FALSE;

	g_return_if_fail (E_IS_CONTACT_LIST_EDITOR (editor));
	g_return_if_fail (E_IS_NAME_SELECTOR_ENTRY (entry));

	store = e_name_selector_entry_peek_destination_store (entry);
	dests = e_destination_store_list_destinations (store);

	/* Add a reference, to not have it freed in e-name-selector-entry.c::user_focus_out() */
	g_list_foreach (dests, (GFunc) g_object_ref, NULL);

	for (diter = dests; diter; diter = g_list_next (diter)) {
		EDestination *dest = diter->data;

		if (dest && e_destination_get_address (dest)) {
			editor->priv->changed = contact_list_editor_add_destination (WIDGET (DIALOG), dest)
						|| editor->priv->changed;
			added = TRUE;
		}
	}

	g_list_free_full (dests, g_object_unref);

	if (!added)
		contact_list_editor_add_email (editor, gtk_entry_get_text (GTK_ENTRY (entry)));
}

/*********************** Autoconnected Signal Handlers ***********************/

void
contact_list_editor_add_button_clicked_cb (GtkWidget *widget);

void
contact_list_editor_add_button_clicked_cb (GtkWidget *widget)
{
	EContactListEditor *editor;

	editor = contact_list_editor_extract (widget);

	contact_list_editor_add_from_email_entry (
		editor,
		E_NAME_SELECTOR_ENTRY (WIDGET (EMAIL_ENTRY)));
	gtk_entry_set_text (GTK_ENTRY (WIDGET (EMAIL_ENTRY)), "");
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
                                           gint x,
                                           gint y,
                                           GtkSelectionData *selection_data,
                                           guint info,
                                           guint time);

void
contact_list_editor_drag_data_received_cb (GtkWidget *widget,
                                           GdkDragContext *context,
                                           gint x,
                                           gint y,
                                           GtkSelectionData *selection_data,
                                           guint info,
                                           guint time)
{
	CamelInternetAddress *address;
	EContactListEditor *editor;
	gboolean changed = FALSE;
	gboolean handled = FALSE;
	const guchar *data;
	GSList *list, *iter;
	GdkAtom target;
	gint n_addresses = 0;
	gchar *text;

	editor = contact_list_editor_extract (widget);

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
		EDestination *dest;

		dest = e_destination_new ();
		e_destination_set_contact (dest, contact, 0);

		changed = contact_list_editor_add_destination (widget, dest) || changed;

		g_object_unref (dest);
	}

	g_slist_free_full (list, (GDestroyNotify) g_object_unref);

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
		contact_list_editor_add_email (editor, text);

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
	GtkEntry *entry;

	editor = contact_list_editor_extract (widget);
	entry = GTK_ENTRY (WIDGET (EMAIL_ENTRY));

	contact_list_editor_add_from_email_entry (
		editor,
		E_NAME_SELECTOR_ENTRY (entry));
	gtk_entry_set_text (entry, "");
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

	if (event->keyval == GDK_KEY_comma) {
		GtkEntry *entry;
		gint cpos = -1;

		entry = GTK_ENTRY (WIDGET (EMAIL_ENTRY));
		g_object_get (entry, "cursor-position", &cpos, NULL);

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

	if (can_comma || event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
		g_signal_emit_by_name (WIDGET (EMAIL_ENTRY), "activate", 0);

		return TRUE;
	}

	return FALSE;
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
	GtkTreeRowReference *new_selection = NULL;
	GtkTreeModel *model;
	GtkTreeView *view;
	GtkTreePath *path;
	GList *list, *liter;

	editor = contact_list_editor_extract (widget);

	view = GTK_TREE_VIEW (WIDGET (TREE_VIEW));
	selection = gtk_tree_view_get_selection (view);
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	/* Convert the GtkTreePaths to GtkTreeRowReferences. */
	for (liter = list; liter != NULL; liter = liter->next) {
		path = liter->data;

		liter->data = gtk_tree_row_reference_new (model, path);

		/* Store reference to next item below current selection */
		if (!liter->next) {
			gtk_tree_path_next (path);
			new_selection = gtk_tree_row_reference_new (model, path);
		}

		gtk_tree_path_free (path);
	}

	/* Delete each row in the list. */
	for (liter = list; liter != NULL; liter = liter->next) {
		GtkTreeRowReference *reference = liter->data;
		GtkTreeIter iter;
		gboolean valid;

		path = gtk_tree_row_reference_get_path (reference);
		valid = gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_path_free (path);
		if (valid) {
			e_contact_list_model_remove_row (E_CONTACT_LIST_MODEL (model), &iter);
		} else {
			g_warn_if_reached ();
		}
		gtk_tree_row_reference_free (reference);
	}

	/* new_selection != NULL when there is at least one item below the
	 * removed selection */
	if (new_selection) {
		path = gtk_tree_row_reference_get_path (new_selection);
		gtk_tree_selection_select_path (selection, path);
		gtk_tree_path_free (path);
		gtk_tree_row_reference_free (new_selection);
	} else {
		/* If selection was including the last item in the list, then
		 * find and select the new last item */
		GtkTreeIter iter, iter2;

		/* When FALSE is returned, there are no items in the list to be selected */
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			iter2 = iter;

			while (gtk_tree_model_iter_next (model, &iter))
				iter2 = iter;

			gtk_tree_selection_select_iter (selection, &iter2);
		}
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
	ENameSelectorDialog *dialog;
	EDestinationStore *store;
	GList *list, *iter;
	GtkWindow *window;

	editor = contact_list_editor_extract (widget);

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

		if (!contact_list_editor_add_destination (widget, destination))
			g_warning ("%s: Failed to add destination", G_STRFUNC);

		e_destination_store_remove_destination (store, destination);
	}

	g_list_free (list);

	gtk_entry_set_text (GTK_ENTRY (WIDGET (EMAIL_ENTRY)), "");

	editor->priv->changed = TRUE;
	contact_list_editor_update (editor);
}

void
contact_list_editor_combo_box_changed_cb (GtkWidget *widget);

void
contact_list_editor_combo_box_changed_cb (GtkWidget *widget)
{
	ESourceComboBox *combo_box;
	EContactListEditor *editor;
	ESource *active_source;
	ESource *client_source;
	EClient *client;

	editor = contact_list_editor_extract (widget);

	combo_box = E_SOURCE_COMBO_BOX (widget);
	active_source = e_source_combo_box_ref_active (combo_box);
	g_return_if_fail (active_source != NULL);

	client = E_CLIENT (editor->priv->book_client);
	client_source = e_client_get_source (client);

	if (!e_source_equal (client_source, active_source)) {
		ConnectClosure *connect_closure;

		connect_closure = g_slice_new0 (ConnectClosure);
		connect_closure->editor = g_object_ref (editor);
		connect_closure->source = g_object_ref (active_source);

		e_client_combo_box_get_client (
			E_CLIENT_COMBO_BOX (widget),
			active_source, NULL,
			contact_list_editor_get_client_cb,
			connect_closure);
	}

	g_object_unref (active_source);
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

	if (event->keyval == GDK_KEY_Delete) {
		g_signal_emit_by_name (WIDGET (REMOVE_BUTTON), "clicked");
		return TRUE;
	}

	return FALSE;
}

void
contact_list_editor_top_button_clicked_cb (GtkButton *button);

void
contact_list_editor_top_button_clicked_cb (GtkButton *button)
{
	EContactListEditor *editor;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	GList *references = NULL;
	GList *l, *selected;

	editor = contact_list_editor_extract (GTK_WIDGET (button));

	tree_view = GTK_TREE_VIEW (WIDGET (TREE_VIEW));
	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	for (l = selected; l; l = l->next)
		references = g_list_prepend (
			references,
			gtk_tree_row_reference_new (model, l->data));

	for (l = references; l; l = l->next) {
		path = gtk_tree_row_reference_get_path (l->data);
		if (gtk_tree_model_get_iter (model, &iter, path))
			gtk_tree_store_move_after (
				GTK_TREE_STORE (model), &iter, NULL);
		gtk_tree_path_free (path);
	}

	g_list_foreach (references, (GFunc) gtk_tree_row_reference_free, NULL);
	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (references);
	g_list_free (selected);

	contact_list_editor_selection_changed_cb (selection, editor);
}

void
contact_list_editor_up_button_clicked_cb (GtkButton *button);

void
contact_list_editor_up_button_clicked_cb (GtkButton *button)
{
	EContactListEditor *editor;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter, iter2;
	GtkTreePath *path;
	GList *selected;

	editor = contact_list_editor_extract (GTK_WIDGET (button));

	tree_view = GTK_TREE_VIEW (WIDGET (TREE_VIEW));
	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	/* Get iter of item above the first selected item */
	path = gtk_tree_path_copy (selected->data);
	gtk_tree_path_prev (path);
	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);
		return;
	}
	gtk_tree_path_free (path);

	/* Get iter of the last selected item */
	if (gtk_tree_model_get_iter (model, &iter2, g_list_last (selected)->data)) {
		gtk_tree_store_move_after (GTK_TREE_STORE (model), &iter, &iter2);
	}

	g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);

	contact_list_editor_selection_changed_cb (selection, editor);
}

void
contact_list_editor_down_button_clicked_cb (GtkButton *button);

void
contact_list_editor_down_button_clicked_cb (GtkButton *button)
{
	EContactListEditor *editor;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter, iter2;
	GList *selected;

	editor = contact_list_editor_extract (GTK_WIDGET (button));

	tree_view = GTK_TREE_VIEW (WIDGET (TREE_VIEW));
	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	/* Iter of the first selected item */
	if (!gtk_tree_model_get_iter (model, &iter, selected->data)) {
		g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);
		return;
	}

	/* Iter of item below the last selected item */
	if (!gtk_tree_model_get_iter (model, &iter2, g_list_last (selected)->data) ||
	    !gtk_tree_model_iter_next (model, &iter2)) {
		g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);
		return;
	}

	gtk_tree_store_move_before (GTK_TREE_STORE (model), &iter2, &iter);

	g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);

	contact_list_editor_selection_changed_cb (selection, editor);
}

void
contact_list_editor_bottom_button_clicked_cb (GtkButton *button);

void
contact_list_editor_bottom_button_clicked_cb (GtkButton *button)
{
	EContactListEditor *editor;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	GList *references = NULL;

	GList *l, *selected;

	editor = contact_list_editor_extract (GTK_WIDGET (button));

	tree_view = GTK_TREE_VIEW (WIDGET (TREE_VIEW));
	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	for (l = selected; l; l = l->next)
		references = g_list_prepend (
			references,
			gtk_tree_row_reference_new (model, l->data));
	references = g_list_reverse (references);

	for (l = references; l; l = l->next) {
		path = gtk_tree_row_reference_get_path (l->data);
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_store_move_before (
			GTK_TREE_STORE (model), &iter, NULL);
		gtk_tree_path_free (path);
	}

	g_list_foreach (references, (GFunc) gtk_tree_row_reference_free, NULL);
	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (references);
	g_list_free (selected);

	contact_list_editor_selection_changed_cb (selection, editor);
}

/******************** GtkBuilder Custom Widgets Functions ********************/

static gpointer
contact_editor_fudge_new (EBookClient *book_client,
                          EContact *contact,
                          gboolean is_new,
                          gboolean editable)
{
	EABEditor *editor;
	EShell *shell = e_shell_get_default ();
	GtkWindow *parent;

	/* XXX Putting this function signature in libedataserverui
	 *     was a terrible idea.  Now we're stuck with it. */

	editor = e_contact_editor_new (shell, book_client, contact, is_new, editable);
	parent = e_shell_get_active_window (shell);
	if (parent)
		gtk_window_set_transient_for (eab_editor_get_window (editor), parent);
	eab_editor_show (editor);

	return editor;
}

static gpointer
contact_list_editor_fudge_new (EBookClient *book_client,
                               EContact *contact,
                               gboolean is_new,
                               gboolean editable)
{
	EABEditor *editor;
	EShell *shell = e_shell_get_default ();
	GtkWindow *parent;

	/* XXX Putting this function signature in libedataserverui
	 *     was a terrible idea.  Now we're stuck with it. */

	editor = e_contact_list_editor_new (shell, book_client, contact, is_new, editable);
	parent = e_shell_get_active_window (shell);
	if (parent)
		gtk_window_set_transient_for (eab_editor_get_window (editor), parent);
	eab_editor_show (editor);

	return editor;
}

static void
setup_custom_widgets (EContactListEditor *editor)
{
	EShell *shell;
	EClientCache *client_cache;
	GtkWidget *combo_box;
	ENameSelectorEntry *name_selector_entry;
	GtkWidget *old, *parent;
	guint la = 0, ta = 0, w = 0, h = 0;

	g_return_if_fail (editor != NULL);

	shell = eab_editor_get_shell (EAB_EDITOR (editor));
	client_cache = e_shell_get_client_cache (shell);

	combo_box = WIDGET (CLIENT_COMBO_BOX);

	e_client_combo_box_set_client_cache (
		E_CLIENT_COMBO_BOX (combo_box), client_cache);

	g_signal_connect (
		combo_box, "changed", G_CALLBACK (
		contact_list_editor_combo_box_changed_cb), NULL);

	old = CONTACT_LIST_EDITOR_WIDGET (editor, "email-entry");
	g_return_if_fail (old != NULL);

	name_selector_entry = e_name_selector_peek_section_entry (editor->priv->name_selector, "Members");

	gtk_widget_set_name (
		GTK_WIDGET (name_selector_entry),
		gtk_widget_get_name (old));
	parent = gtk_widget_get_parent (old);

	gtk_container_child_get (
		GTK_CONTAINER (parent), old,
		"left-attach", &la,
		"top-attach", &ta,
		"width", &w,
		"height", &h,
		NULL);

	/* only hide it... */
	gtk_widget_hide (old);

	/* ... and place the new name selector to the
	 * exact place as is the old one in UI file */
	gtk_widget_show (GTK_WIDGET (name_selector_entry));
	gtk_grid_attach (
		GTK_GRID (parent), GTK_WIDGET (name_selector_entry),
		la, ta, w, h);
	editor->priv->email_entry = name_selector_entry;

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
}

/***************************** GObject Callbacks *****************************/

static void
contact_list_editor_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT:
			e_contact_list_editor_set_client (
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
		case PROP_CLIENT:
			g_value_set_object (
				value,
				e_contact_list_editor_get_client (
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
		e_name_selector_cancel_loading (priv->name_selector);
		g_object_unref (priv->name_selector);
		priv->name_selector = NULL;
	}

	g_clear_object (&priv->contact);
	g_clear_object (&priv->builder);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_contact_list_editor_parent_class)->dispose (object);
}

static void
contact_list_editor_constructed (GObject *object)
{
	EContactListEditor *editor;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeView *view;
	GtkTreeSelection *selection;
	EClientCache *client_cache;
	EShell *shell;

	editor = E_CONTACT_LIST_EDITOR (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_contact_list_editor_parent_class)->constructed (object);

	shell = eab_editor_get_shell (EAB_EDITOR (editor));
	client_cache = e_shell_get_client_cache (shell);

	editor->priv->editable = TRUE;
	editor->priv->allows_contact_lists = TRUE;

	g_type_ensure (E_TYPE_CLIENT_COMBO_BOX);

	editor->priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (
		editor->priv->builder, "contact-list-editor.ui");
	gtk_builder_connect_signals (editor->priv->builder, NULL);

	/* Embed a pointer to the EContactListEditor in the top-level
	 * widget.  Signal handlers can then access the pointer from any
	 * child widget by calling contact_list_editor_extract(widget). */
	g_object_set_data (G_OBJECT (WIDGET (DIALOG)), TOPLEVEL_KEY, editor);

	view = GTK_TREE_VIEW (WIDGET (TREE_VIEW));
	editor->priv->model = e_contact_list_model_new ();
	gtk_tree_view_set_model (view, editor->priv->model);

	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (contact_list_editor_selection_changed_cb), editor);

	gtk_tree_view_enable_model_drag_dest (view, NULL, 0, GDK_ACTION_LINK);
	e_drag_dest_add_directory_targets (WIDGET (TREE_VIEW));
	gtk_drag_dest_add_text_targets (WIDGET (TREE_VIEW));

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_append_column (view, column);

	gtk_tree_view_column_set_cell_data_func (
		column, renderer, (GtkTreeCellDataFunc)
		contact_list_editor_render_destination, NULL, NULL);

	editor->priv->name_selector = e_name_selector_new (client_cache);

	e_name_selector_model_add_section (
		e_name_selector_peek_model (editor->priv->name_selector),
		"Members", _("_Members"), NULL);

	e_signal_connect_notify (
		editor, "notify::book",
		G_CALLBACK (contact_list_editor_notify_cb), NULL);
	e_signal_connect_notify (
		editor, "notify::editable",
		G_CALLBACK (contact_list_editor_notify_cb), NULL);

	gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (WIDGET (DIALOG))));

	setup_custom_widgets (editor);

	e_name_selector_load_books (editor->priv->name_selector);

	contact_list_editor_update (E_CONTACT_LIST_EDITOR (object));
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
	ESourceRegistry *registry;
	EditorCloseStruct *ecs;
	EContact *contact;
	EShell *shell;
	GtkWidget *client_combo_box;
	ESource *active_source;

	shell = eab_editor_get_shell (eab_editor);
	registry = e_shell_get_registry (shell);

	contact = e_contact_list_editor_get_contact (editor);

	if (priv->book_client == NULL)
		return;

	client_combo_box = WIDGET (CLIENT_COMBO_BOX);
	active_source = e_source_combo_box_ref_active (E_SOURCE_COMBO_BOX (client_combo_box));
	g_return_if_fail (active_source != NULL);

	if (!e_source_equal (e_client_get_source (E_CLIENT (priv->book_client)), active_source)) {
		e_alert_run_dialog_for_args (
				GTK_WINDOW (WIDGET (DIALOG)),
				"addressbook:error-still-opening",
				e_source_get_display_name (active_source),
				NULL);
		g_object_unref (active_source);
		return;
	}

	g_object_unref (active_source);

	ecs = g_slice_new (EditorCloseStruct);
	ecs->editor = g_object_ref (editor);
	ecs->should_close = should_close;

	gtk_widget_set_sensitive (WIDGET (DIALOG), FALSE);
	priv->in_async_call = TRUE;

	if (priv->is_new_list)
		eab_merging_book_add_contact (
			registry, priv->book_client, contact,
			contact_list_editor_list_added_cb, ecs, FALSE);
	else
		eab_merging_book_modify_contact (
			registry, priv->book_client, contact,
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
	return E_CONTACT_LIST_EDITOR (editor)->priv->changed;
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
	GtkWindow *window;
	const gchar *message;

	if (error == NULL)
		return;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return;

	window = eab_editor_get_window (editor);
	message = _("Error adding list");

	eab_error_dialog (NULL, window, message, error);
}

static void
contact_list_editor_contact_modified (EABEditor *editor,
                                      const GError *error,
                                      EContact *contact)
{
	GtkWindow *window;
	const gchar *message;

	if (error == NULL)
		return;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return;

	window = eab_editor_get_window (editor);
	message = _("Error modifying list");

	eab_error_dialog (NULL, window, message, error);
}

static void
contact_list_editor_contact_deleted (EABEditor *editor,
                                     const GError *error,
                                     EContact *contact)
{
	GtkWindow *window;
	const gchar *message;

	if (error == NULL)
		return;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return;

	window = eab_editor_get_window (editor);
	message = _("Error removing list");

	eab_error_dialog (NULL, window, message, error);
}

static void
contact_list_editor_closed (EABEditor *editor)
{
	g_object_unref (editor);
}

/****************************** GType Callbacks ******************************/

static void
e_contact_list_editor_class_init (EContactListEditorClass *class)
{
	GObjectClass *object_class;
	EABEditorClass *editor_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = contact_list_editor_set_property;
	object_class->get_property = contact_list_editor_get_property;
	object_class->dispose = contact_list_editor_dispose;
	object_class->constructed = contact_list_editor_constructed;

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
		PROP_CLIENT,
		g_param_spec_object (
			"client",
			"EBookClient",
			NULL,
			E_TYPE_BOOK_CLIENT,
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
e_contact_list_editor_init (EContactListEditor *editor)
{
	editor->priv = e_contact_list_editor_get_instance_private (editor);
}

/***************************** Public Interface ******************************/

EABEditor *
e_contact_list_editor_new (EShell *shell,
                           EBookClient *book_client,
                           EContact *list_contact,
                           gboolean is_new_list,
                           gboolean editable)
{
	EABEditor *editor;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	editor = g_object_new (
		E_TYPE_CONTACT_LIST_EDITOR,
		"shell", shell, NULL);

	g_object_set (
		editor,
		"client", book_client,
		"contact", list_contact,
		"is_new_list", is_new_list,
		"editable", editable,
		NULL);

	return editor;
}

EBookClient *
e_contact_list_editor_get_client (EContactListEditor *editor)
{
	g_return_val_if_fail (E_IS_CONTACT_LIST_EDITOR (editor), NULL);

	return editor->priv->book_client;
}

void
e_contact_list_editor_set_client (EContactListEditor *editor,
                                  EBookClient *book_client)
{
	g_return_if_fail (E_IS_CONTACT_LIST_EDITOR (editor));
	g_return_if_fail (E_IS_BOOK_CLIENT (book_client));

	if (book_client == editor->priv->book_client)
		return;

	if (editor->priv->book_client != NULL)
		g_object_unref (editor->priv->book_client);
	editor->priv->book_client = g_object_ref (book_client);

	editor->priv->allows_contact_lists = e_client_check_capability (
		E_CLIENT (editor->priv->book_client), "contact-lists");

	contact_list_editor_update (editor);

	g_object_notify (G_OBJECT (editor), "client");
}

static void
save_contact_list (GtkTreeModel *model,
                   GtkTreeIter *iter,
                   GSList **attrs,
                   gint *parent_id)
{
	EDestination *dest;
	EVCardAttribute *attr;
	gchar *pid_str = g_strdup_printf ("%d", *parent_id);

	do {
		gtk_tree_model_get (model, iter, 0, &dest, -1);

		if (gtk_tree_model_iter_has_child (model, iter)) {
			GtkTreeIter new_iter;
			gchar *uid;

			(*parent_id)++;
			uid = g_strdup_printf ("%d", *parent_id);

			attr = e_vcard_attribute_new (NULL, EVC_CONTACT_LIST);
			e_vcard_attribute_add_param_with_value (
				attr,
				e_vcard_attribute_param_new (EVC_CL_UID), uid);
			e_vcard_attribute_add_value (
				attr,
				e_destination_get_name (dest));

			g_free (uid);

			/* Set new_iter to first child of iter */
			if (gtk_tree_model_iter_children (model, &new_iter, iter)) {
				/* Go recursive */
				save_contact_list (model, &new_iter, attrs, parent_id);
			}
		} else {
			attr = e_vcard_attribute_new (NULL, EVC_EMAIL);
			e_destination_export_to_vcard_attribute (dest, attr);
		}

		e_vcard_attribute_add_param_with_value (
			attr,
			e_vcard_attribute_param_new (EVC_PARENT_CL), pid_str);

		*attrs = g_slist_prepend (*attrs, attr);

		g_object_unref (dest);

	} while (gtk_tree_model_iter_next (model, iter));

	g_free (pid_str);
}

EContact *
e_contact_list_editor_get_contact (EContactListEditor *editor)
{
	GtkTreeModel *model;
	EContact *contact;
	GtkTreeIter iter;
	const gchar *text;
	GSList *attrs = NULL, *a;
	gint parent_id = 0;

	g_return_val_if_fail (E_IS_CONTACT_LIST_EDITOR (editor), NULL);

	model = editor->priv->model;
	contact = editor->priv->contact;

	if (contact == NULL)
		return NULL;

	if (editor->priv->book_client) {
		EVCardVersion prefer_version;

		prefer_version = e_book_client_get_prefer_vcard_version (editor->priv->book_client);
		if (prefer_version != E_VCARD_VERSION_UNKNOWN) {
			EContact *converted;

			converted = e_contact_convert (contact, prefer_version);
			if (converted) {
				g_clear_object (&editor->priv->contact);
				editor->priv->contact = converted;
				contact = editor->priv->contact;
			}
		}
	}

	text = gtk_entry_get_text (GTK_ENTRY (WIDGET (LIST_NAME_ENTRY)));
	if (text != NULL && *text != '\0') {
		e_contact_set (contact, E_CONTACT_FILE_AS, text);
		e_contact_set (contact, E_CONTACT_FULL_NAME, text);
	}

	e_contact_set (contact, E_CONTACT_LOGO, NULL);
	e_contact_set (contact, E_CONTACT_IS_LIST, GINT_TO_POINTER (TRUE));

	e_contact_set (
		contact, E_CONTACT_LIST_SHOW_ADDRESSES,
		GINT_TO_POINTER (!gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (WIDGET (CHECK_BUTTON)))));

	e_vcard_remove_attributes (E_VCARD (contact), "", EVC_EMAIL);
	e_vcard_remove_attributes (E_VCARD (contact), "", EVC_CONTACT_LIST);

	if (gtk_tree_model_get_iter_first (model, &iter))
		save_contact_list (model, &iter, &attrs, &parent_id);

	/* Put it in reverse order because e_vcard_add_attribute also uses prepend,
	 * but we want to keep order of mails there. Hopefully noone will change
	 * the behaviour of the e_vcard_add_attribute. */
	for (a = attrs; a; a = a->next) {
		e_vcard_add_attribute (E_VCARD (contact), a->data);
	}

	g_slist_free (attrs);

	return contact;
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
		const GList *dests, *dest;

		/* The root destination */
		EDestination *list_dest = e_destination_new ();

		file_as = e_contact_get_const (
			priv->contact, E_CONTACT_FILE_AS);
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

		e_destination_set_name (list_dest, file_as);
		e_destination_set_contact (list_dest, priv->contact, 0);

		dests = e_destination_list_get_root_dests (list_dest);
		for (dest = dests; dest; dest = dest->next) {
			GtkTreePath *path;
			path = e_contact_list_model_add_destination (
				E_CONTACT_LIST_MODEL (priv->model),
				dest->data, NULL, TRUE);
			gtk_tree_path_free (path);
		}

		g_object_unref (list_dest);

		gtk_tree_view_expand_all (GTK_TREE_VIEW (WIDGET (TREE_VIEW)));
	}

	if (priv->book_client != NULL) {
		e_source_combo_box_set_active (
			E_SOURCE_COMBO_BOX (WIDGET (CLIENT_COMBO_BOX)),
			e_client_get_source (E_CLIENT (priv->book_client)));
		gtk_widget_set_sensitive (
			WIDGET (CLIENT_COMBO_BOX), priv->is_new_list);
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

	if (editor->priv->is_new_list == is_new_list)
		return;

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

	if (editor->priv->editable == editable)
		return;

	editor->priv->editable = editable;
	contact_list_editor_update (editor);

	g_object_notify (G_OBJECT (editor), "editable");
}

