/*
 * Evolution calendar - Copy source dialog
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
 * Authors:
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "e-util/e-util.h"

#include "copy-source-dialog.h"
#include "select-source-dialog.h"

typedef struct {
	GtkWindow *parent;
	ESource *orig_source;
	ECalClientSourceType obj_type;
	ESource *selected_source;
	ECalClient *source_client, *dest_client;
} CopySourceDialogData;

static void
show_error (CopySourceDialogData *csdd,
            const gchar *msg,
            const GError *error)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
		csdd->parent, 0, GTK_MESSAGE_ERROR,
		GTK_BUTTONS_CLOSE, error ? "%s\n%s" : "%s", msg, error ? error->message : "");
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

struct ForeachTzidData
{
	ECalClient *source_client;
	ECalClient *dest_client;
};

static void
add_timezone_to_cal_cb (icalparameter *param,
                        gpointer data)
{
	struct ForeachTzidData *ftd = data;
	icaltimezone *tz = NULL;
	const gchar *tzid;

	g_return_if_fail (ftd != NULL);
	g_return_if_fail (ftd->source_client != NULL);
	g_return_if_fail (ftd->dest_client != NULL);

	tzid = icalparameter_get_tzid (param);
	if (!tzid || !*tzid)
		return;

	e_cal_client_get_timezone_sync (
		ftd->source_client, tzid, &tz, NULL, NULL);
	if (tz != NULL)
		e_cal_client_add_timezone_sync (
			ftd->dest_client, tz, NULL, NULL);
}

static void
free_copy_data (CopySourceDialogData *csdd)
{
	if (!csdd)
		return;

	if (csdd->orig_source)
		g_object_unref (csdd->orig_source);
	if (csdd->selected_source)
		g_object_unref (csdd->selected_source);
	if (csdd->source_client)
		g_object_unref (csdd->source_client);
	if (csdd->dest_client)
		g_object_unref (csdd->dest_client);
	g_free (csdd);
}

static void
dest_source_connected_cb (GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
	CopySourceDialogData *csdd = user_data;
	EClient *client;
	GError *error = NULL;

	client = e_cal_client_connect_finish (result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		show_error (csdd, _("Could not open destination"), error);
		g_error_free (error);
		free_copy_data (csdd);
		return;
	}

	csdd->dest_client = E_CAL_CLIENT (client);

	/* check if the destination is read only */
	if (e_client_is_readonly (E_CLIENT (csdd->dest_client))) {
		show_error (csdd, _("Destination is read only"), NULL);
	} else {
		GSList *obj_list = NULL;

		e_cal_client_get_object_list_sync (
			csdd->source_client, "#t", &obj_list, NULL, NULL);
		if (obj_list != NULL) {
			GSList *l;
			icalcomponent *icalcomp;
			struct ForeachTzidData ftd;

			ftd.source_client = csdd->source_client;
			ftd.dest_client = csdd->dest_client;

			for (l = obj_list; l != NULL; l = l->next) {
				icalcomp = NULL;

				/* FIXME: process recurrences */
				/* FIXME: process errors */
				if (e_cal_client_get_object_sync (
					csdd->dest_client,
					icalcomponent_get_uid (l->data),
					NULL, &icalcomp, NULL, NULL) &&
				    icalcomp != NULL) {
					e_cal_client_modify_object_sync (
						csdd->dest_client, l->data,
						CALOBJ_MOD_ALL, NULL, NULL);
					icalcomponent_free (icalcomp);
				} else {
					GError *error = NULL;

					icalcomp = l->data;

					/* Add timezone information from source
					 * ECal to the destination ECal. */
					icalcomponent_foreach_tzid (
						icalcomp,
						add_timezone_to_cal_cb, &ftd);

					e_cal_client_create_object_sync (
						csdd->dest_client,
						icalcomp, NULL, NULL, &error);

					if (error != NULL) {
						show_error (csdd, _("Cannot create object"), error);
						g_error_free (error);
						break;
					}
				}
			}

			e_cal_client_free_icalcomp_slist (obj_list);
		}
	}

	free_copy_data (csdd);
}

static void
orig_source_connected_cb (GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
	CopySourceDialogData *csdd = user_data;
	EClient *client;
	GError *error = NULL;

	client = e_cal_client_connect_finish (result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		show_error (csdd, _("Could not open source"), error);
		g_error_free (error);
		free_copy_data (csdd);
		return;
	}

	csdd->source_client = E_CAL_CLIENT (client);

	e_cal_client_connect (
		csdd->selected_source, csdd->obj_type, NULL,
		dest_source_connected_cb, csdd);
}

static void
copy_source (const CopySourceDialogData *const_csdd)
{
	CopySourceDialogData *csdd;

	if (!const_csdd->selected_source)
		return;

	csdd = g_new0 (CopySourceDialogData, 1);
	csdd->parent = const_csdd->parent;
	csdd->orig_source = g_object_ref (const_csdd->orig_source);
	csdd->obj_type = const_csdd->obj_type;
	csdd->selected_source = g_object_ref (const_csdd->selected_source);

	e_cal_client_connect (
		csdd->orig_source, csdd->obj_type, NULL,
		orig_source_connected_cb, csdd);
}

/**
 * copy_source_dialog
 *
 * Implements the Copy command for sources, allowing the user to select a target
 * source to copy to.
 */
void
copy_source_dialog (GtkWindow *parent,
                    ESourceRegistry *registry,
                    ESource *source,
                    ECalClientSourceType obj_type)
{
	CopySourceDialogData csdd;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (obj_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS ||
			  obj_type == E_CAL_CLIENT_SOURCE_TYPE_TASKS ||
			  obj_type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS);

	csdd.parent = parent;
	csdd.orig_source = source;
	csdd.selected_source = NULL;
	csdd.obj_type = obj_type;

	csdd.selected_source = select_source_dialog (
		parent, registry, obj_type, source);
	if (csdd.selected_source) {
		copy_source (&csdd);

		/* free memory */
		g_object_unref (csdd.selected_source);
	}
}
