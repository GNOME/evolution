
#ifndef _CAMEL_NNTP_NEWSRC_H_
#define _CAMEL_NNTP_NEWSRC_H_

#include <stdio.h>
#include "glib.h"

typedef struct CamelNNTPNewsrc CamelNNTPNewsrc;

int       camel_nntp_newsrc_get_highest_article_read    (CamelNNTPNewsrc *newsrc, char *group_name);
void      camel_nntp_newsrc_mark_article_read           (CamelNNTPNewsrc *newsrc,
							 char *group_name, int num);
void      camel_nntp_newsrc_mark_range_read             (CamelNNTPNewsrc *newsrc,
							 char *group_name, long low, long high);

gboolean  camel_nntp_newsrc_article_is_read             (CamelNNTPNewsrc *newsrc,
							 char *group_name, long num);

GPtrArray *camel_nntp_newsrc_get_subscribed_group_names (CamelNNTPNewsrc *newsrc);
GPtrArray *camel_nntp_newsrc_get_all_group_names        (CamelNNTPNewsrc *newsrc);
void       camel_nntp_newsrc_free_group_names           (CamelNNTPNewsrc *newsrc, GPtrArray *group_names);

void             camel_nntp_newsrc_write_to_file        (CamelNNTPNewsrc *newsrc, FILE *fp);
void             camel_nntp_newsrc_write                (CamelNNTPNewsrc *newsrc);
CamelNNTPNewsrc *camel_nntp_newsrc_read_for_server      (const char *server);

#endif /* _CAMEL_NNTP_NEWSRC_H_ */
