/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "util/eab-book-util.h"
#include "e-alphabet-box.h"
#include "e-contact-card-box.h"

#include "e-card-view.h"

struct _ECardViewPrivate {
	EContactCardBox *card_box; /* not owned */
	EAlphabetBox *alphabet_box; /* not owned */
	GtkLabel *empty_label; /* not owned */

	GCancellable *cancellable;
	EBookClient *book_client;
	EBookClientView *book_view;
	gchar *query;
	EBookClientViewSortFields *sort_fields;
	gboolean searching;

	GPtrArray *dnd_indexes; /* guint */
	GPtrArray *dnd_contacts; /* EContact * */

	gulong view_content_changed_id;
	gulong view_progress_id;
	gulong view_notify_n_total_id;
	gulong view_notify_indices_id;
};

enum {
	STATUS_MESSAGE,
	DOUBLE_CLICK,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
G_DEFINE_TYPE_WITH_PRIVATE (ECardView, e_card_view, GTK_TYPE_EVENT_BOX)

enum RefreshFlags {
	REFRESH_FLAG_NONE		= 0,
	REFRESH_FLAG_WITH_SORT		= 1 << 0,
	REFRESH_FLAG_WITH_QUERY		= 1 << 1
};

static void e_card_view_refresh (ECardView *self, enum RefreshFlags flags);
static void e_card_view_take_book_view (ECardView *self, EBookClientView *view);

static void
e_card_view_update_empty_message (ECardView *self)
{
	if (!self->priv->card_box ||
	    !self->priv->alphabet_box ||
	    !self->priv->empty_label)
		return;

	if (e_contact_card_box_get_n_items (self->priv->card_box) > 0) {
		gtk_widget_set_visible (GTK_WIDGET (self->priv->empty_label), FALSE);
		gtk_widget_set_visible (GTK_WIDGET (self->priv->card_box), TRUE);
		gtk_widget_set_visible (GTK_WIDGET (self->priv->alphabet_box), TRUE);
	} else {
		gboolean editable = FALSE, perform_initial_query = FALSE;
		const gchar *message;

		if (self->priv->book_client) {
			EClient *client = E_CLIENT (self->priv->book_client);

			perform_initial_query = !e_client_check_capability (client, "do-initial-query");
			editable = !e_client_is_readonly (client);
		}

		if (self->priv->searching) {
			message = _("Searching for the Contacts…");
		} else if (editable) {
			if (perform_initial_query)
				message = _("Search for the Contact\n\nor double-click here to create a new Contact.");
			else
				message = _("There are no items to show in this view.\n\nDouble-click here to create a new Contact.");
		} else {
			if (perform_initial_query)
				message = _("Search for the Contact.");
			else
				message = _("There are no items to show in this view.");
		}

		gtk_label_set_label (self->priv->empty_label, message);

		gtk_widget_set_visible (GTK_WIDGET (self->priv->card_box), FALSE);
		gtk_widget_set_visible (GTK_WIDGET (self->priv->alphabet_box), FALSE);
		gtk_widget_set_visible (GTK_WIDGET (self->priv->empty_label), TRUE);
	}
}

static void
e_card_view_got_view_cb (GObject *source_object,
			 GAsyncResult *result,
			 gpointer user_data)
{
	ECardView *self = user_data;
	EBookClientView *book_view = NULL;
	GError *error = NULL;

	if (e_book_client_get_view_finish (E_BOOK_CLIENT (source_object), result, &book_view, &error)) {
		e_card_view_take_book_view (self, book_view);
		e_card_view_refresh (self, REFRESH_FLAG_NONE);
	} else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warning ("%s: Failed to get book view: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
}

static void
e_card_view_refresh (ECardView *self,
		     enum RefreshFlags flags)
{
	if (!self->priv->card_box)
		return;

	if (!self->priv->book_client || !self->priv->query) {
		e_contact_card_box_set_n_items (self->priv->card_box, 0);
		self->priv->searching = FALSE;
		e_card_view_update_empty_message (self);
		return;
	}

	if (!self->priv->book_view) {
		self->priv->searching = FALSE;
		e_card_view_update_empty_message (self);
		e_book_client_get_view (self->priv->book_client, self->priv->query,
			self->priv->cancellable, e_card_view_got_view_cb, self);
		return;
	}

	if ((flags & REFRESH_FLAG_WITH_QUERY) != 0) {
		e_card_view_take_book_view (self, NULL);
		e_contact_card_box_set_n_items (self->priv->card_box, 0);

		e_card_view_update_empty_message (self);
		e_book_client_get_view (self->priv->book_client, self->priv->query,
			self->priv->cancellable, e_card_view_got_view_cb, self);
		return;
	}

	if ((flags & REFRESH_FLAG_WITH_SORT) != 0) {
		GError *local_error = NULL;

		if (!e_book_client_view_set_sort_fields_sync (self->priv->book_view, self->priv->sort_fields, self->priv->cancellable, &local_error) &&
		    !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s: Failed to set view sort fields: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");

		g_clear_error (&local_error);
	}

	e_contact_card_box_set_n_items (self->priv->card_box, e_book_client_view_get_n_total (self->priv->book_view));
	e_card_view_update_empty_message (self);
	e_contact_card_box_refresh (self->priv->card_box);
}

static void
e_card_view_view_content_changed_cb (EBookClientView *book_view,
				     gpointer user_data)
{
	ECardView *self = user_data;

	e_card_view_refresh (self, REFRESH_FLAG_NONE);
}

static void
e_card_view_view_progress_cb (EBookClientView *book_view,
			      guint percentage,
			      const gchar *message,
			      gpointer user_data)
{
	ECardView *self = user_data;

	g_signal_emit (self, signals[STATUS_MESSAGE], 0, message, percentage);
}

static void
e_card_view_view_complete_cb (EBookClientView *book_view,
			      const GError *error,
			      gpointer user_data)
{
	ECardView *self = user_data;

	g_signal_emit (self, signals[STATUS_MESSAGE], 0, NULL, -1);

	self->priv->searching = FALSE;

	e_card_view_update_empty_message (self);
}

static void
e_card_view_view_notify_n_total_cb (GObject *book_view,
				    GParamSpec *param,
				    gpointer user_data)
{
	ECardView *self = user_data;

	if (self->priv->card_box)
		e_contact_card_box_set_n_items (self->priv->card_box, e_book_client_view_get_n_total (self->priv->book_view));
	e_card_view_update_empty_message (self);
}

static void
e_card_view_view_notify_indices_cb (GObject *book_view,
				    GParamSpec *param,
				    gpointer user_data)
{
	ECardView *self = user_data;

	if (self->priv->alphabet_box) {
		e_alphabet_box_take_indices (self->priv->alphabet_box,
			e_book_client_view_dup_indices (self->priv->book_view));
	}
}

static void
e_card_view_take_book_view (ECardView *self,
			    EBookClientView *book_view)
{
	if (book_view == self->priv->book_view)
		return;

	self->priv->searching = FALSE;

	if (self->priv->book_view) {
		if (self->priv->view_content_changed_id)
			g_signal_handler_disconnect (self->priv->book_view, self->priv->view_content_changed_id);
		if (self->priv->view_progress_id)
			g_signal_handler_disconnect (self->priv->book_view, self->priv->view_progress_id);
		if (self->priv->view_notify_n_total_id)
			g_signal_handler_disconnect (self->priv->book_view, self->priv->view_notify_n_total_id);
		if (self->priv->view_notify_indices_id)
			g_signal_handler_disconnect (self->priv->book_view, self->priv->view_notify_indices_id);
		self->priv->view_content_changed_id = 0;
		self->priv->view_progress_id = 0;
		self->priv->view_notify_n_total_id = 0;
		self->priv->view_notify_indices_id = 0;
		g_clear_object (&self->priv->book_view);
	}

	if (book_view) {
		GError *local_error = NULL;

		self->priv->book_view = book_view;
		self->priv->searching = TRUE;

		self->priv->view_content_changed_id = g_signal_connect (self->priv->book_view, "content-changed",
			G_CALLBACK (e_card_view_view_content_changed_cb), self);
		self->priv->view_progress_id = g_signal_connect (self->priv->book_view, "progress",
			G_CALLBACK (e_card_view_view_progress_cb), self);
		self->priv->view_progress_id = g_signal_connect (self->priv->book_view, "complete",
			G_CALLBACK (e_card_view_view_complete_cb), self);
		self->priv->view_notify_n_total_id = g_signal_connect (self->priv->book_view, "notify::n-total",
			G_CALLBACK (e_card_view_view_notify_n_total_cb), self);
		self->priv->view_notify_indices_id = g_signal_connect (self->priv->book_view, "notify::indices",
			G_CALLBACK (e_card_view_view_notify_indices_cb), self);

		e_book_client_view_set_flags (self->priv->book_view, E_BOOK_CLIENT_VIEW_FLAGS_MANUAL_QUERY, NULL);

		if (!e_book_client_view_set_sort_fields_sync (self->priv->book_view, self->priv->sort_fields, self->priv->cancellable, &local_error) &&
		    !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s: Failed to set view sort fields: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");

		g_clear_error (&local_error);

		e_book_client_view_start (self->priv->book_view, NULL);

		if (self->priv->alphabet_box) {
			e_alphabet_box_take_indices (self->priv->alphabet_box,
				e_book_client_view_dup_indices (self->priv->book_view));
		}
	} else if (self->priv->alphabet_box) {
		e_alphabet_box_take_indices (self->priv->alphabet_box, NULL);
	}

	e_card_view_update_empty_message (self);
}

static void
e_card_view_got_items_cb (GObject *source_object,
			  GAsyncResult *result,
			  gpointer user_data)
{
	GTask *task = user_data;
	GPtrArray *contacts = NULL;
	GError *error = NULL;

	if (e_book_client_view_dup_contacts_finish (E_BOOK_CLIENT_VIEW (source_object), result, NULL, &contacts, &error)) {
		g_task_return_pointer (task, contacts, (GDestroyNotify) g_ptr_array_unref);
	} else {
		g_task_return_error (task, error);
	}

	g_clear_object (&task);
}

static void
e_card_view_get_items_cb (gpointer source_data,
			  guint range_start,
			  guint range_length,
			  GCancellable *cancellable,
			  GAsyncReadyCallback callback,
			  gpointer callback_data)
{
	ECardView *self = source_data;
	GTask *task;

	g_return_if_fail (E_IS_CARD_VIEW (self));
	g_warn_if_fail (self->priv->book_client != NULL);
	g_warn_if_fail (self->priv->book_view != NULL);

	task = g_task_new (self, cancellable, callback, callback_data);
	g_task_set_source_tag (task, e_card_view_get_items_cb);

	e_book_client_view_dup_contacts (self->priv->book_view, range_start, range_length, cancellable, e_card_view_got_items_cb, task);
}

static GPtrArray *
e_card_view_get_items_finish_cb (gpointer source_data,
				 GAsyncResult *result,
				 GError **error)
{
	ECardView *self = source_data;

	g_return_val_if_fail (E_IS_CARD_VIEW (self), NULL);
	g_return_val_if_fail (g_task_is_valid (G_TASK (result), self) , NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

static void
e_card_view_alphabet_clicked_cb (EAlphabetBox *alphabet_box,
				 guint index,
				 gpointer user_data)
{
	ECardView *self = user_data;

	if (!self->priv->book_view)
		return;

	if (index < e_contact_card_box_get_n_items (self->priv->card_box)) {
		e_contact_card_box_scroll_to_index (self->priv->card_box, index, FALSE);
		e_contact_card_box_set_focused_index (self->priv->card_box, index);
		e_contact_card_box_set_selected_all (self->priv->card_box, FALSE);
		e_contact_card_box_set_selected (self->priv->card_box, index, TRUE);
	}
}

static gboolean
e_card_view_button_press_event_cb (GtkWidget *widget,
				   GdkEvent *event,
				   gpointer user_data)
{
	ECardView *self = E_CARD_VIEW (widget);

	if (event->type == GDK_2BUTTON_PRESS && event->button.button == GDK_BUTTON_PRIMARY) {
		g_signal_emit (self, signals[DOUBLE_CLICK], 0, NULL);

		return TRUE;
	} else if (event->type == GDK_BUTTON_PRESS && event->button.button == GDK_BUTTON_SECONDARY) {
		gboolean ret = FALSE;

		g_signal_emit_by_name (self, "popup-menu", &ret);

		return ret;
	}

	return FALSE;
}

static void
e_card_view_dnd_contacts_received_cb (GObject *source_object,
				      GAsyncResult *result,
				      gpointer user_data)
{
	ECardView *self = user_data;
	GPtrArray *contacts;
	GError *error = NULL;

	contacts = e_contact_card_box_dup_contacts_finish (E_CONTACT_CARD_BOX (source_object), result, &error);
	if (contacts) {
		if (self->priv->dnd_indexes) {
			g_clear_pointer (&self->priv->dnd_contacts, g_ptr_array_unref);
			self->priv->dnd_contacts = contacts;
		} else {
			g_clear_pointer (&contacts, g_ptr_array_unref);
		}
	} else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warning ("%s: Failed to receive DND contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
}

static void
e_card_view_card_drag_begin_cb (GtkWidget *widget,
				GdkDragContext *context,
				gpointer user_data)
{
	ECardView *self = E_CARD_VIEW (user_data);

	g_clear_pointer (&self->priv->dnd_indexes, g_ptr_array_unref);
	g_clear_pointer (&self->priv->dnd_contacts, g_ptr_array_unref);

	self->priv->dnd_indexes = e_contact_card_box_dup_selected_indexes (self->priv->card_box);
	if (!self->priv->dnd_indexes) {
		gtk_drag_cancel (context);
		return;
	}

	self->priv->dnd_contacts = e_contact_card_box_peek_contacts (self->priv->card_box, self->priv->dnd_indexes);

	if (!self->priv->dnd_contacts) {
		/* Okay, this is bad, there are selected cards, which do not have set
		   their corresponding contacts yet. Read the contacts proactively,
		   they will be received hopefully before the data is requested. */
		e_contact_card_box_dup_contacts	(self->priv->card_box, self->priv->dnd_indexes, self->priv->cancellable,
			e_card_view_dnd_contacts_received_cb, self);
	}

	gtk_drag_set_icon_default (context);
}

static void
e_card_view_card_drag_data_get_cb (GtkWidget *widget,
				   GdkDragContext *context,
				   GtkSelectionData *selection_data,
				   guint info,
				   guint time,
				   gpointer user_data)
{
	ECardView *self = E_CARD_VIEW (user_data);
	gchar *value = NULL;

	if (!self->priv->dnd_contacts) {
		g_warning ("%s: Failed to read contacts before the drag operation finished; repeat the action later", G_STRFUNC);
		gtk_drag_cancel (context);
		return;
	}

	switch (info) {
	case E_CONTACT_CARD_BOX_DND_TYPE_SOURCE_VCARD_LIST:
		value = eab_book_and_contact_array_to_string (self->priv->book_client, self->priv->dnd_contacts);
		break;
	case E_CONTACT_CARD_BOX_DND_TYPE_VCARD_LIST:
		value = eab_contact_array_to_string (self->priv->dnd_contacts);
		break;
	}

	if (value) {
		GdkAtom target;

		target = gtk_selection_data_get_target (selection_data);

		gtk_selection_data_set (selection_data, target, 8, (guchar *) value, strlen (value));

		g_free (value);
	}
}

static void
e_card_view_card_drag_end_cb (GtkWidget *widget,
			      GdkDragContext *context,
			      gpointer user_data)
{
	ECardView *self = E_CARD_VIEW (user_data);

	g_clear_pointer (&self->priv->dnd_indexes, g_ptr_array_unref);
	g_clear_pointer (&self->priv->dnd_contacts, g_ptr_array_unref);
}

static void
e_card_view_constructed (GObject *object)
{
	ECardView *self = E_CARD_VIEW (object);
	GtkBox *box;
	GtkWidget *widget, *scrolled;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_card_view_parent_class)->constructed (object);

	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), GTK_STYLE_CLASS_VIEW);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);
	gtk_container_add (GTK_CONTAINER (self), widget);

	box = GTK_BOX (widget);

	widget = e_contact_card_box_new (e_card_view_get_items_cb, e_card_view_get_items_finish_cb, self, NULL);
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);
	gtk_box_pack_start (box, widget, TRUE, TRUE, 0);

	self->priv->card_box = E_CONTACT_CARD_BOX (widget);

	widget = e_alphabet_box_new ();
	g_object_set (widget,
		"visible", TRUE,
		"margin-start", 4,
		NULL);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (scrolled,
		"visible", TRUE,
		"hscrollbar-policy", GTK_POLICY_NEVER,
		"overlay-scrolling", FALSE,
		"vexpand", TRUE,
		NULL);
	gtk_container_add (GTK_CONTAINER (scrolled), widget);

	gtk_box_pack_start (box, scrolled, FALSE, FALSE, 0);

	self->priv->alphabet_box = E_ALPHABET_BOX (widget);

	g_signal_connect (self->priv->alphabet_box, "clicked",
		G_CALLBACK (e_card_view_alphabet_clicked_cb), self);

	widget = gtk_label_new ("");
	g_object_set (widget,
		"visible", FALSE,
		"halign", GTK_ALIGN_CENTER,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_CENTER,
		"vexpand", TRUE,
		"margin", 24,
		NULL);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	self->priv->empty_label = GTK_LABEL (widget);

	g_signal_connect (self, "button-press-event",
		G_CALLBACK (e_card_view_button_press_event_cb), NULL);
	g_signal_connect_object (self->priv->card_box, "card-drag-begin",
		G_CALLBACK (e_card_view_card_drag_begin_cb), self, 0);
	g_signal_connect_object (self->priv->card_box, "card-drag-data-get",
		G_CALLBACK (e_card_view_card_drag_data_get_cb), self, 0);
	g_signal_connect_object (self->priv->card_box, "card-drag-end",
		G_CALLBACK (e_card_view_card_drag_end_cb), self, 0);
}

static void
e_card_view_dispose (GObject *object)
{
	ECardView *self = E_CARD_VIEW (object);

	self->priv->card_box = NULL;
	self->priv->alphabet_box = NULL;
	self->priv->empty_label = NULL;

	g_cancellable_cancel (self->priv->cancellable);

	e_card_view_take_book_view (self, NULL);

	g_clear_object (&self->priv->cancellable);
	g_clear_object (&self->priv->book_client);
	g_clear_pointer (&self->priv->dnd_indexes, g_ptr_array_unref);
	g_clear_pointer (&self->priv->dnd_contacts, g_ptr_array_unref);
	g_clear_pointer (&self->priv->query, g_free);
	g_clear_pointer (&self->priv->sort_fields, e_book_client_view_sort_fields_free);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_card_view_parent_class)->dispose (object);
}

static void
e_card_view_class_init (ECardViewClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_card_view_constructed;
	object_class->dispose = e_card_view_dispose;

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECardViewClass, status_message),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 2,
		G_TYPE_STRING,
		G_TYPE_INT);

	signals[DOUBLE_CLICK] = g_signal_new (
		"double-click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECardViewClass, double_click),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);
}

