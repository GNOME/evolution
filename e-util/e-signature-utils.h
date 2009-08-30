/*
 * e-signature-utils.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef E_SIGNATURE_UTILS_H
#define E_SIGNATURE_UTILS_H

#include <gtk/gtk.h>
#include <e-util/e-signature.h>
#include <e-util/e-signature-list.h>

G_BEGIN_DECLS

ESignatureList *e_get_signature_list		(void);
ESignature *	e_get_signature_by_name		(const gchar *name);
ESignature *	e_get_signature_by_uid		(const gchar *uid);
gchar *		e_create_signature_file		(GError **error);
gchar *		e_read_signature_file		(ESignature *signature,
						 gboolean convert_to_html,
						 GError **error);
gchar *		e_run_signature_script		(const gchar *filename);

G_END_DECLS

#endif /* E_SIGNATURE_UTILS_H */
