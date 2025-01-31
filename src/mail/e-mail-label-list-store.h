/*
 * e-mail-label-list-store.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_LABEL_LIST_STORE_H
#define E_MAIL_LABEL_LIST_STORE_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_LABEL_LIST_STORE \
	(e_mail_label_list_store_get_type ())
#define E_MAIL_LABEL_LIST_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_LABEL_LIST_STORE, EMailLabelListStore))
#define E_MAIL_LABEL_LIST_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_LABEL_LIST_STORE, EMailLabelListStoreClass))
#define E_IS_MAIL_LABEL_LIST_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_LABEL_LIST_STORE))
#define E_IS_MAIL_LABEL_LIST_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_LABEL_LIST_STORE))
#define E_MAIL_LABEL_LIST_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_LABEL_LIST_STORE, EMailLabelListStoreClass))

G_BEGIN_DECLS

typedef struct _EMailLabelListStore EMailLabelListStore;
typedef struct _EMailLabelListStoreClass EMailLabelListStoreClass;
typedef struct _EMailLabelListStorePrivate EMailLabelListStorePrivate;

struct _EMailLabelListStore {
	GtkListStore parent;
	EMailLabelListStorePrivate *priv;
};

struct _EMailLabelListStoreClass {
	GtkListStoreClass parent_class;
};

GType		e_mail_label_list_store_get_type	(void);
EMailLabelListStore *
		e_mail_label_list_store_new		(void);
gchar *		e_mail_label_list_store_get_name	(EMailLabelListStore *store,
							 GtkTreeIter *iter);
gboolean	e_mail_label_list_store_get_color	(EMailLabelListStore *store,
							 GtkTreeIter *iter,
							 GdkRGBA *color);
gchar *		e_mail_label_list_store_dup_icon_name	(EMailLabelListStore *store,
							 GtkTreeIter *iter);
gchar *		e_mail_label_list_store_get_tag		(EMailLabelListStore *store,
							 GtkTreeIter *iter);
void		e_mail_label_list_store_set		(EMailLabelListStore *store,
							 GtkTreeIter *iter,
							 const gchar *name,
							 const GdkRGBA *color);
void		e_mail_label_list_store_set_with_tag	(EMailLabelListStore *store,
							 GtkTreeIter *iter,
							 const gchar *tag,
							 const gchar *name,
							 const GdkRGBA *color);
gboolean	e_mail_label_list_store_lookup		(EMailLabelListStore *store,
							 const gchar *tag,
							 GtkTreeIter *iter);
gboolean	e_mail_label_list_store_lookup_by_name	(EMailLabelListStore *store,
							 const gchar *name,
							 GtkTreeIter *out_iter);
gboolean	e_mail_label_tag_is_default		(const gchar *tag);

G_END_DECLS

#endif /* E_MAIL_LABEL_LIST_STORE_H */
