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
#include <ical.h>



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
	GtkObject object;

	/* Private data */
	ETimezoneDialogPrivate *priv;
};

struct _ETimezoneDialogClass {
	GtkObjectClass parent_class;
};


GtkType		 e_timezone_dialog_get_type		(void);
ETimezoneDialog* e_timezone_dialog_construct		(ETimezoneDialog  *etd);

ETimezoneDialog* e_timezone_dialog_new			(void);

/* Returns the TZID of the timezone set, and optionally its displayed name.
   The TZID may be NULL, in which case the builtin timezone with the city name
   of display_name should be used. If display_name is also NULL or "", then it
   is assumed to be a 'local time'. Note that display_name may be translated,
   so you need to convert it back to English before trying to load it. 
   It will be in the GTK+ encoding, i.e. not UTF-8. */
char*		 e_timezone_dialog_get_timezone		(ETimezoneDialog  *etd,
							 const char	 **display_name);

/* Sets the TZID and displayed name of the timezone. The TZID may be NULL for
   a 'local time' (i.e. display_name is NULL or "") or if it is a builtin
   timezone which hasn't been loaded yet. (This is done so we don't load
   timezones until we really need them.) The display_name should be the
   translated name in the GTK+ - it will be displayed exactly as it is. */
void		 e_timezone_dialog_set_timezone		(ETimezoneDialog  *etd,
							 char		  *tzid,
							 char		  *display_name);

GtkWidget*	 e_timezone_dialog_get_toplevel		(ETimezoneDialog  *etd);

void             e_timezone_dialog_reparent             (ETimezoneDialog *etd,
							 GtkWidget *new_parent);

/* Returns the builtin timezone corresponding to display_name, which is
   the translated location, e.g. 'Europe/London', in the GTK+ encoding.
   If display_name is NULL or "" it returns NULL. */
icaltimezone *e_timezone_dialog_get_builtin_timezone  (const char *display_name);



#endif /* __E_TIMEZONE_DIALOG_H__ */
