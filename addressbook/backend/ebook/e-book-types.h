/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * A client-side GtkObject which exposes the
 * Evolution:BookListener interface.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __E_BOOK_TYPES_H__
#define __E_BOOK_TYPES_H__

#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

typedef enum {
	E_BOOK_STATUS_SUCCESS,
	E_BOOK_STATUS_UNKNOWN,
	E_BOOK_STATUS_REPOSITORY_OFFLINE,
	E_BOOK_STATUS_PERMISSION_DENIED,
	E_BOOK_STATUS_CARD_NOT_FOUND,
	E_BOOK_STATUS_PROTOCOL_NOT_SUPPORTED,
	E_BOOK_STATUS_OTHER_ERROR
} EBookStatus;

END_GNOME_DECLS

#endif /* ! __E_BOOK_TYPES_H__ */
