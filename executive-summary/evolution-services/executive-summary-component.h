/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* executive-summary-component.h
 *
 * Authors: Iain Holmes <iain@helixcode.com>
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
 */

#ifndef _EXECUTIVE_SUMMARY_COMPONENT_H__
#define _EXECUTIVE_SUMMARY_COMPONENT_H__

#include <gtk/gtksignal.h>
#include <bonobo/bonobo-object.h>

#define EXECUTIVE_SUMMARY_COMPONENT_TYPE (executive_summary_component_get_type ())
#define EXECUTIVE_SUMMARY_COMPONENT(obj) (GTK_CHECK_CAST ((obj), EXECUTIVE_SUMMARY_COMPONENT_TYPE, ExecutiveSummaryComponent))
#define EXECUTIVE_SUMMARY_COMPONENT_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), EXECUTIVE_SUMMARY_COMPONENT_TYPE, ExecutiveSummaryComponentClass))
#define IS_EXECUTIVE_SUMMARY_COMPONENT(obj) (GTK_CHECK_TYPE ((obj), EXECUTIVE_SUMMARY_COMPONENT_TYPE))
#define IS_EXECUTIVE_SUMMARY_COMPONENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EXECUTIVE_SUMMARY_COMPONENT_TYPE))

typedef struct _ExecutiveSummaryComponentPrivate ExecutiveSummaryComponentPrivate;
typedef struct _ExecutiveSummaryComponent ExecutiveSummaryComponent;
typedef struct _ExecutiveSummaryComponentClass ExecutiveSummaryComponentClass;

struct _ExecutiveSummaryComponent {
  BonoboObject parent;

  ExecutiveSummaryComponentPrivate *private;
};

struct _ExecutiveSummaryComponentClass {
  BonoboObjectClass parent_class;
};

GtkType executive_summary_component_get_type (void);

BonoboObject *executive_summary_component_new (void);


/* Factory */

#define EXECUTIVE_SUMMARY_COMPONENT_FACTORY_TYPE (executive_summary_component_factory_get_type ())
#define EXECUTIVE_SUMMARY_COMPONENT_FACTORY(obj) (GTK_CHECK_CAST ((obj), EXECUTIVE_SUMMARY_COMPONENT_FACTORY_TYPE, ExecutiveSummaryComponentFactory))
#define EXECUTIVE_SUMMARY_COMPONENT_FACTORY_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), EXECUTIVE_SUMMARY_COMPONENT_FACTORY_TYPE, ExecutiveSummaryComponentFactoryClass))
#define IS_EXECUTIVE_SUMMARY_COMPONENT_FACTORY(obj) (GTK_CHECK_TYPE ((obj), EXECUTIVE_SUMMARY_COMPONENT_FACTORY_TYPE))
#define IS_EXECUTIVE_SUMMARY_COMPONENT_FACTORY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EXECUTIVE_SUMMARY_COMPONENT_FACTORY_TYPE))

typedef struct _ExecutiveSummaryComponentFactoryPrivate ExecutiveSummaryComponentFactoryPrivate;
typedef struct _ExecutiveSummaryComponentFactory ExecutiveSummaryComponentFactory;
typedef struct _ExecutiveSummaryComponentFactoryClass ExecutiveSummaryComponentFactoryClass;

typedef BonoboObject *(* EvolutionServicesCreateViewFn) (ExecutiveSummaryComponentFactory *factory,
							 void *closure);

struct _ExecutiveSummaryComponentFactory {
	BonoboObject parent;

	ExecutiveSummaryComponentFactoryPrivate *private;
};

struct _ExecutiveSummaryComponentFactoryClass {
	BonoboObjectClass parent_class;
};

GtkType executive_summary_component_factory_get_type (void);

BonoboObject *executive_summary_component_factory_new (EvolutionServicesCreateViewFn create_view,
						       void *closure);
#endif
