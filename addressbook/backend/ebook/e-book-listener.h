/*
 * A client-side GtkObject which exposes the
 * Evolution:BookListener interface.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __E_BOOK_LISTENER_H__
#define __E_BOOK_LISTENER_H__

#include <libgnome/gnome-defs.h>

#include <bonobo/gnome-object.h>
#include <e-book.h>

#include <addressbook.h>

BEGIN_GNOME_DECLS

typedef struct _EBookListener EBookListener;

typedef void (*EBookListenerRespondOpenBookCallback)  (EBook          *book,
						       EBookStatus     status,
						       Evolution_Book  corba_book,
						       gpointer        closure);
						     
typedef void (*EBookListenerConnectionStatusCallback) (EBook          *book,
	       					       gboolean        connected,
	       					       gpointer        closure);
	       	 
struct _EBookListener {
	GnomeObject                            parent;

	EBook                                 *book;

	gpointer                               closure;
					       
	EBookCallback                          create_response;
	EBookCallback                          remove_response;
	EBookCallback                          modify_response;
					       
	EBookOpenProgressCallback              open_progress;
	EBookListenerRespondOpenBookCallback   open_response;
	EBookListenerConnectionStatusCallback  connect_status;
};

typedef struct {
	GnomeObjectClass parent;
} EBookListenerClass;

EBookListener *e_book_listener_new       (EBook *book);
GtkType        e_book_listener_get_type  (void);

#define E_BOOK_LISTENER_TYPE        (e_book_listener_get_type ())
#define E_BOOK_LISTENER(o)          (GTK_CHECK_CAST ((o), E_BOOK_LISTENER_TYPE, EBookListener))
#define E_BOOK_LISTENER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_BOOK_LISTENER_TYPE, EBookListenerClass))
#define E_IS_BOOK_LISTENER(o)       (GTK_CHECK_TYPE ((o), E_BOOK_LISTENER_TYPE))
#define E_IS_BOOK_LISTENER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_BOOK_LISTENER_TYPE))

END_GNOME_DECLS

#endif /* ! __E_BOOK_LISTENER_H__ */
