/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * ea-minicard-view.h
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Leon Zhang <leon.zhang@sun.com> Sun Microsystem Inc., 2003
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __EA_MINICARD_VIEW_H__
#define __EA_MINICARD_VIEW_H__

#include <atk/atkgobjectaccessible.h>
#include "e-minicard-view.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EA_TYPE_MINICARD_VIEW			(ea_minicard_view_get_type ())
#define EA_MINICARD_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_MINICARD_VIEW, EaMinicardView))
#define EA_MINICARD_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_MINICARD_VIEW, EaMiniCardViewClass))
#define EA_IS_MINICARD_VIEW(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_MINICARD_VIEW))
#define EA_IS_MINICARD_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EA_TYPE_MINICARD_VIEW))


typedef struct _EaMinicardView       EaMinicardView;
typedef struct _EaMinicardViewClass  EaMinicardViewClass;

struct _EaMinicardView
{
	AtkGObjectAccessible parent;
};


struct _EaMinicardViewClass
{
	AtkGObjectAccessibleClass parent_class;
};

GType ea_minicard_view_get_type (void);

AtkObject* ea_minicard_view_new(GObject *obj);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __EA_MINICARD_VIEW_H__ */
