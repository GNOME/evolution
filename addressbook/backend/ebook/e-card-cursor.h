/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 2000, Ximian, Inc.
 */

#ifndef __E_CARD_CURSOR_H__
#define __E_CARD_CURSOR_H__

#include <glib.h>
#include <glib-object.h>
#include <ebook/addressbook.h>
#include <ebook/e-card.h>

#define E_TYPE_CARD_CURSOR          (e_card_cursor_get_type ())
#define E_CARD_CURSOR(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_CARD_CURSOR, ECardCursor))
#define E_CARD_CURSOR_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), E_TYPE_CARD_CURSOR, ECardCursorClass))
#define E_IS_CARD_CURSOR(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_CARD_CURSOR))
#define E_IS_CARD_CURSOR_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_CARD_CURSOR))
#define E_CARD_CURSOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_CARD_CURSOR, ECardCursorClass))

G_BEGIN_DECLS

typedef struct _ECardCursor        ECardCursor;
typedef struct _ECardCursorPrivate ECardCursorPrivate;
typedef struct _ECardCursorClass   ECardCursorClass;

struct _ECardCursor {
	GObject             parent;
	ECardCursorPrivate *priv;
};

struct _ECardCursorClass {
	GObjectClass parent;
};

/* Creating a new addressbook. */
ECardCursor *e_card_cursor_new       (GNOME_Evolution_Addressbook_CardCursor  corba_cursor);
ECardCursor *e_card_cursor_construct (ECardCursor          *cursor,
				      GNOME_Evolution_Addressbook_CardCursor  corba_cursor);

GType        e_card_cursor_get_type    (void);

/* Fetching cards. */
long         e_card_cursor_get_length  (ECardCursor          *cursor);
ECard       *e_card_cursor_get_nth     (ECardCursor          *cursor,
					const long            nth);
G_END_DECLS

#endif /* ! __E_CARD_CURSOR_H__ */
