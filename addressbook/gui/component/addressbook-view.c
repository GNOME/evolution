/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* addressbook-view.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Chris Toshok (toshok@ximian.com)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-href.h>
#include <libgnomeui/gnome-uidefs.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-exception.h>
#include <e-util/e-util.h>
#include <libedataserverui/e-source-selector.h>
#include <libedataserverui/e-passwords.h>

#include "e-util/e-error.h"
#include "e-util/e-request.h"
#include "misc/e-task-bar.h"
#include "misc/e-info-label.h"

#include "e-util/e-icon-factory.h"
#include "e-util/e-util-private.h"
#include "shell/e-user-creatable-items-handler.h"

#include "evolution-shell-component-utils.h"
#include "e-activity-handler.h"
#include "e-contact-editor.h"
#include "addressbook-config.h"
#include "addressbook.h"
#include "addressbook-view.h"
#include "addressbook-component.h"
#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/gui/merging/eab-contact-merging.h"
#include "addressbook/printing/e-contact-print.h"
#include "addressbook/util/eab-book-util.h"
#include "addressbook/gui/widgets/eab-popup.h"
#include "addressbook/gui/widgets/eab-menu.h"

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

#define d(x)

struct _AddressbookViewPrivate {
	GtkWidget *notebook;
	BonoboControl *folder_view_control;

	GtkWidget *statusbar_widget;
	EActivityHandler *activity_handler;

	GtkWidget *info_widget;
	GtkWidget *sidebar_widget;
	GtkWidget *selector;

	GConfClient *gconf_client;

	GHashTable *uid_to_view;
	GHashTable *uid_to_editor;

	EBook *book;
	guint activity_id;
	ESourceList *source_list;
	char *passwd;
	EUserCreatableItemsHandler *creatable_items_handler;

	EABMenu *menu;
};

enum DndTargetType {
	DND_TARGET_TYPE_VCARD_LIST,
	DND_TARGET_TYPE_SOURCE_VCARD_LIST
};
#define VCARD_TYPE        "text/x-vcard"
#define SOURCE_VCARD_TYPE "text/x-source-vcard"
static GtkTargetEntry drag_types[] = {
	{ SOURCE_VCARD_TYPE, 0, DND_TARGET_TYPE_SOURCE_VCARD_LIST },
	{ VCARD_TYPE, 0, DND_TARGET_TYPE_VCARD_LIST }
};
static gint num_drag_types = sizeof(drag_types) / sizeof(drag_types[0]);

static void set_status_message (EABView *eav, const char *message, AddressbookView *view);
static void search_result (EABView *eav, EBookViewStatus status, AddressbookView *view);

static void activate_source (AddressbookView *view, ESource *source);

static void addressbook_view_init	(AddressbookView      *view);
static void addressbook_view_class_init	(AddressbookViewClass *klass);

static void
set_status_message (EABView *eav, const char *message, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	EActivityHandler *activity_handler = priv->activity_handler;

	if (!message || !*message) {
		if (priv->activity_id != 0) {
			e_activity_handler_operation_finished (activity_handler, priv->activity_id);
			priv->activity_id = 0;
		}
	} else if (priv->activity_id == 0) {
		char *clientid = g_strdup_printf ("%p", view);

		priv->activity_id = e_activity_handler_operation_started (
			activity_handler, clientid, message, TRUE);

		g_free (clientid);
	} else {
		e_activity_handler_operation_progressing (activity_handler, priv->activity_id, message, -1.0);
	}

}

static void
set_folder_bar_message (EABView *eav, const char *message, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	EABView *current_view = get_current_view (view);

	if (eav == current_view) {
		ESource *source = eav->source;

		if (source) {
			const char *name = e_source_peek_name (source);

			e_info_label_set_info((EInfoLabel*)priv->info_widget, name, message);
		}
	}
}

static void
search_result (EABView *eav, EBookViewStatus status, AddressbookView *view)
{
	eab_search_result_dialog (NULL /* XXX */, status);
}

