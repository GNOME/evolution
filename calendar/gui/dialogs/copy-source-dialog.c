/* Evolution calendar - Copy source dialog
 *
 * Copyright (C) 2003 Novell, Inc.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkmessagedialog.h>
#include <bonobo/bonobo-i18n.h>
#include "copy-source-dialog.h"
#include "select-source-dialog.h"
#include "common/authentication.h"

typedef struct {
	ESource *orig_source;
	ECalSourceType obj_type;
	ESource *selected_source;
} CopySourceDialogData;

static void
show_error (GtkWindow *parent, const char *msg)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (parent, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, msg);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
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
	source_client = auth_new_cal_from_source (csdd->orig_source, csdd->obj_type);
	if (!e_cal_open (source_client, TRUE, NULL)) {
		show_error (NULL, _("Could not open source"));
		g_object_unref (source_client);
		return FALSE;
	}

	/* open the destination */
	dest_client = auth_new_cal_from_source (csdd->selected_source, csdd->obj_type);
	if (!e_cal_open (dest_client, FALSE, NULL)) {
		show_error (NULL, _("Could not open destination"));
		g_object_unref (dest_client);
		g_object_unref (source_client);
		return FALSE;
	}

	/* check if the destination is read only */
	e_cal_is_read_only (dest_client, &read_only, NULL);
	if (read_only) {
		show_error (NULL, _("Destination is read only"));
	} else {
		if (e_cal_get_object_list (source_client, "#t", &obj_list, NULL)) {
			GList *l;
			const char *uid;
			icalcomponent *icalcomp;

			for (l = obj_list; l != NULL; l = l->next) {
				/* FIXME: process recurrences */
				/* FIXME: process errors */
				if (e_cal_get_object (dest_client, icalcomponent_get_uid (l->data), NULL,
						      &icalcomp, NULL)) {
					e_cal_modify_object (dest_client, l->data, CALOBJ_MOD_ALL, NULL);
					icalcomponent_free (icalcomp);
				} else {
					e_cal_create_object (dest_client, l->data, (char **) &uid, NULL);
					g_free ((gpointer) uid);
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

	csdd.orig_source = source;
	csdd.selected_source = NULL;
	csdd.obj_type = obj_type;

	csdd.selected_source = select_source_dialog (parent, obj_type);
	if (csdd.selected_source) {
		result = copy_source (&csdd);

		/* free memory */
		g_object_unref (csdd.selected_source);
	}

	return result;
}
