/*
 *
 * This is only a CORBA wrapper around e_shell_window.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_SHELL_VIEW_H_
#define _E_SHELL_VIEW_H_

#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-object.h>

G_BEGIN_DECLS

struct _EShell;

typedef struct _EShellView        EShellView;
typedef struct _EShellViewPrivate EShellViewPrivate;
typedef struct _EShellViewClass   EShellViewClass;

#include "Evolution.h"

#define E_TYPE_SHELL_VIEW			(e_shell_view_get_type ())
#define E_SHELL_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SHELL_VIEW, EShellView))
#define E_SHELL_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_VIEW, EShellViewClass))
#define E_IS_SHELL_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SHELL_VIEW))
#define E_IS_SHELL_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_VIEW))

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

G_END_DECLS

#endif /* _E_SHELL_VIEW_H_ */