static void
e_card_view_init (ECardView *self)
{
	EBookClientViewSortFields tmp_fields[] = {
		{ E_CONTACT_FILE_AS, E_BOOK_CURSOR_SORT_ASCENDING },
		{ E_CONTACT_FIELD_LAST, E_BOOK_CURSOR_SORT_ASCENDING }
	};

	self->priv = e_card_view_get_instance_private (self);
	self->priv->cancellable = g_cancellable_new ();

	e_card_view_set_sort_fields (self, tmp_fields);
}

GtkWidget *
e_card_view_new (void)
{
	return g_object_new (E_TYPE_CARD_VIEW, NULL);
}

EContactCardBox *
e_card_view_get_card_box (ECardView *self)
{
	g_return_val_if_fail (E_IS_CARD_VIEW (self), NULL);

	return self->priv->card_box;
}

EAlphabetBox *
e_card_view_get_alphabet_box (ECardView *self)
{
	g_return_val_if_fail (E_IS_CARD_VIEW (self), NULL);

	return self->priv->alphabet_box;
}

EBookClient *
e_card_view_get_book_client (ECardView *self)
{
	g_return_val_if_fail (E_IS_CARD_VIEW (self), NULL);

	return self->priv->book_client;
}

void
e_card_view_set_book_client (ECardView *self,
			     EBookClient *book_client)
{
	g_return_if_fail (E_IS_CARD_VIEW (self));
	if (book_client)
		g_return_if_fail (E_IS_BOOK_CLIENT (book_client));

	if (self->priv->book_client == book_client)
		return;

	g_clear_object (&self->priv->book_client);
	self->priv->book_client = book_client ? g_object_ref (book_client) : NULL;

	e_card_view_take_book_view (self, NULL);

	e_card_view_refresh (self, REFRESH_FLAG_NONE);
}

