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

#include <bonobo/bonobo-object.h>
#include <pas/addressbook.h>
#include <pas/pas-book-view.h>
#include "e-util/e-list.h"

#include <pas/pas-backend.h>
#include <pas/pas-card-cursor.h>

#define PAS_TYPE_BOOK        (pas_book_get_type ())
#define PAS_BOOK(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_BOOK, PASBook))
#define PAS_BOOK_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), PAS_BOOK_FACTORY_TYPE, PASBookClass))
#define PAS_IS_BOOK(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_BOOK))
#define PAS_IS_BOOK_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_BOOK))
#define PAS_BOOK_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), PAS_TYPE_BOOK, PASBookClass))

typedef struct _PASBook        PASBook;
typedef struct _PASBookPrivate PASBookPrivate;

typedef enum {
	CreateCard,
	RemoveCards,
	ModifyCard,
	GetVCard,
	GetCursor,
	GetBookView,
	GetCompletionView,
	GetChanges,
	CheckConnection,
	AuthenticateUser,
	GetSupportedFields,
	GetSupportedAuthMethods
} PASOperation;

typedef struct {
	PASOperation op;
	char *id;
	char *vcard;
} PASCreateCardRequest;

typedef struct {
	PASOperation op;
	GList *ids;
} PASRemoveCardsRequest;

typedef struct {
	PASOperation op;
	char *vcard;
} PASModifyCardRequest;

typedef struct {
	PASOperation op;
	char *id;
} PASGetVCardRequest;

typedef struct {
	PASOperation op;
	char *search;
} PASGetCursorRequest;

typedef struct {
	PASOperation op;
	char *search;
	GNOME_Evolution_Addressbook_BookViewListener listener;
} PASGetBookViewRequest;

typedef struct {
	PASOperation op;
	char *search;
	GNOME_Evolution_Addressbook_BookViewListener listener;
} PASGetCompletionViewRequest;

typedef struct {
	PASOperation op;
	char *change_id;
	GNOME_Evolution_Addressbook_BookViewListener listener;
} PASGetChangesRequest;

typedef struct {
	PASOperation op;
} PASCheckConnectionRequest;

typedef struct {
	PASOperation op;
	char *user;
        char *passwd;
	char *auth_method;
} PASAuthenticateUserRequest;

typedef struct {
	PASOperation op;
} PASGetSupportedFieldsRequest;

typedef struct {
	PASOperation op;
} PASGetSupportedAuthMethodsRequest;

typedef union {
	PASOperation                      op;

	PASCreateCardRequest              create;
	PASRemoveCardsRequest             remove;
	PASModifyCardRequest              modify;
	PASGetVCardRequest                get_vcard;
	PASGetCursorRequest               get_cursor;
	PASGetBookViewRequest             get_book_view;
	PASGetCompletionViewRequest       get_completion_view;
	PASGetChangesRequest              get_changes;
	PASCheckConnectionRequest         check_connection;
	PASAuthenticateUserRequest        auth_user;
	PASGetSupportedFieldsRequest      get_supported_fields;
	PASGetSupportedAuthMethodsRequest get_supported_auth_methods;
} PASRequest;

struct _PASBook {
	BonoboObject     parent_object;
	PASBookPrivate *priv;
};

typedef struct {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Addressbook_Book__epv epv;

	/* Signals */
	void (*requests_queued) (void);
} PASBookClass;


typedef gboolean (*PASBookCanWriteFn)     (PASBook *book);
typedef gboolean (*PASBookCanWriteCardFn) (PASBook *book, const char *id);

PASBook                *pas_book_new                    (PASBackend                        *backend,
							 GNOME_Evolution_Addressbook_BookListener             listener);
PASBackend             *pas_book_get_backend            (PASBook                           *book);
GNOME_Evolution_Addressbook_BookListener  pas_book_get_listener           (PASBook                           *book);
int                     pas_book_check_pending          (PASBook                           *book);
PASRequest             *pas_book_pop_request            (PASBook                           *book);
void                    pas_book_free_request           (PASRequest                        *request);
void                    pas_book_respond_open           (PASBook                           *book,
							 GNOME_Evolution_Addressbook_BookListener_CallStatus  status);
void                    pas_book_respond_create         (PASBook                           *book,
							 GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
							 const char                        *id);
void                    pas_book_respond_remove         (PASBook                           *book,
							 GNOME_Evolution_Addressbook_BookListener_CallStatus  status);
void                    pas_book_respond_modify         (PASBook                           *book,
							 GNOME_Evolution_Addressbook_BookListener_CallStatus  status);
void                    pas_book_respond_authenticate_user (PASBook                           *book,
							    GNOME_Evolution_Addressbook_BookListener_CallStatus  status);
void                    pas_book_respond_get_supported_fields (PASBook *book,
							       GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
							       EList   *fields);
void                    pas_book_respond_get_supported_auth_methods (PASBook *book,
								     GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
								     EList   *fields);

void                    pas_book_respond_get_cursor     (PASBook                           *book,
							 GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
							 PASCardCursor                     *cursor);
void                    pas_book_respond_get_book_view  (PASBook                           *book,
							 GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
							 PASBookView                       *book_view);
void                    pas_book_respond_get_completion_view (PASBook                           *book,
						      GNOME_Evolution_Addressbook_BookListener_CallStatus status,
						      PASBookView                       *completion_view);
void                    pas_book_respond_get_vcard      (PASBook                           *book,
							 GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
							 char                              *vcard);
void                    pas_book_respond_get_changes    (PASBook                           *book,
							 GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
							 PASBookView                       *book_view);
void                    pas_book_report_connection      (PASBook                           *book,
							 gboolean                           connected);

void                    pas_book_report_writable        (PASBook                           *book,
							 gboolean                           writable);

GType                   pas_book_get_type               (void);

#endif /* ! __PAS_BOOK_H__ */
