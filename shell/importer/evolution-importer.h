/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-importer.h
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

#ifndef EVOLUTION_IMPORTER_H
#define EVOLUTION_IMPORTER_H

#include <bonobo/bonobo-object.h>

#include "GNOME_Evolution_Importer.h"
#include "evolution-importer.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define EVOLUTION_TYPE_IMPORTER            (evolution_importer_get_type ())
#define EVOLUTION_IMPORTER(obj)            (GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_IMPORTER, EvolutionImporter))
#define EVOLUTION_IMPORTER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_IMPORTER, EvolutionImporterClass))
#define EVOLUTION_IS_IMPORTER(obj)         (GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_IMPORTER))
#define EVOLUTION_IS_IMPORTER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_IMPORTER))

typedef struct _EvolutionImporter        EvolutionImporter;
typedef struct _EvolutionImporterPrivate EvolutionImporterPrivate;
typedef struct _EvolutionImporterClass   EvolutionImporterClass;
typedef enum _EvolutionImporterResult EvolutionImporterResult;

typedef void (* EvolutionImporterProcessItemFn) (EvolutionImporter *importer,
						 GNOME_Evolution_ImporterListener listener,
						 void *closure,
						 CORBA_Environment *ev);
typedef char *(* EvolutionImporterGetErrorFn) (EvolutionImporter *importer,
					       void *closure);

enum _EvolutionImporterResult {
	EVOLUTION_IMPORTER_OK,
	EVOLUTION_IMPORTER_UNSUPPORTED_OPERATION,
	EVOLUTION_IMPORTER_INTERRUPTED,
	EVOLUTION_IMPORTER_BUSY,
	EVOLUTION_IMPORTER_NOT_READY,
	EVOLUTION_IMPORTER_UNKNOWN_DATA,
	EVOLUTION_IMPORTER_BAD_DATA,
	EVOLUTION_IMPORTER_BAD_FILE,
	EVOLUTION_IMPORTER_UNKNOWN_ERROR
};

struct _EvolutionImporter {
  BonoboObject parent;

  EvolutionImporterPrivate *private;
};

struct _EvolutionImporterClass {
  BonoboObjectClass parent_class;
};

GtkType evolution_importer_get_type (void);
void evolution_importer_construct (EvolutionImporter *importer,
				   CORBA_Object corba_object,
				   EvolutionImporterProcessItemFn process_item_fn,
				   EvolutionImporterGetErrorFn get_error_fn,
				   void *closure);

EvolutionImporter *evolution_importer_new (EvolutionImporterProcessItemFn process_item_fn,
					   EvolutionImporterGetErrorFn get_error_fn,
					   void *closure);

#ifdef __cplusplus
}
#endif

#endif
