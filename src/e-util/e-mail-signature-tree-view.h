/*
 * e-mail-signature-tree-view.h
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

#ifndef E_MAIL_SIGNATURE_TREE_VIEW_H
#define E_MAIL_SIGNATURE_TREE_VIEW_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SIGNATURE_TREE_VIEW \
	(e_mail_signature_tree_view_get_type ())
#define E_MAIL_SIGNATURE_TREE_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SIGNATURE_TREE_VIEW, EMailSignatureTreeView))
#define E_MAIL_SIGNATURE_TREE_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SIGNATURE_TREE_VIEW, EMailSignatureTreeViewClass))
#define E_IS_MAIL_SIGNATURE_TREE_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SIGNATURE_TREE_VIEW))
#define E_IS_MAIL_SIGNATURE_TREE_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SIGNATURE_TREE_VIEW))
#define E_MAIL_SIGNATURE_TREE_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SIGNATURE_TREE_VIEW, EMailSignatureTreeViewClass))

G_BEGIN_DECLS

typedef struct _EMailSignatureTreeView EMailSignatureTreeView;
typedef struct _EMailSignatureTreeViewClass EMailSignatureTreeViewClass;
typedef struct _EMailSignatureTreeViewPrivate EMailSignatureTreeViewPrivate;

struct _EMailSignatureTreeView {
	GtkTreeView parent;
	EMailSignatureTreeViewPrivate *priv;
};

struct _EMailSignatureTreeViewClass {
	GtkTreeViewClass parent_class;
};

GType		e_mail_signature_tree_view_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_signature_tree_view_new
					(ESourceRegistry *registry);
void		e_mail_signature_tree_view_refresh
					(EMailSignatureTreeView *tree_view);
ESourceRegistry *
		e_mail_signature_tree_view_get_registry
					(EMailSignatureTreeView *tree_view);
ESource *	e_mail_signature_tree_view_ref_selected_source
					(EMailSignatureTreeView *tree_view);
void		e_mail_signature_tree_view_set_selected_source
					(EMailSignatureTreeView *tree_view,
					 ESource *selected_source);

G_END_DECLS

#endif /* E_MAIL_SIGNATURE_TREE_VIEW_H */
