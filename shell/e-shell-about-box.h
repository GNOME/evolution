/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-about-box.h
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

#ifndef _E_SHELL_ABOUT_BOX_H_
#define _E_SHELL_ABOUT_BOX_H_

#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SHELL_ABOUT_BOX			(e_shell_about_box_get_type ())
#define E_SHELL_ABOUT_BOX(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHELL_ABOUT_BOX, EShellAboutBox))
#define E_SHELL_ABOUT_BOX_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_ABOUT_BOX, EShellAboutBoxClass))
#define E_IS_SHELL_ABOUT_BOX(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHELL_ABOUT_BOX))
#define E_IS_SHELL_ABOUT_BOX_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_ABOUT_BOX))


typedef struct _EShellAboutBox        EShellAboutBox;
typedef struct _EShellAboutBoxPrivate EShellAboutBoxPrivate;
typedef struct _EShellAboutBoxClass   EShellAboutBoxClass;

struct _EShellAboutBox {
	GtkEventBox parent;

	EShellAboutBoxPrivate *priv;
};

struct _EShellAboutBoxClass {
	GtkEventBoxClass parent_class;
};


GtkType    e_shell_about_box_get_type   (void);
void       e_shell_about_box_construct  (EShellAboutBox *about_box);
GtkWidget *e_shell_about_box_new        (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHELL_ABOUT_BOX_H_ */
