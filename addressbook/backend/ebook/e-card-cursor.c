/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-card-cursor.c: Implements card cursors.
 *
 * Author:
 *   Christopher James Lahey <clahey@helixcode.com.
 */

#include <config.h>
#include <gtk/gtk.h>
#include "addressbook.h"
#include "e-card-cursor.h"

struct _ECardCursorPrivate {
	Evolution_CardCursor corba_cursor;
};

/*
 * A pointer to our parent object class
 */
static GtkObjectClass *parent_class;

/*
 * Implemented GtkObject::destroy
 */
static void
e_card_cursor_destroy (GtkObject *object)
{
	ECardCursor *cursor = E_CARD_CURSOR (object);
	CORBA_Environment      ev;

	CORBA_exception_init (&ev);

	Evolution_CardCursor_unref( cursor->priv->corba_cursor, &ev );

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("e_card_cursor_destroy: Exception unreffing "
			  "corba cursor.\n");
		CORBA_exception_free (&ev);
		CORBA_exception_init (&ev);
	}

	CORBA_Object_release (cursor->priv->corba_cursor, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("e_card_cursor_destroy: Exception releasing "
			  "corba cursor.\n");
	}

	CORBA_exception_free (&ev);

	if ( cursor->priv )
		g_free ( cursor->priv );

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

/**
 * e_card_cursor_get_length:
 * @cursor: the #ECardCursor whose length is being queried
 *
 * Returns: the number of items the cursor references, or -1 there's
 * an error.
 */
long
e_card_cursor_get_length (ECardCursor *cursor)
{
	if ( cursor->priv->corba_cursor != CORBA_OBJECT_NIL ) {
		CORBA_Environment      ev;
		long ret_val;

		CORBA_exception_init (&ev);

		ret_val = Evolution_CardCursor_get_length(cursor->priv->corba_cursor, &ev);
		
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning("e_card_cursor_get_length: Exception during "
				  "get_length corba call.\n");
			ret_val = -1;
		}

		CORBA_exception_free (&ev);

		return ret_val;
	}
	else
		return -1;
}

/**
 * e_card_cursor_get_nth:
 * @cursor: an #ECardCursor object
 * @n: the index of the item requested
 *
 * Gets an #ECard based on an index.
 *
 * Returns: a new #ECard on success, or %NULL on failure.
 */
ECard *
e_card_cursor_get_nth (ECardCursor *cursor,
		       const long   n)
{
	if ( cursor->priv->corba_cursor != CORBA_OBJECT_NIL ) {
		CORBA_Environment      en;
		CORBA_char *vcard;
		ECard *card;

		CORBA_exception_init (&en);

		vcard = Evolution_CardCursor_get_nth(cursor->priv->corba_cursor, n, &en);
		
		if (en._major != CORBA_NO_EXCEPTION) {
			g_warning("e_card_cursor_get_nth: Exception during "
				  "get_nth corba call.\n");
		}
		
		CORBA_exception_free (&en);

		card = e_card_new (vcard);

		CORBA_free(vcard);

		return card;
	}
	else
		return e_card_new("");
}

static void
e_card_cursor_class_init (ECardCursorClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	parent_class = gtk_type_class (gtk_object_get_type ());

	object_class->destroy = e_card_cursor_destroy;
}

static void
e_card_cursor_init (ECardCursor *cursor)
{
	cursor->priv = g_new(ECardCursorPrivate, 1);
	cursor->priv->corba_cursor = CORBA_OBJECT_NIL;
}

GtkType
e_card_cursor_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ECardCursor",
			sizeof (ECardCursor),
			sizeof (ECardCursorClass),
			(GtkClassInitFunc) e_card_cursor_class_init,
			(GtkObjectInitFunc) e_card_cursor_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_object_get_type (), &info);
	}

	return type;
}

/**
 * e_card_cursor_construct:
 * @cursor: an #ECardCursor object
 * @corba_cursor: an #Evolution_CardCursor
 *
 * Wraps an #Evolution_CardCursor object inside the #ECardCursor
 * @cursor object.
 *
 * Returns: a new #ECardCursor on success, or %NULL on failure.
 */
ECardCursor *
e_card_cursor_construct (ECardCursor          *cursor,
			 Evolution_CardCursor  corba_cursor)
{
	CORBA_Environment      ev;
	g_return_val_if_fail (cursor != NULL, NULL);
	g_return_val_if_fail (E_IS_CARD_CURSOR (cursor), NULL);
	g_return_val_if_fail (corba_cursor != CORBA_OBJECT_NIL, NULL);

	CORBA_exception_init (&ev);

	/*
	 * Initialize cursor
	 */
	cursor->priv->corba_cursor = CORBA_Object_duplicate(corba_cursor, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("e_card_cursor_construct: Exception duplicating "
			  "corba cursor.\n");
		CORBA_exception_free (&ev);
		CORBA_exception_init (&ev);
	}
	
	Evolution_CardCursor_ref(cursor->priv->corba_cursor, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("e_card_cursor_construct: Exception reffing "
			  "corba cursor.\n");
	}

	CORBA_exception_free (&ev);
	
	/*
	 * Success: return the GtkType we were given
	 */
	return cursor;
}

/**
 * e_card_cursor_new:
 * @cursor: the #Evolution_CardCursor to be wrapped
 *
 * Creates a new #ECardCursor, which wraps an #Evolution_CardCursor
 * object.
 *
 * Returns: a new #ECardCursor on success, or %NULL on failure.
 */
ECardCursor *
e_card_cursor_new (Evolution_CardCursor corba_cursor)
{
	ECardCursor *cursor;

	cursor = gtk_type_new (e_card_cursor_get_type ());
	
	return e_card_cursor_construct (cursor,
					corba_cursor);
}
