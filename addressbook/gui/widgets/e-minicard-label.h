/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-minicard-label.h
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
#ifndef __E_MINICARD_LABEL_H__
#define __E_MINICARD_LABEL_H__

#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EMinicardLabel - A label doing focus with non-marching ants.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * width        double          RW              width of the label
 * height       double          R               height of the label
 * field        string          RW              text in the field label
 * fieldname    string          RW              text in the fieldname label
 */

#define E_MINICARD_LABEL_TYPE			(e_minicard_label_get_type ())
#define E_MINICARD_LABEL(obj)			(GTK_CHECK_CAST ((obj), E_MINICARD_LABEL_TYPE, EMinicardLabel))
#define E_MINICARD_LABEL_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_MINICARD_LABEL_TYPE, EMiniCardLabelClass))
#define E_IS_MINICARD_LABEL(obj)		(GTK_CHECK_TYPE ((obj), E_MINICARD_LABEL_TYPE))
#define E_IS_MINICARD_LABEL_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_MINICARD_LABEL_TYPE))


typedef struct _EMinicardLabel       EMinicardLabel;
typedef struct _EMinicardLabelClass  EMinicardLabelClass;

struct _EMinicardLabel
{
  GnomeCanvasGroup parent;

  /* item specific fields */
  double width;
  double height;
  GnomeCanvasItem *fieldname;
  GnomeCanvasItem *field;
  GnomeCanvasItem *rect;
  char *fieldname_text;
  char *field_text;

	gboolean has_focus;
};

struct _EMinicardLabelClass
{
	GnomeCanvasGroupClass parent_class;

	void (* resize) (EMinicardLabel *text);
};


GtkType    e_minicard_label_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_MINICARD_LABEL_H__ */
