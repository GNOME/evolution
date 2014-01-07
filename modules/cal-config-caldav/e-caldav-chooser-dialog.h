/*
 * e-caldav-chooser-dialog.h
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
 */

#ifndef E_CALDAV_CHOOSER_DIALOG_H
#define E_CALDAV_CHOOSER_DIALOG_H

#include "e-caldav-chooser.h"

/* Standard GObject macros */
#define E_TYPE_CALDAV_CHOOSER_DIALOG \
	(e_caldav_chooser_dialog_get_type ())
#define E_CALDAV_CHOOSER_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CALDAV_CHOOSER_DIALOG, ECaldavChooserDialog))
#define E_CALDAV_CHOOSER_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CALDAV_CHOOSER_DIALOG, ECaldavChooserDialogClass))
#define E_IS_CALDAV_CHOOSER_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CALDAV_CHOOSER_DIALOG))
#define E_IS_CALDAV_CHOOSER_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CALDAV_CHOOSER_DIALOG))
#define E_CALDAV_CHOOSER_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CALDAV_CHOOSER_DIALOG, ECaldavChooserDialogClass))

G_BEGIN_DECLS

typedef struct _ECaldavChooserDialog ECaldavChooserDialog;
typedef struct _ECaldavChooserDialogClass ECaldavChooserDialogClass;
typedef struct _ECaldavChooserDialogPrivate ECaldavChooserDialogPrivate;

struct _ECaldavChooserDialog {
	GtkDialog parent;
	ECaldavChooserDialogPrivate *priv;
};

struct _ECaldavChooserDialogClass {
	GtkDialogClass parent_class;
};

GType		e_caldav_chooser_dialog_get_type (void);
void		e_caldav_chooser_dialog_type_register
						(GTypeModule *type_module);
GtkWidget *	e_caldav_chooser_dialog_new	(ECaldavChooser *chooser,
						 GtkWindow *parent);
ECaldavChooser *e_caldav_chooser_dialog_get_chooser
						(ECaldavChooserDialog *dialog);

G_END_DECLS

#endif /* E_CALDAV_CHOOSER_DIALOG_H */
