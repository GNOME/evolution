/*
 * e-mail-request.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_MAIL_REQUEST_H
#define E_MAIL_REQUEST_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_REQUEST \
	(e_mail_request_get_type ())
#define E_MAIL_REQUEST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_REQUEST, EMailRequest))
#define E_MAIL_REQUEST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_REQUEST, EMailRequestClass))
#define E_IS_MAIL_REQUEST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_REQUEST))
#define E_IS_MAIL_REQUEST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_REQUEST))
#define E_MAIL_REQUEST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_REQUEST, EMailRequestClass))

G_BEGIN_DECLS

typedef struct _EMailRequest EMailRequest;
typedef struct _EMailRequestClass EMailRequestClass;
typedef struct _EMailRequestPrivate EMailRequestPrivate;

struct _EMailRequest {
	GObject parent;
	EMailRequestPrivate *priv;
};

struct _EMailRequestClass {
	GObjectClass parent;
};

GType		e_mail_request_get_type		(void) G_GNUC_CONST;
EContentRequest *
		e_mail_request_new		(void);
gint		e_mail_request_get_scale_factor	(EMailRequest *mail_request);
void		e_mail_request_set_scale_factor	(EMailRequest *mail_request,
						 gint scale_factor);

G_END_DECLS

#endif /* E_MAIL_REQUEST_H */
