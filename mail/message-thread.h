#ifndef _MESSAGE_THREAD_H
#define _MESSAGE_THREAD_H

#include <gnome.h>
#include "message-list.h"

struct _container {
	struct _container *next,
		*parent,
		*child;
	const CamelMessageInfo *message;
	char *root_subject;	/* cached root equivalent subject */
	int re;			/* re version of subject? */
	int order;
};

struct _thread_messages {
	struct _container *tree;
};

struct _thread_messages *thread_messages(CamelFolder *folder, GPtrArray *uids);
void thread_messages_add(struct _thread_messages *thread, CamelFolder *folder, GPtrArray *uids);
void thread_messages_remove(struct _thread_messages *thread, CamelFolder *folder, GPtrArray *uids);
void thread_messages_free(struct _thread_messages *c);

void mail_do_thread_messages (MessageList *ml, GPtrArray *uids, 
			      gboolean use_camel_uidfree,
			      void (*build) (MessageList *,
					     struct _container *));

#endif /* !_MESSAGE_THREAD_H */

