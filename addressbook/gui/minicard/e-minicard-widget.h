/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-minicard-widget.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#ifndef __E_MINICARD_WIDGET_H__
#define __E_MINICARD_WIDGET_H__

#include <gnome.h>
#include "addressbook/backend/ebook/e-card.h"
#include "e-util/e-canvas.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EMinicardWidget - A card displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define E_MINICARD_WIDGET_TYPE			(e_minicard_widget_get_type ())
#define E_MINICARD_WIDGET(obj)			(GTK_CHECK_CAST ((obj), E_MINICARD_WIDGET_TYPE, EMinicardWidget))
#define E_MINICARD_WIDGET_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_MINICARD_WIDGET_TYPE, EMinicardWidgetClass))
#define E_IS_MINICARD_WIDGET(obj)		(GTK_CHECK_TYPE ((obj), E_MINICARD_WIDGET_TYPE))
#define E_IS_MINICARD_WIDGET_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_MINICARD_WIDGET_TYPE))


typedef struct _EMinicardWidget       EMinicardWidget;
typedef struct _EMinicardWidgetClass  EMinicardWidgetClass;

struct _EMinicardWidget
{
	ECanvas parent;
	
	/* item specific fields */
	GnomeCanvasItem *item;

	GnomeCanvasItem *rect;
	ECard *card;
};

struct _EMinicardWidgetClass
{
	ECanvasClass parent_class;
};


GtkWidget *e_minicard_widget_new(void);
GtkType    e_minicard_widget_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_MINICARD_WIDGET_H__ */
