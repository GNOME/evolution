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

#ifndef _E_ADDRESSBOOK_REFLOW_ADAPTER_H_
#define _E_ADDRESSBOOK_REFLOW_ADAPTER_H_

#include <libebook/libebook.h>

#include <e-util/e-util.h>

#include "e-addressbook-model.h"

#define E_TYPE_ADDRESSBOOK_REFLOW_ADAPTER        (e_addressbook_reflow_adapter_get_type ())
#define E_ADDRESSBOOK_REFLOW_ADAPTER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_ADDRESSBOOK_REFLOW_ADAPTER, EAddressbookReflowAdapter))
#define E_ADDRESSBOOK_REFLOW_ADAPTER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_ADDRESSBOOK_REFLOW_ADAPTER, EAddressbookReflowAdapterClass))
#define E_IS_ADDRESSBOOK_REFLOW_ADAPTER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_ADDRESSBOOK_REFLOW_ADAPTER))
#define E_IS_ADDRESSBOOK_REFLOW_ADAPTER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_ADDRESSBOOK_REFLOW_ADAPTER))

typedef struct _EAddressbookReflowAdapter EAddressbookReflowAdapter;
typedef struct _EAddressbookReflowAdapterPrivate EAddressbookReflowAdapterPrivate;
typedef struct _EAddressbookReflowAdapterClass EAddressbookReflowAdapterClass;

struct _EAddressbookReflowAdapter {
	EReflowModel parent;

	EAddressbookReflowAdapterPrivate *priv;
};

struct _EAddressbookReflowAdapterClass {
	EReflowModelClass parent_class;

	/*
	 * Signals
	 */
	gint		(*drag_begin)		(EAddressbookReflowAdapter *adapter,
						 GdkEvent *event);
	void		(*open_contact)		(EAddressbookReflowAdapter *adapter,
						 EContact *contact);
};

GType         e_addressbook_reflow_adapter_get_type          (void);
void          e_addressbook_reflow_adapter_construct         (EAddressbookReflowAdapter *adapter,
							      EAddressbookModel         *model);
EReflowModel *e_addressbook_reflow_adapter_new               (EAddressbookModel         *model);

/* Returns object with ref count of 1. */
EContact     *e_addressbook_reflow_adapter_get_contact       (EAddressbookReflowAdapter *adapter,
							      gint                        index);
#endif /* _E_ADDRESSBOOK_REFLOW_ADAPTER_H_ */
