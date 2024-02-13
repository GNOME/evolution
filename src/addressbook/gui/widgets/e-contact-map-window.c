/*
 * e-contact-map-window.c
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
 * Copyright (C) 2011 Dan Vratil <dvratil@redhat.com>
 *
 */

#include "evolution-config.h"

#ifdef ENABLE_CONTACT_MAPS

#include "e-contact-map-window.h"

#include <string.h>
#include <glib/gi18n.h>

#include <champlain/champlain.h>

#include <e-util/e-util.h>

#include "e-contact-map.h"

struct _EContactMapWindowPrivate {
	EContactMap *map;

	GtkWidget *zoom_in_btn;
	GtkWidget *zoom_out_btn;

	GtkWidget *search_entry;
	GtkListStore *completion_model;

	/* contact name -> marker */
	GHashTable *hash_table;

	GtkWidget *spinner;
	gint tasks_cnt;
};

enum {
	SHOW_CONTACT_EDITOR,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EContactMapWindow, e_contact_map_window, GTK_TYPE_WINDOW)

static gboolean
contact_map_marker_button_release_event_cb (ClutterActor *actor,
                                            ClutterEvent *event,
                                            EContactMapWindow *window)
{
	const gchar *contact_uid;

	if (clutter_event_get_click_count (event) != 2)
		return FALSE;

	contact_uid = g_object_get_data (G_OBJECT (actor), "contact-uid");
	g_return_val_if_fail (contact_uid != NULL, FALSE);

	g_signal_emit (
		window,
		signals[SHOW_CONTACT_EDITOR], 0,
		contact_uid);

	return TRUE;
}

static void
contact_map_window_get_contacts_cb (GObject *source_object,
                                    GAsyncResult *result,
                                    gpointer user_data)
{
	EContactMapWindow *window;
	GSList *list = NULL, *link;
	GError *local_error = NULL;

	window = E_CONTACT_MAP_WINDOW (user_data);

	e_book_client_get_contacts_finish (
		E_BOOK_CLIENT (source_object),
		result, &list, &local_error);

	if (local_error != NULL) {
		g_warning (
			"%s: Failed to get contacts: %s",
			G_STRFUNC, local_error->message);
		g_error_free (local_error);
	}

	for (link = list; link != NULL; link = g_slist_next (link)) {
		EContact *contact = E_CONTACT (link->data);
		e_contact_map_add_contact (window->priv->map, contact);
	}

	g_slist_free_full (list, (GDestroyNotify) g_object_unref);
}

static void
contact_map_window_zoom_in_cb (GtkButton *button,
                               EContactMapWindow *window)
{
	ChamplainView *view;

	view = e_contact_map_get_view (window->priv->map);

	champlain_view_zoom_in (view);
}

static void
contact_map_window_zoom_out_cb (GtkButton *button,
                                EContactMapWindow *window)
{
	ChamplainView *view;

	view = e_contact_map_get_view (window->priv->map);

	champlain_view_zoom_out (view);
}

static void
contact_map_window_zoom_level_changed_cb (ChamplainView *view,
                                          GParamSpec *pspec,
                                          EContactMapWindow *window)
{
	gint zoom_level;

	zoom_level = champlain_view_get_zoom_level (view);

	gtk_widget_set_sensitive (
		window->priv->zoom_in_btn,
		(zoom_level < champlain_view_get_max_zoom_level (view)));

	gtk_widget_set_sensitive (
		window->priv->zoom_out_btn,
		(zoom_level > champlain_view_get_min_zoom_level (view)));
}

/**
 * Add contact to hash_table only when EContactMap tells us
 * that the contact has really been added to map.
 */
static void
contact_map_window_contact_added_cb (EContactMap *map,
                                     ClutterActor *marker,
                                     EContactMapWindow *window)
{
	GtkListStore *list_store;
	GtkTreeIter iter;
	const gchar *name;

	name = champlain_label_get_text (CHAMPLAIN_LABEL (marker));

	g_hash_table_insert (
		window->priv->hash_table,
		g_strdup (name), marker);

	list_store = window->priv->completion_model;
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter, 0, name, -1);

	g_signal_connect (
		marker, "button-release-event",
		G_CALLBACK (contact_map_marker_button_release_event_cb),
		window);

	window->priv->tasks_cnt--;
	if (window->priv->tasks_cnt == 0) {
		e_spinner_stop (E_SPINNER (window->priv->spinner));
		gtk_widget_hide (window->priv->spinner);
	}
}

