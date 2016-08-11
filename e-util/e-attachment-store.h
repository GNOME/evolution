/*
 * e-attachment-store.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ATTACHMENT_STORE_H
#define E_ATTACHMENT_STORE_H

#include <gtk/gtk.h>
#include <e-util/e-attachment.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_STORE \
	(e_attachment_store_get_type ())
#define E_ATTACHMENT_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_STORE, EAttachmentStore))
#define E_ATTACHMENT_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_STORE, EAttachmentStoreClass))
#define E_IS_ATTACHMENT_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_STORE))
#define E_IS_ATTACHMENT_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_STORE))
#define E_ATTACHMENT_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT_STORE, EAttachmentStoreClass))

G_BEGIN_DECLS

typedef struct _EAttachmentStore EAttachmentStore;
typedef struct _EAttachmentStoreClass EAttachmentStoreClass;
typedef struct _EAttachmentStorePrivate EAttachmentStorePrivate;

struct _EAttachmentStore {
	GtkListStore parent;
	EAttachmentStorePrivate *priv;
};

struct _EAttachmentStoreClass {
	GtkListStoreClass parent_class;

	/* Signals */
	void	(* attachment_added)	(EAttachmentStore *store,
					 EAttachment *attachment);
	void	(* attachment_removed)	(EAttachmentStore *store,
					 EAttachment *attachment);
};

enum {
	E_ATTACHMENT_STORE_COLUMN_ATTACHMENT,	/* E_TYPE_ATTACHMENT */
	E_ATTACHMENT_STORE_COLUMN_CAPTION,	/* G_TYPE_STRING */
	E_ATTACHMENT_STORE_COLUMN_CONTENT_TYPE, /* G_TYPE_STRING */
	E_ATTACHMENT_STORE_COLUMN_DESCRIPTION,	/* G_TYPE_STRING */
	E_ATTACHMENT_STORE_COLUMN_ICON,		/* G_TYPE_ICON */
	E_ATTACHMENT_STORE_COLUMN_LOADING,	/* G_TYPE_BOOLEAN */
	E_ATTACHMENT_STORE_COLUMN_PERCENT,	/* G_TYPE_INT */
	E_ATTACHMENT_STORE_COLUMN_SAVING,	/* G_TYPE_BOOLEAN */
	E_ATTACHMENT_STORE_COLUMN_SIZE,		/* G_TYPE_UINT64 */
	E_ATTACHMENT_STORE_NUM_COLUMNS
};

GType		e_attachment_store_get_type	(void) G_GNUC_CONST;
GtkTreeModel *	e_attachment_store_new		(void);
void		e_attachment_store_add_attachment
						(EAttachmentStore *store,
						 EAttachment *attachment);
gboolean	e_attachment_store_remove_attachment
						(EAttachmentStore *store,
						 EAttachment *attachment);
void		e_attachment_store_remove_all	(EAttachmentStore *store);
void		e_attachment_store_add_to_multipart
						(EAttachmentStore *store,
						 CamelMultipart *multipart,
						 const gchar *default_charset);
GList *		e_attachment_store_get_attachments
						(EAttachmentStore *store);
guint		e_attachment_store_get_num_attachments
						(EAttachmentStore *store);
guint		e_attachment_store_get_num_loading
						(EAttachmentStore *store);
goffset		e_attachment_store_get_total_size
						(EAttachmentStore *store);
void		e_attachment_store_run_load_dialog
						(EAttachmentStore *store,
						 GtkWindow *parent);
GFile *		e_attachment_store_run_save_dialog
						(EAttachmentStore *store,
						 GList *attachment_list,
						 GtkWindow *parent);

gboolean	e_attachment_store_transform_num_attachments_to_visible_boolean
						(GBinding *binding,
						 const GValue *from_value,
						 GValue *to_value,
						 gpointer user_data);
gboolean	e_attachment_store_find_attachment_iter
						(EAttachmentStore *store,
						 EAttachment *attachment,
						 GtkTreeIter *out_iter);
/* Asynchronous Operations */
void		e_attachment_store_get_uris_async
						(EAttachmentStore *store,
						 GList *attachment_list,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gchar **	e_attachment_store_get_uris_finish
						(EAttachmentStore *store,
						 GAsyncResult *result,
						 GError **error);
void		e_attachment_store_load_async	(EAttachmentStore *store,
						 GList *attachment_list,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_attachment_store_load_finish	(EAttachmentStore *store,
						 GAsyncResult *result,
						 GError **error);
void		e_attachment_store_save_async	(EAttachmentStore *store,
						 GFile *destination,
						 const gchar *filename_prefix,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gchar **	e_attachment_store_save_finish	(EAttachmentStore *store,
						 GAsyncResult *result,
						 GError **error);

G_END_DECLS

#endif /* E_ATTACHMENT_STORE_H */

