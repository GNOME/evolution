/*
 * Evolution calendar - Timezone selector dialog
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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_TIMEZONE_DIALOG_H__
#define __E_TIMEZONE_DIALOG_H__

#include <gtk/gtk.h>
#include <libical/ical.h>



#define E_TYPE_TIMEZONE_DIALOG       (e_timezone_dialog_get_type ())
#define E_TIMEZONE_DIALOG(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_TIMEZONE_DIALOG, ETimezoneDialog))
#define E_TIMEZONE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_TIMEZONE_DIALOG,	\
				      ETimezoneDialogClass))
#define E_IS_TIMEZONE_DIALOG(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_TIMEZONE_DIALOG))
#define E_IS_TIMEZONE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_TIMEZONE_DIALOG))

typedef struct _ETimezoneDialog		ETimezoneDialog;
typedef struct _ETimezoneDialogClass	ETimezoneDialogClass;
typedef struct _ETimezoneDialogPrivate	ETimezoneDialogPrivate;

struct _ETimezoneDialog {
	GObject object;

	/* Private data */
	ETimezoneDialogPrivate *priv;
};

struct _ETimezoneDialogClass {
	GObjectClass parent_class;
};

GType            e_timezone_dialog_get_type     (void);
ETimezoneDialog *e_timezone_dialog_construct    (ETimezoneDialog  *etd);

ETimezoneDialog *e_timezone_dialog_new          (void);

icaltimezone    *e_timezone_dialog_get_timezone (ETimezoneDialog  *etd);
void             e_timezone_dialog_set_timezone (ETimezoneDialog  *etd,
						 icaltimezone     *zone);

GtkWidget       *e_timezone_dialog_get_toplevel (ETimezoneDialog  *etd);

void             e_timezone_dialog_reparent     (ETimezoneDialog  *etd,
						 GtkWidget        *new_parent);

#endif /* __E_TIMEZONE_DIALOG_H__ */