static void
contact_map_window_contact_removed_cb (EContactMap *map,
                                       const gchar *name,
                                       EContactMapWindow *window)
{
	GtkListStore *list_store;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean iter_valid;

	list_store = window->priv->completion_model;
	tree_model = GTK_TREE_MODEL (list_store);

	g_hash_table_remove (window->priv->hash_table, name);

	iter_valid = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (iter_valid) {
		gchar *name_str;
		gboolean match;

		gtk_tree_model_get (tree_model, &iter, 0, &name_str, -1);
		match = (g_ascii_strcasecmp (name_str, name) == 0);
		g_free (name_str);

		if (match) {
			gtk_list_store_remove (list_store, &iter);
			break;
		}

		iter_valid = gtk_tree_model_iter_next (tree_model, &iter);
	}
}

static void
contact_map_window_geocoding_started_cb (EContactMap *map,
                                         ClutterActor *marker,
                                         EContactMapWindow *window)
{
	e_spinner_start (E_SPINNER (window->priv->spinner));
	gtk_widget_show (window->priv->spinner);

	window->priv->tasks_cnt++;
}

static void
contact_map_window_geocoding_failed_cb (EContactMap *map,
                                        const gchar *name,
                                        EContactMapWindow *window)
{
	window->priv->tasks_cnt--;

	if (window->priv->tasks_cnt == 0) {
		e_spinner_stop (E_SPINNER (window->priv->spinner));
		gtk_widget_hide (window->priv->spinner);
	}
}

static void
contact_map_window_find_contact_cb (GtkButton *button,
                                    gpointer user_data)
{
	EContactMapWindowPrivate *priv = E_CONTACT_MAP_WINDOW (user_data)->priv;
	ClutterActor *marker;

	marker = g_hash_table_lookup (
		priv->hash_table,
		gtk_entry_get_text (GTK_ENTRY (priv->search_entry)));

	if (marker)
		e_contact_map_zoom_on_marker (priv->map, marker);
}

static gboolean
contact_map_window_entry_key_pressed_cb (GtkWidget *entry,
                                         GdkEventKey *event,
                                         gpointer user_data)
{
	if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)
		contact_map_window_find_contact_cb (NULL, user_data);

	return FALSE;
}

static gboolean
entry_completion_match_selected_cb (GtkEntryCompletion *widget,
                                    GtkTreeModel *model,
                                    GtkTreeIter *iter,
                                    gpointer user_data)
{
	GValue name_val = {0};
	EContactMapWindowPrivate *priv = E_CONTACT_MAP_WINDOW (user_data)->priv;
	const gchar *name;

	gtk_tree_model_get_value (model, iter, 0, &name_val);
	g_return_val_if_fail (G_VALUE_HOLDS_STRING (&name_val), FALSE);

	name = g_value_get_string (&name_val);
	gtk_entry_set_text (GTK_ENTRY (priv->search_entry), name);

	contact_map_window_find_contact_cb (NULL, user_data);

	return TRUE;
}

static void
contact_map_window_dispose (GObject *object)
{
	EContactMapWindow *self = E_CONTACT_MAP_WINDOW (object);

	if (self->priv->map != NULL) {
		gtk_widget_destroy (GTK_WIDGET (self->priv->map));
		self->priv->map = NULL;
	}

	g_clear_object (&self->priv->completion_model);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_contact_map_window_parent_class)->dispose (object);
}

static void
contact_map_window_finalize (GObject *object)
{
	EContactMapWindow *self = E_CONTACT_MAP_WINDOW (object);

	g_hash_table_destroy (self->priv->hash_table);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_contact_map_window_parent_class)->finalize (object);
}

