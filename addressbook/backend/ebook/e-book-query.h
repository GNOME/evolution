
#ifndef __E_BOOK_QUERY_H__
#define __E_BOOK_QUERY_H__

#include <ebook/e-contact.h>

G_BEGIN_DECLS

typedef struct EBookQuery EBookQuery;

typedef enum {
  E_BOOK_QUERY_IS,
  E_BOOK_QUERY_CONTAINS,
  E_BOOK_QUERY_BEGINS_WITH,
  E_BOOK_QUERY_ENDS_WITH,

#if notyet
  E_BOOK_QUERY_LT,
  E_BOOK_QUERY_LE,
  E_BOOK_QUERY_GT,
  E_BOOK_QUERY_GE,
  E_BOOK_QUERY_EQ,
#endif
} EBookQueryTest;

EBookQuery* e_book_query_from_string  (const char *sexp);
char*       e_book_query_to_string    (EBookQuery *q);

void        e_book_query_ref          (EBookQuery *q);
void        e_book_query_unref        (EBookQuery *q);

EBookQuery* e_book_query_and          (int nqs, EBookQuery **qs, gboolean unref);
EBookQuery* e_book_query_andv         (EBookQuery *q, ...);
EBookQuery* e_book_query_or           (int nqs, EBookQuery **qs, gboolean unref);
EBookQuery* e_book_query_orv          (EBookQuery *q, ...);

EBookQuery* e_book_query_not          (EBookQuery *qs, gboolean unref);

EBookQuery* e_book_query_field_exists (EContactField   field);
EBookQuery* e_book_query_field_test   (EContactField   field,
				       EBookQueryTest     test,
				       const char        *value);

/* a special any field contains query */
EBookQuery* e_book_query_any_field_contains (const char  *value);

G_END_DECLS

#endif /* __E_BOOK_QUERY_H__ */
