/*
 * e-selection.c
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

/**
 * SECTION: e-selection
 * @short_description: selection and clipboard utilities
 * @include: e-util/e-util.h
 **/

#include "evolution-config.h"

#include "e-selection.h"

#include <string.h>

typedef struct _RequestTextInfo RequestTextInfo;
typedef struct _WaitForDataResults WaitForDataResults;

struct _RequestTextInfo {
	GtkClipboardTextReceivedFunc callback;
	gpointer user_data;
};

struct _WaitForDataResults {
	GMainLoop *loop;
	gpointer data;
};

enum {
	ATOM_CALENDAR,
	ATOM_X_VCALENDAR,
	NUM_CALENDAR_ATOMS
};

enum {
	ATOM_DIRECTORY,
	ATOM_X_VCARD,
	NUM_DIRECTORY_ATOMS
};

enum {
	ATOM_HTML,
	NUM_HTML_ATOMS
};

static GdkAtom calendar_atoms[NUM_CALENDAR_ATOMS];
static GdkAtom directory_atoms[NUM_DIRECTORY_ATOMS];
static GdkAtom html_atoms[NUM_HTML_ATOMS];

static void
init_atoms (void)
{
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	/* Calendar Atoms */

	calendar_atoms[ATOM_CALENDAR] =
		gdk_atom_intern_static_string ("text/calendar");

	calendar_atoms[ATOM_X_VCALENDAR] =
		gdk_atom_intern_static_string ("text/x-vcalendar");

	/* Directory Atoms */

	directory_atoms[ATOM_DIRECTORY] =
		gdk_atom_intern_static_string ("text/directory");

	directory_atoms[ATOM_X_VCARD] =
		gdk_atom_intern_static_string ("text/x-vcard");

	/* HTML Atoms */

	html_atoms[ATOM_HTML] =
		gdk_atom_intern_static_string ("text/html");

	initialized = TRUE;
}

static void
clipboard_wait_for_text_cb (GtkClipboard *clipboard,
                            const gchar *source,
                            WaitForDataResults *results)
{
	results->data = g_strdup (source);
	g_main_loop_quit (results->loop);
}

void
e_target_list_add_calendar_targets (GtkTargetList *list,
                                    guint info)
{
	gint ii;

	g_return_if_fail (list != NULL);

	init_atoms ();

	for (ii = 0; ii < NUM_CALENDAR_ATOMS; ii++)
		gtk_target_list_add (list, calendar_atoms[ii], 0, info);
}

void
e_target_list_add_directory_targets (GtkTargetList *list,
                                     guint info)
{
	gint ii;

	g_return_if_fail (list != NULL);

	init_atoms ();

	for (ii = 0; ii < NUM_DIRECTORY_ATOMS; ii++)
		gtk_target_list_add (list, directory_atoms[ii], 0, info);
}

void
e_target_list_add_html_targets (GtkTargetList *list,
                                guint info)
{
	gint ii;

	g_return_if_fail (list != NULL);

	init_atoms ();

	for (ii = 0; ii < NUM_HTML_ATOMS; ii++)
		gtk_target_list_add (list, html_atoms[ii], 0, info);
}

gboolean
e_selection_data_set_calendar (GtkSelectionData *selection_data,
                               const gchar *source,
                               gint length)
{
	GdkAtom atom;
	gint ii;

	g_return_val_if_fail (selection_data != NULL, FALSE);
	g_return_val_if_fail (source != NULL, FALSE);

	if (length < 0)
		length = strlen (source);

	init_atoms ();

	atom = gtk_selection_data_get_target (selection_data);

	/* All calendar atoms are treated the same. */
	for (ii = 0; ii < NUM_CALENDAR_ATOMS; ii++) {
		if (atom == calendar_atoms[ii]) {
			gtk_selection_data_set (
				selection_data, atom, 8,
				(guchar *) source, length);
			return TRUE;
		}
	}

	return FALSE;
}

gboolean
e_selection_data_set_directory (GtkSelectionData *selection_data,
                                const gchar *source,
                                gint length)
{
	GdkAtom atom;
	gint ii;

	g_return_val_if_fail (selection_data != NULL, FALSE);
	g_return_val_if_fail (source != NULL, FALSE);

	if (length < 0)
		length = strlen (source);

	init_atoms ();

	atom = gtk_selection_data_get_target (selection_data);

	/* All directory atoms are treated the same. */
	for (ii = 0; ii < NUM_DIRECTORY_ATOMS; ii++) {
		if (atom == directory_atoms[ii]) {
			gtk_selection_data_set (
				selection_data, atom, 8,
				(guchar *) source, length);
			return TRUE;
		}
	}

	return FALSE;
}

