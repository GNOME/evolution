/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-select-names-bonobo.h
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

#ifndef __E_SELECT_NAMES_BONOBO_H__
#define __E_SELECT_NAMES_BONOBO_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>

#include "Evolution-Addressbook-SelectNames.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SELECT_NAMES_BONOBO			(e_select_names_bonobo_get_type ())
#define E_SELECT_NAMES_BONOBO(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SELECT_NAMES_BONOBO, ESelectNamesBonobo))
#define E_SELECT_NAMES_BONOBO_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SELECT_NAMES_BONOBO, ESelectNamesBonoboClass))
#define E_IS_SELECT_NAMES_BONOBO(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SELECT_NAMES_BONOBO))
#define E_IS_SELECT_NAMES_BONOBO_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SELECT_NAMES_BONOBO))


typedef struct _ESelectNamesBonobo        ESelectNamesBonobo;
typedef struct _ESelectNamesBonoboPrivate ESelectNamesBonoboPrivate;
typedef struct _ESelectNamesBonoboClass   ESelectNamesBonoboClass;

struct _ESelectNamesBonobo {
	BonoboObject parent;

	ESelectNamesBonoboPrivate *priv;
};

struct _ESelectNamesBonoboClass {
	BonoboObjectClass parent_class;
};


GtkType             e_select_names_bonobo_get_type   (void);
void                e_select_names_bonobo_construct  (ESelectNamesBonobo *select_names,
						      Evolution_Addressbook_SelectNames corba_object);
ESelectNamesBonobo *e_select_names_bonobo_new        (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_SELECT_NAMES_BONOBO_H__ */
