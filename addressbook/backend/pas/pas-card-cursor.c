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
	PASCardCursorServant *servant;
	GNOME_Evolution_Addressbook_CardCursor corba_cursor;
	long     (*get_length) (PASCardCursor *cursor, gpointer data);
	char *   (*get_nth)    (PASCardCursor *cursor, long n, gpointer data);
	gpointer   data;
};

/*
 * A pointer to our parent object class
 */
static BonoboObjectClass *parent_class;

/*
 * The VEPV for the CardCursor object
 */
static POA_GNOME_Evolution_Addressbook_CardCursor__vepv pas_card_cursor_vepv;

/*
 * Implemented GObject::dispose
 */
static void
pas_card_cursor_dispose (GObject *object)
{
	PASCardCursor *cursor = PAS_CARD_CURSOR (object);

	if ( cursor->priv )
		g_free ( cursor->priv );

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

/*
 * CORBA Demo::Echo::echo method implementation
 */
static CORBA_long
impl_pas_card_cursor_get_length (PortableServer_Servant  servant,
			       CORBA_Environment      *ev)
{
	PASCardCursor *cursor = PAS_CARD_CURSOR (bonobo_object_from_servant (servant));
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
	PASCardCursor *cursor = PAS_CARD_CURSOR (bonobo_object_from_servant (servant));
	if ( cursor->priv->get_nth ) {
		char *vcard = cursor->priv->get_nth( cursor, n, cursor->priv->data );
		char *retval = CORBA_string_dup (vcard);
		g_free (vcard);
		return retval;
	} else
		return CORBA_string_dup ("");
}


static void
corba_class_init (PASCardCursorClass *klass)
{
	POA_GNOME_Evolution_Addressbook_CardCursor__vepv *vepv;
	POA_GNOME_Evolution_Addressbook_CardCursor__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	epv = &klass->epv;

	epv->count  = impl_pas_card_cursor_get_length;
	epv->getNth = impl_pas_card_cursor_get_nth;

	vepv = &pas_card_cursor_vepv;
	vepv->_base_epv                            = base_epv;
	vepv->GNOME_Evolution_Addressbook_CardCursor_epv = epv;
}

static void
pas_card_cursor_class_init (PASCardCursorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (BONOBO_TYPE_OBJECT);

	object_class->dispose = pas_card_cursor_dispose;

	corba_class_init (klass);
}

static void
pas_card_cursor_init (PASCardCursor *cursor)
{
	cursor->priv = g_new(PASCardCursorPrivate, 1);
	cursor->priv->corba_cursor = CORBA_OBJECT_NIL;
	cursor->priv->get_length = NULL;
	cursor->priv->get_nth = NULL;
	cursor->priv->data = NULL;
}

BONOBO_TYPE_FUNC_FULL (
		       PASCardCursor,
		       GNOME_Evolution_Addressbook_CardCursor,
		       BONOBO_TYPE_OBJECT,
		       pas_card_cursor);

void
pas_card_cursor_construct (PASCardCursor           *cursor,
			   GNOME_Evolution_Addressbook_CardCursor     corba_cursor,
			   PASCardCursorLengthFunc  get_length,
			   PASCardCursorNthFunc     get_nth,
			   gpointer data)
{
	PASCardCursorPrivate *priv;

	g_return_if_fail (cursor != NULL);
	g_return_if_fail (PAS_IS_CARD_CURSOR (cursor));
	g_return_if_fail (corba_cursor != CORBA_OBJECT_NIL);

	priv = cursor->priv;

	g_return_if_fail (priv->corba_cursor == CORBA_OBJECT_NIL);

	priv->corba_cursor = corba_cursor;
	priv->get_length   = get_length;
	priv->get_nth = get_nth;
	priv->data = data;
}

static PASCardCursorServant *
create_servant (PASCardCursor *cursor)
{
	PASCardCursorServant *servant;
	POA_GNOME_Evolution_Addressbook_CardCursor *corba_servant;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	servant = g_new0 (PASCardCursorServant, 1);
	corba_servant = (POA_GNOME_Evolution_Addressbook_CardCursor *) servant;

	corba_servant->vepv = &pas_card_cursor_vepv;
	POA_GNOME_Evolution_Addressbook_CardCursor__init ((PortableServer_Servant) corba_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	servant->object = cursor;

	CORBA_exception_free (&ev);

	return servant;
}

static GNOME_Evolution_Addressbook_CardCursor
activate_servant (PASCardCursor *cursor,
		  POA_GNOME_Evolution_Addressbook_CardCursor *servant)
{
	GNOME_Evolution_Addressbook_CardCursor corba_object;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	CORBA_free (PortableServer_POA_activate_object (bonobo_poa (), servant, &ev));

	corba_object = PortableServer_POA_servant_to_reference (bonobo_poa(), servant, &ev);

	if (ev._major == CORBA_NO_EXCEPTION && ! CORBA_Object_is_nil (corba_object, &ev)) {
		CORBA_exception_free (&ev);
		return corba_object;
	}

	CORBA_exception_free (&ev);

	return CORBA_OBJECT_NIL;
}

PASCardCursor *
pas_card_cursor_new (PASCardCursorLengthFunc  get_length,
		     PASCardCursorNthFunc     get_nth,
		     gpointer data)
{
	PASCardCursor *cursor;
	PASCardCursorPrivate *priv;
	GNOME_Evolution_Addressbook_CardCursor corba_cursor;

	cursor = g_object_new (PAS_TYPE_CARD_CURSOR, NULL);
	priv = cursor->priv;

	priv->servant = create_servant (cursor);
	corba_cursor = activate_servant (cursor, (POA_GNOME_Evolution_Addressbook_CardCursor*)priv->servant);

	pas_card_cursor_construct (cursor,
				   corba_cursor,
				   get_length,
				   get_nth,
				   data);

	return cursor;
}
