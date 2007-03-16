/* Evolution calendar - Timezone selector dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Damon Chaplin <damon@ximian.com>
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
 */

#ifndef __E_TIMEZONE_DIALOG_H__
#define __E_TIMEZONE_DIALOG_H__

#include <gtk/gtkwidget.h>
#include <libical/ical.h>



#define E_TYPE_TIMEZONE_DIALOG       (e_timezone_dialog_get_type ())
#define E_TIMEZONE_DIALOG(obj)       (GTK_CHECK_CAST ((obj), E_TYPE_TIMEZONE_DIALOG, ETimezoneDialog))
#define E_TIMEZONE_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_TIMEZONE_DIALOG,	\
				      ETimezoneDialogClass))
#define E_IS_TIMEZONE_DIALOG(obj)    (GTK_CHECK_TYPE ((obj), E_TYPE_TIMEZONE_DIALOG))
#define E_IS_TIMEZONE_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_TIMEZONE_DIALOG))


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
