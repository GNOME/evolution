/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-view.h
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef __EVOLUTION_SHELL_VIEW_H__
#define __EVOLUTION_SHELL_VIEW_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
 
#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_SHELL_VIEW			(evolution_shell_view_get_type ())
#define EVOLUTION_SHELL_VIEW(obj)			(GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_SHELL_VIEW, EvolutionShellView))
#define EVOLUTION_SHELL_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_SHELL_VIEW, EvolutionShellViewClass))
#define EVOLUTION_IS_SHELL_VIEW(obj)			(GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_SHELL_VIEW))
#define EVOLUTION_IS_SHELL_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_SHELL_VIEW))


typedef struct _EvolutionShellView        EvolutionShellView;
typedef struct _EvolutionShellViewPrivate EvolutionShellViewPrivate;
typedef struct _EvolutionShellViewClass   EvolutionShellViewClass;

struct _EvolutionShellView {
	BonoboObject parent;

	EvolutionShellViewPrivate *priv;
};

struct _EvolutionShellViewClass {
	BonoboObjectClass parent_class;

	/* Signals.  */

	void  (* set_message)   (EvolutionShellView *shell_view, const char *message, gboolean busy);
	void  (* unset_message) (EvolutionShellView *shell_view);
};


GtkType             evolution_shell_view_get_type   (void);
void                evolution_shell_view_construct  (EvolutionShellView  *shell_view,
						     Evolution_ShellView  corba_object);
EvolutionShellView *evolution_shell_view_new        (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_SHELL_VIEW_H__ */
