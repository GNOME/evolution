/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2000, Helix Code, Inc.
 * Copyright 2000, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * CalPrefsDialog - a GtkObject which handles a libglade-loaded dialog
 * to edit the calendar preference settings.
 */

#ifndef _CAL_PREFS_DIALOG_H_
#define _CAL_PREFS_DIALOG_H_

#include <gtk/gtkobject.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS


#define CAL_PREFS_DIALOG(obj)          GTK_CHECK_CAST (obj, cal_prefs_dialog_get_type (), CalPrefsDialog)
#define CAL_PREFS_DIALOG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, cal_prefs_dialog_get_type (), CalPrefsDialogClass)
#define IS_CAL_PREFS_DIALOG(obj)       GTK_CHECK_TYPE (obj, cal_prefs_dialog_get_type ())


typedef struct _CalPrefsDialog       CalPrefsDialog;
typedef struct _CalPrefsDialogClass  CalPrefsDialogClass;

struct _CalPrefsDialog
{
	GtkObject object;

	/* Private data */
	gpointer priv;
};

struct _CalPrefsDialogClass
{
	GtkObjectClass parent_class;
};


GtkType		cal_prefs_dialog_get_type	(void);
CalPrefsDialog* cal_prefs_dialog_construct	(CalPrefsDialog *prefs);
CalPrefsDialog* cal_prefs_dialog_new		(void);

void		cal_prefs_dialog_show		(CalPrefsDialog *prefs);

END_GNOME_DECLS

#endif /* _CAL_PREFS_DIALOG_H_ */
