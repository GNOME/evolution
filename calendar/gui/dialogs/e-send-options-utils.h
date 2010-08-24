/*
 * Evolution calendar - Timezone selector dialog
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_SENDOPTIONS_UTILS_H__
#define __E_SENDOPTIONS_UTILS_H__

#include "misc/e-send-options.h"
#include <libecal/e-cal-component.h>
#include <libedataserver/e-source-list.h>

void e_send_options_utils_set_default_data (ESendOptionsDialog *sod, ESource *source, const gchar *type);
void e_send_options_utils_fill_component (ESendOptionsDialog *sod, ECalComponent *comp);
#endif
