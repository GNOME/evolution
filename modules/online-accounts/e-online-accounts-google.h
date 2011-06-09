/*
 * e-online-accounts-google.h
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
 */

#ifndef E_ONLINE_ACCOUNTS_GOOGLE_H
#define E_ONLINE_ACCOUNTS_GOOGLE_H

#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>

G_BEGIN_DECLS

void		e_online_accounts_google_sync	(GoaObject *goa_object,
						 const gchar *evo_id);

G_END_DECLS

#endif /* E_ONLINE_ACCOUNTS_GOOGLE_H */
