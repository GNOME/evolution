#ifndef _MESSAGE_THREAD_H
#define _MESSAGE_THREAD_H

struct _container {
	struct _container *next,
		*parent,
		*child;
	const CamelMessageInfo *message;
	char *root_subject;	/* cached root equivalent subject */
	int re;			/* re version of subject? */
	int order;		/* the order of this message in the folder */
};

struct _container *thread_messages(CamelFolder *folder, GPtrArray *uids);
void thread_messages_free(struct _container *);

/* for debug only */
int dump_tree(struct _container *c, int depth);

#endif /* !_MESSAGE_THREAD_H */

