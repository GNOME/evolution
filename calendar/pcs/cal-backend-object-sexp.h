/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * cal-backend-card-sexp.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __CAL_BACKEND_OBJECT_SEXP_H__
#define __CAL_BACKEND_OBJECT_SEXP_H__

#include <glib.h>
#include <glib-object.h>
#include <pcs/cal-backend.h>
#include <cal-util/cal-component.h>

G_BEGIN_DECLS

#define CAL_TYPE_BACKEND_OBJECT_SEXP        (cal_backend_object_sexp_get_type ())
#define CAL_BACKEND_OBJECT_SEXP(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), CAL_TYPE_BACKEND_OBJECT_SEXP, CalBackendObjectSExp))
#define CAL_BACKEND_OBJECT_SEXP_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), CAL_BACKEND_TYPE, CalBackendObjectSExpClass))
#define CAL_IS_BACKEND_OBJECT_SEXP(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), CAL_TYPE_BACKEND_OBJECT_SEXP))
#define CAL_IS_BACKEND_OBJECT_SEXP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), CAL_TYPE_BACKEND_OBJECT_SEXP))
#define CAL_BACKEND_OBJECT_SEXP_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), CAL_TYPE_BACKEND_OBJECT_SEXP, CALBackendObjectSExpClass))

typedef struct _CalBackendObjectSExpPrivate CalBackendObjectSExpPrivate;

struct _CalBackendObjectSExp {
	GObject parent_object;

	CalBackendObjectSExpPrivate *priv;
};

struct _CalBackendObjectSExpClass {
	GObjectClass parent_class;
};

GType                 cal_backend_object_sexp_get_type     (void);
CalBackendObjectSExp *cal_backend_object_sexp_new          (const char           *text);
const char *cal_backend_object_sexp_text (CalBackendObjectSExp *sexp);


gboolean              cal_backend_object_sexp_match_object (CalBackendObjectSExp *sexp,
							    const char           *object,
							    CalBackend           *backend);
gboolean              cal_backend_object_sexp_match_comp   (CalBackendObjectSExp *sexp,
							    CalComponent         *comp,
							    CalBackend           *backend);

G_END_DECLS

#endif /* __CAL_BACKEND_OBJECT_SEXP_H__ */
