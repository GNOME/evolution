/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-jump-button.h
 *
 * Copyright (C) 2003 Ximian, Inc.
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
 *
 * Author: Yang Wu <yang.wu@sun.com> Sun Microsystem Inc., 2003
 *
 */

#ifndef __EA_JUMP_BUTTON_H__
#define __EA_JUMP_BUTTON_H__

#include <atk/atkgobjectaccessible.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define EA_TYPE_JUMP_BUTTON                   (ea_jump_button_get_type ())
#define EA_JUMP_BUTTON(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_JUMP_BUTTON, EaJumpButton))
#define EA_JUMP_BUTTON_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_JUMP_BUTTON, EaJumpButtonClass))
#define EA_IS_JUMP_BUTTON(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_JUMP_BUTTON))
#define EA_IS_JUMP_BUTTON_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_JUMP_BUTTON))
#define EA_JUMP_BUTTON_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_JUMP_BUTTON, EaJumpButtonClass))

typedef struct _EaJumpButton                   EaJumpButton;
typedef struct _EaJumpButtonClass              EaJumpButtonClass;

struct _EaJumpButton
{
	AtkGObjectAccessible parent;
};

GType ea_jump_button_get_type (void);

struct _EaJumpButtonClass
{
	AtkGObjectAccessibleClass parent_class;
};

AtkObject *ea_jump_button_new (GObject *obj);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __EA_JUMP_BUTTON_H__ */
