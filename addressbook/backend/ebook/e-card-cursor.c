/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-card-cursor.c: Implements card cursors.
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com.
 */

#include <config.h>
#include "addressbook.h"
#include "e-card-cursor.h"

struct _ECardCursorPrivate {
	GNOME_Evolution_Addressbook_CardCursor corba_cursor;
};

/*
 * A pointer to our parent object class
 */
static GObjectClass *parent_class;

/*
 * Implemented GObject::dispose
 */
static void
e_card_cursor_dispose (GObject *object)
{
	ECardCursor *cursor = E_CARD_CURSOR (object);

	if (cursor->priv) {
		CORBA_Environment      ev;

		CORBA_exception_init (&ev);

		GNOME_Evolution_Addressbook_CardCursor_unref( cursor->priv->corba_cursor, &ev );

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

		g_free ( cursor->priv );
		cursor->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
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

		ret_val = GNOME_Evolution_Addressbook_CardCursor_count (cursor->priv->corba_cursor, &ev);
		
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

		vcard = GNOME_Evolution_Addressbook_CardCursor_getNth(cursor->priv->corba_cursor, n, &en);
		
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
	GObjectClass *object_class = (GObjectClass *) klass;

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	object_class->dispose = e_card_cursor_dispose;
}

static void
e_card_cursor_init (ECardCursor *cursor)
{
	cursor->priv = g_new(ECardCursorPrivate, 1);
	cursor->priv->corba_cursor = CORBA_OBJECT_NIL;
}

GType
e_card_cursor_get_type (void)
{
	static GType type = 0;

	if (!type){
		static const GTypeInfo info =  {
			sizeof (ECardCursorClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_card_cursor_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ECardCursor),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_card_cursor_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT, "ECardCursor", &info, 0);
	}

	return type;
}

/**
 * e_card_cursor_construct:
 * @cursor: an #ECardCursor object
 * @corba_cursor: an #GNOME_Evolution_Addressbook_CardCursor
 *
 * Wraps an #GNOME_Evolution_Addressbook_CardCursor object inside the #ECardCursor
 * @cursor object.
 *
 * Returns: a new #ECardCursor on success, or %NULL on failure.
 */
ECardCursor *
e_card_cursor_construct (ECardCursor          *cursor,
			 GNOME_Evolution_Addressbook_CardCursor  corba_cursor)
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
	
	GNOME_Evolution_Addressbook_CardCursor_ref(cursor->priv->corba_cursor, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("e_card_cursor_construct: Exception reffing "
			  "corba cursor.\n");
	}

	CORBA_exception_free (&ev);
	
	/*
	 * Success: return the GType we were given
	 */
	return cursor;
}

/**
 * e_card_cursor_new:
 * @cursor: the #GNOME_Evolution_Addressbook_CardCursor to be wrapped
 *
 * Creates a new #ECardCursor, which wraps an #GNOME_Evolution_Addressbook_CardCursor
 * object.
 *
 * Returns: a new #ECardCursor on success, or %NULL on failure.
 */
ECardCursor *
e_card_cursor_new (GNOME_Evolution_Addressbook_CardCursor corba_cursor)
{
	ECardCursor *cursor;

	cursor = g_object_new (E_TYPE_CARD_CURSOR, NULL);
	
	return e_card_cursor_construct (cursor,
					corba_cursor);
}
