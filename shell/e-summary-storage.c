/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-storage.c
 *
 * Copyright (C) 2001  Ximian, Inc.
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

#include "e-summary-storage.h"

#include "e-folder.h"

#include <gal/util/e-unicode-i18n.h>

#include <gal/util/e-util.h>

#include <gtk/gtktypeutils.h>

#include <libgnome/gnome-i18n.h>


#define PARENT_TYPE e_storage_get_type ()
static EStorageClass *parent_class = NULL;


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* EStorage methods.  */

static GList *
impl_get_subfolder_paths  (EStorage *storage,
			   const char *path)
{
	/* We never have any child folders.  */
	return NULL;
}

static EFolder *
impl_get_folder (EStorage *storage,
		 const char *path)
{
	return NULL;
}

static const char *
impl_get_name (EStorage *storage)
{
	return E_SUMMARY_STORAGE_NAME;
}

static const char *
impl_get_display_name (EStorage *storage)
{
	return U_("Summary");
}


static void
class_init (GtkObjectClass *object_class)
{
	EStorageClass *storage_class;

	object_class->destroy = impl_destroy;

	storage_class = E_STORAGE_CLASS (object_class);
	storage_class->get_subfolder_paths = impl_get_subfolder_paths;
	storage_class->get_folder          = impl_get_folder;
	storage_class->get_name            = impl_get_name;
	storage_class->get_display_name    = impl_get_display_name;

	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
init (ESummaryStorage *summary_storage)
{
	/* No members.  */
}


/**
 * e_summary_storage_new:
 * 
 * Create a new summary storage.
 * 
 * Return value: The newly created ESummaryStorage object.
 **/
EStorage *
e_summary_storage_new (void)
{
	EStorage *storage;

	storage = gtk_type_new (e_summary_storage_get_type ());

	e_storage_construct (storage, "/", "summary");

	return storage;
}


E_MAKE_TYPE (e_summary_storage, "ESummaryStorage", ESummaryStorage, class_init, init, PARENT_TYPE)
