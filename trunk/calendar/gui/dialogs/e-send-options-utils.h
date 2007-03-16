/* Evolution calendar - Timezone selector dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Damon Chaplin <damon@ximian.com>
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
 */

#ifndef __E_SENDOPTIONS_UTILS_H__
#define __E_SENDOPTIONS_UTILS_H__

#include "misc/e-send-options.h"
#include <libecal/e-cal-component.h>
#include <libedataserver/e-source-list.h>

void e_sendoptions_utils_set_default_data (ESendOptionsDialog *sod, ESource *source, char* type);   
void e_sendoptions_utils_fill_component (ESendOptionsDialog *sod, ECalComponent *comp);
#endif
