/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-importer-client.h
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
 * Author: Iain Holmes  <iain@helixcode.com>
 */

#ifndef EVOLUTION_IMPORTER_CLIENT_H
#define EVOLUTION_IMPORTER_CLIENT_H

#include <bonobo/bonobo-object-client.h>

#include "evolution-importer.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

#define EVOLUTION_TYPE_IMPORTER_CLIENT (evolution_importer_client_get_type ())
#define EVOLUTION_IMPORTER_CLIENT(obj) (GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_IMPORTER_CLIENT, EvolutionImporterClient))
#define EVOLUTION_IMPORTER_CLIENT_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_IMPORTER_CLIENT, EvolutionImporterClientClass))
#define EVOLUTION_IS_IMPORTER_CLIENT(obj) (GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_IMPORTER_CLIENT))
#define EVOLUTION_IS_IMPORTER_CLIENT_CLASS(klass) (GTK_CHECK_TYPE ((klass), EVOLUTION_TYPE_IMPORTER_CLIENT))


typedef struct _EvolutionImporterClient EvolutionImporterClient;
typedef struct _EvolutionImporterClientPrivate EvolutionImporterClientPrivate;
typedef struct _EvolutionImporterClientClass EvolutionImporterClientClass;

struct _EvolutionImporterClient {
  BonoboObjectClient parent;

  EvolutionImporterClientPrivate *private;
};

struct _EvolutionImporterClientClass {
  BonoboObjectClientClass parent_class;
};

typedef void (* EvolutionImporterClientCallback) (EvolutionImporterClient *client,
						  EvolutionImporterResult result,
						  gboolean more_items,
						  void *data);

GtkType evolution_importer_client_get_type (void);
void evolution_importer_client_construct (EvolutionImporterClient *client,
					  CORBA_Object corba_object);
EvolutionImporterClient *evolution_importer_client_new (const GNOME_Evolution_Importer objref);

void evolution_importer_client_process_item (EvolutionImporterClient *client,
					     EvolutionImporterClientCallback callback,
					     void *closure);
const char *evolution_importer_client_get_error (EvolutionImporterClient *client);

#ifdef __cplusplus
}
#endif

#endif
