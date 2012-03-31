/*
 * e-mail-request.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef E_MAIL_REQUEST_H
#define E_MAIL_REQUEST_H

#define LIBSOUP_USE_UNSTABLE_REQUEST_API

#include <libsoup/soup.h>
#include <libsoup/soup-request.h>

G_BEGIN_DECLS

#define E_TYPE_MAIL_REQUEST            (e_mail_request_get_type ())
#define E_MAIL_REQUEST(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), E_TYPE_MAIL_REQUEST, EMailRequest))
#define E_MAIL_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MAIL_REQUEST, EMailRequestClass))
#define E_IS_MAIL_REQUEST(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), E_TYPE_MAIL_REQUEST))
#define E_IS_MAIL_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_MAIL_REQUEST))
#define E_MAIL_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_MAIL_REQUEST, EMailRequestClass))

typedef struct _EMailRequest EMailRequest;
typedef struct _EMailRequestClass EMailRequestClass;
typedef struct _EMailRequestPrivate EMailRequestPrivate;

struct _EMailRequest {
	SoupRequest parent;

	EMailRequestPrivate *priv;
};

struct _EMailRequestClass {
	SoupRequestClass parent;
};

GType e_mail_request_get_type (void);

G_END_DECLS

#endif /* E_MAIL_REQUEST_H */
