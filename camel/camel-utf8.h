
#ifndef _CAMEL_UTF8_H
#define _CAMEL_UTF8_H

void camel_utf8_putc(unsigned char **ptr, guint32 c);
guint32 camel_utf8_getc(const unsigned char **ptr);

/* utility func for utf8 gstrings */
void g_string_append_u(GString *out, guint32 c);

/* convert utf7 to/from utf8, actually this is modified IMAP utf7 */
char *camel_utf7_utf8(const char *ptr);
char *camel_utf8_utf7(const char *ptr);


#endif /* ! _CAMEL_UTF8_H */
