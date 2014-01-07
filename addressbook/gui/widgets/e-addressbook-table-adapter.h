/*
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

#ifndef _EAB_TABLE_ADAPTER_H_
#define _EAB_TABLE_ADAPTER_H_

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_ADDRESSBOOK_TABLE_ADAPTER \
	(e_addressbook_table_adapter_get_type ())
#define E_ADDRESSBOOK_TABLE_ADAPTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ADDRESSBOOK_TABLE_ADAPTER, EAddressbookTableAdapter))
#define E_ADDRESSBOOK_TABLE_ADAPTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ADDRESSBOOK_TABLE_ADAPTER, EAddressbookTableAdapterClass))
#define E_IS_ADDRESSBOOK_TABLE_ADAPTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ADDRESSBOOK_TABLE_ADAPTER))
#define E_IS_ADDRESSBOOK_TABLE_ADAPTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ADDRESSBOOK_TABLE_ADAPTER))
#define E_ADDRESSBOOK_TABLE_ADAPTER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ADDRESSBOOK_TABLE_ADAPTER, EAddressbookTableAdapterClass))

G_BEGIN_DECLS

typedef struct _EAddressbookTableAdapter EAddressbookTableAdapter;
typedef struct _EAddressbookTableAdapterClass EAddressbookTableAdapterClass;
typedef struct _EAddressbookTableAdapterPrivate EAddressbookTableAdapterPrivate;

struct _EAddressbookTableAdapter {
	GObject parent;
	EAddressbookTableAdapterPrivate *priv;
};

struct _EAddressbookTableAdapterClass {
	GObjectClass parent_class;
};

GType		e_addressbook_table_adapter_get_type
					(void) G_GNUC_CONST;
void		e_addressbook_table_adapter_construct
					(EAddressbookTableAdapter *adapter,
					 EAddressbookModel *model);
ETableModel *	e_addressbook_table_adapter_new
					(EAddressbookModel *model);

G_END_DECLS

#endif /* _EAB_TABLE_ADAPTER_H_ */
