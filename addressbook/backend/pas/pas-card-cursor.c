/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * pas-card-cursor.c: Implements card cursors.
 *
 * Author:
 *   Christopher James Lahey <clahey@helixcode.com.
 */

#include <config.h>
#include <bonobo.h>
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
 * The VEPV for the CardCursor object
 */
static POA_Evolution_CardCursor__vepv cursor_vepv;

/*
 * Implemented GtkObject::destroy
 */
static void
pas_card_cursor_destroy (GtkObject *object)
{
	PASCardCursor *cursor = PAS_CARD_CURSOR (object);

	if ( cursor->priv )
		g_free ( cursor->priv );

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
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

/*
 * If you want users to derive classes from your implementation
 * you need to support this method.
 */
POA_Evolution_CardCursor__epv *
pas_card_cursor_get_epv (void)
{
	POA_Evolution_CardCursor__epv *epv;

	epv = g_new0 (POA_Evolution_CardCursor__epv, 1);

	/*
	 * This is the method invoked by CORBA
	 */
	epv->get_length = impl_pas_card_cursor_get_length;
	epv->get_nth    = impl_pas_card_cursor_get_nth;

	return epv;
}

static void
init_pas_card_cursor_corba_class (void)
{
	cursor_vepv.Bonobo_Unknown_epv       = bonobo_object_get_epv ();
	cursor_vepv.Evolution_CardCursor_epv = pas_card_cursor_get_epv ();
}

static void
pas_card_cursor_class_init (PASCardCursorClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	parent_class = gtk_type_class (bonobo_object_get_type ());

	object_class->destroy = pas_card_cursor_destroy;

	init_pas_card_cursor_corba_class ();
}

static void
pas_card_cursor_init (PASCardCursor *cursor)
{
	cursor->priv = g_new(PASCardCursorPrivate, 1);
	cursor->priv->get_length = NULL;
	cursor->priv->get_nth = NULL;
	cursor->priv->data = NULL;
}

GtkType
pas_card_cursor_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"PASCardCursor",
			sizeof (PASCardCursor),
			sizeof (PASCardCursorClass),
			(GtkClassInitFunc) pas_card_cursor_class_init,
			(GtkObjectInitFunc) pas_card_cursor_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_object_get_type (), &info);
	}

	return type;
}

PASCardCursor *
pas_card_cursor_construct (PASCardCursor           *cursor,
			   Evolution_CardCursor     corba_cursor,
			   PASCardCursorLengthFunc  get_length,
			   PASCardCursorNthFunc     get_nth,
			   gpointer data)
{
	g_return_val_if_fail (cursor != NULL, NULL);
	g_return_val_if_fail (PAS_IS_CARD_CURSOR (cursor), NULL);
	g_return_val_if_fail (corba_cursor != CORBA_OBJECT_NIL, NULL);

	/*
	 * Call parent constructor
	 */
	if (!bonobo_object_construct (BONOBO_OBJECT (cursor), (CORBA_Object) corba_cursor))
		return NULL;

	/*
	 * Initialize cursor
	 */
	cursor->priv->get_length = get_length;
	cursor->priv->get_nth = get_nth;
	cursor->priv->data = data;
	
	/*
	 * Success: return the GtkType we were given
	 */
	return cursor;
}

/*
 * This routine creates the ORBit CORBA server and initializes the
 * CORBA side of things
 */
static Evolution_CardCursor
create_cursor (BonoboObject *cursor)
{
	POA_Evolution_CardCursor *servant;
	CORBA_Environment ev;

	servant = (POA_Evolution_CardCursor *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &cursor_vepv;

	CORBA_exception_init (&ev);
	POA_Evolution_CardCursor__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);

	/*
	 * Activates the CORBA object.
	 */
	return (Evolution_CardCursor) bonobo_object_activate_servant (cursor, servant);
}

PASCardCursor *
pas_card_cursor_new (PASCardCursorLengthFunc  get_length,
		     PASCardCursorNthFunc     get_nth,
		     gpointer data)
{
	PASCardCursor *cursor;
	Evolution_CardCursor corba_cursor;

	cursor = gtk_type_new (pas_card_cursor_get_type ());
	corba_cursor = create_cursor (BONOBO_OBJECT (cursor));

	if (corba_cursor == CORBA_OBJECT_NIL){
		gtk_object_destroy (GTK_OBJECT (cursor));
		return NULL;
	}
	
	return pas_card_cursor_construct (cursor,
					  corba_cursor,
					  get_length,
					  get_nth,
					  data);
}