gboolean
e_selection_data_set_html (GtkSelectionData *selection_data,
                           const gchar *source,
                           gint length)
{
	GdkAtom atom;
	gint ii;

	g_return_val_if_fail (selection_data != NULL, FALSE);
	g_return_val_if_fail (source != NULL, FALSE);

	if (length < 0)
		length = strlen (source);

	init_atoms ();

	atom = gtk_selection_data_get_target (selection_data);

	/* All HTML atoms are treated the same. */
	for (ii = 0; ii < NUM_HTML_ATOMS; ii++) {
		if (atom == html_atoms[ii]) {
			gtk_selection_data_set (
				selection_data, atom, 8,
				(guchar *) source, length);
			return TRUE;
		}
	}

	return FALSE;
}

gchar *
e_selection_data_get_calendar (GtkSelectionData *selection_data)
{
	GdkAtom data_type;
	const guchar *data = NULL;
	gint ii;

	/* XXX May need to do encoding and line ending conversions
	 *     here.  Not worrying about it for now. */

	g_return_val_if_fail (selection_data != NULL, NULL);

	data = gtk_selection_data_get_data (selection_data);
	data_type = gtk_selection_data_get_data_type (selection_data);

	/* All calendar atoms are treated the same. */
	for (ii = 0; ii < NUM_CALENDAR_ATOMS; ii++)
		if (data_type == calendar_atoms[ii])
			return g_strdup ((gchar *) data);

	return NULL;
}

gchar *
e_selection_data_get_directory (GtkSelectionData *selection_data)
{
	GdkAtom data_type;
	const guchar *data = NULL;
	gint ii;

	/* XXX May need to do encoding and line ending conversions
	 *     here.  Not worrying about it for now. */

	g_return_val_if_fail (selection_data != NULL, NULL);

	data = gtk_selection_data_get_data (selection_data);
	data_type = gtk_selection_data_get_data_type (selection_data);

	/* All directory atoms are treated the same. */
	for (ii = 0; ii < NUM_DIRECTORY_ATOMS; ii++)
		if (data_type == directory_atoms[ii])
			return g_strdup ((gchar *) data);

	return NULL;
}

gchar *
e_selection_data_get_html (GtkSelectionData *selection_data)
{
	GdkAtom data_type;
	const guchar *data = NULL;
	gchar *utf8_text;
	gint length;
	gint ii;
	GError *error = NULL;

	/* XXX May need to do encoding conversions here.
	 *     Not worrying about it for now. */

	g_return_val_if_fail (selection_data != NULL, NULL);

	data = gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);
	data_type = gtk_selection_data_get_data_type (selection_data);

	g_return_val_if_fail (data != NULL, NULL);

	/* First validate the data.  Assume it's UTF-8 or UTF-16. */
	if (g_utf8_validate ((const gchar *) data, length - 1, NULL))
		utf8_text = g_strdup ((const gchar *) data);
	else
		utf8_text = g_convert (
			(const gchar *) data, length,
			"UTF-8", "UTF-16", NULL, NULL, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	/* All HTML atoms are treated the same. */
	for (ii = 0; ii < NUM_HTML_ATOMS; ii++)
		if (data_type == html_atoms[ii])
			return utf8_text;

	g_free (utf8_text);

	return NULL;
}

gboolean
e_selection_data_targets_include_calendar (GtkSelectionData *selection_data)
{
	GdkAtom *targets;
	gint n_targets;
	gboolean result = FALSE;

	g_return_val_if_fail (selection_data != NULL, FALSE);

	if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets)) {
		result = e_targets_include_calendar (targets, n_targets);
		g_free (targets);
	}

	return result;
}

gboolean
e_selection_data_targets_include_directory (GtkSelectionData *selection_data)
{
	GdkAtom *targets;
	gint n_targets;
	gboolean result = FALSE;

	g_return_val_if_fail (selection_data != NULL, FALSE);

	if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets)) {
		result = e_targets_include_directory (targets, n_targets);
		g_free (targets);
	}

	return result;
}

