/* test-source-selector.c - Test program for the ESourceSelector widget.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 */

#include <e-util/e-util.h>

#define OPENED_KEY "sources-opened-key"
#define VIEW_KEY "sources-view-key"
#define SOURCE_TYPE_KEY "sources-source-type-key"
#define EXTENSION_NAME_KEY "sources-extension-name-key"
#define TOOLTIP_ENTRY_KEY "sources-tooltip-entry-key"

static void
dump_selection (ESourceSelector *selector,
                const gchar *extension_name)
{
	GList *list, *link;

	list = e_source_selector_get_selection (selector);

	g_print ("Current selection at %s:\n", extension_name);

	if (list == NULL)
		g_print ("\t(None)\n");

	for (link = list; link != NULL; link = g_list_next (link->next)) {
		ESource *source = E_SOURCE (link->data);
		ESourceBackend *extension;

		extension = e_source_get_extension (source, extension_name);

		g_print (
			"\tSource %s (backend %s)\n",
			e_source_get_display_name (source),
			e_source_backend_get_backend_name (extension));
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

static void
selection_changed_callback (ESourceSelector *selector)
{
	const gchar *extension_name;

	g_print ("Selection changed!\n");

	extension_name = g_object_get_data (
		G_OBJECT (selector), EXTENSION_NAME_KEY);
	dump_selection (selector, extension_name);
}

static void
unset_view_and_unref_client (gpointer ptr)
{
	EClient *client = ptr;

	g_return_if_fail (E_IS_CLIENT (client));

	/* To have it free the view and unref its own reference to 'client' */
	g_object_set_data (G_OBJECT (client), VIEW_KEY, NULL);

	g_object_unref (client);
}

static void
enable_widget_if_opened_cb (ESourceSelector *selector,
                            GtkWidget *widget)
{
	GHashTable *opened_sources;
	ESource *source;
	gboolean sensitive = FALSE;

	opened_sources = g_object_get_data (G_OBJECT (selector), OPENED_KEY);
	g_return_if_fail (opened_sources != NULL);

	source = e_source_selector_ref_primary_selection (selector);
	if (source != NULL)
		sensitive = g_hash_table_contains (opened_sources, source);
	gtk_widget_set_sensitive (widget, sensitive);
	g_clear_object (&source);
}

static void
enable_widget_if_opened_and_view (ESourceSelector *selector,
				  GtkWidget *widget,
				  gboolean need_view)
{
	GHashTable *opened_sources;
	ESource *source;
	gboolean sensitive = FALSE;

	opened_sources = g_object_get_data (G_OBJECT (selector), OPENED_KEY);
	g_return_if_fail (opened_sources != NULL);

	source = e_source_selector_ref_primary_selection (selector);
	if (source != NULL) {
		EClient *client = g_hash_table_lookup (opened_sources, source);

		if (client) {
			gpointer view = g_object_get_data (G_OBJECT (client), VIEW_KEY);
			if (need_view)
				sensitive = view != NULL;
			else
				sensitive = view == NULL;
		}
	}
	gtk_widget_set_sensitive (widget, sensitive);
	g_clear_object (&source);
}

static void
enable_widget_if_opened_and_no_view_cb (ESourceSelector *selector,
					GtkWidget *widget)
{
	enable_widget_if_opened_and_view (selector, widget, FALSE);
}

static void
enable_widget_if_opened_and_has_view_cb (ESourceSelector *selector,
					GtkWidget *widget)
{
	enable_widget_if_opened_and_view (selector, widget, TRUE);
}

static void
disable_widget_if_opened_cb (ESourceSelector *selector,
                             GtkWidget *widget)
{
	GHashTable *opened_sources;
	ESource *source;
	gboolean sensitive = FALSE;

	opened_sources = g_object_get_data (G_OBJECT (selector), OPENED_KEY);
	g_return_if_fail (opened_sources != NULL);

	source = e_source_selector_ref_primary_selection (selector);
	if (source != NULL)
		sensitive = !g_hash_table_contains (opened_sources, source);
	gtk_widget_set_sensitive (widget, sensitive);
	g_clear_object (&source);
}

static void
enable_widget_if_any_selected (ESourceSelector *selector,
			       GtkWidget *widget)
{
	ESource *source;
	gboolean sensitive;

	source = e_source_selector_ref_primary_selection (selector);
	sensitive = (source != NULL);
	gtk_widget_set_sensitive (widget, sensitive);
	g_clear_object (&source);
}

static void
open_selected_clicked_cb (GtkWidget *button,
                          ESourceSelector *selector)
{
	GHashTable *opened_sources;
	ESource *source;

	opened_sources = g_object_get_data (G_OBJECT (selector), OPENED_KEY);
	g_return_if_fail (opened_sources != NULL);

	source = e_source_selector_ref_primary_selection (selector);
	if (source == NULL)
		return;

	if (!g_hash_table_contains (opened_sources, source)) {
		EClient *client;
		ECalClientSourceType source_type;
		gpointer data;
		GError *local_error = NULL;

		data = g_object_get_data (G_OBJECT (selector), SOURCE_TYPE_KEY);
		source_type = GPOINTER_TO_UINT (data);

		if (source_type == E_CAL_CLIENT_SOURCE_TYPE_LAST)
			client = e_book_client_connect_sync (
				source, (guint32) -1, NULL, &local_error);
		else
			client = e_cal_client_connect_sync (
				source, source_type, (guint32) -1, NULL, &local_error);

		if (client != NULL) {
			g_hash_table_insert (
				opened_sources,
				g_object_ref (source),
				g_object_ref (client));
			g_signal_emit_by_name (
				selector, "primary-selection-changed", 0);
			g_object_unref (client);
		}

		if (local_error != NULL) {
			g_warning (
				"Failed to open '%s': %s",
				e_source_get_display_name (source),
				local_error->message);
			g_error_free (local_error);
		}
	}

	g_object_unref (source);
}

static void
close_selected_clicked_cb (GtkWidget *button,
                           ESourceSelector *selector)
{
	GHashTable *opened_sources;
	ESource *source;

	opened_sources = g_object_get_data (G_OBJECT (selector), OPENED_KEY);
	g_return_if_fail (opened_sources != NULL);

	source = e_source_selector_ref_primary_selection (selector);
	if (source == NULL)
		return;

	if (g_hash_table_remove (opened_sources, source))
		g_signal_emit_by_name (
			selector, "primary-selection-changed", 0);

	g_object_unref (source);
}

static void
refresh_selected_clicked_cb (GtkWidget *button,
			     ESourceSelector *selector)
{
	GHashTable *opened_sources;
	EClient *client;
	ESource *source;
	GError *error = NULL;

	opened_sources = g_object_get_data (G_OBJECT (selector), OPENED_KEY);
	g_return_if_fail (opened_sources != NULL);

	source = e_source_selector_ref_primary_selection (selector);
	if (source == NULL)
		return;

	client = g_hash_table_lookup (opened_sources, source);

	g_return_if_fail (client != NULL);

	if (!e_client_refresh_sync (client, NULL, &error)) {
		g_warning (
			"Failed to call 'refresh' on '%s': %s",
			e_source_get_display_name (source),
			error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
	g_object_unref (source);
}

static void
cal_view_objects_added_cb (ECalClientView *client_view,
			   const GSList *objects,
			   ESource *source)
{
	GSList *link;

	g_print ("%s: view:%p of %s; %d modified\n", G_STRFUNC, client_view, e_source_get_display_name (source),
		g_slist_length ((GSList *) objects));

	for (link = (GSList *) objects; link; link = g_slist_next (link)) {
		ICalComponent *icomp = link->data;

		if (icomp) {
			gchar *str = i_cal_component_as_ical_string (icomp);
			g_print ("%s\n    -----------------------------\n", str);
			g_free (str);
		} else {
			g_print ("\tnull\n");
		}
	}
}

static void
cal_view_objects_modified_cb (ECalClientView *client_view,
			      const GSList *objects,
			      ESource *source)
{
	GSList *link;

	g_print ("%s: view:%p of %s; %d modified\n", G_STRFUNC, client_view, e_source_get_display_name (source),
		g_slist_length ((GSList *) objects));

	for (link = (GSList *) objects; link; link = g_slist_next (link)) {
		ICalComponent *icomp = link->data;

		if (icomp) {
			gchar *str = i_cal_component_as_ical_string (icomp);
			g_print ("%s\n    -----------------------------\n", str);
			g_free (str);
		} else
			g_print ("\tnull\n");
	}
}

static void
cal_view_objects_removed_cb (ECalClientView *client_view,
			     const GSList *uids,
			     ESource *source)
{
	GSList *link;

	g_print ("%s: view:%p of %s; %d removed\n", G_STRFUNC, client_view, e_source_get_display_name (source),
		g_slist_length ((GSList *) uids));

	for (link = (GSList *) uids; link; link = g_slist_next (link)) {
		ECalComponentId *id = link->data;
		const gchar *uid, *rid;

		uid = id ? e_cal_component_id_get_uid (id) : NULL;
		rid = id ? e_cal_component_id_get_uid (id) : NULL;

		if (id)
			g_print ("\tuid:%s%s%s\n", uid, rid ? " rid:" : "", rid ? rid : "");
		else
			g_print ("\tnull\n");
	}
}

static void
cal_view_progress_cb (ECalClientView *client_view,
		      guint percent,
		      const gchar *message,
		      ESource *source)
{
	g_print ("   %s: view:%p of %s; percent:%u message:%s\n", G_STRFUNC, client_view, e_source_get_display_name (source),
		percent, message);
}

static void
cal_view_complete_cb (ECalClientView *client_view,
		      const GError *error,
		      ESource *source)
{
	g_print ("   %s: view:%p of %s: %s\n", G_STRFUNC, client_view, e_source_get_display_name (source),
		error ? error->message : "Success");
}

static void
book_view_objects_added_cb (EBookClientView *client_view,
			    const GSList *objects,
			    ESource *source)
{
	GSList *link;

	g_print ("%s: view:%p of %s; %d modified\n", G_STRFUNC, client_view, e_source_get_display_name (source),
		g_slist_length ((GSList *) objects));

	for (link = (GSList *) objects; link; link = g_slist_next (link)) {
		EContact *contact = link->data;

		if (contact) {
			gchar *vcard;

			vcard = e_vcard_to_string (E_VCARD (contact));
			g_print ("%s\n    -----------------------------\n", vcard);
			g_free (vcard);
		} else {
			g_print ("\tnull\n");
		}
	}
}

static void
book_view_objects_modified_cb (EBookClientView *client_view,
			       const GSList *objects,
			       ESource *source)
{
	GSList *link;

	g_print ("%s: view:%p of %s; %d modified\n", G_STRFUNC, client_view, e_source_get_display_name (source),
		g_slist_length ((GSList *) objects));

	for (link = (GSList *) objects; link; link = g_slist_next (link)) {
		EContact *contact = link->data;

		if (contact) {
			gchar *vcard;

			vcard = e_vcard_to_string (E_VCARD (contact));
			g_print ("%s\n    -----------------------------\n", vcard);
			g_free (vcard);
		} else {
			g_print ("\tnull\n");
		}
	}
}

static void
book_view_objects_removed_cb (EBookClientView *client_view,
			      const GSList *uids,
			      ESource *source)
{
	GSList *link;

	g_print ("%s: view:%p of %s; %d removed\n", G_STRFUNC, client_view, e_source_get_display_name (source),
		g_slist_length ((GSList *) uids));

	for (link = (GSList *) uids; link; link = g_slist_next (link)) {
		const gchar *id = link->data;

		if (id)
			g_print ("\tuid:%s\n", id);
		else
			g_print ("\tnull\n");
	}
}

static void
book_view_progress_cb (EBookClientView *client_view,
		       guint percent,
		       const gchar *message,
		       ESource *source)
{
	g_print ("   %s: view:%p of %s; percent:%u message:%s\n", G_STRFUNC, client_view, e_source_get_display_name (source),
		percent, message);
}

static void
book_view_complete_cb (EBookClientView *client_view,
		       const GError *error,
		       ESource *source)
{
	g_print ("   %s: view:%p of %s: %s\n", G_STRFUNC, client_view, e_source_get_display_name (source),
		error ? error->message : "Success");
}

static void
create_view_clicked_cb (GtkWidget *button,
			ESourceSelector *selector)
{
	GHashTable *opened_sources;
	EClient *client;
	ESource *source;
	gboolean success = FALSE;
	GError *error = NULL;

	opened_sources = g_object_get_data (G_OBJECT (selector), OPENED_KEY);
	g_return_if_fail (opened_sources != NULL);

	source = e_source_selector_ref_primary_selection (selector);
	if (source == NULL)
		return;

	client = g_hash_table_lookup (opened_sources, source);

	g_return_if_fail (client != NULL);
	g_return_if_fail (g_object_get_data (G_OBJECT (client), VIEW_KEY) == NULL);

	if (E_IS_CAL_CLIENT (client)) {
		ECalClientView *view = NULL;
		gchar *expr = NULL;

		if (e_cal_client_get_source_type (E_CAL_CLIENT (client)) == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
			ICalTime *tt;
			gchar *start, *end;

			tt = i_cal_time_new_today ();
			start = isodate_from_time_t (i_cal_time_as_timet (tt));
			i_cal_time_adjust (tt, 14, 0, 0, 0);
			end = isodate_from_time_t (i_cal_time_as_timet (tt));

			expr = g_strdup_printf (
				"(occur-in-time-range? (make-time \"%s\") (make-time \"%s\") \"UTC\")",
				start, end);

			g_clear_object (&tt);
			g_free (start);
			g_free (end);
		}

		success = e_cal_client_get_view_sync (E_CAL_CLIENT (client), expr ? expr : "#t", &view, NULL, &error) && view;

		g_free (expr);

		if (view) {
			g_object_set_data_full (G_OBJECT (client), VIEW_KEY, view, g_object_unref);

			g_signal_connect (view, "objects-added", G_CALLBACK (cal_view_objects_added_cb), source);
			g_signal_connect (view, "objects-modified", G_CALLBACK (cal_view_objects_modified_cb), source);
			g_signal_connect (view, "objects-removed", G_CALLBACK (cal_view_objects_removed_cb), source);
			g_signal_connect (view, "progress", G_CALLBACK (cal_view_progress_cb), source);
			g_signal_connect (view, "complete", G_CALLBACK (cal_view_complete_cb), source);

			g_print ("Calendar view %p for %s created\n", view, e_source_get_display_name (source));

			e_cal_client_view_start (view, &error);
		}
	} else if (E_IS_BOOK_CLIENT (client)) {
		EBookClientView *view = NULL;

		success = e_book_client_get_view_sync (E_BOOK_CLIENT (client), "(contains \"full_name\" \"a\")", &view, NULL, &error) && view;

		if (view) {
			g_object_set_data_full (G_OBJECT (client), VIEW_KEY, view, g_object_unref);

			g_signal_connect (view, "objects-added", G_CALLBACK (book_view_objects_added_cb), source);
			g_signal_connect (view, "objects-modified", G_CALLBACK (book_view_objects_modified_cb), source);
			g_signal_connect (view, "objects-removed", G_CALLBACK (book_view_objects_removed_cb), source);
			g_signal_connect (view, "progress", G_CALLBACK (book_view_progress_cb), source);
			g_signal_connect (view, "complete", G_CALLBACK (book_view_complete_cb), source);

			g_print ("Book view %p for %s created\n", view, e_source_get_display_name (source));

			e_book_client_view_start (view, &error);
		}
	} else {
		g_warn_if_reached ();
	}

	if (success) {
		g_signal_emit_by_name (selector, "primary-selection-changed", 0);

		if (error) {
			g_warning ("Failed to call 'startView' on '%s': %s",
				e_source_get_display_name (source), error->message);
		}
	} else {
		g_warning ("Failed to call 'getView' on '%s': %s",
			e_source_get_display_name (source),
			error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
	g_object_unref (source);
}

static void
destroy_view_clicked_cb (GtkWidget *button,
			 ESourceSelector *selector)
{
	GHashTable *opened_sources;
	EClient *client;
	ESource *source;
	gpointer view;
	GError *error = NULL;

	opened_sources = g_object_get_data (G_OBJECT (selector), OPENED_KEY);
	g_return_if_fail (opened_sources != NULL);

	source = e_source_selector_ref_primary_selection (selector);
	if (source == NULL)
		return;

	client = g_hash_table_lookup (opened_sources, source);

	g_return_if_fail (client != NULL);

	view = g_object_get_data (G_OBJECT (client), VIEW_KEY);
	g_return_if_fail (view != NULL);

	if (E_IS_CAL_CLIENT_VIEW (view))
		e_cal_client_view_stop (view, &error);
	else if (E_IS_BOOK_CLIENT_VIEW (view))
		e_book_client_view_stop (view, &error);
	else
		g_warn_if_reached ();

	g_object_set_data (G_OBJECT (client), VIEW_KEY, NULL);
	g_signal_emit_by_name (selector, "primary-selection-changed", 0);

	if (error) {
		g_warning ("Failed to destroy view %p on '%s': %s",
			view, e_source_get_display_name (source), error->message);
	} else {
		g_print ("View %p for %s destroyed\n", view, e_source_get_display_name (source));
	}

	g_clear_error (&error);
	g_object_unref (source);
}

static void
flip_busy_clicked_cb (GtkWidget *button,
		      ESourceSelector *selector)
{
	ESource *source;

	source = e_source_selector_ref_primary_selection (selector);
	if (source)
		e_source_selector_set_source_is_busy (selector, source,
			!e_source_selector_get_source_is_busy (selector, source));

	g_clear_object (&source);
}

static void
set_tooltip_clicked_cb (GtkWidget *button,
			ESourceSelector *selector)
{
	ESource *source;
	GtkEntry *entry;

	entry = g_object_get_data (G_OBJECT (button), TOOLTIP_ENTRY_KEY);
	g_return_if_fail (entry != NULL);

	source = e_source_selector_ref_primary_selection (selector);
	if (source)
		e_source_selector_set_source_tooltip (selector, source,
			gtk_entry_get_text (entry));

	g_clear_object (&source);
}

static GtkWidget *
create_page (ESourceRegistry *registry,
             const gchar *extension_name,
             ECalClientSourceType source_type)
{
	GtkWidget *widget, *subwindow, *selector, *button_box, *entry;
	GtkGrid *grid;
	GHashTable *opened_sources;

	grid = GTK_GRID (gtk_grid_new ());

	subwindow = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (
		G_OBJECT (subwindow),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"min-content-width", 200,
		NULL);

	selector = e_source_selector_new (registry, extension_name);
	g_object_set (
		G_OBJECT (selector),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"show-toggles", FALSE,
		"show-colors", source_type != E_CAL_CLIENT_SOURCE_TYPE_LAST,
		NULL);
	gtk_container_add (GTK_CONTAINER (subwindow), selector);

	gtk_grid_attach (grid, subwindow, 0, 0, 1, 5);

	button_box = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	g_object_set (
		G_OBJECT (button_box),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		NULL);
	gtk_grid_attach (grid, button_box, 1, 0, 1, 1);

	widget = gtk_button_new_with_label ("Open selected");
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (open_selected_clicked_cb), selector);
	g_signal_connect (
		selector, "primary-selection-changed",
		G_CALLBACK (disable_widget_if_opened_cb), widget);

	widget = gtk_button_new_with_label ("Close selected");
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (close_selected_clicked_cb), selector);
	g_signal_connect (
		selector, "primary-selection-changed",
		G_CALLBACK (enable_widget_if_opened_cb), widget);

	widget = gtk_button_new_with_label ("Refresh selected");
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (refresh_selected_clicked_cb), selector);
	g_signal_connect (
		selector, "primary-selection-changed",
		G_CALLBACK (enable_widget_if_opened_cb), widget);

	widget = gtk_button_new_with_label ("Create View");
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (create_view_clicked_cb), selector);
	g_signal_connect (
		selector, "primary-selection-changed",
		G_CALLBACK (enable_widget_if_opened_and_no_view_cb), widget);

	widget = gtk_button_new_with_label ("Destroy View");
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (destroy_view_clicked_cb), selector);
	g_signal_connect (
		selector, "primary-selection-changed",
		G_CALLBACK (enable_widget_if_opened_and_has_view_cb), widget);

	widget = gtk_button_new_with_label ("Flip busy status");
	gtk_widget_set_margin_top (widget, 10);
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (flip_busy_clicked_cb), selector);
	g_signal_connect (
		selector, "primary-selection-changed",
		G_CALLBACK (enable_widget_if_any_selected), widget);

	entry = gtk_entry_new ();
	gtk_container_add (GTK_CONTAINER (button_box), entry);

	widget = gtk_button_new_with_label ("Set Tooltip");
	g_object_set_data (G_OBJECT (widget), TOOLTIP_ENTRY_KEY, entry);
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (set_tooltip_clicked_cb), selector);
	g_signal_connect (
		selector, "primary-selection-changed",
		G_CALLBACK (enable_widget_if_any_selected), widget);

	widget = gtk_label_new ("");
	g_object_set (
		G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);

	widget = gtk_check_button_new_with_label ("Show colors");
	g_object_set (
		G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_END,
		"vexpand", FALSE,
		NULL);
	gtk_grid_attach (grid, widget, 1, 2, 1, 1);

	e_binding_bind_property (
		selector, "show-colors",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	widget = gtk_check_button_new_with_label ("Show icons");
	g_object_set (
		G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_END,
		"vexpand", FALSE,
		NULL);
	gtk_grid_attach (grid, widget, 1, 3, 1, 1);

	e_binding_bind_property (
		selector, "show-icons",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	widget = gtk_check_button_new_with_label ("Show toggles");
	g_object_set (
		G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_END,
		"vexpand", FALSE,
		NULL);
	gtk_grid_attach (grid, widget, 1, 4, 1, 1);

	e_binding_bind_property (
		selector, "show-toggles",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	opened_sources = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) unset_view_and_unref_client);
	g_object_set_data_full (
		G_OBJECT (selector),
		OPENED_KEY,
		opened_sources,
		(GDestroyNotify) g_hash_table_unref);
	g_object_set_data (
		G_OBJECT (selector),
		SOURCE_TYPE_KEY,
		GUINT_TO_POINTER (source_type));
	g_object_set_data_full (
		G_OBJECT (selector),
		EXTENSION_NAME_KEY,
		g_strdup (extension_name),
		(GDestroyNotify) g_free);

	/* update buttons */
	g_signal_emit_by_name (selector, "primary-selection-changed", 0);

	g_signal_connect (
		selector, "selection-changed",
		G_CALLBACK (selection_changed_callback), NULL);

	return GTK_WIDGET (grid);
}

