/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Author : 
 *  JP Rosevear <jpr@ximian.com>
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

#ifndef _E_DATE_EDIT_CONFIG_H_
#define _E_DATE_EDIT_CONFIG_H_

#include <misc/e-dateedit.h>

G_BEGIN_DECLS

#define E_DATE_EDIT_CONFIG(obj)          GTK_CHECK_CAST (obj, e_date_edit_config_get_type (), EDateEditConfig)
#define E_DATE_EDIT_CONFIG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_date_edit_config_get_type (), EDateEditConfigClass)
#define E_IS_DATE_EDIT_CONFIG(obj)       GTK_CHECK_TYPE (obj, e_date_edit_config_get_type ())
        
typedef struct _EDateEditConfig        EDateEditConfig;
typedef struct _EDateEditConfigClass   EDateEditConfigClass;
typedef struct _EDateEditConfigPrivate EDateEditConfigPrivate;

struct _EDateEditConfig {
	GObject parent;

	EDateEditConfigPrivate *priv;
};

struct _EDateEditConfigClass {
	GObjectClass parent_class;
};

GType          e_date_edit_config_get_type (void);
EDateEditConfig *e_date_edit_config_new (EDateEdit *date_edit);
EDateEdit *e_date_edit_config_get_edit (EDateEditConfig *edit_config);
void e_date_edit_config_set_edit (EDateEditConfig *edit_config, EDateEdit *date_edit);

G_END_DECLS

#endif
