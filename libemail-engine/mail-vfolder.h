/*
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _MAIL_VFOLDER_H
#define _MAIL_VFOLDER_H

#include <camel/camel.h>

#include <e-util/e-util.h>
#include <libemail-engine/e-mail-session.h>
#include <libemail-engine/em-vfolder-rule.h>

void		vfolder_load_storage		(EMailSession *session);

/* close up, clean up */
void		mail_vfolder_shutdown		(void);

#endif
