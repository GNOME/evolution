/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-minicard-view.h
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
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
#ifndef __E_MINICARD_VIEW_H__
#define __E_MINICARD_VIEW_H__

#include "e-minicard.h"

#include <gal/widgets/e-reflow.h>
#include <gal/widgets/e-selection-model-simple.h>
#include <libebook/e-book.h>
#include "e-addressbook-reflow-adapter.h"

G_BEGIN_DECLS

/* EMinicardView - A canvas item container.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * book         EBook           RW              book to query
 * query        string          RW              query string
 *
 * From EReflowSorted:   (you should really know what you're doing if you set these.)
 * compare_func  GCompareFunc   RW              compare function
 * string_func   EReflowStringFunc RW           string function
 *
 * From EReflow:
 * minimum_width double         RW              minimum width of the reflow.  width >= minimum_width
 * width        double          R               width of the reflow
 * height       double          RW              height of the reflow
 */

#define E_TYPE_MINICARD_VIEW			(e_minicard_view_get_type ())
#define E_MINICARD_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MINICARD_VIEW, EMinicardView))
#define E_MINICARD_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MINICARD_VIEW, EMinicardViewClass))
#define E_IS_MINICARD_VIEW(obj) 		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MINICARD_VIEW))
#define E_IS_MINICARD_VIEW_CLASS(klass) 	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_MINICARD_VIEW))


typedef struct _EMinicardView       EMinicardView;
typedef struct _EMinicardViewClass  EMinicardViewClass;

struct _EMinicardView
{
	EReflow parent;

	EAddressbookReflowAdapter *adapter;
	
	/* item specific fields */

	GList *drag_list;

	guint canvas_drag_data_get_id;
	guint writable_status_id;
};

struct _EMinicardViewClass
{
	EReflowClass parent_class;

	void (*right_click) (EMinicardView *view, GdkEvent *event);
};

GType    e_minicard_view_get_type          (void);
void     e_minicard_view_remove_selection  (EMinicardView *view,
					    EBookCallback  cb,
					    gpointer       closure);
void     e_minicard_view_jump_to_letter    (EMinicardView *view,
					    gunichar       letter);
GList   *e_minicard_view_get_card_list     (EMinicardView *view);


G_END_DECLS

#endif /* __E_MINICARD_VIEW_H__ */
