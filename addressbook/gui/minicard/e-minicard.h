/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-minicard.h
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
#ifndef __E_MINICARD_H__
#define __E_MINICARD_H__

#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EMinicard - A small card displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * width        double          RW              width of the card
 * height       double          R               height of the card
 * card		ECard*		RW		Pointer to the ECard
 */

#define E_MINICARD_TYPE			(e_minicard_get_type ())
#define E_MINICARD(obj)			(GTK_CHECK_CAST ((obj), E_MINICARD_TYPE, EMinicard))
#define E_MINICARD_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_MINICARD_TYPE, EMinicardClass))
#define E_IS_MINICARD(obj)		(GTK_CHECK_TYPE ((obj), E_MINICARD_TYPE))
#define E_IS_MINICARD_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_MINICARD_TYPE))


typedef struct _EMinicard       EMinicard;
typedef struct _EMinicardClass  EMinicardClass;

struct _EMinicard
{
  GnomeCanvasGroup parent;

  /* item specific fields */
  /*  ECard *card; */

  GnomeCanvasItem *rect;
  GnomeCanvasItem *header_rect;
  GnomeCanvasItem *header_text;
  GList *fields; /* Of type GnomeCanvasItem. */

  double width;
  double height;
};

struct _EMinicardClass
{
  GnomeCanvasGroupClass parent_class;
};


GtkType    e_minicard_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_MINICARD_H__ */
