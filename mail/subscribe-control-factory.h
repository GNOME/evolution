/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * subscribe-control-factory.h: A Bonobo Control factory for Subscribe Controls
 *
 * Author:
 *   Chris Toshok (toshok@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */

#ifndef _SUBSCRIBE_CONTROL_FACTORY_H
#define _SUBSCRIBE_CONTROL_FACTORY_H

#include <bonobo.h>
#include "Evolution.h"

BonoboControl *subscribe_control_factory_new_control       (const char            *uri,
							    const Evolution_Shell  shell);
GList         *subscribe_control_factory_get_control_list  (void);

#endif /* _SUBSCRIBE_CONTROL_FACTORY_H */
