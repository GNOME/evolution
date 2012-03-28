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