static void
control_activate (BonoboControl     *control,
		  BonoboUIComponent *uic,
		  AddressbookView   *view)
{
	AddressbookViewPrivate *priv = view->priv;
	Bonobo_UIContainer remote_ui_container;
	EABView *v = get_current_view (view);
	char *xmlfile;

	remote_ui_container = bonobo_control_get_remote_ui_container (control, NULL);
	bonobo_ui_component_set_container (uic, remote_ui_container, NULL);
	bonobo_object_release_unref (remote_ui_container, NULL);

	bonobo_ui_component_freeze (uic, NULL);

	xmlfile = g_build_filename (EVOLUTION_UIDIR,
				    "evolution-addressbook.xml",
				    NULL);
	bonobo_ui_util_set_ui (uic, PREFIX,
			       xmlfile,
			       "evolution-addressbook", NULL);
	g_free (xmlfile);

	if (v)
		eab_view_setup_menus (v, uic);

	e_user_creatable_items_handler_activate (priv->creatable_items_handler, uic);

	bonobo_ui_component_thaw (uic, NULL);

	if (v)
		update_command_state (v, view);
}

static void
control_activate_cb (BonoboControl *control,
		     gboolean activate,
		     AddressbookView *view)
{
	BonoboUIComponent *uic;
	EABView *v = get_current_view (view);

	uic = bonobo_control_get_ui_component (control);
	g_return_if_fail (uic != NULL);

	if (activate) {
		control_activate (control, uic, view);
		e_menu_activate((EMenu *)view->priv->menu, uic, activate);
		if (activate && v && v->model)
			eab_model_force_folder_bar_message (v->model);
	} else {
		e_menu_activate((EMenu *)view->priv->menu, uic, activate);
		bonobo_ui_component_unset_container (uic, NULL);
		eab_view_discard_menus (v);
	}
}

static void
load_uri_for_selection (ESourceSelector *selector,
			AddressbookView *view,
			gboolean force)
{
	ESource *selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (selector));
	ESource *primary = get_primary_source (view);

	if (selected_source != NULL &&
	    ((primary && (!g_str_equal (e_source_peek_uid (primary),e_source_peek_uid (selected_source) )))||force))
		activate_source (view, selected_source);
}

static void
save_primary_selection (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *source;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	if (!source)
		return;

	/* Save the selection for next time we start up */
	gconf_client_set_string (priv->gconf_client,
				 "/apps/evolution/addressbook/display/primary_addressbook",
				 e_source_peek_uid (source), NULL);
}

/* Folder popup menu callbacks */
typedef struct {
	AddressbookView *view;
	ESource *selected_source;
	GtkWidget *toplevel;
} BookRemovedClosure;

static void
book_removed (EBook *book, EBookStatus status, gpointer data)
{
	BookRemovedClosure *closure = data;
	AddressbookView *view = closure->view;
	AddressbookViewPrivate *priv = view->priv;
	ESource *source = closure->selected_source;
	GtkWidget *toplevel = closure->toplevel;

	g_free (closure);

	g_object_unref (book);

	if (E_BOOK_ERROR_OK == status) {
		/* Remove source */
		if (e_source_selector_source_is_selected (E_SOURCE_SELECTOR (priv->selector),
							  source))
			e_source_selector_unselect_source (E_SOURCE_SELECTOR (priv->selector),
							   source);

		e_source_group_remove_source (e_source_peek_group (source), source);

		e_source_list_sync (priv->source_list, NULL);
	}
	else {
		e_error_run (GTK_WINDOW (toplevel),
			     "addressbook:remove-addressbook",
			     NULL);
	}
}

static void
delete_addressbook_cb(EPopup *ep, EPopupItem *pitem, void *data)
{
	AddressbookView *view = data;
	AddressbookViewPrivate *priv = view->priv;
	ESource *selected_source;
	EBook  *book;
	GError *error = NULL;
	GtkWindow *toplevel;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	if (!selected_source)
		return;

	toplevel = (GtkWindow *)gtk_widget_get_toplevel(ep->target->widget);

	if (e_error_run(toplevel, "addressbook:ask-delete-addressbook", e_source_peek_name(selected_source)) != GTK_RESPONSE_YES)
		return;

	/* Remove local data */
	book = e_book_new (selected_source, &error);
	if (book) {
		BookRemovedClosure *closure = g_new (BookRemovedClosure, 1);

		closure->toplevel = (GtkWidget *)toplevel;
		closure->view = view;
		closure->selected_source = selected_source;

		if (e_book_async_remove (book, book_removed, closure)) {
			e_error_run (toplevel, "addressbook:remove-addressbook", NULL);
			g_free (closure);
			g_object_unref (book);
		}
	}
}

