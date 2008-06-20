/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-intelligent-importer.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Iain Holmes  <iain@ximian.com>
 */

#ifndef EVOLUTION_INTELLIGENT_IMPORTER_H
#define EVOLUTION_INTELLIGENT_IMPORTER_H

#include <glib.h>
#include <bonobo/bonobo-object.h>
#include <importer/GNOME_Evolution_Importer.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_INTELLIGENT_IMPORTER            (evolution_intelligent_importer_get_type ())
#define EVOLUTION_INTELLIGENT_IMPORTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EVOLUTION_TYPE_INTELLIGENT_IMPORTER, EvolutionIntelligentImporter))
#define EVOLUTION_INTELLIGENT_IMPORTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_INTELLIGENT_IMPORTER, EvolutionIntelligentImporterClass))
#define EVOLUTION_IS_INTELLIGENT_IMPORTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVOLUTION_TYPE_INTELLIGENT_IMPORTER))
#define EVOLUTION_IS_INTELLIGENT_IMPORTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_INTELLIGENT_IMPORTER))

typedef struct _EvolutionIntelligentImporter EvolutionIntelligentImporter;
typedef struct _EvolutionIntelligentImporterPrivate EvolutionIntelligentImporterPrivate;
typedef struct _EvolutionIntelligentImporterClass EvolutionIntelligentImporterClass;

typedef gboolean (* EvolutionIntelligentImporterCanImportFn) (EvolutionIntelligentImporter *ii,
							      void *closure);
typedef void (* EvolutionIntelligentImporterImportDataFn) (EvolutionIntelligentImporter *ii,
							   void *closure);

struct _EvolutionIntelligentImporter {
  BonoboObject parent;

  EvolutionIntelligentImporterPrivate *priv;
};

struct _EvolutionIntelligentImporterClass {
  BonoboObjectClass parent_class;

  POA_GNOME_Evolution_IntelligentImporter__epv epv;
};

GType evolution_intelligent_importer_get_type (void);

EvolutionIntelligentImporter *evolution_intelligent_importer_new (EvolutionIntelligentImporterCanImportFn can_import_fn,
								  EvolutionIntelligentImporterImportDataFn import_data_fn,
								  const char *importername,
								  const char *message,
								  void *closure);

#ifdef __cplusplus
}
#endif

#endif
