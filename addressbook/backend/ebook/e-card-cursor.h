/*
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <gtk/gtkobject.h>
#include <libgnome/gnome-defs.h>

#include <e-book.h>

#ifndef __E_CARD_CURSOR_H__
#define __E_CARD_CURSOR_H__

BEGIN_GNOME_DECLS

typedef struct _ECardCursorPrivate ECardCursorPrivate;

typedef struct {
	GtkObject     parent;
	ECardCursorPrivate *priv;
} ECardCursor;

typedef struct {
	GtkObjectClass parent;
} ECardCursorClass;

/* Creating a new addressbook. */
ECardCursor *e_card_cursor_new         (EBook                *book,
					Evolution_CardCursor  corba_cursor);
GtkType      e_card_cursor_get_type    (void);

/* Fetching cards. */
int          e_card_cursor_get_length  (ECardCursor          *cursor);
ECard       *e_card_cursor_get_nth     (ECardCursor          *cursor,
					int                   nth);
#define E_CARD_CURSOR_TYPE        (e_card_cursor_get_type ())
#define E_CARD_CURSOR(o)          (GTK_CHECK_CAST ((o), E_CARD_CURSOR_TYPE, ECardCursor))
#define E_CARD_CURSOR_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CARD_CURSOR_TYPE, ECardCursorClass))
#define E_IS_CARD_CURSOR(o)       (GTK_CHECK_TYPE ((o), E_CARD_CURSOR_TYPE))
#define E_IS_CARD_CURSOR_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CARD_CURSOR_TYPE))

END_GNOME_DECLS

#endif /* ! __E_CARD_CURSOR_H__ */
