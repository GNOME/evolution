/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
