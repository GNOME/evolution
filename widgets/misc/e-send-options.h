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

#ifndef __E_SENDOPTIONS_DIALOG_H__
#define __E_SENDOPTIONS_DIALOG_H__

#include <gtk/gtkwidget.h>
#include <gtk/gtk.h>
#include <time.h>

#define E_TYPE_SENDOPTIONS_DIALOG       (e_sendoptions_dialog_get_type ())
#define E_SENDOPTIONS_DIALOG(obj)       (GTK_CHECK_CAST ((obj), E_TYPE_SENDOPTIONS_DIALOG, ESendOptionsDialog))
#define E_SENDOPTIONS_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SENDOPTIONS_DIALOG, ESendOptionsDialogClass))
#define E_IS_SENDOPTIONS_DIALOG(obj)    (GTK_CHECK_TYPE ((obj), E_TYPE_SENDOPTIONS_DIALOG))
#define E_IS_SENDOPTIONS_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_SENDOPTIONS_DIALOG))

typedef struct _ESendOptionsDialog		ESendOptionsDialog;
typedef struct _ESendOptionsDialogClass		ESendOptionsDialogClass;
typedef struct _ESendOptionsDialogPrivate	ESendOptionsDialogPrivate;

typedef enum {
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
	gboolean reply_enabled;
	gboolean reply_convenient;
	gint reply_within;
	gboolean expiration_enabled;
	gint expire_after;
	gboolean delay_enabled;
	time_t delay_until;
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
	
} ESendOptionsData;

struct _ESendOptionsDialog {
	GObject object;

	ESendOptionsData *data;
	/* Private data */
	ESendOptionsDialogPrivate *priv;
};

struct _ESendOptionsDialogClass {
	GObjectClass parent_class;
};

GType  e_sendoptions_dialog_get_type     (void);
ESendOptionsDialog *e_sendoptions_dialog_new (void);
void e_sendoptions_set_need_general_options (ESendOptionsDialog *sod, gboolean needed);
gboolean e_sendoptions_get_need_general_options (ESendOptionsDialog *sod);
gboolean e_sendoptions_dialog_run (ESendOptionsDialog *sod, GtkWidget *parent, Item_type type);

#endif 
