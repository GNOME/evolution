/* e-destination-store.h - EDestination store with GtkTreeModel interface.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Authors: Hans Petter Jansson <hpj@novell.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_DESTINATION_STORE_H
#define E_DESTINATION_STORE_H

#include <gtk/gtk.h>
#include <libebook/libebook.h>

/* Standard GObject macros */
#define E_TYPE_DESTINATION_STORE \
	(e_destination_store_get_type ())
#define E_DESTINATION_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_DESTINATION_STORE, EDestinationStore))
#define E_DESTINATION_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_DESTINATION_STORE, EDestinationStoreClass))
#define E_IS_DESTINATION_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_DESTINATION_STORE))
#define E_IS_DESTINATION_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_DESTINATION_STORE))
#define E_DESTINATION_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_DESTINATION_STORE, EDestinationStoreClass))

G_BEGIN_DECLS

typedef struct _EDestinationStore EDestinationStore;
typedef struct _EDestinationStoreClass EDestinationStoreClass;
typedef struct _EDestinationStorePrivate EDestinationStorePrivate;

struct _EDestinationStore {
	GObject parent;
	EDestinationStorePrivate *priv;
};

struct _EDestinationStoreClass {
	GObjectClass parent_class;
};

typedef enum {
	E_DESTINATION_STORE_COLUMN_NAME,
	E_DESTINATION_STORE_COLUMN_EMAIL,
	E_DESTINATION_STORE_COLUMN_ADDRESS,
	E_DESTINATION_STORE_NUM_COLUMNS
} EDestinationStoreColumnType;

GType		e_destination_store_get_type	(void) G_GNUC_CONST;
EDestinationStore *
		e_destination_store_new		(void);
EDestination *	e_destination_store_get_destination
						(EDestinationStore *destination_store,
						 GtkTreeIter *iter);

/* Returns a shallow copy; free the list when done, but don't unref elements */
GList *		e_destination_store_list_destinations
						(EDestinationStore *destination_store);

void		e_destination_store_insert_destination
						(EDestinationStore *destination_store,
						 gint index,
						 EDestination *destination);
void		e_destination_store_append_destination
						(EDestinationStore *destination_store,
						 EDestination *destination);
void		e_destination_store_remove_destination
						(EDestinationStore *destination_store,
						 EDestination *destination);
void		e_destination_store_remove_destination_nth
						(EDestinationStore *destination_store,
						 gint n);
guint		e_destination_store_get_destination_count
						(EDestinationStore *destination_store);
GtkTreePath *	e_destination_store_get_path	(GtkTreeModel *tree_model,
						 GtkTreeIter *iter);
gint		e_destination_store_get_stamp	(EDestinationStore *destination_store);

G_END_DECLS

#endif  /* E_DESTINATION_STORE_H */
