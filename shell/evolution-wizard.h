/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * evolution-wizard.h
 *
 * Copyright (C) 2000-2003 Ximian, Inc.
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
 */

#ifndef __EVOLUTION_WIZARD_H__
#define __EVOLUTION_WIZARD_H__

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-control.h>

#include "Evolution.h"

typedef struct _EvolutionWizard EvolutionWizard;
typedef struct _EvolutionWizardPrivate EvolutionWizardPrivate;

#define EVOLUTION_WIZARD_SET_PAGE "GNOME::Evolution::Wizard_setPage"
#define EVOLUTION_WIZARD_SET_SHOW_FINISH "GNOME::Evolution::Wizard_setShowFinish"
#define EVOLUTION_WIZARD_SET_BUTTONS_SENSITIVE "GNOME::Evolution::Wizard_setButtonsSensitive"

#define EVOLUTION_TYPE_WIZARD (evolution_wizard_get_type ())
#define EVOLUTION_WIZARD(o) (GTK_CHECK_CAST ((o), EVOLUTION_TYPE_WIZARD, EvolutionWizard))
#define EVOLUTION_WIZARD_CLASS(k) (GTK_CHECK_CLASS_CAST((k), EVOLUTION_TYPE_WIZARD, EvolutionWizardClass))
#define EVOLUTION_IS_WIZARD(o) (GTK_CHECK_TYPE ((o), EVOLUTION_TYPE_WIZARD))
#define EVOLUTION_IS_WIZARD_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), EVOLUTION_TYPE_WIZARD))

struct _EvolutionWizard {
	BonoboObject object;

	EvolutionWizardPrivate *priv;
};

typedef struct {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Wizard__epv epv;

	void (* next) (EvolutionWizard *wizard, int page_number);
	void (* prepare) (EvolutionWizard *wizard, int page_number);
	void (* back) (EvolutionWizard *wizard, int page_number);
	void (* finish) (EvolutionWizard *wizard);
	void (* cancel) (EvolutionWizard *wizard);
	void (* help) (EvolutionWizard *wizard, int page_number);
} EvolutionWizardClass;

GtkType evolution_wizard_get_type (void);

EvolutionWizard *evolution_wizard_new       (void);

void             evolution_wizard_add_page  (EvolutionWizard   *wizard,
					     const char        *title,
					     GdkPixbuf         *icon,
					     GtkWidget         *page);

void evolution_wizard_set_buttons_sensitive (EvolutionWizard *wizard,
					     gboolean back_sensitive,
					     gboolean next_sensitive,
					     gboolean cancel_sensitive,
					     CORBA_Environment *opt_ev);
void evolution_wizard_set_show_finish (EvolutionWizard *wizard,
				       gboolean show_finish,
				       CORBA_Environment *opt_ev);
void evolution_wizard_set_page (EvolutionWizard *wizard,
				int page_number,
				CORBA_Environment *opt_ev);

#endif /* __EVOLUTION_WIZARD_H__ */
