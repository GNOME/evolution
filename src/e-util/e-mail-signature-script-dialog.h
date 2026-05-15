/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_MAIL_SIGNATURE_SCRIPT_DIALOG_H
#define E_MAIL_SIGNATURE_SCRIPT_DIALOG_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SIGNATURE_SCRIPT_DIALOG \
	(e_mail_signature_script_dialog_get_type ())
#define E_MAIL_SIGNATURE_SCRIPT_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SIGNATURE_SCRIPT_DIALOG, \
	EMailSignatureScriptDialog))
#define E_MAIL_SIGNATURE_SCRIPT_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SIGNATURE_SCRIPT_DIALOG, \
	EMailSignatureScriptDialogClass))
#define E_IS_MAIL_SIGNATURE_SCRIPT_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SIGNATURE_SCRIPT_DIALOG))
#define E_IS_MAIL_SIGNATURE_SCRIPT_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SIGNATURE_SCRIPT_DIALOG))
#define E_MAIL_SIGNATURE_SCRIPT_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SIGNATURE_SCRIPT_DIALOG, \
	EMailSignatureScriptDialogClass))

G_BEGIN_DECLS

typedef struct _EMailSignatureScriptDialog EMailSignatureScriptDialog;
typedef struct _EMailSignatureScriptDialogClass EMailSignatureScriptDialogClass;
typedef struct _EMailSignatureScriptDialogPrivate EMailSignatureScriptDialogPrivate;

struct _EMailSignatureScriptDialog {
	GtkDialog parent;
	EMailSignatureScriptDialogPrivate *priv;
};

struct _EMailSignatureScriptDialogClass {
	GtkDialogClass parent_class;
};

GType		e_mail_signature_script_dialog_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_signature_script_dialog_new
					(ESourceRegistry *registry,
					 GtkWindow *parent,
					 ESource *source);
ESourceRegistry *
		e_mail_signature_script_dialog_get_registry
					(EMailSignatureScriptDialog *dialog);
ESource *	e_mail_signature_script_dialog_get_source
					(EMailSignatureScriptDialog *dialog);
const gchar *	e_mail_signature_script_dialog_get_symlink_target
					(EMailSignatureScriptDialog *dialog);
void		e_mail_signature_script_dialog_set_symlink_target
					(EMailSignatureScriptDialog *dialog,
					 const gchar *symlink_target);
void		e_mail_signature_script_dialog_commit
					(EMailSignatureScriptDialog *dialog,
					 GCancellable *cancellable,
					 GAsyncReadyCallback callback,
					 gpointer user_data);
gboolean	e_mail_signature_script_dialog_commit_finish
					(EMailSignatureScriptDialog *dialog,
					 GAsyncResult *result,
					 GError **error);

G_END_DECLS

#endif /* E_MAIL_SIGNATURE_SCRIPT_DIALOG_H */
