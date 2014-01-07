/*
 * Evolution calendar - Timezone selector dialog
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#include <libecal/libecal.h>

#include <e-util/e-util.h>

void		e_send_options_utils_set_default_data
						(ESendOptionsDialog *sod,
						 ESource *source,
						 const gchar *type);
void		e_send_options_utils_fill_component
						(ESendOptionsDialog *sod,
						 ECalComponent *comp,
						 icaltimezone *zone);

#endif
