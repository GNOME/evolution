/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _EVOLUTION_SERVICE_REPOSITORY_H
#define _EVOLUTION_SERVICE_REPOSITORY_H 1

#include <bonobo/bonobo-object.h>
#include "Evolution.h"

BEGIN_GNOME_DECLS

#define EVOLUTION_SERVICE_REPOSITORY_TYPE        (evolution_service_repository_get_type ())
#define EVOLUTION_SERVICE_REPOSITORY(o)          (GTK_CHECK_CAST ((o), EVOLUTION_SERVICE_REPOSITORY_TYPE, EvolutionServiceRepository))
#define EVOLUTION_SERVICE_REPOSITORY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), EVOLUTION_SERVICE_REPOSITORY_TYPE, EvolutionServiceRepositoryClass))
#define EVOLUTION_IS_SERVICE_REPOSITORY(o)       (GTK_CHECK_TYPE ((o), EVOLUTION_SERVICE_REPOSITORY_TYPE))
#define EVOLUTION_IS_SERVICE_REPOSITORY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), EVOLUTION_SERVICE_REPOSITORY_TYPE))

typedef struct _EvolutionServiceRepository EvolutionServiceRepositoryPrivate;
typedef struct _EvolutionServiceRepository EvolutionServiceRepository;

typedef void (*EvolutionServiceRepositorySetShellFn) (EvolutionServiceRepository *sr, const Evolution_Shell shell, void *closure);


struct  _EvolutionServiceRepository {
	BonoboObject object;

	
	EvolutionServiceRepositorySetShellFn set_shell_fn;

	void *closure;

	EvolutionServiceRepositoryPrivate *priv;

};



typedef struct {
	BonoboObjectClass parent_class;

	void        (*set_shell) (EvolutionServiceRepository *sr, Evolution_Shell shell);

} EvolutionServiceRepositoryClass;



GtkType                              evolution_service_repository_get_type  (void);

EvolutionServiceRepository *         evolution_service_repository_new       (
					    EvolutionServiceRepositorySetShellFn set_shell_fn,
					    void *closure);

EvolutionServiceRepository *         evolution_service_repository_construct (
					    EvolutionServiceRepository *sr,
					    Evolution_ServiceRepository corba_service_repository,
					    EvolutionServiceRepositorySetShellFn set_shell_fn,
					    void *closure);


POA_Evolution_ServiceRepository__epv *evolution_service_repository_get_epv  (void);

extern POA_Evolution_ServiceRepository__vepv evolution_service_repository_vepv;

END_GNOME_DECLS


#endif /* _EVOLUTION_SERVICE_REPOSITORY_H */

