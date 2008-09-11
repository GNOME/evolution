/*
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
 * Authors:
 *		Iain Holmes  <iain@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EVOLUTION_IMPORTER_LISTENER_H
#define EVOLUTION_IMPORTER_LISTENER_H

#include <glib.h>
#include <bonobo/bonobo-object.h>
#include <importer/GNOME_Evolution_Importer.h>
#include "evolution-importer.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define EVOLUTION_TYPE_IMPORTER_LISTENER            (evolution_importer_listener_get_type ())
#define EVOLUTION_IMPORTER_LISTENER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EVOLUTION_TYPE_IMPORTER_LISTENER, EvolutionImporterListener))
#define EVOLUTION_IMPORTER_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_IMPORTER_LISTENER, EvolutionImporterListenerClass))
#define EVOLUTION_IS_IMPORTER_LISTENER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVOLUTION_TYPE_IMPORTER_LISTENER))
#define EVOLUTION_IS_IMPORTER_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_IMPORTER_LISTENER))

typedef struct _EvolutionImporterListener        EvolutionImporterListener;
typedef struct _EvolutionImporterListenerPrivate EvolutionImporterListenerPrivate;
typedef struct _EvolutionImporterListenerClass   EvolutionImporterListenerClass;

typedef void (* EvolutionImporterListenerCallback) (EvolutionImporterListener *listener,
						    EvolutionImporterResult result,
						    gboolean more_items,
						    void *closure);
struct _EvolutionImporterListener {
	BonoboObject parent;

	EvolutionImporterListenerPrivate *priv;
};

struct _EvolutionImporterListenerClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_ImporterListener__epv epv;
};

GType evolution_importer_listener_get_type (void);

EvolutionImporterListener *evolution_importer_listener_new (EvolutionImporterListenerCallback callback,
							    void *closure);

#ifdef __cplusplus
}
#endif

#endif
