/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _VFOLDER_CONTEXT_H
#define _VFOLDER_CONTEXT_H

#include "rule-context.h"

#define VFOLDER_CONTEXT(obj)	GTK_CHECK_CAST (obj, vfolder_context_get_type (), VfolderContext)
#define VFOLDER_CONTEXT_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, vfolder_context_get_type (), VfolderContextClass)
#define IS_VFOLDER_CONTEXT(obj)      GTK_CHECK_TYPE (obj, vfolder_context_get_type ())

typedef struct _VfolderContext	VfolderContext;
typedef struct _VfolderContextClass	VfolderContextClass;

struct _VfolderContext {
	RuleContext parent;
	struct _VfolderContextPrivate *priv;

};

struct _VfolderContextClass {
	RuleContextClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		vfolder_context_get_type	(void);
VfolderContext	*vfolder_context_new	(void);

/* methods */

#endif /* ! _VFOLDER_CONTEXT_H */

