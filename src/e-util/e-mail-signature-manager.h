/*
 * e-mail-signature-manager.h
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

#ifndef E_MAIL_SIGNATURE_MANAGER_H
#define E_MAIL_SIGNATURE_MANAGER_H

#include <gtk/gtk.h>

#include <e-util/e-util-enumtypes.h>
#include <e-util/e-mail-signature-editor.h>
#include <e-util/e-mail-signature-tree-view.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SIGNATURE_MANAGER \
	(e_mail_signature_manager_get_type ())
#define E_MAIL_SIGNATURE_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SIGNATURE_MANAGER, EMailSignatureManager))
#define E_MAIL_SIGNATURE_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SIGNATURE_MANAGER, EMailSignatureManagerClass))
#define E_IS_MAIL_SIGNATURE_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SIGNATURE_MANAGER))
#define E_IS_MAIL_SIGNATURE_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SIGNATURE_MANAGER))
#define E_MAIL_SIGNATURE_MANAGER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SIGNATURE_MANAGER, EMailSignatureManagerClass))

G_BEGIN_DECLS

typedef struct _EMailSignatureManager EMailSignatureManager;
typedef struct _EMailSignatureManagerClass EMailSignatureManagerClass;
typedef struct _EMailSignatureManagerPrivate EMailSignatureManagerPrivate;

struct _EMailSignatureManager {
	GtkPaned parent;
	EMailSignatureManagerPrivate *priv;
};

struct _EMailSignatureManagerClass {
	GtkPanedClass parent_class;

	void	(*add_signature)	(EMailSignatureManager *manager);
	void	(*add_signature_script)	(EMailSignatureManager *manager);
	void	(*editor_created)	(EMailSignatureManager *manager,
					 EMailSignatureEditor *editor);
	void	(*edit_signature)	(EMailSignatureManager *manager);
	void	(*remove_signature)	(EMailSignatureManager *manager);
};

GType		e_mail_signature_manager_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_signature_manager_new
					(ESourceRegistry *registry);
void		e_mail_signature_manager_add_signature
					(EMailSignatureManager *manager);
void		e_mail_signature_manager_add_signature_script
					(EMailSignatureManager *manager);
void		e_mail_signature_manager_edit_signature
					(EMailSignatureManager *manager);
void		e_mail_signature_manager_remove_signature
					(EMailSignatureManager *manager);
EContentEditorMode
		e_mail_signature_manager_get_prefer_mode
					(EMailSignatureManager *manager);
void		e_mail_signature_manager_set_prefer_mode
					(EMailSignatureManager *manager,
					 EContentEditorMode prefer_mode);
ESourceRegistry *
		e_mail_signature_manager_get_registry
					(EMailSignatureManager *manager);

#endif /* E_MAIL_SIGNATURE_MANAGER_H */