const gchar *
e_card_view_get_query (ECardView *self)
{
	g_return_val_if_fail (E_IS_CARD_VIEW (self), NULL);

	return self->priv->query;
}

void
e_card_view_set_query (ECardView *self,
		       const gchar *query)
{
	g_return_if_fail (E_IS_CARD_VIEW (self));

	if (g_strcmp0 (self->priv->query, query) == 0)
		return;

	g_free (self->priv->query);
	self->priv->query = g_strdup (query);

	e_card_view_refresh (self, REFRESH_FLAG_WITH_QUERY);
}

void
e_card_view_set_sort_fields (ECardView *self,
			     const EBookClientViewSortFields *sort_fields)
{
	guint ii;

	g_return_if_fail (E_IS_CARD_VIEW (self));

	if (sort_fields == self->priv->sort_fields)
		return;

	if (sort_fields && self->priv->sort_fields) {
		for (ii = 0; sort_fields[ii].field != E_CONTACT_FIELD_LAST && self->priv->sort_fields[ii].field != E_CONTACT_FIELD_LAST; ii++) {
			if (sort_fields[ii].field != self->priv->sort_fields[ii].field ||
			    sort_fields[ii].sort_type != self->priv->sort_fields[ii].sort_type)
				break;
		}

		/* All had been read, thus the sort options are the same */
		if (sort_fields[ii].field == E_CONTACT_FIELD_LAST &&
		    self->priv->sort_fields[ii].field == E_CONTACT_FIELD_LAST)
			return;
	}

	e_book_client_view_sort_fields_free (self->priv->sort_fields);
	self->priv->sort_fields = e_book_client_view_sort_fields_copy (sort_fields);

	e_card_view_refresh (self, REFRESH_FLAG_WITH_SORT);
}

EBookClientViewSortFields *
e_card_view_dup_sort_fields (ECardView *self)
{
	g_return_val_if_fail (E_IS_CARD_VIEW (self), NULL);

	return e_book_client_view_sort_fields_copy (self->priv->sort_fields);
}