gboolean
e_selection_data_targets_include_html (GtkSelectionData *selection_data)
{
	GdkAtom *targets;
	gint n_targets;
	gboolean result = FALSE;

	g_return_val_if_fail (selection_data != NULL, FALSE);

	if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets)) {
		result = e_targets_include_html (targets, n_targets);
		g_free (targets);
	}

	return result;
}

gboolean
e_targets_include_calendar (GdkAtom *targets,
                            gint n_targets)
{
	gint ii, jj;

	g_return_val_if_fail (targets != NULL || n_targets == 0, FALSE);

	init_atoms ();

	for (ii = 0; ii < n_targets; ii++)
		for (jj = 0; jj < NUM_CALENDAR_ATOMS; jj++)
			if (targets[ii] == calendar_atoms[jj])
				return TRUE;

	return FALSE;
}

gboolean
e_targets_include_directory (GdkAtom *targets,
                             gint n_targets)
{
	gint ii, jj;

	g_return_val_if_fail (targets != NULL || n_targets == 0, FALSE);

	init_atoms ();

	for (ii = 0; ii < n_targets; ii++)
		for (jj = 0; jj < NUM_DIRECTORY_ATOMS; jj++)
			if (targets[ii] == directory_atoms[jj])
				return TRUE;

	return FALSE;
}

gboolean
e_targets_include_html (GdkAtom *targets,
                        gint n_targets)
{
	gint ii, jj;

	g_return_val_if_fail (targets != NULL || n_targets == 0, FALSE);

	init_atoms ();

	for (ii = 0; ii < n_targets; ii++)
		for (jj = 0; jj < NUM_HTML_ATOMS; jj++)
			if (targets[ii] == html_atoms[jj])
				return TRUE;

	return FALSE;
}

static void
clipboard_get_calendar (GtkClipboard *clipboard,
                        GtkSelectionData *selection_data,
                        guint info,
                        gchar *source)
{
	e_selection_data_set_calendar (selection_data, source, -1);
}

static void
clipboard_clear_calendar (GtkClipboard *clipboard,
                          gchar *source)
{
	g_free (source);
}

void
e_clipboard_set_calendar (GtkClipboard *clipboard,
                          const gchar *source,
                          gint length)
{
	GtkTargetList *list;
	GtkTargetEntry *targets;
	gint n_targets;

	g_return_if_fail (clipboard != NULL);
	g_return_if_fail (source != NULL);

	list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (list, 0);

	targets = gtk_target_table_new_from_list (list, &n_targets);

	if (length < 0)
		length = strlen (source);

	gtk_clipboard_set_with_data (
		clipboard, targets, n_targets,
		(GtkClipboardGetFunc) clipboard_get_calendar,
		(GtkClipboardClearFunc) clipboard_clear_calendar,
		g_strndup (source, length));

	gtk_clipboard_set_can_store (clipboard, NULL, 0);

	gtk_target_table_free (targets, n_targets);
	gtk_target_list_unref (list);
}

static void
clipboard_get_directory (GtkClipboard *clipboard,
                         GtkSelectionData *selection_data,
                         guint info,
                         gchar *source)
{
	e_selection_data_set_directory (selection_data, source, -1);
}

static void
clipboard_clear_directory (GtkClipboard *clipboard,
                           gchar *source)
{
	g_free (source);
}

void
e_clipboard_set_directory (GtkClipboard *clipboard,
                           const gchar *source,
                           gint length)
{
	GtkTargetList *list;
	GtkTargetEntry *targets;
	gint n_targets;

	g_return_if_fail (clipboard != NULL);
	g_return_if_fail (source != NULL);

	list = gtk_target_list_new (NULL, 0);
	e_target_list_add_directory_targets (list, 0);

	targets = gtk_target_table_new_from_list (list, &n_targets);

	if (length < 0)
		length = strlen (source);

	gtk_clipboard_set_with_data (
		clipboard, targets, n_targets,
		(GtkClipboardGetFunc) clipboard_get_directory,
		(GtkClipboardClearFunc) clipboard_clear_directory,
		g_strndup (source, length));

	gtk_clipboard_set_can_store (clipboard, NULL, 0);

	gtk_target_table_free (targets, n_targets);
	gtk_target_list_unref (list);
}

static void
clipboard_get_html (GtkClipboard *clipboard,
                    GtkSelectionData *selection_data,
                    guint info,
                    gchar *source)
{
	e_selection_data_set_html (selection_data, source, -1);
}

static void
clipboard_clear_html (GtkClipboard *clipboard,
                      gchar *source)
{
	g_free (source);
}

