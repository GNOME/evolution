/*
 * The Evolution addressbook client object.
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
 *
 * Authors:
 *		Christopher James Lahey <clahey@ximian.com>
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_CONTACT_MERGING_H__
#define __E_CONTACT_MERGING_H__

#include <libebook/e-book.h>

G_BEGIN_DECLS

gboolean  eab_merging_book_add_contact    (EBook           *book,
					   EContact        *contact,
					   EBookIdCallback  cb,
					   gpointer         closure);
gboolean  eab_merging_book_commit_contact (EBook           *book,
					   EContact        *contact,
					   EBookCallback    cb,
					   gpointer         closure);

G_END_DECLS

#endif /* ! __EAB_CONTACT_MERGING_H__ */
