/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
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

#define VFOLDER_TYPE_CONTEXT            (vfolder_context_get_type ())
#define VFOLDER_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VFOLDER_TYPE_CONTEXT, VfolderContext))
#define VFOLDER_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VFOLDER_TYPE_CONTEXT, VfolderContextClass))
#define IS_VFOLDER_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VFOLDER_TYPE_CONTEXT))
#define IS_VFOLDER_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VFOLDER_TYPE_CONTEXT))
#define VFOLDER_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VFOLDER_TYPE_CONTEXT, VfolderContextClass))

typedef struct _VfolderContext VfolderContext;
typedef struct _VfolderContextClass VfolderContextClass;

struct _VfolderContext {
	RuleContext parent_object;
	
};

struct _VfolderContextClass {
	RuleContextClass parent_class;
	
	/* virtual methods */
	
	/* signals */
};


GType vfolder_context_get_type (void);

VfolderContext *vfolder_context_new (void);

/* methods */

#endif /* ! _VFOLDER_CONTEXT_H */
