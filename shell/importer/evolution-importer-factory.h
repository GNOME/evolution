/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-importer-factory.h
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

#ifndef EVOLUTION_IMPORTER_FACTORY_H
#define EVOLUTION_IMPORTER_FACTORY_H

#include <bonobo/bonobo-object.h>
#include <evolution-importer.h>
#include "GNOME_Evolution_Importer.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define EVOLUTION_TYPE_IMPORTER_FACTORY            (evolution_importer_factory_get_type ())
#define EVOLUTION_IMPORTER_FACTORY(obj)            (GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_IMPORTER_FACTORY, EvolutionImporterFactory))
#define EVOLUTION_IMPORTER_FACTORY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_IMPORTER_FACTORY, EvolutionImporterFactoryClass))
#define EVOLUTION_IS_IMPORTER_FACTORY(obj)         (GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_IMPORTER_FACTORY))
#define EVOLUTION_IS_IMPORTER_FACTORY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_IMPORTER_FACTORY))

typedef struct _EvolutionImporterFactory        EvolutionImporterFactory;
typedef struct _EvolutionImporterFactoryPrivate EvolutionImporterFactoryPrivate;
typedef struct _EvolutionImporterFactoryClass   EvolutionImporterFactoryClass;

typedef gboolean (* EvolutionImporterFactorySupportFormatFn) (EvolutionImporterFactory *factory,
							      const char *filename,
							      void *closure);
typedef EvolutionImporter *(* EvolutionImporterFactoryLoadFileFn) (EvolutionImporterFactory *factory,
								   const char *filename,
								   void *closure);
struct _EvolutionImporterFactory {
  BonoboObject parent;

  EvolutionImporterFactoryPrivate *private;
};

struct _EvolutionImporterFactoryClass {
  BonoboObjectClass parent_class;
};

GtkType evolution_importer_factory_get_type (void);
void evolution_importer_factory_construct (EvolutionImporterFactory *factory,
					   CORBA_Object corba_object,
					   EvolutionImporterFactorySupportFormatFn support_format_fn,
					   EvolutionImporterFactoryLoadFileFn load_file_fn,
					   void *closure);

EvolutionImporterFactory *evolution_importer_factory_new (EvolutionImporterFactorySupportFormatFn support_format_fn,
							  EvolutionImporterFactoryLoadFileFn load_file_fn,
							  void *closure);

#ifdef __cplusplus
}
#endif

#endif