void
e_clipboard_set_html (GtkClipboard *clipboard,
                      const gchar *source,
                      gint length)
{
	GtkTargetList *list;
	GtkTargetEntry *targets;
	gint n_targets;

	g_return_if_fail (clipboard != NULL);
	g_return_if_fail (source != NULL);

	list = gtk_target_list_new (NULL, 0);
	e_target_list_add_html_targets (list, 0);

	targets = gtk_target_table_new_from_list (list, &n_targets);

	if (length < 0)
		length = strlen (source);

	gtk_clipboard_set_with_data (
		clipboard, targets, n_targets,
		(GtkClipboardGetFunc) clipboard_get_html,
		(GtkClipboardClearFunc) clipboard_clear_html,
		g_strndup (source, length));

	gtk_clipboard_set_can_store (clipboard, NULL, 0);

	gtk_target_table_free (targets, n_targets);
	gtk_target_list_unref (list);
}

static void
clipboard_request_calendar_cb (GtkClipboard *clipboard,
                               GtkSelectionData *selection_data,
                               RequestTextInfo *info)
{
	gchar *source;

	source = e_selection_data_get_calendar (selection_data);
	info->callback (clipboard, source, info->user_data);
	g_free (source);

	g_slice_free (RequestTextInfo, info);
}

void
e_clipboard_request_calendar (GtkClipboard *clipboard,
                              GtkClipboardTextReceivedFunc callback,
                              gpointer user_data)
{
	RequestTextInfo *info;

	g_return_if_fail (clipboard != NULL);
	g_return_if_fail (callback != NULL);

	init_atoms ();

	info = g_slice_new (RequestTextInfo);
	info->callback = callback;
	info->user_data = user_data;

	gtk_clipboard_request_contents (
		clipboard, calendar_atoms[ATOM_CALENDAR],
		(GtkClipboardReceivedFunc)
		clipboard_request_calendar_cb, info);
}

static void
clipboard_request_directory_cb (GtkClipboard *clipboard,
                                GtkSelectionData *selection_data,
                                RequestTextInfo *info)
{
	gchar *source;

	source = e_selection_data_get_directory (selection_data);
	info->callback (clipboard, source, info->user_data);
	g_free (source);

	g_slice_free (RequestTextInfo, info);
}

void
e_clipboard_request_directory (GtkClipboard *clipboard,
                               GtkClipboardTextReceivedFunc callback,
                               gpointer user_data)
{
	RequestTextInfo *info;

	g_return_if_fail (clipboard != NULL);
	g_return_if_fail (callback != NULL);

	init_atoms ();

	info = g_slice_new (RequestTextInfo);
	info->callback = callback;
	info->user_data = user_data;

	gtk_clipboard_request_contents (
		clipboard, directory_atoms[ATOM_DIRECTORY],
		(GtkClipboardReceivedFunc)
		clipboard_request_directory_cb, info);
}

static void
clipboard_request_html_cb (GtkClipboard *clipboard,
                           GtkSelectionData *selection_data,
                           RequestTextInfo *info)
{
	gchar *source;

	source = e_selection_data_get_html (selection_data);
	info->callback (clipboard, source, info->user_data);
	g_free (source);

	g_slice_free (RequestTextInfo, info);
}

void
e_clipboard_request_html (GtkClipboard *clipboard,
                          GtkClipboardTextReceivedFunc callback,
                          gpointer user_data)
{
	RequestTextInfo *info;

	g_return_if_fail (clipboard != NULL);
	g_return_if_fail (callback != NULL);

	init_atoms ();

	info = g_slice_new (RequestTextInfo);
	info->callback = callback;
	info->user_data = user_data;

	gtk_clipboard_request_contents (
		clipboard, html_atoms[ATOM_HTML],
		(GtkClipboardReceivedFunc)
		clipboard_request_html_cb, info);
}

gchar *
e_clipboard_wait_for_calendar (GtkClipboard *clipboard)
{
	WaitForDataResults results;

	g_return_val_if_fail (clipboard != NULL, NULL);

	results.data = NULL;
	results.loop = g_main_loop_new (NULL, TRUE);

	e_clipboard_request_calendar (
		clipboard, (GtkClipboardTextReceivedFunc)
		clipboard_wait_for_text_cb, &results);

	if (g_main_loop_is_running (results.loop))
		g_main_loop_run (results.loop);

	g_main_loop_unref (results.loop);

	return results.data;
}

