/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * pas-card-cursor.c: Implements card cursors.
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com.
 */

#include <config.h>
#include <bonobo/bonobo-main.h>
#include "addressbook.h"
#include "pas-card-cursor.h"

struct _PASCardCursorPrivate {
	long     (*get_length) (PASCardCursor *cursor, gpointer data);
	char *   (*get_nth)    (PASCardCursor *cursor, long n, gpointer data);
	gpointer   data;
};

/*
 * A pointer to our parent object class
 */
static BonoboObjectClass *parent_class;

/*
 * Implemented GObject::dispose
 */
static void
pas_card_cursor_dispose (GObject *object)
{
	PASCardCursor *cursor = PAS_CARD_CURSOR (object);

	if ( cursor->priv ) {
		g_free ( cursor->priv );
		cursor->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}

/*
 * CORBA Demo::Echo::echo method implementation
 */
static CORBA_long
impl_pas_card_cursor_get_length (PortableServer_Servant  servant,
			       CORBA_Environment      *ev)
{
	PASCardCursor *cursor = PAS_CARD_CURSOR (bonobo_object (servant));
	if ( cursor->priv->get_length )
		return cursor->priv->get_length( cursor, cursor->priv->data );
	else
		return 0;
}

/*
 * CORBA Demo::Echo::echo method implementation
 */
static char *
impl_pas_card_cursor_get_nth (PortableServer_Servant  servant,
			    const CORBA_long        n,
			    CORBA_Environment      *ev)
{
	PASCardCursor *cursor = PAS_CARD_CURSOR (bonobo_object (servant));
	if ( cursor->priv->get_nth ) {
		char *vcard = cursor->priv->get_nth( cursor, n, cursor->priv->data );
		char *retval = CORBA_string_dup (vcard);
		g_free (vcard);
		return retval;
	} else
		return CORBA_string_dup ("");
}

static void
pas_card_cursor_class_init (PASCardCursorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	POA_GNOME_Evolution_Addressbook_CardCursor__epv *epv;

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = pas_card_cursor_dispose;


	epv = &klass->epv;

	epv->count  = impl_pas_card_cursor_get_length;
	epv->getNth = impl_pas_card_cursor_get_nth;
}

static void
pas_card_cursor_init (PASCardCursor *cursor)
{
	cursor->priv = g_new0(PASCardCursorPrivate, 1);
	cursor->priv->get_length = NULL;
	cursor->priv->get_nth = NULL;
	cursor->priv->data = NULL;
}

static void
pas_card_cursor_construct (PASCardCursor           *cursor,
			   PASCardCursorLengthFunc  get_length,
			   PASCardCursorNthFunc     get_nth,
			   gpointer data)
{
	PASCardCursorPrivate *priv;

	g_return_if_fail (cursor != NULL);
	g_return_if_fail (PAS_IS_CARD_CURSOR (cursor));

	priv->get_length   = get_length;
	priv->get_nth = get_nth;
	priv->data = data;
}

PASCardCursor *
pas_card_cursor_new (PASCardCursorLengthFunc  get_length,
		     PASCardCursorNthFunc     get_nth,
		     gpointer data)
{
	PASCardCursor *cursor;

	cursor = g_object_new (PAS_TYPE_CARD_CURSOR, NULL);

	pas_card_cursor_construct (cursor,
				   get_length,
				   get_nth,
				   data);

	return cursor;
}

BONOBO_TYPE_FUNC_FULL (
		       PASCardCursor,
		       GNOME_Evolution_Addressbook_CardCursor,
		       BONOBO_TYPE_OBJECT,
		       pas_card_cursor);