static void
primary_source_selection_changed_callback (ESourceSelector *selector,
					   AddressbookView *view)
{
	load_uri_for_selection (selector, view, FALSE);
	save_primary_selection (view);
}

static gboolean
selector_tree_drag_drop (GtkWidget *widget,
			 GdkDragContext *context,
			 int x,
			 int y,
			 guint time,
			 AddressbookView *view)
{
	GtkTreeViewColumn *column;
	int cell_x;
	int cell_y;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer data;

	if (!gtk_tree_view_get_path_at_pos  (GTK_TREE_VIEW (widget), x, y, &path, &column, &cell_x, &cell_y))
		return FALSE;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	gtk_tree_model_get (model, &iter, 0, &data, -1);

	if (E_IS_SOURCE_GROUP (data)) {
		g_object_unref (data);
		gtk_tree_path_free (path);
		return FALSE;
	}

	gtk_tree_path_free (path);
	return TRUE;
}

static gboolean
selector_tree_drag_motion (GtkWidget *widget,
			   GdkDragContext *context,
			   int x,
			   int y)
{
	GtkTreePath *path = NULL;
	gpointer data = NULL;
	GtkTreeViewDropPosition pos;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GdkDragAction action = { 0, };

	if (!gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
						x, y, &path, &pos))
		goto finish;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto finish;

	gtk_tree_model_get (model, &iter, 0, &data, -1);

	if (E_IS_SOURCE_GROUP (data) || e_source_get_readonly (data))
		goto finish;

	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW (widget), path, GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
	/* Make default action move, not copy */
	if (context->actions & GDK_ACTION_MOVE)
		action = GDK_ACTION_MOVE;
	else
		action = context->suggested_action;

 finish:
	if (path)
		gtk_tree_path_free (path);
	if (data)
		g_object_unref (data);

	gdk_drag_status (context, action, GDK_CURRENT_TIME);
	return TRUE;
}

typedef struct
{
	guint     remove_from_source : 1;
	guint     copy_done          : 1;
	gint      pending_removals;

	EContact *current_contact;
	GList    *remaining_contacts;

	EBook    *source_book;
	EBook    *target_book;
}
MergeContext;

static void
destroy_merge_context (MergeContext *merge_context)
{
	if (merge_context->source_book)
		g_object_unref (merge_context->source_book);
	if (merge_context->target_book)
		g_object_unref (merge_context->target_book);

	g_free (merge_context);
}

static void
removed_contact_cb (EBook *book, EBookStatus status, gpointer closure)
{
	MergeContext *merge_context = closure;

	merge_context->pending_removals--;

	if (merge_context->copy_done && merge_context->pending_removals == 0) {
		/* Finished */

		destroy_merge_context (merge_context);
	}
}

static void
merged_contact_cb (EBook *book, EBookStatus status, const char *id, gpointer closure)
{
	MergeContext *merge_context = closure;

	if (merge_context->remove_from_source && status == E_BOOK_ERROR_OK) {
		/* Remove previous contact from source */

		e_book_async_remove_contact (merge_context->source_book, merge_context->current_contact,
					     removed_contact_cb, merge_context);
		merge_context->pending_removals++;
	}

	g_object_unref (merge_context->current_contact);

	if (merge_context->remaining_contacts) {
		/* Copy next contact */

		merge_context->current_contact = merge_context->remaining_contacts->data;
		merge_context->remaining_contacts = g_list_delete_link (merge_context->remaining_contacts,
									merge_context->remaining_contacts);
		eab_merging_book_add_contact (merge_context->target_book, merge_context->current_contact,
					      merged_contact_cb, merge_context);
	} else if (merge_context->pending_removals == 0) {
		/* Finished */

		destroy_merge_context (merge_context);
	} else {
		/* Finished, but have pending removals */

		merge_context->copy_done = TRUE;
	}
}

