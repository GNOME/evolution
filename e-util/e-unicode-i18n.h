#ifndef _E_UNICODE_I18N_H
#define _E_UNICODE_I18N_H

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

const char *e_utf8_gettext (const char *string);
const char *e_utf8_dgettext (const char *domain, const char *string);

#undef U_
#ifdef GNOME_EXPLICIT_TRANSLATION_DOMAIN
#	define U_(domain,string) e_utf8_dgettext (domain, string)
#else 
#	define U_(string) e_utf8_gettext (string)
#endif

#endif /* _E_UNICODE_I18N_H */
