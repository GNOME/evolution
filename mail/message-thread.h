#ifndef _MESSAGE_THREAD_H
#define _MESSAGE_THREAD_H

struct _container {
	struct _container *next,
		*parent,
		*child;
	CamelMessageInfo *message;
	char *root_subject;	/* cached root equivalent subject */
	int re;			/* re version of subject? */
};

struct _container *thread_messages(CamelMessageInfo **messages, int count);
void thread_messages_free(struct _container *);

/* for debug only */
int dump_tree(struct _container *c, int depth);

#endif /* !_MESSAGE_THREAD_H */