static gboolean
selector_tree_drag_data_received (GtkWidget *widget,
				  GdkDragContext *context,
				  gint x,
				  gint y,
				  GtkSelectionData *data,
				  guint info,
				  guint time,
				  gpointer user_data)
{
	GtkTreePath *path = NULL;
	GtkTreeViewDropPosition pos;
	gpointer target = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean success = FALSE;
	EBook *source_book, *target_book;
	MergeContext *merge_context = NULL;
	GList *contactlist;
	AddressbookView *view;
	EABView *v;

	if (!gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
						x, y, &path, &pos))
		goto finish;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto finish;

	gtk_tree_model_get (model, &iter, 0, &target, -1);

	if (E_IS_SOURCE_GROUP (target) || e_source_get_readonly (target))
		goto finish;

	target_book = e_book_new (target, NULL);
	if (!target_book) {
		g_message (G_STRLOC ":Couldn't create EBook.");
		return FALSE;
	}
	e_book_open (target_book, FALSE, NULL);

	eab_book_and_contact_list_from_string ((char *)data->data, &source_book, &contactlist);

	view = (AddressbookView *) user_data;
	v = get_current_view (view);
	g_object_get (v->model, "book",&source_book, NULL);

	/* Set up merge context */

	merge_context = g_new0 (MergeContext, 1);

	merge_context->source_book = source_book;
	merge_context->target_book = target_book;

	merge_context->current_contact = contactlist->data;
	merge_context->remaining_contacts = g_list_delete_link (contactlist, contactlist);

	merge_context->remove_from_source = context->action == GDK_ACTION_MOVE ? TRUE : FALSE;

	/* Start merge */

	eab_merging_book_add_contact (target_book, merge_context->current_contact,
				      merged_contact_cb, merge_context);

 finish:
	if (path)
		gtk_tree_path_free (path);
	if (target)
		g_object_unref (target);

	gtk_drag_finish (context, success, merge_context->remove_from_source, time);

	return TRUE;
}

static void
selector_tree_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, gpointer data)
{
	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW (widget), NULL, GTK_TREE_VIEW_DROP_BEFORE);
}

static void
addressbook_view_init (AddressbookView *view)
{
	AddressbookViewPrivate *priv;
	GtkWidget *selector_scrolled_window;
	AtkObject *a11y;

	priv->menu = eab_menu_new("org.gnome.evolution.addressbook.view");

	g_signal_connect (priv->folder_view_control, "activate",
			  G_CALLBACK (control_activate_cb), view);

	g_signal_connect  (priv->selector, "drag-motion", G_CALLBACK (selector_tree_drag_motion), view);
	g_signal_connect  (priv->selector, "drag-leave", G_CALLBACK (selector_tree_drag_leave), view);
	g_signal_connect  (priv->selector, "drag-drop", G_CALLBACK (selector_tree_drag_drop), view);
	g_signal_connect  (priv->selector, "drag-data-received", G_CALLBACK (selector_tree_drag_data_received), view);
	gtk_drag_dest_set (priv->selector, GTK_DEST_DEFAULT_ALL, drag_types, num_drag_types, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect_object (priv->selector, "primary_selection_changed",
				 G_CALLBACK (primary_source_selection_changed_callback),
				 G_OBJECT (view), 0);

	load_uri_for_selection (E_SOURCE_SELECTOR (priv->selector), view, TRUE);
}

static void
destroy_editor (char *key,
		gpointer value,
		gpointer nada)
{
	EditorUidClosure *closure = value;

	g_object_weak_unref (G_OBJECT (closure->editor),
			     editor_weak_notify, closure);

	gtk_widget_destroy (GTK_WIDGET (closure->editor));
}

typedef struct {
	EABView *view;
	ESource *source;
} BookOpenData;

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	BookOpenData *data = closure;
	EABView *view = data->view;
	ESource *source = data->source;

	g_free (data);

	/* we always set the "source" property on the EABView, since
	   we use it to reload a previously failed book. */
	g_object_set(view,
		     "source", source,
		     NULL);

	if (status == E_BOOK_ERROR_OK) {
		g_object_set(view,
			     "book", book,
			     NULL);

		if (view->model)
			eab_model_force_folder_bar_message (view->model);
	}
	else if (status != E_BOOK_ERROR_CANCELLED) {
		eab_load_error_dialog (NULL /* XXX */, source, status);
	}


	g_object_unref (source);
}

