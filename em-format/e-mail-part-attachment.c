/*
 * e-mail-part-attachment.c
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

#include "e-mail-part-attachment.h"

void
e_mail_part_attachment_free (EMailPartAttachment *empa)
{
	g_clear_object (&empa->attachment);

	if (empa->attachment_view_part_id) {
		g_free (empa->attachment_view_part_id);
		empa->attachment_view_part_id = NULL;
	}
}

EAttachment *
e_mail_part_attachment_ref_attachment (EMailPartAttachment *part)
{
	g_return_val_if_fail (part != NULL, NULL);

	return g_object_ref (part->attachment);
}

