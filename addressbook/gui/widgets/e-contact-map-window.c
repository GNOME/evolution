/*
 * e-contact-map-window.c
 *
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
 * Copyright (C) 2011 Dan Vratil <dvratil@redhat.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WITH_CONTACT_MAPS

#include "e-contact-map.h"
#include "e-contact-map-window.h"
#include "e-contact-marker.h"

#include <champlain/champlain.h>

#include <string.h>

#include <glib/gi18n.h>
#include <glib-object.h>

#define E_CONTACT_MAP_WINDOW_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE \
        ((obj), E_TYPE_CONTACT_MAP_WINDOW, EContactMapWindowPrivate))

G_DEFINE_TYPE (EContactMapWindow, e_contact_map_window, GTK_TYPE_WINDOW)

struct _EContactMapWindowPrivate {
	EContactMap *map;

	GtkWidget *zoom_in_btn;
	GtkWidget *zoom_out_btn;

	GtkWidget *search_entry;
	GtkListStore *completion_model;

	GHashTable *hash_table;		/* Hash table contact-name -> marker */

	GtkWidget *spinner;
	gint tasks_cnt;
};

enum {
	SHOW_CONTACT_EDITOR,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = {0};

static void
marker_doubleclick_cb (ClutterActor *actor,
                       gpointer user_data)
{
	EContactMapWindow *window = user_data;
	EContactMarker *marker;
	const gchar *contact_uid;

	marker = E_CONTACT_MARKER (actor);
	contact_uid = e_contact_marker_get_contact_uid (marker);

	g_signal_emit (window, signals[SHOW_CONTACT_EDITOR], 0, contact_uid);
}

static void
book_contacts_received_cb (GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
	EContactMapWindow *window = user_data;
	EBookClient *client = E_BOOK_CLIENT (source_object);
	GSList *contacts = NULL, *p;
	GError *error = NULL;

	if (!e_book_client_get_contacts_finish (client, result, &contacts, &error))
		contacts = NULL;

	if (error != NULL) {
		g_warning (
			"%s: Failed to get contacts: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}

	for (p = contacts; p; p = p->next)
		e_contact_map_add_contact (
			window->priv->map, (EContact *) p->data);

	g_slist_free_full (contacts, (GDestroyNotify) g_object_unref);
	g_object_unref (client);
}

static void
contact_map_window_zoom_in_cb (GtkButton *button,
                               gpointer user_data)
{
	EContactMapWindow *window = user_data;
	ChamplainView *view;

	view = e_contact_map_get_view (window->priv->map);

	champlain_view_zoom_in (view);
}

static void
contact_map_window_zoom_out_cb (GtkButton *button,
                                gpointer user_data)
{
	EContactMapWindow *window = user_data;
	ChamplainView *view;

	view = e_contact_map_get_view (window->priv->map);

	champlain_view_zoom_out (view);
}
static void
zoom_level_changed_cb (ChamplainView *view,
                       GParamSpec *pspec,
                       gpointer user_data)
{
	EContactMapWindow *window = user_data;
	gint zoom_level = champlain_view_get_zoom_level (view);

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
map_contact_added_cb (EContactMap *map,
                      ClutterActor *marker,
                      gpointer user_data)
{
	EContactMapWindowPrivate *priv = E_CONTACT_MAP_WINDOW (user_data)->priv;
	const gchar *name;
	GtkTreeIter iter;

	name = champlain_label_get_text (CHAMPLAIN_LABEL (marker));

	g_hash_table_insert (
		priv->hash_table,
		g_strdup (name), marker);

	gtk_list_store_append (priv->completion_model, &iter);
	gtk_list_store_set (
		priv->completion_model, &iter,
		0, name, -1);

	g_signal_connect (
		marker, "double-clicked",
		G_CALLBACK (marker_doubleclick_cb), user_data);

	priv->tasks_cnt--;
	if (priv->tasks_cnt == 0) {
		gtk_spinner_stop (GTK_SPINNER (priv->spinner));
		gtk_widget_hide (priv->spinner);
	}
}

static void
map_contact_removed_cb (EContactMap *map,
                        const gchar *name,
                        gpointer user_data)
{
	EContactMapWindowPrivate *priv = E_CONTACT_MAP_WINDOW (user_data)->priv;
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL (priv->completion_model);

	g_hash_table_remove (priv->hash_table, name);

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gchar *name_str;
			gtk_tree_model_get (model, &iter, 0, &name_str, -1);
			if (g_ascii_strcasecmp (name_str, name) == 0) {
				g_free (name_str);
				gtk_list_store_remove (priv->completion_model, &iter);
				break;
			}
			g_free (name_str);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

static void
map_contact_geocoding_started_cb (EContactMap *map,
                                  ClutterActor *marker,
                                  gpointer user_data)
{
	EContactMapWindowPrivate *priv = E_CONTACT_MAP_WINDOW (user_data)->priv;

	gtk_spinner_start (GTK_SPINNER (priv->spinner));
	gtk_widget_show (priv->spinner);

	priv->tasks_cnt++;
}

static void
map_contact_geocoding_failed_cb (EContactMap *map,
                                 const gchar *name,
                                 gpointer user_data)
{
	EContactMapWindowPrivate *priv = E_CONTACT_MAP_WINDOW (user_data)->priv;

	priv->tasks_cnt--;

	if (priv->tasks_cnt == 0) {
		gtk_spinner_stop (GTK_SPINNER (priv->spinner));
		gtk_widget_hide (priv->spinner);
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
	if (event->keyval == GDK_KEY_Return)
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
contact_map_window_finalize (GObject *object)
{
	EContactMapWindowPrivate *priv;

	priv = E_CONTACT_MAP_WINDOW (object)->priv;

	if (priv->hash_table) {
		g_hash_table_destroy (priv->hash_table);
		priv->hash_table = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_contact_map_window_parent_class)->finalize (object);
}

static void
contact_map_window_dispose (GObject *object)
{
	EContactMapWindowPrivate *priv;

	priv = E_CONTACT_MAP_WINDOW (object)->priv;

	if (priv->map) {
		gtk_widget_destroy (GTK_WIDGET (priv->map));
		priv->map = NULL;
	}

	if (priv->completion_model) {
		g_object_unref (priv->completion_model);
		priv->completion_model = NULL;
	}

	G_OBJECT_CLASS (e_contact_map_window_parent_class)->dispose (object);
}

static void
e_contact_map_window_class_init (EContactMapWindowClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EContactMapWindowPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = contact_map_window_finalize;
	object_class->dispose = contact_map_window_dispose;

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
	EContactMapWindowPrivate *priv;
	GtkWidget *map;
	GtkWidget *button, *entry;
	GtkWidget *hbox, *vbox, *viewport;
	GtkEntryCompletion *entry_completion;
	GtkListStore *completion_model;
	ChamplainView *view;
	GHashTable *hash_table;

	priv = E_CONTACT_MAP_WINDOW_GET_PRIVATE (window);
	window->priv = priv;

	priv->tasks_cnt = 0;

	hash_table = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);
	priv->hash_table = hash_table;

	gtk_window_set_title (GTK_WINDOW (window), _("Contacts Map"));
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);
	gtk_widget_set_size_request (GTK_WIDGET (window), 800, 600);

	/* The map view itself */
	map = e_contact_map_new ();
	view = e_contact_map_get_view (E_CONTACT_MAP (map));
	champlain_view_set_zoom_level (view, 2);
	priv->map = E_CONTACT_MAP (map);
	g_signal_connect (
		view, "notify::zoom-level",
		G_CALLBACK (zoom_level_changed_cb), window);
	g_signal_connect (
		map, "contact-added",
		G_CALLBACK (map_contact_added_cb), window);
	g_signal_connect (
		map, "contact-removed",
		G_CALLBACK (map_contact_removed_cb), window);
	g_signal_connect (
		map, "geocoding-started",
		G_CALLBACK (map_contact_geocoding_started_cb), window);
	g_signal_connect (
		map, "geocoding-failed",
		G_CALLBACK (map_contact_geocoding_failed_cb), window);

	/* HBox container */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 7);

	/* Spinner */
	button = gtk_spinner_new ();
	gtk_container_add (GTK_CONTAINER (hbox), button);
	gtk_widget_hide (button);
	priv->spinner = button;

	/* Zoom-in button */
	button = gtk_button_new_from_stock (GTK_STOCK_ZOOM_IN);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (contact_map_window_zoom_in_cb), window);
	priv->zoom_in_btn = button;
	gtk_container_add (GTK_CONTAINER (hbox), button);

	/* Zoom-out button */
	button = gtk_button_new_from_stock (GTK_STOCK_ZOOM_OUT);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (contact_map_window_zoom_out_cb), window);
	priv->zoom_out_btn = button;
	gtk_container_add (GTK_CONTAINER (hbox), button);

	/* Completion model */
	completion_model = gtk_list_store_new (1, G_TYPE_STRING);
	priv->completion_model = completion_model;

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
	button = gtk_button_new_from_stock (GTK_STOCK_FIND);
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
	gtk_widget_hide (priv->spinner);
}

EContactMapWindow *
e_contact_map_window_new (void)
{
	return g_object_new (
		E_TYPE_CONTACT_MAP_WINDOW, NULL);
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

	/* Reference book, so that it does not get deleted before the callback is
	 * involved. The book is unrefed in the callback */
	g_object_ref (book_client);

	book_query = e_book_query_field_exists (E_CONTACT_ADDRESS);
	query_string = e_book_query_to_string (book_query);
	e_book_query_unref (book_query);

	e_book_client_get_contacts (
		book_client, query_string, NULL,
		book_contacts_received_cb, map);

	g_free (query_string);
}

EContactMap *
e_contact_map_window_get_map (EContactMapWindow *window)
{
	g_return_val_if_fail (E_IS_CONTACT_MAP_WINDOW (window), NULL);

	return window->priv->map;
}

#endif /* WITH_CONTACT_MAPS */
