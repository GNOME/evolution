/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * A wrapper object which exports the GNOME_Evolution_Addressbook_Book CORBA interface
 * and which maintains a request queue.
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 2000, Ximian, Inc.
 */

#ifndef __PAS_BOOK_H__
#define __PAS_BOOK_H__

#include <pas/addressbook.h>
#include <bonobo/bonobo-object.h>
#include "e-util/e-list.h"

#include <pas/pas-types.h>

#define PAS_TYPE_BOOK        (pas_book_get_type ())
#define PAS_BOOK(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_BOOK, PASBook))
#define PAS_BOOK_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), PAS_BOOK_FACTORY_TYPE, PASBookClass))
#define PAS_IS_BOOK(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_BOOK))
#define PAS_IS_BOOK_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_BOOK))
#define PAS_BOOK_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), PAS_TYPE_BOOK, PASBookClass))

typedef struct _PASBookPrivate PASBookPrivate;

struct _PASBook {
	BonoboObject       parent_object;
	PASBookPrivate    *priv;
};

struct _PASBookClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Addressbook_Book__epv epv;

	/* Padding for future expansion */
	void (*_pas_reserved0) (void);
	void (*_pas_reserved1) (void);
	void (*_pas_reserved2) (void);
	void (*_pas_reserved3) (void);
	void (*_pas_reserved4) (void);
};


PASBook                *pas_book_new                    (PASBackend                               *backend,
							 const char                               *uri,
							 GNOME_Evolution_Addressbook_BookListener  listener);
GNOME_Evolution_Addressbook_BookListener pas_book_get_listener (PASBook                         *book);
PASBackend             *pas_book_get_backend            (PASBook                                *book);
const char             *pas_book_get_uri                (PASBook                                *book);

void                    pas_book_respond_open           (PASBook                                *book,
							 GNOME_Evolution_Addressbook_CallStatus  status);
void                    pas_book_respond_remove         (PASBook                                *book,
							 GNOME_Evolution_Addressbook_CallStatus  status);
void                    pas_book_respond_create         (PASBook                                *book,
							 GNOME_Evolution_Addressbook_CallStatus  status,
							 EContact                               *contact);
void                    pas_book_respond_remove_contacts (PASBook                                *book,
							  GNOME_Evolution_Addressbook_CallStatus  status,
							  GList                                  *ids);
void                    pas_book_respond_modify         (PASBook                                *book,
							 GNOME_Evolution_Addressbook_CallStatus  status,
							 EContact                               *contact);
void                    pas_book_respond_authenticate_user (PASBook                                *book,
							    GNOME_Evolution_Addressbook_CallStatus  status);
void                    pas_book_respond_get_supported_fields (PASBook                                *book,
							       GNOME_Evolution_Addressbook_CallStatus  status,
							       GList                                  *fields);
void                    pas_book_respond_get_supported_auth_methods (PASBook                                *book,
								     GNOME_Evolution_Addressbook_CallStatus  status,
								     GList                                  *fields);

void                    pas_book_respond_get_book_view  (PASBook                           *book,
							 GNOME_Evolution_Addressbook_CallStatus  status,
							 PASBookView                       *book_view);
void                    pas_book_respond_get_contact    (PASBook                           *book,
							 GNOME_Evolution_Addressbook_CallStatus  status,
							 char                              *vcard);
void                    pas_book_respond_get_contact_list (PASBook                           *book,
							   GNOME_Evolution_Addressbook_CallStatus  status,
							   GList *cards);
void                    pas_book_respond_get_changes    (PASBook                                *book,
							 GNOME_Evolution_Addressbook_CallStatus  status,
							 GList                                  *changes);

void                    pas_book_report_writable        (PASBook                           *book,
							 gboolean                           writable);

GType                   pas_book_get_type               (void);

#endif /* ! __PAS_BOOK_H__ */
