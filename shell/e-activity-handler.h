/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-activity-handler.h
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_ACTIVITY_HANDLER_H_
#define _E_ACTIVITY_HANDLER_H_

#include "Evolution.h"

#include "e-task-bar.h"

#include <bonobo/bonobo-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_ACTIVITY_HANDLER			(e_activity_handler_get_type ())
#define E_ACTIVITY_HANDLER(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_ACTIVITY_HANDLER, EActivityHandler))
#define E_ACTIVITY_HANDLER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_ACTIVITY_HANDLER, EActivityHandlerClass))
#define E_IS_ACTIVITY_HANDLER(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_ACTIVITY_HANDLER))
#define E_IS_ACTIVITY_HANDLER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_ACTIVITY_HANDLER))


typedef struct _EActivityHandler        EActivityHandler;
typedef struct _EActivityHandlerPrivate EActivityHandlerPrivate;
typedef struct _EActivityHandlerClass   EActivityHandlerClass;

struct _EActivityHandler {
	BonoboObject parent;

	EActivityHandlerPrivate *priv;
};

struct _EActivityHandlerClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Activity__epv epv;
};


GtkType           e_activity_handler_get_type   (void);
void              e_activity_handler_construct  (EActivityHandler *activity_hanlder);
EActivityHandler *e_activity_handler_new        (void);

void  e_activity_handler_attach_task_bar  (EActivityHandler *activity_hanlder,
					   ETaskBar         *task_bar);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_ACTIVITY_HANDLER_H_ */
