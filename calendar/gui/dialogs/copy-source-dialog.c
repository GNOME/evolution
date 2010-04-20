/*
 * Evolution calendar - Copy source dialog
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
#include "copy-source-dialog.h"
#include "select-source-dialog.h"
#include "common/authentication.h"

typedef struct {
	GtkWindow *parent;
	ESource *orig_source;
	ECalSourceType obj_type;
	ESource *selected_source;
} CopySourceDialogData;

static void
show_error (CopySourceDialogData *csdd, const gchar *msg)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
		csdd->parent, 0, GTK_MESSAGE_ERROR,
		GTK_BUTTONS_CLOSE, "%s", msg);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

struct ForeachTzidData
{
	ECal *source_client;
	ECal *dest_client;
};

static void
add_timezone_to_cal_cb (icalparameter *param, gpointer data)
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

	if (e_cal_get_timezone (ftd->source_client, tzid, &tz, NULL) && tz)
		e_cal_add_timezone (ftd->dest_client, tz, NULL);
}

static gboolean
copy_source (CopySourceDialogData *csdd)
{
	ECal *source_client, *dest_client;
	gboolean read_only = TRUE;
	GList *obj_list = NULL;
	gboolean result = FALSE;

	if (!csdd->selected_source)
		return FALSE;

	/* open the source */
	source_client = e_auth_new_cal_from_source (csdd->orig_source, csdd->obj_type);
	if (!e_cal_open (source_client, TRUE, NULL)) {
		show_error (csdd, _("Could not open source"));
		g_object_unref (source_client);
		return FALSE;
	}

	/* open the destination */
	dest_client = e_auth_new_cal_from_source (csdd->selected_source, csdd->obj_type);
	if (!e_cal_open (dest_client, FALSE, NULL)) {
		show_error (csdd, _("Could not open destination"));
		g_object_unref (dest_client);
		g_object_unref (source_client);
		return FALSE;
	}

	/* check if the destination is read only */
	e_cal_is_read_only (dest_client, &read_only, NULL);
	if (read_only) {
		show_error (csdd, _("Destination is read only"));
	} else {
		if (e_cal_get_object_list (source_client, "#t", &obj_list, NULL)) {
			GList *l;
			icalcomponent *icalcomp;
			struct ForeachTzidData ftd;

			ftd.source_client = source_client;
			ftd.dest_client = dest_client;

			for (l = obj_list; l != NULL; l = l->next) {
				/* FIXME: process recurrences */
				/* FIXME: process errors */
				if (e_cal_get_object (dest_client, icalcomponent_get_uid (l->data), NULL,
						      &icalcomp, NULL)) {
					e_cal_modify_object (dest_client, l->data, CALOBJ_MOD_ALL, NULL);
					icalcomponent_free (icalcomp);
				} else {
					gchar *uid = NULL;
					GError *error = NULL;

					icalcomp = l->data;

					/* add timezone information from source ECal to the destination ECal */
					icalcomponent_foreach_tzid (icalcomp, add_timezone_to_cal_cb, &ftd);

					if (e_cal_create_object (dest_client, icalcomp, &uid, &error)) {
						g_free (uid);
					} else {
						if (error) {
							show_error (csdd, error->message);
							g_error_free (error);
						}
						break;
					}
				}
			}

			e_cal_free_object_list (obj_list);
		}
	}

	/* free memory */
	g_object_unref (dest_client);
	g_object_unref (source_client);

	return result;
}

/**
 * copy_source_dialog
 *
 * Implements the Copy command for sources, allowing the user to select a target
 * source to copy to.
 */
gboolean
copy_source_dialog (GtkWindow *parent, ESource *source, ECalSourceType obj_type)
{
	CopySourceDialogData csdd;
	gboolean result = FALSE;

	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	csdd.parent = parent;
	csdd.orig_source = source;
	csdd.selected_source = NULL;
	csdd.obj_type = obj_type;

	csdd.selected_source = select_source_dialog (parent, obj_type, source);
	if (csdd.selected_source) {
		result = copy_source (&csdd);

		/* free memory */
		g_object_unref (csdd.selected_source);
	}

	return result;
}
