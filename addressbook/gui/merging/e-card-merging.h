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

#include <addressbook/backend/ebook/e-book.h>

G_BEGIN_DECLS

gboolean  e_card_merging_book_add_card     (EBook           *book,
					    ECard           *card,
					    EBookIdCallback  cb,
					    gpointer         closure);
gboolean  e_card_merging_book_commit_card  (EBook           *book,
					    ECard           *card,
					    EBookCallback    cb,
					    gpointer         closure);

G_END_DECLS

#endif /* ! __E_CARD_MERGING_H__ */
