/*
 * e-mail-folder-create-dialog.h
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

#ifndef E_MAIL_FOLDER_CREATE_DIALOG_H
#define E_MAIL_FOLDER_CREATE_DIALOG_H

#include <mail/em-folder-selector.h>
#include <mail/e-mail-ui-session.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_FOLDER_CREATE_DIALOG \
	(e_mail_folder_create_dialog_get_type ())
#define E_MAIL_FOLDER_CREATE_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_FOLDER_CREATE_DIALOG, EMailFolderCreateDialog))
#define E_MAIL_FOLDER_CREATE_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_FOLDER_CREATE_DIALOG, EMailFolderCreateDialogClass))
#define E_IS_MAIL_FOLDER_CREATE_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_FOLDER_CREATE_DIALOG))
#define E_IS_MAIL_FOLDER_CREATE_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_FOLDER_CREATE_DIALOG))
#define E_MAIL_FOLDER_CREATE_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_FOLDER_CREATE_DIALOG, EMailFolderCreateDialogClass))

G_BEGIN_DECLS

typedef struct _EMailFolderCreateDialog EMailFolderCreateDialog;
typedef struct _EMailFolderCreateDialogClass EMailFolderCreateDialogClass;
typedef struct _EMailFolderCreateDialogPrivate EMailFolderCreateDialogPrivate;

struct _EMailFolderCreateDialog {
	EMFolderSelector parent;
	EMailFolderCreateDialogPrivate *priv;
};

struct _EMailFolderCreateDialogClass {
	EMFolderSelectorClass parent_class;

	/* Signals */
	void		(*folder_created)
					(EMailFolderCreateDialog *dialog,
					 CamelStore *store,
					 const gchar *folder_name);
};

GType		e_mail_folder_create_dialog_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_folder_create_dialog_new
					(GtkWindow *parent,
					 EMailUISession *session);
EMailUISession *
		e_mail_folder_create_dialog_get_session
					(EMailFolderCreateDialog *dialog);

G_END_DECLS

#endif /* E_MAIL_FOLDER_CREATE_DIALOG_H */

