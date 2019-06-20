/*
 * Evolution calendar - Timezone selector dialog
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TIMEZONE_DIALOG_H
#define E_TIMEZONE_DIALOG_H

#include <gtk/gtk.h>

#define LIBICAL_GLIB_UNSTABLE_API 1
#include <libical-glib/libical-glib.h>
#undef LIBICAL_GLIB_UNSTABLE_API

/* Standard GObject macros */
#define E_TYPE_TIMEZONE_DIALOG \
	(e_timezone_dialog_get_type ())
#define E_TIMEZONE_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TIMEZONE_DIALOG, ETimezoneDialog))
#define E_TIMEZONE_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TIMEZONE_DIALOG, ETimezoneDialogClass))
#define E_IS_TIMEZONE_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TIMEZONE_DIALOG))
#define E_IS_TIMEZONE_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TIMEZONE_DIALOG))
#define E_TIMEZONE_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TIMEZONE_DIALOG, ETimezoneDialogClass))

typedef struct _ETimezoneDialog ETimezoneDialog;
typedef struct _ETimezoneDialogClass ETimezoneDialogClass;
typedef struct _ETimezoneDialogPrivate ETimezoneDialogPrivate;

struct _ETimezoneDialog {
	GObject object;
	ETimezoneDialogPrivate *priv;
};

struct _ETimezoneDialogClass {
	GObjectClass parent_class;
};

GType		e_timezone_dialog_get_type	(void) G_GNUC_CONST;
ETimezoneDialog *
		e_timezone_dialog_construct	(ETimezoneDialog  *etd);
ETimezoneDialog *
		e_timezone_dialog_new		(void);
ICalTimezone *	e_timezone_dialog_get_timezone	(ETimezoneDialog *etd);
void		e_timezone_dialog_set_timezone	(ETimezoneDialog *etd,
						 const ICalTimezone *zone);
gboolean	e_timezone_dialog_get_allow_none(ETimezoneDialog *etd);
void		e_timezone_dialog_set_allow_none(ETimezoneDialog *etd,
						 gboolean allow_none);
GtkWidget *	e_timezone_dialog_get_toplevel	(ETimezoneDialog *etd);

#endif /* E_TIMEZONE_DIALOG_H */
