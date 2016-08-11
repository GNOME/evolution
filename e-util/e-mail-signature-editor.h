/*
 * e-mail-signature-editor.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_MAIL_SIGNATURE_EDITOR_H
#define E_MAIL_SIGNATURE_EDITOR_H

#include <libedataserver/libedataserver.h>

#include <e-util/e-html-editor.h>
#include <e-util/e-focus-tracker.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SIGNATURE_EDITOR \
	(e_mail_signature_editor_get_type ())
#define E_MAIL_SIGNATURE_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SIGNATURE_EDITOR, EMailSignatureEditor))
#define E_MAIL_SIGNATURE_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SIGNATURE_EDITOR, EMailSignatureEditorClass))
#define E_IS_MAIL_SIGNATURE_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SIGNATURE_EDITOR))
#define E_IS_MAIL_SIGNATURE_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SIGNATURE_EDITOR))
#define E_MAIL_SIGNATURE_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SIGNATURE_EDITOR, EMailSignatureEditorClass))

G_BEGIN_DECLS

typedef struct _EMailSignatureEditor EMailSignatureEditor;
typedef struct _EMailSignatureEditorClass EMailSignatureEditorClass;
typedef struct _EMailSignatureEditorPrivate EMailSignatureEditorPrivate;

struct _EMailSignatureEditor {
	GtkWindow parent;
	EMailSignatureEditorPrivate *priv;
};

struct _EMailSignatureEditorClass {
	GtkWindowClass parent_class;
};

GType		e_mail_signature_editor_get_type
						(void) G_GNUC_CONST;
void		e_mail_signature_editor_new	(ESourceRegistry *registry,
						 ESource *source,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
GtkWidget *	e_mail_signature_editor_new_finish
						(GAsyncResult *result,
						 GError **error);
EHTMLEditor *	e_mail_signature_editor_get_editor
						(EMailSignatureEditor *editor);
EFocusTracker *	e_mail_signature_editor_get_focus_tracker
						(EMailSignatureEditor *editor);
ESourceRegistry *
		e_mail_signature_editor_get_registry
						(EMailSignatureEditor *editor);
ESource *	e_mail_signature_editor_get_source
						(EMailSignatureEditor *editor);
void		e_mail_signature_editor_commit	(EMailSignatureEditor *editor,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_signature_editor_commit_finish
						(EMailSignatureEditor *editor,
						 GAsyncResult *result,
						 GError **error);

G_END_DECLS

#endif /* E_MAIL_SIGNATURE_EDITOR_H */
