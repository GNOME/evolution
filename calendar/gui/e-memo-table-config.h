/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Author : 
 *  JP Rosevear <jpr@ximian.com>
 *  Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright 2003, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef _E_MEMO_TABLE_CONFIG_H_
#define _E_MEMO_TABLE_CONFIG_H_

#include "e-memo-table.h"

G_BEGIN_DECLS

#define E_MEMO_TABLE_CONFIG(obj)          GTK_CHECK_CAST (obj, e_memo_table_config_get_type (), EMemoTableConfig)
#define E_MEMO_TABLE_CONFIG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_memo_table_config_get_type (), EMemoTableConfigClass)
#define E_IS_MEMO_TABLE_CONFIG(obj)       GTK_CHECK_TYPE (obj, e_memo_table_config_get_type ())
        
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
