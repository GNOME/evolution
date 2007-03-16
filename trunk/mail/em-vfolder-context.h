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

#ifndef _EM_VFOLDER_CONTEXT_H
#define _EM_VFOLDER_CONTEXT_H

#include "filter/rule-context.h"

#define EM_VFOLDER_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), em_vfolder_context_get_type(), EMVFolderContext))
#define EM_VFOLDER_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), em_vfolder_context_get_type(), EMVFolderContextClass))
#define EM_IS_VFOLDER_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), em_vfolder_context_get_type()))
#define EM_IS_VFOLDER_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), em_vfolder_context_get_type()))
#define EM_VFOLDER_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), em_vfolder_context_get_type(), EMVFolderContextClass))

typedef struct _EMVFolderContext EMVFolderContext;
typedef struct _EMVFolderContextClass EMVFolderContextClass;

struct _EMVFolderContext {
	RuleContext parent_object;
	
};

struct _EMVFolderContextClass {
	RuleContextClass parent_class;
};

GType em_vfolder_context_get_type (void);

EMVFolderContext *em_vfolder_context_new (void);

#endif /* ! _EM_VFOLDER_CONTEXT_H */
