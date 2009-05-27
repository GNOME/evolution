/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		JP Rosevear <jpr@ximian.com>
 *		Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_MEMO_TABLE_CONFIG_H_
#define _E_MEMO_TABLE_CONFIG_H_

#include "e-memo-table.h"

G_BEGIN_DECLS

#define E_MEMO_TABLE_CONFIG(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, e_memo_table_config_get_type (), EMemoTableConfig)
#define E_MEMO_TABLE_CONFIG_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, e_memo_table_config_get_type (), EMemoTableConfigClass)
#define E_IS_MEMO_TABLE_CONFIG(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, e_memo_table_config_get_type ())

typedef struct _EMemoTableConfig        EMemoTableConfig;
typedef struct _EMemoTableConfigClass   EMemoTableConfigClass;
typedef struct _EMemoTableConfigPrivate EMemoTableConfigPrivate;

struct _EMemoTableConfig {
	GObject parent;

	EMemoTableConfigPrivate *priv;
};

struct _EMemoTableConfigClass {
	GObjectClass parent_class;
};

GType          e_memo_table_config_get_type (void);
EMemoTableConfig *e_memo_table_config_new (EMemoTable *table);
EMemoTable *e_memo_table_config_get_table (EMemoTableConfig *view_config);
void e_memo_table_config_set_table (EMemoTableConfig *view_config, EMemoTable *table);

G_END_DECLS

#endif
