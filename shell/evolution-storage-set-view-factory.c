/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage-set-view-factory.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-storage-set-view.h"
#include "e-shell.h"
#include "evolution-storage-set-view.h"

#include "evolution-storage-set-view-factory.h"


BonoboControl *
evolution_storage_set_view_factory_new_view (EShell *shell)
{
	EStorageSet *storage_set;
	GtkWidget *storage_set_view;
	BonoboControl *control;
	EvolutionStorageSetView *storage_set_view_interface;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	storage_set = e_shell_get_storage_set (shell);
	storage_set_view = e_storage_set_view_new (storage_set);

	storage_set_view_interface = evolution_storage_set_view_new (storage_set_view);
	if (storage_set_view_interface == NULL) {
		gtk_widget_destroy (storage_set_view);
		return NULL;
	}

	control = bonobo_control_new (storage_set_view);
	bonobo_object_add_interface (BONOBO_OBJECT (control), BONOBO_OBJECT (storage_set_view));

	return control;
}
