/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * The Evolution addressbook client object.
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 */

#ifndef __E_CARD_MERGING_H__
#define __E_CARD_MERGING_H__

#include <libgnome/gnome-defs.h>

#include <addressbook/backend/ebook/e-book.h>

BEGIN_GNOME_DECLS

gboolean  e_card_merging_book_add_card  (EBook                 *book,
					 ECard                 *card,
					 EBookIdCallback        cb,
					 gpointer               closure);

END_GNOME_DECLS

#endif /* ! __E_CARD_MERGING_H__ */
