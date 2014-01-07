/*
 * e-google-chooser-dialog.h
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

#ifndef E_GOOGLE_CHOOSER_DIALOG_H
#define E_GOOGLE_CHOOSER_DIALOG_H

#include "e-google-chooser.h"

/* Standard GObject macros */
#define E_TYPE_GOOGLE_CHOOSER_DIALOG \
	(e_google_chooser_dialog_get_type ())
#define E_GOOGLE_CHOOSER_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_GOOGLE_CHOOSER_DIALOG, EGoogleChooserDialog))
#define E_GOOGLE_CHOOSER_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_GOOGLE_CHOOSER_DIALOG, EGoogleChooserDialogClass))
#define E_IS_GOOGLE_CHOOSER_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_GOOGLE_CHOOSER_DIALOG))
#define E_IS_GOOGLE_CHOOSER_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_GOOGLE_CHOOSER_DIALOG))
#define E_GOOGLE_CHOOSER_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_GOOGLE_CHOOSER_DIALOG, EGoogleChooserDialogClass))

G_BEGIN_DECLS

typedef struct _EGoogleChooserDialog EGoogleChooserDialog;
typedef struct _EGoogleChooserDialogClass EGoogleChooserDialogClass;
typedef struct _EGoogleChooserDialogPrivate EGoogleChooserDialogPrivate;

struct _EGoogleChooserDialog {
	GtkDialog parent;
	EGoogleChooserDialogPrivate *priv;
};

struct _EGoogleChooserDialogClass {
	GtkDialogClass parent_class;
};

GType		e_google_chooser_dialog_get_type (void);
void		e_google_chooser_dialog_type_register
						(GTypeModule *type_module);
GtkWidget *	e_google_chooser_dialog_new	(EGoogleChooser *chooser,
						 GtkWindow *parent);
EGoogleChooser *e_google_chooser_dialog_get_chooser
						(EGoogleChooserDialog *dialog);

G_END_DECLS

#endif /* E_GOOGLE_CHOOSER_DIALOG_H */
