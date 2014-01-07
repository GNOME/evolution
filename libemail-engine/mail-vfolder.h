/*
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef _MAIL_VFOLDER_H
#define _MAIL_VFOLDER_H

#include <camel/camel.h>

#include <libemail-engine/e-mail-session.h>
#include <libemail-engine/em-vfolder-rule.h>

void		vfolder_load_storage		(EMailSession *session);

/* close up, clean up */
void		mail_vfolder_shutdown		(void);

#endif
