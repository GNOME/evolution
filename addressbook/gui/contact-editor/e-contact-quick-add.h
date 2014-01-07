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
 *		Jon Trowbridge <trow@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_CONTACT_QUICK_ADD_H__
#define __E_CONTACT_QUICK_ADD_H__

#include <libebook/libebook.h>

#include <e-util/e-util.h>

typedef void	(*EContactQuickAddCallback)	(EContact *new_contact,
						 gpointer closure);

void		e_contact_quick_add		(EClientCache *client_cache,
						 const gchar *name,
						 const gchar *email,
						 EContactQuickAddCallback cb,
						 gpointer closure);
void		e_contact_quick_add_free_form	(EClientCache *client_cache,
						 const gchar *text,
						 EContactQuickAddCallback cb,
						 gpointer closure);
void		e_contact_quick_add_email	(EClientCache *client_cache,
						 const gchar *email,
						 EContactQuickAddCallback cb,
						 gpointer closure);
void		e_contact_quick_add_vcard	(EClientCache *client_cache,
						 const gchar *vcard,
						 EContactQuickAddCallback cb,
						 gpointer closure);

#endif /* __E_CONTACT_QUICK_ADD_H__ */

