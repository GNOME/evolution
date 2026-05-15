/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Jon Trowbridge <trow@ximian.com>
 */

#ifndef __E_CONTACT_QUICK_ADD_H__
#define __E_CONTACT_QUICK_ADD_H__

#include <libebook/libebook.h>

#include <e-util/e-util.h>

void		e_contact_quick_add		(EClientCache *client_cache,
						 const gchar *name,
						 const gchar *email);
void		e_contact_quick_add_free_form	(EClientCache *client_cache,
						 const gchar *text);
void		e_contact_quick_add_email	(EClientCache *client_cache,
						 const gchar *email);
void		e_contact_quick_add_vcard	(EClientCache *client_cache,
						 const gchar *vcard);

#endif /* __E_CONTACT_QUICK_ADD_H__ */

