/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * The Evolution addressbook client object.
 *
 * Authors:
 *   Christopher James Lahey <clahey@ximian.com>
 *   Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 2001, 2002, 2003 Ximian, Inc.
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
