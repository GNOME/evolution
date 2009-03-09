/*
 * e-mime-part-utils.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-mime-part-utils.h"

#include <errno.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <camel/camel-stream-vfs.h>

#include "e-util/e-util.h"

static void
mime_part_utils_open_in_cb (GtkAction *action,
                            CamelMimePart *mime_part)
{
	GtkWindow *parent;
	GFile *file;
	gchar *path;
	gchar *uri;
	gint fd;
	GError *error = NULL;

	parent = g_object_get_data (G_OBJECT (action), "parent-window");

	fd = e_file_open_tmp (&path, &error);
	if (error != NULL)
		goto fail;

	close (fd);

	file = g_file_new_for_path (path);
	e_mime_part_utils_save_to_file (mime_part, file, &error);
	g_free (path);

	if (error != NULL) {
		g_object_unref (file);
		goto fail;
	}

	uri = g_file_get_uri (file);
	e_show_uri (parent, uri);
	g_free (uri);

	g_object_unref (file);

	return;

fail:
	g_warning ("%s", error->message);
	g_error_free (error);
}

GList *
e_mime_part_utils_get_apps (CamelMimePart *mime_part)
{
	GList *app_info_list;
	const gchar *filename;
	gchar *content_type;
	gchar *mime_type;
	gchar *cp;

	g_return_val_if_fail (CAMEL_IS_MIME_PART (mime_part), NULL);

	filename = camel_mime_part_get_filename (mime_part);
	mime_type = camel_content_type_simple (
		camel_mime_part_get_content_type (mime_part));
	g_return_val_if_fail (mime_type != NULL, NULL);

	/* GIO expects lowercase MIME types. */
	for (cp = mime_type; *cp != '\0'; cp++)
		*cp = g_ascii_tolower (*cp);

	content_type = g_content_type_from_mime_type (mime_type);
	if (content_type != NULL)
		app_info_list = g_app_info_get_all_for_type (content_type);
	else
		app_info_list = g_app_info_get_all_for_type (mime_type);
	g_free (content_type);

	if (app_info_list != NULL || filename == NULL)
		goto exit;

	if (strcmp (mime_type, "application/octet-stream") != 0)
		goto exit;

	content_type = g_content_type_guess (filename, NULL, 0, NULL);
	app_info_list = g_app_info_get_all_for_type (content_type);
	g_free (content_type);

exit:
	g_free (mime_type);

	return app_info_list;
}

gboolean
e_mime_part_utils_save_to_file (CamelMimePart *mime_part,
                                GFile *file,
                                GError **error)
{
	GFileOutputStream *output_stream;
	CamelDataWrapper *content;
	CamelStream *stream;

	g_return_val_if_fail (CAMEL_IS_MIME_PART (mime_part), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	output_stream = g_file_replace (
		file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, error);
	if (output_stream == NULL)
		return FALSE;

	/* The CamelStream takes ownership of the GFileOutputStream. */
	content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	stream = camel_stream_vfs_new_with_stream (G_OBJECT (output_stream));

	/* XXX Camel's streams are synchronous only, so we have to write
	 *     the whole thing in one shot and hope it doesn't block the
	 *     main loop for too long. */
	if (camel_data_wrapper_decode_to_stream (content, stream) < 0)
		goto file_error;

	if (camel_stream_flush (stream) < 0)
		goto file_error;

	camel_object_unref (stream);

	return TRUE;

file_error:
	g_set_error (
		error, G_FILE_ERROR,
		g_file_error_from_errno (errno),
		"%s", g_strerror (errno));

	camel_object_unref (stream);

	return FALSE;
}

void
e_mime_part_utils_add_open_actions (CamelMimePart *mime_part,
                                    GtkUIManager *ui_manager,
                                    GtkActionGroup *action_group,
                                    const gchar *widget_path,
                                    GtkWindow *parent,
                                    guint merge_id)
{
	GList *app_info_list;
	GList *iter;

	g_return_if_fail (CAMEL_IS_MIME_PART (mime_part));
	g_return_if_fail (GTK_IS_UI_MANAGER (ui_manager));
	g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
	g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
	g_return_if_fail (widget_path != NULL);

	app_info_list = e_mime_part_utils_get_apps (mime_part);

	for (iter = app_info_list; iter != NULL; iter = iter->next) {
		GAppInfo *app_info = iter->data;
		GtkAction *action;
		const gchar *app_executable;
		const gchar *app_name;
		gchar *action_tooltip;
		gchar *action_label;
		gchar *action_name;

		if (!g_app_info_should_show (app_info))
			continue;

		app_executable = g_app_info_get_executable (app_info);
		app_name = g_app_info_get_name (app_info);

		action_name = g_strdup_printf ("open-in-%s", app_executable);
		action_label = g_strdup_printf (_("Open in %s..."), app_name);

		action_tooltip = g_strdup_printf (
			_("Open this attachment in %s"), app_name);

		action = gtk_action_new (
			action_name, action_label, action_tooltip, NULL);

		g_object_set_data (
			G_OBJECT (action), "parent-window", parent);

		g_signal_connect (
			action, "activate",
			G_CALLBACK (mime_part_utils_open_in_cb), mime_part);

		gtk_action_group_add_action (action_group, action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id, widget_path, action_name,
			action_name, GTK_UI_MANAGER_AUTO, FALSE);

		g_free (action_name);
		g_free (action_label);
		g_free (action_tooltip);
	}

	g_list_foreach (app_info_list, (GFunc) g_object_unref, NULL);
	g_list_free (app_info_list);
}
