/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-importer.h
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 *
 * Author: Iain Holmes  <iain@ximian.com>
 */

#ifndef EVOLUTION_IMPORTER_H
#define EVOLUTION_IMPORTER_H

#include <glib.h>
#include <bonobo/bonobo-object.h>
#include <importer/GNOME_Evolution_Importer.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define EVOLUTION_TYPE_IMPORTER            (evolution_importer_get_type ())
#define EVOLUTION_IMPORTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EVOLUTION_TYPE_IMPORTER, EvolutionImporter))
#define EVOLUTION_IMPORTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_IMPORTER, EvolutionImporterClass))
#define EVOLUTION_IS_IMPORTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVOLUTION_TYPE_IMPORTER))
#define EVOLUTION_IS_IMPORTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_IMPORTER))

typedef struct _EvolutionImporter        EvolutionImporter;
typedef struct _EvolutionImporterPrivate EvolutionImporterPrivate;
typedef struct _EvolutionImporterClass   EvolutionImporterClass;

typedef gboolean (* EvolutionImporterSupportFormatFn) (EvolutionImporter *importer,
						       const char *filename,
						       void *closure);
typedef gboolean (* EvolutionImporterLoadFileFn) (EvolutionImporter *importer,
						  const char *filename,
						  const char *physical_uri,
						  const char *folder_type,
						  void *closure);
typedef void (* EvolutionImporterProcessItemFn) (EvolutionImporter *importer,
						 CORBA_Object listener,
						 void *closure,
						 CORBA_Environment *ev);
typedef char *(* EvolutionImporterGetErrorFn) (EvolutionImporter *importer,
					       void *closure);

typedef enum {
	EVOLUTION_IMPORTER_OK,
	EVOLUTION_IMPORTER_UNSUPPORTED_OPERATION,
	EVOLUTION_IMPORTER_INTERRUPTED,
	EVOLUTION_IMPORTER_BUSY,
	EVOLUTION_IMPORTER_NOT_READY,
	EVOLUTION_IMPORTER_UNKNOWN_DATA,
	EVOLUTION_IMPORTER_BAD_DATA,
	EVOLUTION_IMPORTER_BAD_FILE,
	EVOLUTION_IMPORTER_UNKNOWN_ERROR
} EvolutionImporterResult;

struct _EvolutionImporter {
	BonoboObject parent;
	
	EvolutionImporterPrivate *priv;
};

struct _EvolutionImporterClass {
	BonoboObjectClass parent_class;
	
	POA_GNOME_Evolution_Importer__epv epv;
};

GType evolution_importer_get_type (void);

EvolutionImporter *evolution_importer_new (EvolutionImporterSupportFormatFn support_format_fn,
					   EvolutionImporterLoadFileFn load_file_fn,
					   EvolutionImporterProcessItemFn process_item_fn,
					   EvolutionImporterGetErrorFn get_error_fn,
					   void *closure);

#ifdef __cplusplus
}
#endif

#endif
