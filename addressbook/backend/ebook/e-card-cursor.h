/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __E_CARD_CURSOR_H__
#define __E_CARD_CURSOR_H__

#include <libgnome/gnome-defs.h>
#include <gtk/gtk.h>
#include "addressbook/backend/ebook/addressbook.h"
#include "e-card.h"

BEGIN_GNOME_DECLS

typedef struct _ECardCursor        ECardCursor;
typedef struct _ECardCursorPrivate ECardCursorPrivate;
typedef struct _ECardCursorClass   ECardCursorClass;

struct _ECardCursor {
	GtkObject           parent;
	ECardCursorPrivate *priv;
};

struct _ECardCursorClass {
	GtkObjectClass parent;
};

/* Creating a new addressbook. */
ECardCursor *e_card_cursor_new       (Evolution_CardCursor  corba_cursor);
ECardCursor *e_card_cursor_construct (ECardCursor          *cursor,
				      Evolution_CardCursor  corba_cursor);

GtkType      e_card_cursor_get_type    (void);

/* Fetching cards. */
long         e_card_cursor_get_length  (ECardCursor          *cursor);
ECard       *e_card_cursor_get_nth     (ECardCursor          *cursor,
					const long            nth);
#define E_CARD_CURSOR_TYPE        (e_card_cursor_get_type ())
#define E_CARD_CURSOR(o)          (GTK_CHECK_CAST ((o), E_CARD_CURSOR_TYPE, ECardCursor))
#define E_CARD_CURSOR_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CARD_CURSOR_TYPE, ECardCursorClass))
#define E_IS_CARD_CURSOR(o)       (GTK_CHECK_TYPE ((o), E_CARD_CURSOR_TYPE))
#define E_IS_CARD_CURSOR_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CARD_CURSOR_TYPE))

END_GNOME_DECLS

#endif /* ! __E_CARD_CURSOR_H__ */
