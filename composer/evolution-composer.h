/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-composer.h
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
 * Author: Dan Winship
 */

#ifndef __EVOLUTION_COMPOSER_H__
#define __EVOLUTION_COMPOSER_H__

#include <bonobo/bonobo-object.h>

#include "Composer.h"
#include "e-msg-composer.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_COMPOSER            (evolution_composer_get_type ())
#define EVOLUTION_COMPOSER(obj)            (GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_COMPOSER, EvolutionComposer))
#define EVOLUTION_COMPOSER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_COMPOSER, EvolutionComposerClass))
#define EVOLUTION_IS_COMPOSER(obj)         (GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_COMPOSER))
#define EVOLUTION_IS_COMPOSER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_COMPOSER))

typedef struct _EvolutionComposer        EvolutionComposer;
typedef struct _EvolutionComposerClass   EvolutionComposerClass;

struct _EvolutionComposer {
	BonoboObject parent;

	EMsgComposer *composer;
};

struct _EvolutionComposerClass {
	BonoboObjectClass parent_class;
};

POA_Evolution_Composer__epv *evolution_composer_get_epv            (void);

GtkType            evolution_composer_get_type     (void);
void               evolution_composer_construct    (EvolutionComposer *,
						    Evolution_Composer);
EvolutionComposer *evolution_composer_new          (void);

void               evolution_composer_factory_init (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_COMPOSER_H__ */