static void
activate_source (AddressbookView *view,
		 ESource *source)
{
	AddressbookViewPrivate *priv = view->priv;
	const char *uid;
	GtkWidget *uid_view;
	EBook *book;
	BookOpenData *data;

	uid = e_source_peek_uid (source);
	uid_view = g_hash_table_lookup (priv->uid_to_view, uid);

	if (uid_view) {
		/* there is a view for this uid.  make
		   sure that the view actually
		   contains an EBook (if it doesn't
		   contain an EBook a previous load
		   failed.  try to load it again */
		g_object_get (uid_view,
			      "book", &book,
			      NULL);

		if (book) {
			g_object_unref (book);
		}
		else {
			g_object_get (uid_view,
				      "source", &source,
				      NULL);

			/* source can be NULL here, if
			   a previous load hasn't
			   actually made it to
			   book_open_cb yet. */
			if (source) {
				book = e_book_new (source, NULL);

				if (!book) {
					g_object_unref (source);
				}
				else {
					data = g_new (BookOpenData, 1);
					data->view = g_object_ref (uid_view);
					data->source = source; /* transfer the ref we get back from g_object_get */

					addressbook_load (book, book_open_cb, data);
				}
			}
		}
	}
	else {
		/* we don't have a view for this uid already
		   set up. */
		GtkWidget *label = gtk_label_new (uid);
		GError *error = NULL;

		uid_view = eab_view_new ();

		gtk_widget_show (uid_view);
		gtk_widget_show (label);

		g_object_set (uid_view, "type", EAB_VIEW_TABLE, NULL);

		gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
					  uid_view,
					  label);

		g_hash_table_insert (priv->uid_to_view, g_strdup (uid), uid_view);

		g_signal_connect (uid_view, "status_message",
				  G_CALLBACK(set_status_message), view);

		g_signal_connect (uid_view, "search_result",
				  G_CALLBACK(search_result), view);

		g_signal_connect (uid_view, "folder_bar_message",
				  G_CALLBACK(set_folder_bar_message), view);

		g_signal_connect (uid_view, "command_state_change",
				  G_CALLBACK(update_command_state), view);

		book = e_book_new (source, &error);

		if (book) {
			data = g_new (BookOpenData, 1);
			data->view = g_object_ref (uid_view);
			data->source = g_object_ref (source);

			addressbook_load (book, book_open_cb, data);
		}
		else {
			g_warning ("error loading addressbook : %s", error->message);
			g_error_free (error);
		}
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
				       gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
							      uid_view));

	if (EAB_VIEW (uid_view)->model)
		eab_model_force_folder_bar_message (EAB_VIEW (uid_view)->model);

	/* change menus/toolbars to reflect the new view, assuming we are already displayed */
	if (bonobo_ui_component_get_container (bonobo_control_get_ui_component (priv->folder_view_control)) != CORBA_OBJECT_NIL) {
		eab_view_setup_menus (EAB_VIEW (uid_view), bonobo_control_get_ui_component (priv->folder_view_control));
		update_command_state (EAB_VIEW (uid_view), view);
	}
}

void
addressbook_view_edit_contact (AddressbookView* view,
			       const char* source_uid,
			       const char* contact_uid)
{
	AddressbookViewPrivate *priv = view->priv;

	ESource* source = NULL;
	EContact* contact = NULL;
	EBook* book = NULL;

	if (!source_uid || !contact_uid)
		return;

	source = e_source_list_peek_source_by_uid (priv->source_list, source_uid);
	if (!source)
		return;

	/* FIXME: Can I unref this book? */
	book = e_book_new (source, NULL);
	if (!book)
		return;

	if (!e_book_open (book, TRUE, NULL)) {
		g_object_unref (book);
		return;
	}

	e_book_get_contact (book, contact_uid, &contact, NULL);

	if (!contact) {
		g_object_unref (book);
		return;
	}
	eab_show_contact_editor (book, contact, FALSE, FALSE);
	g_object_unref (contact);
	g_object_unref (book);
}