static gboolean
window_delete_event_cb (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	gtk_main_quit ();

	return FALSE;
}

static gint
on_idle_create_widget (ESourceRegistry *registry)
{
	GtkWidget *window, *notebook;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 300, 400);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (window_delete_event_cb), NULL);

	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (notebook));

	gtk_notebook_append_page (
		GTK_NOTEBOOK (notebook),
		create_page (
			registry,
			E_SOURCE_EXTENSION_CALENDAR,
			E_CAL_CLIENT_SOURCE_TYPE_EVENTS),
		gtk_label_new ("Calendars"));

	gtk_notebook_append_page (
		GTK_NOTEBOOK (notebook),
		create_page (
			registry,
			E_SOURCE_EXTENSION_MEMO_LIST,
			E_CAL_CLIENT_SOURCE_TYPE_MEMOS),
		gtk_label_new ("Memos"));

	gtk_notebook_append_page (
		GTK_NOTEBOOK (notebook),
		create_page (
			registry,
			E_SOURCE_EXTENSION_TASK_LIST,
			E_CAL_CLIENT_SOURCE_TYPE_TASKS),
		gtk_label_new ("Tasks"));

	gtk_notebook_append_page (
		GTK_NOTEBOOK (notebook),
		create_page (
			registry,
			E_SOURCE_EXTENSION_ADDRESS_BOOK,
			E_CAL_CLIENT_SOURCE_TYPE_LAST),
		gtk_label_new ("Books"));

	gtk_widget_show_all (window);

	return FALSE;
}

gint
main (gint argc,
      gchar **argv)
{
	ESourceRegistry *registry;
	GError *local_error = NULL;

	gtk_init (&argc, &argv);

	registry = e_source_registry_new_sync (NULL, &local_error);

	if (local_error != NULL) {
		g_error (
			"Failed to load ESource registry: %s",
			local_error->message);
		g_return_val_if_reached (-1);
	}

	g_idle_add ((GSourceFunc) on_idle_create_widget, registry);

	gtk_main ();

	g_object_unref (registry);
	e_misc_util_free_global_memory ();

	return 0;
}
