/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-config-offline.c - Configuration page for offline synchronization.
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "e-shell-config-offline.h"

#include "evolution-config-control.h"
#include "e-storage-set-view.h"

#include "Evolution.h"

#include <bonobo-conf/Bonobo_Config.h>
#include <bonobo/bonobo-exception.h>

#include <gal/widgets/e-scroll-frame.h>
#include <gtk/gtkwidget.h>


struct _PageData {
	EShell *shell;
	GtkWidget *storage_set_view;
	EvolutionConfigControl *config_control;
};
typedef struct _PageData PageData;


/* Callbacks.  */

static void
config_control_destroy_callback (GtkObject *object,
				 void *data)
{
	PageData *page_data;

	page_data = (PageData *) data;
	gtk_widget_destroy (page_data->storage_set_view);
	g_free (page_data);
}

static void
config_control_apply_callback (EvolutionConfigControl *config_control,
			       void *data)
{
	CORBA_Environment ev;
	CORBA_sequence_CORBA_string *paths;
	CORBA_any any;
	PageData *page_data;
	GList *checked_paths;
	GList *p;
	int i;

	page_data = (PageData *) data;

	checked_paths = e_storage_set_view_get_checkboxes_list (E_STORAGE_SET_VIEW (page_data->storage_set_view));

	paths = CORBA_sequence_CORBA_string__alloc ();
	paths->_maximum = paths->_length = g_list_length (checked_paths);
	paths->_buffer = CORBA_sequence_CORBA_string_allocbuf (paths->_maximum);

	for (p = checked_paths, i = 0; p != NULL; p = p->next, i ++)
		paths->_buffer[i] = CORBA_string_dup ((const char *) p->data);

	any._type = TC_CORBA_sequence_CORBA_string;
	any._value = paths;

	CORBA_exception_init (&ev);

	Bonobo_ConfigDatabase_setValue (e_shell_get_config_db (page_data->shell),
					"/OfflineFolders/paths", &any, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Cannot set /OfflineFolders/paths from ConfigDatabase -- %s", BONOBO_EX_ID (&ev));

	CORBA_exception_free (&ev);

	g_list_free (checked_paths);
}

static void
storage_set_view_checkboxes_changed_callback (EStorageSetView *storage_set_view,
					      void *data)
{
	PageData *page_data;

	page_data = (PageData *) data;
	evolution_config_control_changed (page_data->config_control);
}


/* Construction.  */

static void
init_storage_set_view_status_from_config (EStorageSetView *storage_set_view,
					  EShell *shell)
{
	Bonobo_ConfigDatabase config_db;
	CORBA_Environment ev;
	CORBA_any *any;
	CORBA_sequence_CORBA_string *sequence;
	GList *list;
	int i;

	config_db = e_shell_get_config_db (shell);
	g_assert (config_db != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	any = Bonobo_ConfigDatabase_getValue (config_db, "/OfflineFolders/paths", "", &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Cannot get /OfflineFolders/paths from ConfigDatabase -- %s", BONOBO_EX_ID (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	if (! CORBA_TypeCode_equal (any->_type, TC_CORBA_sequence_CORBA_string, &ev) || BONOBO_EX (&ev)) {
		g_warning ("/OfflineFolders/Paths in ConfigDatabase is not the expected type");
		CORBA_free (any);
		CORBA_exception_free (&ev);
		return;
	}

	sequence = (CORBA_sequence_CORBA_string *) any->_value;

	list = NULL;
	for (i = 0; i < sequence->_length; i ++)
		list = g_list_prepend (list, sequence->_buffer[i]);

	e_storage_set_view_set_checkboxes_list (storage_set_view, list);

	g_list_free (list);
	CORBA_free (any);

	CORBA_exception_free (&ev);
}

BonoboObject *
e_shell_config_offline_create_control (EShell *shell)
{
	PageData *page_data;
	GtkWidget *scroll_frame;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	page_data = g_new (PageData, 1);
	page_data->shell = shell;

	page_data->storage_set_view = e_storage_set_new_view (e_shell_get_storage_set (shell), NULL);
        e_storage_set_view_set_show_checkboxes (E_STORAGE_SET_VIEW (page_data->storage_set_view), TRUE);
	gtk_widget_show (page_data->storage_set_view);

	init_storage_set_view_status_from_config (E_STORAGE_SET_VIEW (page_data->storage_set_view), shell);
	gtk_signal_connect (GTK_OBJECT (page_data->storage_set_view), "checkboxes_changed",
			    GTK_SIGNAL_FUNC (storage_set_view_checkboxes_changed_callback), page_data);

	scroll_frame = e_scroll_frame_new (NULL, NULL);
	e_scroll_frame_set_shadow_type (E_SCROLL_FRAME (scroll_frame), GTK_SHADOW_IN);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (scroll_frame),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll_frame), page_data->storage_set_view);
	gtk_widget_show (scroll_frame);

	page_data->config_control = evolution_config_control_new (scroll_frame);

	gtk_signal_connect (GTK_OBJECT (page_data->config_control), "destroy",
			    GTK_SIGNAL_FUNC (config_control_destroy_callback), page_data);
	gtk_signal_connect (GTK_OBJECT (page_data->config_control), "apply",
			    GTK_SIGNAL_FUNC (config_control_apply_callback), page_data);

	return BONOBO_OBJECT (page_data->config_control);
}
