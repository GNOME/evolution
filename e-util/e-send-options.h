/*
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

#ifndef E_SEND_OPTIONS_DIALOG_H
#define E_SEND_OPTIONS_DIALOG_H

#include <gtk/gtk.h>
#include <time.h>

/* Standard GObject macros */
#define E_TYPE_SEND_OPTIONS_DIALOG \
	(e_send_options_dialog_get_type ())
#define E_SEND_OPTIONS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SEND_OPTIONS_DIALOG, ESendOptionsDialog))
#define E_SEND_OPTIONS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SEND_OPTIONS_DIALOG, ESendOptionsDialogClass))
#define E_IS_SEND_OPTIONS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SEND_OPTIONS_DIALOG))
#define E_IS_SEND_OPTIONS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SEND_OPTIONS_DIALOG))
#define E_SEND_OPTIONS_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SEND_OPTIONS_DIALOG, ESendOptionsDialogClass))

G_BEGIN_DECLS

typedef struct _ESendOptionsDialog ESendOptionsDialog;
typedef struct _ESendOptionsDialogClass ESendOptionsDialogClass;
typedef struct _ESendOptionsDialogPrivate ESendOptionsDialogPrivate;

typedef enum {
	E_ITEM_NONE,
	E_ITEM_MAIL,
	E_ITEM_CALENDAR,
	E_ITEM_TASK
} Item_type;

typedef enum {
	E_PRIORITY_UNDEFINED,
	E_PRIORITY_HIGH,
	E_PRIORITY_STANDARD,
	E_PRIORITY_LOW
} ESendOptionsPriority;

typedef enum {
	E_SECURITY_NORMAL,
	E_SECURITY_PROPRIETARY,
	E_SECURITY_CONFIDENTIAL,
	E_SECURITY_SECRET,
	E_SECURITY_TOP_SECRET,
	E_SECURITY_FOR_YOUR_EYES_ONLY
} ESendOptionsSecurity;

typedef enum {
	E_RETURN_NOTIFY_NONE,
	E_RETURN_NOTIFY_MAIL
} ESendOptionsReturnNotify;

typedef enum {
	E_DELIVERED = 1,
	E_DELIVERED_OPENED = 2,
	E_ALL = 3
} TrackInfo;

typedef struct {
	ESendOptionsPriority priority;
	gint classify;
	gboolean reply_enabled;
	gboolean reply_convenient;
	gint reply_within;
	gboolean expiration_enabled;
	gint expire_after;
	gboolean delay_enabled;
	time_t delay_until;
	gint security;
} ESendOptionsGeneral;

typedef struct {
	gboolean tracking_enabled;
	TrackInfo track_when;
	gboolean autodelete;
	ESendOptionsReturnNotify opened;
	ESendOptionsReturnNotify accepted;
	ESendOptionsReturnNotify declined;
	ESendOptionsReturnNotify completed;
} ESendOptionsStatusTracking;

typedef struct {
	gboolean initialized;

	ESendOptionsGeneral *gopts;
	ESendOptionsStatusTracking *sopts;
	ESendOptionsStatusTracking *mopts;
	ESendOptionsStatusTracking *copts;
	ESendOptionsStatusTracking *topts;
} ESendOptionsData;

struct _ESendOptionsDialog {
	GObject parent;

	ESendOptionsData *data;
	ESendOptionsDialogPrivate *priv;
};

struct _ESendOptionsDialogClass {
	GObjectClass parent_class;

	void		(*sod_response)		(ESendOptionsDialog *sod,
						 gint status);
};

GType		e_send_options_dialog_get_type	(void) G_GNUC_CONST;
ESendOptionsDialog *
		e_send_options_dialog_new	(void);
void		e_send_options_set_need_general_options
						(ESendOptionsDialog *sod,
						 gboolean needed);
gboolean	e_send_options_get_need_general_options
						(ESendOptionsDialog *sod);
gboolean	e_send_options_dialog_run	(ESendOptionsDialog *sod,
						 GtkWidget *parent,
						 Item_type type);
gboolean	e_send_options_set_global	(ESendOptionsDialog *sod,
						 gboolean set);

#endif /* E_SEND_OPTIONS_DIALOG_H */
