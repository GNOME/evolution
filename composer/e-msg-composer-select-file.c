/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer-select-file.c
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#include <gtk/gtkfilesel.h>

#include "e-msg-composer-select-file.h"


struct _FileSelectionInfo {
	GtkWidget *widget;
	char *selected_file;
};
typedef struct _FileSelectionInfo FileSelectionInfo;


static void
confirm (FileSelectionInfo *info)
{
	const char *filename;

	filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (info->widget));
	info->selected_file = g_strdup (filename);

	gtk_widget_hide (info->widget);

	gtk_main_quit ();
}

static void
cancel (FileSelectionInfo *info)
{
	g_assert (info->selected_file == NULL);

	gtk_widget_hide (info->widget);

	gtk_main_quit ();
}


/* Callbacks.  */

static void
ok_clicked_cb (GtkWidget *widget,
	       void *data)
{
	FileSelectionInfo *info;

	info = (FileSelectionInfo *) data;
	confirm (info);
}

static void
cancel_clicked_cb (GtkWidget *widget,
		   void *data)
{
	FileSelectionInfo *info;

	info = (FileSelectionInfo *) data;
	cancel (info);
}

static int
delete_event_cb (GtkWidget *widget,
		 GdkEventAny *event,
		 void *data)
{
	FileSelectionInfo *info;

	info = (FileSelectionInfo *) data;
	cancel (info);

	return TRUE;
}


/* Setup.  */

static FileSelectionInfo *
create_file_selection (EMsgComposer *composer)
{
	FileSelectionInfo *info;
	GtkWidget *widget;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;

	info = g_new (FileSelectionInfo, 1);

	widget        = gtk_file_selection_new (NULL);
	ok_button     = GTK_FILE_SELECTION (widget)->ok_button;
	cancel_button = GTK_FILE_SELECTION (widget)->cancel_button;

	gtk_signal_connect (GTK_OBJECT (ok_button),
			    "clicked", GTK_SIGNAL_FUNC (ok_clicked_cb), info);
	gtk_signal_connect (GTK_OBJECT (cancel_button),
			    "clicked", GTK_SIGNAL_FUNC (cancel_clicked_cb), info);
	gtk_signal_connect (GTK_OBJECT (widget), "delete_event",
			    GTK_SIGNAL_FUNC (delete_event_cb), info);

	info->widget        = widget;
	info->selected_file = NULL;

	return info;
}

static void
file_selection_info_destroy_notify (void *data)
{
	FileSelectionInfo *info;

	info = (FileSelectionInfo *) data;

	g_free (info->selected_file);
	g_free (info);
}


char *
e_msg_composer_select_file (EMsgComposer *composer,
			    const char *title)
{
	FileSelectionInfo *info;
	char *retval;

	g_return_val_if_fail (composer != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	info = gtk_object_get_data (GTK_OBJECT (composer),
				    "e-msg-composer-file-selection-info");

	if (info == NULL) {
		info = create_file_selection (composer);
		gtk_object_set_data_full (GTK_OBJECT (composer),
					  "e-msg-composer-file-selection-info", info,
					  file_selection_info_destroy_notify);
	}

	if (GTK_WIDGET_VISIBLE (info->widget))
		return NULL;		/* Busy!  */

	gtk_window_set_title (GTK_WINDOW (info->widget), title);
	gtk_widget_show (info->widget);

	GDK_THREADS_ENTER();
	gtk_main ();
	GDK_THREADS_LEAVE();

	retval = info->selected_file;
	info->selected_file = NULL;

	return retval;
}
