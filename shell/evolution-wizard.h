/*
 * evolution-wizard.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifndef __EVOLUTION_WIZARD_H__
#define __EVOLUTION_WIZARD_H__

#include <bonobo/bonobo-xobject.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-event-source.h>

#include "Evolution.h"

typedef struct _EvolutionWizard EvolutionWizard;
typedef struct _EvolutionWizardPrivate EvolutionWizardPrivate;

#define EVOLUTION_WIZARD_SET_PAGE "GNOME::Evolution::Wizard_setPage"
#define EVOLUTION_WIZARD_SET_SHOW_FINISH "GNOME::Evolution::Wizard_setShowFinish"
#define EVOLUTION_WIZARD_SET_BUTTONS_SENSITIVE "GNOME::Evolution::Wizard_setButtonsSensitive"

#define EVOLUTION_WIZARD_TYPE (evolution_wizard_get_type ())
#define EVOLUTION_WIZARD(o) (GTK_CHECK_CAST ((o), EVOLUTION_WIZARD_TYPE, EvolutionWizard))
#define EVOLUTION_WIZARD_CLASS(k) (GTK_CHECK_CLASS_CAST((k), EVOLUTION_WIZARD_TYPE, EvolutionWizardClass))
#define IS_EVOLUTION_WIZARD(o) (GTK_CHECK_TYPE ((o), EVOLUTION_WIZARD_TYPE))
#define IS_EVOLUTION_WIZARD_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), EVOLUTION_WIZARD_TYPE))

typedef BonoboControl *(* EvolutionWizardGetControlFn) (EvolutionWizard *wizard,
							int page_number,
							void *closure);

struct _EvolutionWizard {
	BonoboXObject object;

	EvolutionWizardPrivate *priv;
};

typedef struct {
	BonoboXObjectClass parent_class;

	POA_GNOME_Evolution_Wizard__epv epv;

	void (* next) (EvolutionWizard *wizard, int page_number);
	void (* prepare) (EvolutionWizard *wizard, int page_number);
	void (* back) (EvolutionWizard *wizard, int page_number);
	void (* finish) (EvolutionWizard *wizard, int page_number);
	void (* cancel) (EvolutionWizard *wizard, int page_number);
	void (* help) (EvolutionWizard *wizard, int page_number);
} EvolutionWizardClass;

GtkType evolution_wizard_get_type (void);

EvolutionWizard *evolution_wizard_construct (EvolutionWizard *wizard,
					     BonoboEventSource *event_source,
					     EvolutionWizardGetControlFn get_fn,
					     int num_pages,
					     void *closure);
EvolutionWizard *evolution_wizard_new_full (EvolutionWizardGetControlFn get_fn,
					    int num_pages,
					    BonoboEventSource *event_source,
					    void *closure);
EvolutionWizard *evolution_wizard_new (EvolutionWizardGetControlFn get_fn,
				       int num_pages,
				       void *closure);

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

BonoboEventSource * evolution_wizard_get_event_source (EvolutionWizard *wizard);

#endif /* __EVOLUTION_WIZARD_H__ */
