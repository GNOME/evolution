/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-simple-card-bonobo.h
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
 * Authors:
 * Ettore Perazzoli <ettore@ximian.com>
 * Chris Lahey <clahey@ximian.com>
 */

#ifndef __E_SIMPLE_CARD_BONOBO_H__
#define __E_SIMPLE_CARD_BONOBO_H__

#include <bonobo/bonobo-object.h>

#include "Evolution-Addressbook-SelectNames.h"
#include <addressbook/backend/ebook/e-card-simple.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SIMPLE_CARD_BONOBO	    (e_simple_card_bonobo_get_type ())
#define E_SIMPLE_CARD_BONOBO(obj)	    (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SIMPLE_CARD_BONOBO, ESimpleCardBonobo))
#define E_SIMPLE_CARD_BONOBO_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SIMPLE_CARD_BONOBO, ESimpleCardBonoboClass))
#define E_IS_SIMPLE_CARD_BONOBO(obj)	    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SIMPLE_CARD_BONOBO))
#define E_IS_SIMPLE_CARD_BONOBO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_SIMPLE_CARD_BONOBO))


typedef struct _ESimpleCardBonobo        ESimpleCardBonobo;
typedef struct _ESimpleCardBonoboPrivate ESimpleCardBonoboPrivate;
typedef struct _ESimpleCardBonoboClass   ESimpleCardBonoboClass;

struct _ESimpleCardBonobo {
	BonoboObject parent;

	ESimpleCardBonoboPrivate *priv;
};

struct _ESimpleCardBonoboClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Addressbook_SimpleCard__epv epv;
};


GType              e_simple_card_bonobo_get_type   (void);
ESimpleCardBonobo *e_simple_card_bonobo_new        (ECardSimple                            *card_simple);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_SIMPLE_CARD_BONOBO_H__ */
