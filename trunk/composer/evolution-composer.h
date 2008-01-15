/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-composer.h
 *
 * Copyright (C) 2000  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * published by the Free Software Foundation; either version 2 of the
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
#define EVOLUTION_COMPOSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EVOLUTION_TYPE_COMPOSER, EvolutionComposer))
#define EVOLUTION_COMPOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_COMPOSER, EvolutionComposerClass))
#define EVOLUTION_IS_COMPOSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVOLUTION_TYPE_COMPOSER))
#define EVOLUTION_IS_COMPOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_COMPOSER))

typedef struct _EvolutionComposer        EvolutionComposer;
typedef struct _EvolutionComposerClass   EvolutionComposerClass;

struct _EvolutionComposer {
	BonoboObject parent;

	struct _EvolutionComposerPrivate *priv;

	EMsgComposer *composer;
};

struct _EvolutionComposerClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Composer__epv epv;
};

POA_GNOME_Evolution_Composer__epv *evolution_composer_get_epv            (void);

GtkType            evolution_composer_get_type     (void);
void               evolution_composer_construct    (EvolutionComposer *,
						    GNOME_Evolution_Composer);
EvolutionComposer *evolution_composer_new          (void (*send_cb) (EMsgComposer *, gpointer),
						    void (*save_draft_cb) (EMsgComposer *, int, gpointer));

void               evolution_composer_factory_init (void (*send) (EMsgComposer *, gpointer),
						    void (*save_draft) (EMsgComposer *, int, gpointer));

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_COMPOSER_H__ */
