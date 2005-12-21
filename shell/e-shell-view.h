/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-view.h
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * This is only a CORBA wrapper around e_shell_window.
 */

#ifndef _E_SHELL_VIEW_H_
#define _E_SHELL_VIEW_H_

#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

struct _EShell;

typedef struct _EShellView        EShellView;
typedef struct _EShellViewPrivate EShellViewPrivate;
typedef struct _EShellViewClass   EShellViewClass;

#include "Evolution.h"

#define E_TYPE_SHELL_VIEW			(e_shell_view_get_type ())
#define E_SHELL_VIEW(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHELL_VIEW, EShellView))
#define E_SHELL_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_VIEW, EShellViewClass))
#define E_IS_SHELL_VIEW(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHELL_VIEW))
#define E_IS_SHELL_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_VIEW))

struct _EShellView {
	BonoboObject parent;

	struct _EShellWindow *window;

	EShellViewPrivate *priv;
};

struct _EShellViewClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_ShellView__epv epv;
};

GType                e_shell_view_get_type   (void);
EShellView *e_shell_view_new(struct _EShellWindow *window);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHELL_VIEW_H_ */

