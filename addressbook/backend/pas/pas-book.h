/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * A wrapper object which exports the Evolution_Book CORBA interface
 * and which maintains a request queue.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __PAS_BOOK_H__
#define __PAS_BOOK_H__

#include <bonobo/bonobo-object.h>
#include <libgnome/gnome-defs.h>
#include <addressbook.h>

typedef struct _PASBook        PASBook;
typedef struct _PASBookPrivate PASBookPrivate;

#include <pas-backend.h>
#include <pas-card-cursor.h>

typedef enum {
	CreateCard,
	RemoveCard,
	ModifyCard,
	GetAllCards,
	CheckConnection
} PASOperation;

typedef struct {
	PASOperation  op;
	char         *id;
	char         *vcard;
} PASRequest;

struct _PASBook {
	BonoboObject     parent_object;
	PASBookPrivate *priv;
};

typedef struct {
	BonoboObjectClass parent_class;

	/* Signals */
	void (*requests_queued) (void);
} PASBookClass;

typedef char * (*PASBookGetVCardFn) (PASBook *book, const char *id);

PASBook                *pas_book_new                (PASBackend                        *backend,
						     Evolution_BookListener             listener,
						     PASBookGetVCardFn                  get_vcard);
PASBackend             *pas_book_get_backend        (PASBook                           *book);
Evolution_BookListener  pas_book_get_listener       (PASBook                           *book);
int                     pas_book_check_pending      (PASBook                           *book);
PASRequest             *pas_book_pop_request        (PASBook                           *book);

void                    pas_book_respond_open       (PASBook                           *book,
						     Evolution_BookListener_CallStatus  status);
void                    pas_book_respond_create     (PASBook                           *book,
						     Evolution_BookListener_CallStatus  status,
						     const char                        *id);
void                    pas_book_respond_remove     (PASBook                           *book,
						     Evolution_BookListener_CallStatus  status);
void                    pas_book_respond_modify     (PASBook                           *book,
						     Evolution_BookListener_CallStatus  status);
void                    pas_book_respond_get_cursor (PASBook                           *book,
						     Evolution_BookListener_CallStatus  status,
						     PASCardCursor                     *cursor);
void                    pas_book_report_connection  (PASBook                           *book,
						     gboolean                           connected);

void                    pas_book_notify_change      (PASBook                           *book,
						     const char                        *id);
void                    pas_book_notify_remove      (PASBook                           *book,
						     const char                        *id);
void                    pas_book_notify_add         (PASBook                           *book,
						     const char                        *id);

GtkType                 pas_book_get_type           (void);

#define PAS_BOOK_TYPE        (pas_book_get_type ())
#define PAS_BOOK(o)          (GTK_CHECK_CAST ((o), PAS_BOOK_TYPE, PASBook))
#define PAS_BOOK_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), PAS_BOOK_FACTORY_TYPE, PASBookClass))
#define PAS_IS_BOOK(o)       (GTK_CHECK_TYPE ((o), PAS_BOOK_TYPE))
#define PAS_IS_BOOK_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), PAS_BOOK_TYPE))

#endif /* ! __PAS_BOOK_H__ */
