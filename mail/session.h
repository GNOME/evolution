#ifndef EVOLUTION_MAIL_SESSION_H
#define EVOLUTION_MAIL_SESSION_H

#include <camel/camel-store.h>
#include <camel/camel-session.h>
typedef struct {
	CamelSession *session;
	CamelStore   *store;
} SessionStore;

SessionStore *session_store_new     (const char *uri);
void          session_store_destroy (SessionStore *ss);
void          session_init          (void);

extern SessionStore *default_session;

#endif /* EVOLUTION_MAIL_SESSION_H */