static void
e_contact_map_window_class_init (EContactMapWindowClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = contact_map_window_dispose;
	object_class->finalize = contact_map_window_finalize;

	signals[SHOW_CONTACT_EDITOR] = g_signal_new (
		"show-contact-editor",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EContactMapWindowClass, show_contact_editor),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
e_contact_map_window_init (EContactMapWindow *window)
{
	GtkWidget *map;
	GtkWidget *button, *entry;
	GtkWidget *hbox, *vbox, *viewport;
	GtkEntryCompletion *entry_completion;
	GtkListStore *completion_model;
	ChamplainView *view;
	GHashTable *hash_table;

	window->priv = e_contact_map_window_get_instance_private (window);

	window->priv->tasks_cnt = 0;

	hash_table = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);
	window->priv->hash_table = hash_table;

	gtk_window_set_title (GTK_WINDOW (window), _("Contacts Map"));
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);
	gtk_widget_set_size_request (GTK_WIDGET (window), 800, 600);

	/* The map view itself */
	map = e_contact_map_new ();
	view = e_contact_map_get_view (E_CONTACT_MAP (map));
	champlain_view_set_zoom_level (view, 2);
	window->priv->map = E_CONTACT_MAP (map);
	e_signal_connect_notify (
		view, "notify::zoom-level",
		G_CALLBACK (contact_map_window_zoom_level_changed_cb), window);
	g_signal_connect (
		map, "contact-added",
		G_CALLBACK (contact_map_window_contact_added_cb), window);
	g_signal_connect (
		map, "contact-removed",
		G_CALLBACK (contact_map_window_contact_removed_cb), window);
	g_signal_connect (
		map, "geocoding-started",
		G_CALLBACK (contact_map_window_geocoding_started_cb), window);
	g_signal_connect (
		map, "geocoding-failed",
		G_CALLBACK (contact_map_window_geocoding_failed_cb), window);

	/* HBox container */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 7);

	/* Spinner */
	button = e_spinner_new ();
	gtk_container_add (GTK_CONTAINER (hbox), button);
	gtk_widget_hide (button);
	window->priv->spinner = button;

	/* Zoom-in button */
	button = e_dialog_button_new_with_icon ("zoom-in", _("Zoom _In"));
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (contact_map_window_zoom_in_cb), window);
	window->priv->zoom_in_btn = button;
	gtk_container_add (GTK_CONTAINER (hbox), button);

	/* Zoom-out button */
	button = e_dialog_button_new_with_icon ("zoom-out", _("Zoom _Out"));
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (contact_map_window_zoom_out_cb), window);
	window->priv->zoom_out_btn = button;
	gtk_container_add (GTK_CONTAINER (hbox), button);

	/* Completion model */
	completion_model = gtk_list_store_new (1, G_TYPE_STRING);
	window->priv->completion_model = completion_model;

	/* Entry completion */
	entry_completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_model (
		entry_completion, GTK_TREE_MODEL (completion_model));
	gtk_entry_completion_set_text_column (entry_completion, 0);
	g_signal_connect (
		entry_completion, "match-selected",
		G_CALLBACK (entry_completion_match_selected_cb), window);

	/* Search entry */
	entry = gtk_entry_new ();
	gtk_entry_set_completion (GTK_ENTRY (entry), entry_completion);
	g_signal_connect (
		entry, "key-press-event",
		G_CALLBACK (contact_map_window_entry_key_pressed_cb), window);
	window->priv->search_entry = entry;
	gtk_container_add (GTK_CONTAINER (hbox), entry);

	/* Search button */
	button = e_dialog_button_new_with_icon ("edit-find", _("_Find"));
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (contact_map_window_find_contact_cb), window);
	gtk_container_add (GTK_CONTAINER (hbox), button);

	viewport = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (viewport), map);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add (GTK_CONTAINER (vbox), viewport);
	gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtk_widget_show_all (vbox);
	gtk_widget_hide (window->priv->spinner);
}

EContactMapWindow *
e_contact_map_window_new (void)
{
	return g_object_new (E_TYPE_CONTACT_MAP_WINDOW, NULL);
}

EContactMap *
e_contact_map_window_get_map (EContactMapWindow *window)
{
	g_return_val_if_fail (E_IS_CONTACT_MAP_WINDOW (window), NULL);

	return window->priv->map;
}

/**
 * Gets all contacts from @book and puts them
 * on the map view
 */
void
e_contact_map_window_load_addressbook (EContactMapWindow *map,
                                       EBookClient *book_client)
{
	EBookQuery *book_query;
	gchar *query_string;

	g_return_if_fail (E_IS_CONTACT_MAP_WINDOW (map));
	g_return_if_fail (E_IS_BOOK_CLIENT (book_client));

	book_query = e_book_query_field_exists (E_CONTACT_ADDRESS);
	query_string = e_book_query_to_string (book_query);
	e_book_query_unref (book_query);

	e_book_client_get_contacts (
		book_client, query_string, NULL,
		contact_map_window_get_contacts_cb, map);

	g_free (query_string);
}

#endif /* ENABLE_CONTACT_MAPS */
