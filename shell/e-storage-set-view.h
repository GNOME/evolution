/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-set-view.h
 *
 * Copyright (C) 2000  Helix Code, Inc.
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

#ifndef __E_STORAGE_SET_VIEW_H__
#define __E_STORAGE_SET_VIEW_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-table/e-table.h"
#include "e-storage-set.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_STORAGE_SET_VIEW			(e_storage_set_view_get_type ())
#define E_STORAGE_SET_VIEW(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_STORAGE_SET_VIEW, EStorageSetView))
#define E_STORAGE_SET_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_STORAGE_SET_VIEW, EStorageSetViewClass))
#define E_IS_STORAGE_SET_VIEW(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_STORAGE_SET_VIEW))
#define E_IS_STORAGE_SET_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_STORAGE_SET_VIEW))


typedef struct _EStorageSetView        EStorageSetView;
typedef struct _EStorageSetViewPrivate EStorageSetViewPrivate;
typedef struct _EStorageSetViewClass   EStorageSetViewClass;

struct _EStorageSetView {
	ETable parent;

	EStorageSetViewPrivate *priv;
};

struct _EStorageSetViewClass {
	ETableClass parent_class;

	/* Signals.  */

	void (* folder_selected)  (EStorageSetView *storage_set_view,
				   const char *uri);
};


GtkType     e_storage_set_view_get_type            (void);
GtkWidget  *e_storage_set_view_new                 (EStorageSet     *storage_set);
void        e_storage_set_view_construct           (EStorageSetView *storage_set_view,
						    EStorageSet     *storage_set);
void        e_storage_set_view_set_current_folder  (EStorageSetView *storage_set_view,
						    const char      *path);
const char *e_storage_set_view_get_current_folder  (EStorageSetView *storage_set_view);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_STORAGE_SET_VIEW_H__ */