gchar *
e_clipboard_wait_for_directory (GtkClipboard *clipboard)
{
	WaitForDataResults results;

	g_return_val_if_fail (clipboard != NULL, NULL);

	results.data = NULL;
	results.loop = g_main_loop_new (NULL, TRUE);

	e_clipboard_request_directory (
		clipboard, (GtkClipboardTextReceivedFunc)
		clipboard_wait_for_text_cb, &results);

	if (g_main_loop_is_running (results.loop))
		g_main_loop_run (results.loop);

	g_main_loop_unref (results.loop);

	return results.data;
}

gchar *
e_clipboard_wait_for_html (GtkClipboard *clipboard)
{
	WaitForDataResults results;

	g_return_val_if_fail (clipboard != NULL, NULL);

	results.data = NULL;
	results.loop = g_main_loop_new (NULL, TRUE);

	e_clipboard_request_html (
		clipboard, (GtkClipboardTextReceivedFunc)
		clipboard_wait_for_text_cb, &results);

	if (g_main_loop_is_running (results.loop))
		g_main_loop_run (results.loop);

	g_main_loop_unref (results.loop);

	return results.data;
}

gboolean
e_clipboard_wait_is_calendar_available (GtkClipboard *clipboard)
{
	GdkAtom *targets;
	gint n_targets;
	gboolean result = FALSE;

	if (gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets)) {
		result = e_targets_include_calendar (targets, n_targets);
		g_free (targets);
	}

	return result;
}

gboolean
e_clipboard_wait_is_directory_available (GtkClipboard *clipboard)
{
	GdkAtom *targets;
	gint n_targets;
	gboolean result = FALSE;

	if (gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets)) {
		result = e_targets_include_directory (targets, n_targets);
		g_free (targets);
	}

	return result;
}

gboolean
e_clipboard_wait_is_html_available (GtkClipboard *clipboard)
{
	GdkAtom *targets;
	gint n_targets;
	gboolean result = FALSE;

	if (gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets)) {
		result = e_targets_include_html (targets, n_targets);
		g_free (targets);
	}

	return result;
}

void
e_drag_dest_add_calendar_targets (GtkWidget *widget)
{
	GtkTargetList *target_list;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	target_list = gtk_drag_dest_get_target_list (widget);
	if (target_list != NULL)
		gtk_target_list_ref (target_list);
	else
		target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	gtk_drag_dest_set_target_list (widget, target_list);
	gtk_target_list_unref (target_list);
}

void
e_drag_dest_add_directory_targets (GtkWidget *widget)
{
	GtkTargetList *target_list;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	target_list = gtk_drag_dest_get_target_list (widget);
	if (target_list != NULL)
		gtk_target_list_ref (target_list);
	else
		target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_directory_targets (target_list, 0);
	gtk_drag_dest_set_target_list (widget, target_list);
	gtk_target_list_unref (target_list);
}

void
e_drag_dest_add_html_targets (GtkWidget *widget)
{
	GtkTargetList *target_list;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	target_list = gtk_drag_dest_get_target_list (widget);
	if (target_list != NULL)
		gtk_target_list_ref (target_list);
	else
		target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_html_targets (target_list, 0);
	gtk_drag_dest_set_target_list (widget, target_list);
	gtk_target_list_unref (target_list);
}

void
e_drag_source_add_calendar_targets (GtkWidget *widget)
{
	GtkTargetList *target_list;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	target_list = gtk_drag_source_get_target_list (widget);
	if (target_list != NULL)
		gtk_target_list_ref (target_list);
	else
		target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	gtk_drag_source_set_target_list (widget, target_list);
	gtk_target_list_unref (target_list);
}

void
e_drag_source_add_directory_targets (GtkWidget *widget)
{
	GtkTargetList *target_list;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	target_list = gtk_drag_source_get_target_list (widget);
	if (target_list != NULL)
		gtk_target_list_ref (target_list);
	else
		target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_directory_targets (target_list, 0);
	gtk_drag_source_set_target_list (widget, target_list);
	gtk_target_list_unref (target_list);
}

void
e_drag_source_add_html_targets (GtkWidget *widget)
{
	GtkTargetList *target_list;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	target_list = gtk_drag_source_get_target_list (widget);
	if (target_list != NULL)
		gtk_target_list_ref (target_list);
	else
		target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_html_targets (target_list, 0);
	gtk_drag_source_set_target_list (widget, target_list);
	gtk_target_list_unref (target_list);
}
