/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-card-cursor.c: Implements card cursors.
 *
 * Author:
 *   Christopher James Lahey <clahey@helixcode.com.
 */

#include <config.h>
#include <bonobo.h>
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
	}

	CORBA_exception_free (&ev);

	if ( cursor->priv )
		g_free ( cursor->priv );

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

/*
 * CORBA Demo::Echo::echo method implementation
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
		}

		CORBA_exception_free (&ev);

		return ret_val;
	}
	else
		return 0;
}

/*
 * CORBA Demo::Echo::echo method implementation
 */
ECard *
e_card_cursor_get_nth (ECardCursor *cursor,
		       const long   n)
{
	if ( cursor->priv->corba_cursor != CORBA_OBJECT_NIL ) {
		CORBA_Environment      ev;
		CORBA_char * ret_val;
		ECard *card;

		CORBA_exception_init (&ev);

		ret_val = Evolution_CardCursor_get_nth(cursor->priv->corba_cursor, n, &ev);
		
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning("e_card_cursor_get_nth: Exception during "
				  "get_nth corba call.\n");
			CORBA_exception_free (&ev);
			CORBA_exception_init (&ev);
		}

		card = e_card_new(ret_val);
#if 0		
		CORBA_string__free(ret_val, &ev);
		
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning("e_card_cursor_get_nth: Exception freeing "
				  "string.\n");
		}
#endif
		CORBA_exception_free (&ev);

		return card;
	}
	else
		return e_card_new("");
}

static void
e_card_cursor_class_init (ECardCursorClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	parent_class = gtk_type_class (bonobo_object_get_type ());

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

ECardCursor *
e_card_cursor_construct (ECardCursor          *cursor,
			 Evolution_CardCursor  corba_cursor)
{
	CORBA_Environment      ev;
	g_return_val_if_fail (cursor != NULL, NULL);
	g_return_val_if_fail (E_IS_CARD_CURSOR (cursor), NULL);
	g_return_val_if_fail (corba_cursor != CORBA_OBJECT_NIL, NULL);

	/*
	 * Initialize cursor
	 */
	cursor->priv->corba_cursor = corba_cursor;

	CORBA_exception_init (&ev);
	
	Evolution_CardCursor_ref(corba_cursor, &ev);

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

ECardCursor *
e_card_cursor_new (Evolution_CardCursor corba_cursor)
{
	ECardCursor *cursor;

	cursor = gtk_type_new (e_card_cursor_get_type ());
	
	return e_card_cursor_construct (cursor,
					corba_cursor);
}
