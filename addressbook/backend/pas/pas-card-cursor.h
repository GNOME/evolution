/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 2000, Ximian, Inc.
 */

#ifndef __PAS_CARD_CURSOR_H__
#define __PAS_CARD_CURSOR_H__

#include <bonobo/bonobo-object.h>
#include <pas/addressbook.h>

G_BEGIN_DECLS

#define PAS_TYPE_CARD_CURSOR        (pas_card_cursor_get_type ())
#define PAS_CARD_CURSOR(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_CARD_CURSOR, PASCardCursor))
#define PAS_CARD_CURSOR_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), PAS_TYPE_CARD_CURSOR, PASCardCursorClass))
#define PAS_IS_CARD_CURSOR(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_CARD_CURSOR))
#define PAS_IS_CARD_CURSOR_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_CARD_CURSOR))
#define PAS_CARD_CURSOR_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), PAS_TYPE_CARD_CURSOR, PASCardCursorClass))

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

	POA_GNOME_Evolution_Addressbook_CardCursor__epv epv;
};



/* Creating a new addressbook. */
PASCardCursor *pas_card_cursor_new       (PASCardCursorLengthFunc  get_length,
				          PASCardCursorNthFunc     get_nth,
					  gpointer data);

GType          pas_card_cursor_get_type  (void);
POA_GNOME_Evolution_Addressbook_CardCursor__epv *
               pas_card_cursor_get_epv   (void);

G_END_DECLS

#endif /* ! __PAS_CARD_CURSOR_H__ */
