/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __PAS_CARD_CURSOR_H__
#define __PAS_CARD_CURSOR_H__

#include <libgnome/gnome-defs.h>
#include <bonobo/bonobo-object.h>
#include <pas/addressbook.h>

BEGIN_GNOME_DECLS

typedef struct _PASCardCursor        PASCardCursor;
typedef struct _PASCardCursorPrivate PASCardCursorPrivate;
typedef struct _PASCardCursorClass   PASCardCursorClass;

typedef long (*PASCardCursorLengthFunc) (PASCardCursor *cursor, gpointer data);
typedef char * (*PASCardCursorNthFunc) (PASCardCursor *cursor, long n, gpointer data);

struct _PASCardCursor {
	BonoboObject     parent;
	PASCardCursorPrivate *priv;
};

struct _PASCardCursorClass {
	BonoboObjectClass parent;
};

/* Creating a new addressbook. */
PASCardCursor *pas_card_cursor_new       (PASCardCursorLengthFunc  get_length,
				          PASCardCursorNthFunc     get_nth,
					  gpointer data);
PASCardCursor *pas_card_cursor_construct (PASCardCursor           *cursor,
					  Evolution_CardCursor     corba_cursor,
					  PASCardCursorLengthFunc  get_length,
					  PASCardCursorNthFunc     get_nth,
					  gpointer data);

GtkType        pas_card_cursor_get_type  (void);
POA_Evolution_CardCursor__epv *
               pas_card_cursor_get_epv   (void);

/* Fetching cards. */
#define PAS_CARD_CURSOR_TYPE        (pas_card_cursor_get_type ())
#define PAS_CARD_CURSOR(o)          (GTK_CHECK_CAST ((o), PAS_CARD_CURSOR_TYPE, PASCardCursor))
#define PAS_CARD_CURSOR_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), PAS_CARD_CURSOR_TYPE, PASCardCursorClass))
#define PAS_IS_CARD_CURSOR(o)       (GTK_CHECK_TYPE ((o), PAS_CARD_CURSOR_TYPE))
#define PAS_IS_CARD_CURSOR_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), PAS_CARD_CURSOR_TYPE))

END_GNOME_DECLS

#endif /* ! __PAS_CARD_CURSOR_H__ */
