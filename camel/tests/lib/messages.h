
#include <camel/camel-mime-message.h>

/* how many ways to set the content contents */
#define SET_CONTENT_WAYS (5)

/* messages.c */
CamelMimeMessage *test_message_create_simple(void);
void test_message_set_content_simple(CamelMimePart *part, int how, const char *type, const char *text, int len);
int test_message_write_file(CamelMimeMessage *msg, const char *name);
CamelMimeMessage *test_message_read_file(const char *name);
int test_message_compare_content(CamelDataWrapper *dw, const char *text, int len);
int test_message_compare (CamelMimeMessage *msg);

void test_message_dump_structure(CamelMimeMessage *m);

int test_message_compare_header(CamelMimeMessage *m1, CamelMimeMessage *m2);
int test_message_compare_messages(CamelMimeMessage *m1, CamelMimeMessage *m2);
