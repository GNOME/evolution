/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * A client-side GObject which exposes the
 * Evolution:BookListener interface.
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 2000, Ximian, Inc.
 */

#ifndef __E_BOOK_TYPES_H__
#define __E_BOOK_TYPES_H__

#include <glib.h>

G_BEGIN_DECLS

#define E_BOOK_ERROR e_book_error_quark()

GQuark e_book_error_quark (void) G_GNUC_CONST;

typedef enum {
	E_BOOK_ERROR_OK,
	E_BOOK_ERROR_INVALID_ARG,
	E_BOOK_ERROR_BUSY,
	E_BOOK_ERROR_REPOSITORY_OFFLINE,
	E_BOOK_ERROR_NO_SUCH_BOOK,
	E_BOOK_ERROR_URI_NOT_LOADED,
	E_BOOK_ERROR_URI_ALREADY_LOADED,
	E_BOOK_ERROR_PERMISSION_DENIED,
	E_BOOK_ERROR_CONTACT_NOT_FOUND,
	E_BOOK_ERROR_CONTACT_ID_ALREADY_EXISTS,
	E_BOOK_ERROR_PROTOCOL_NOT_SUPPORTED,
	E_BOOK_ERROR_CANCELLED,
	E_BOOK_ERROR_COULD_NOT_CANCEL,
	E_BOOK_ERROR_AUTHENTICATION_FAILED,
	E_BOOK_ERROR_AUTHENTICATION_REQUIRED,
	E_BOOK_ERROR_TLS_NOT_AVAILABLE,
	E_BOOK_ERROR_CORBA_EXCEPTION,
	E_BOOK_ERROR_OTHER_ERROR
} EBookStatus;


typedef enum {
	E_BOOK_VIEW_STATUS_OK,
	E_BOOK_VIEW_STATUS_TIME_LIMIT_EXCEEDED,
	E_BOOK_VIEW_STATUS_SIZE_LIMIT_EXCEEDED,
	E_BOOK_VIEW_ERROR_INVALID_QUERY,
	E_BOOK_VIEW_ERROR_QUERY_REFUSED,
	E_BOOK_VIEW_ERROR_OTHER_ERROR
} EBookViewStatus;

typedef enum {
	E_BOOK_CHANGE_CARD_ADDED,
	E_BOOK_CHANGE_CARD_DELETED,
	E_BOOK_CHANGE_CARD_MODIFIED
} EBookChangeType;

typedef struct {
	EBookChangeType  change_type;
	char            *vcard; /* used in the ADDED/MODIFIED case */
	char            *id;    /* used in the DELETED case */
} EBookChange;

G_END_DECLS

#endif /* ! __E_BOOK_TYPES_H__ */
