/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * ea-minicard.h
 * Copyright (C) 2000  Ximian, Inc.
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
 * 
 * Author: Leon Zhang <leon.zhang@sun.com> Sun Microsystem Inc., 2003
 */
#ifndef __EA_MINICARD_H__
#define __EA_MINICARD_H__

#include <atk/atkgobjectaccessible.h>
#include "e-minicard.h"
#include "e-minicard-label.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EA_TYPE_MINICARD			(ea_minicard_get_type ())
#define EA_MINICARD(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_MINICARD, EaMinicard))
#define EA_MINICARD_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_MINICARD, EaMiniCardClass))
#define EA_IS_MINICARD(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_MINICARD))
#define EA_IS_MINICARD_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), EA_TYPE_MINICARD))

typedef struct _EaMinicard       EaMinicard;
typedef struct _EaMinicardClass  EaMinicardClass;

struct _EaMinicard
{
	AtkGObjectAccessible parent;
};


struct _EaMinicardClass
{
	AtkGObjectAccessibleClass parent_class;
};

GType ea_minicard_get_type (void);
AtkObject* ea_minicard_new(GObject *obj);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __EA_MINICARD_H__ */
