/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-corba-shortcuts.h
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

#ifndef _E_CORBA_SHORTCUTS_H_
#define _E_CORBA_SHORTCUTS_H_

#include "e-shortcuts.h"

#include <bonobo/bonobo-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_CORBA_SHORTCUTS			(e_corba_shortcuts_get_type ())
#define E_CORBA_SHORTCUTS(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_CORBA_SHORTCUTS, ECorbaShortcuts))
#define E_CORBA_SHORTCUTS_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_CORBA_SHORTCUTS, ECorbaShortcutsClass))
#define E_IS_CORBA_SHORTCUTS(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_CORBA_SHORTCUTS))
#define E_IS_CORBA_SHORTCUTS_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_CORBA_SHORTCUTS))


typedef struct _ECorbaShortcuts        ECorbaShortcuts;
typedef struct _ECorbaShortcutsPrivate ECorbaShortcutsPrivate;
typedef struct _ECorbaShortcutsClass   ECorbaShortcutsClass;

struct _ECorbaShortcuts {
	BonoboObject parent;

	ECorbaShortcutsPrivate *priv;
};

struct _ECorbaShortcutsClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Shortcuts__epv epv;
};


GtkType          e_corba_shortcuts_get_type  (void);
ECorbaShortcuts *e_corba_shortcuts_new       (EShortcuts *shortcuts);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_CORBA_SHORTCUTS_H_ */
