/*
 * languages.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include "languages.h"

#include <glib/gi18n-lib.h>

static const gchar **mime_types = NULL;
G_LOCK_DEFINE_STATIC (mime_types);

static Language languages[] = {

	{ "txt", N_("_Plain text"),
	  (const gchar *[]) { (gchar[]) { "text" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/plain" },
			      (gchar[]) { "text/*" }, NULL }
	},

	{ "assembler", N_("_Assembler"),
	  (const gchar *[]) { (gchar[]) { "asm" } , NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-asm" }, NULL }
	},

	{ "sh", N_("_Bash"),
	  (const gchar *[]) { (gchar[]) { "bash" }, (gchar[]) { "sh" },
			      (gchar[]) { "ebuild" },  (gchar[])  {"eclass" },
			      NULL },
	  (const gchar *[]) { (gchar[]) { "application/x-bsh" },
			      (gchar[]) { "application/x-sh" },
			      (gchar[]) { "application/x-shar" },
			      (gchar[]) { "application/x-shellscript" },
			      (gchar[]) { "text/x-script.sh" }, NULL }
	},

	{ "c", N_("_C/C++"),
	  (const gchar *[]) { (gchar[]) { "c" }, (gchar[]) { "c++" },
			      (gchar[]) { "cc" }, (gchar[]) { "cpp" },
			      (gchar[]) { "cu" }, (gchar[]) { "cxx" },
			      (gchar[]) { "h" }, (gchar[]) { "hh" },
			      (gchar[]) { "hpp" }, (gchar[]) { "hxx" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-c" }, NULL }
	},

	{ "csharp", N_("_C#"),
	  (const gchar *[]) { (gchar[]) { "cs" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-csharp" }, NULL }
	},

	{ "css", N_("_Cascade Style Sheet"),
	  (const gchar *[]) { (gchar[]) { "css" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/css" }, NULL }
	},

	{ "html", N_("_HTML"),
	  (const gchar *[]) { (gchar[]) { "html" }, (gchar[]) { "html" },
			      (gchar[]) { "xhtml" }, (gchar[]) { "dhtml" }, NULL },
	  (const gchar *[]) { NULL } /* Don't register text-highlight as formatter
					or parser of text / html as it would break
					all text / html emails. */
	},

	{ "java", N_("_Java"),
	  (const gchar *[]) { (gchar[]) { "java" }, (gchar[]) { "groovy" },
			      (gchar[]) { "grv" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/java-source" }, NULL }
	},

	{ "js", N_("_JavaScript"),
	  (const gchar *[]) { (gchar[]) { "js" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/javascript" },
			      (gchar[]) { "application/x-javascript" }, NULL }
	},

	{ "diff", N_("_Patch/diff"),
	  (const gchar *[]) { (gchar[]) { "diff" }, (gchar[]) { "patch" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-diff" },
			      (gchar[]) { "text/x-patch" }, NULL }
	},

	{ "markdown", N_("_Markdown"),
	  (const gchar *[]) { (gchar[]) { "md" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "perl", N_("_Perl"),
	  (const gchar *[]) { (gchar[]) { "perl" }, (gchar[]) { "cgi"},
			      (gchar[]) { "perl" }, (gchar[]) { "pl" },
			      (gchar[]) { "plex" }, (gchar[]) { "plx" },
			      (gchar[]) { "pm" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-script.perl" },
			      (gchar[]) { "text/x-script.perl-module" },
			      (gchar[]) { "application/x-pixclscript" },
			      (gchar[]) { "application/x-xpixmap" }, NULL }
	},

	{ "php", N_("_PHP"),
	  (const gchar *[]) { (gchar[]) { "php" }, (gchar[]) { "php3" },
			      (gchar[]) { "php4" }, (gchar[]) { "php5" },
			      (gchar[]) { "php6" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/php" },
			      (gchar[]) { "text/x-php" },
			      (gchar[]) { "application/php" },
			      (gchar[]) { "application/x-php" },
			      (gchar[]) { "application/x-httpd-php" },
			      (gchar[]) { "application/x-httpd-php-source" },
			      NULL }
	},

	{ "python", N_("_Python"),
	  (const gchar *[]) { (gchar[]) { "py" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-script.python" }, NULL }
	},

	{ "ruby", N_("_Ruby"),
	  (const gchar *[]) { (gchar[]) { "ruby" }, (gchar[]) { "pp" },
			      (gchar[]) { "rb" }, (gchar[]) { "rjs" },
			      (gchar[]) { "ruby" }, NULL },
	  (const gchar *[]) { (gchar[]) { "application/x-ruby" }, NULL }
	},

	{ "tcl", N_("_Tcl/Tk"),
	  (const gchar *[]) { (gchar[]) { "tcl" }, (gchar[]) { "ictl" },
			      (gchar[]) { "wish" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-tcl" }, NULL }
	},

	{ "tex", N_("_TeX/LaTeX"),
	  (const gchar *[]) { (gchar[]) { "tex" }, (gchar[]) { "csl" },
			      (gchar[]) { "sty" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-tex" }, NULL }
	},

	{ "vala", N_("_Vala"),
	  (const gchar *[]) { (gchar[]) { "vala" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-vala" }, NULL }
	},

	{ "vb", N_("_Visual Basic"),
	  (const gchar *[]) { (gchar[]) { "vb" }, (gchar[]) { "bas" },
			      (gchar[]) { "basic" }, (gchar[]) { "bi" },
			      (gchar[]) { "vbs" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "xml", N_("_XML"),
	  (const gchar *[]) { (gchar[]) { "xml" }, (gchar[]) { "dtd" },
			      (gchar[]) { "ecf" }, (gchar[]) { "ent" },
			      (gchar[]) { "hdr" }, (gchar[]) { "hub" },
			      (gchar[]) { "jnlp" }, (gchar[]) { "nrm" },
			      (gchar[]) { "resx" }, (gchar[]) { "sgm" },
			      (gchar[]) { "sgml" }, (gchar[]) { "svg" },
			      (gchar[]) { "tld" }, (gchar[]) { "vxml" },
			      (gchar[]) { "wml" }, (gchar[]) { "xsd" },
			      (gchar[]) { "xsl" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/xml" },
			      (gchar[]) { "application/xml" },
			      (gchar[]) { "application/x-xml" }, NULL }
	}
};

static struct Language other_languages[] = {

	{ "actionscript", N_("_ActionScript"),
	  (const gchar *[]) { (gchar[]) { "as" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "ada", N_("_ADA95"),
	  (const gchar *[]) { (gchar[]) { "a" }, (gchar[]) { "adb" },
			      (gchar[]) { "ads" }, (gchar[]) { "gnad" },
			      NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-adasrc" }, NULL }
	},

	{ "algol", N_("_ALGOL 68"),
	  (const gchar *[]) { (gchar[]) { "alg" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "awk", N_("(_G)AWK"),
	  (const gchar *[]) { (gchar[]) { "awk" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-awk" }, NULL }
	},

	{ "cobol", N_("_COBOL"),
	  (const gchar *[]) { (gchar[]) { "cbl" }, (gchar[]) { "cob" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-cobol" }, NULL }
	},

	{ "bat", N_("_DOS Batch"),
	  (const gchar *[]) { (gchar[]) { "bat" }, (gchar[]) { "cmd" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "d", N_("_D"),
	  (const gchar *[]) { (gchar[]) { "d" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "erlang", N_("_Erlang"),
	  (const gchar *[]) { (gchar[]) { "erl" }, (gchar[]) { "hrl" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-erlang" }, NULL }
	},

	{ "fortran77", N_("_FORTRAN 77"),
	  (const gchar *[]) { (gchar[]) { "f" }, (gchar[]) { "for" },
			      (gchar[]) { "ftn" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-fortran" }, NULL }
	},

	{ "fortran90", N_("_FORTRAN 90"),
	  (const gchar *[]) { (gchar[]) { "f90" }, (gchar[]) { "f95" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-fortran" }, NULL }
	},

	{ "fsharp", N_("_F#"),
	  (const gchar *[]) { (gchar[]) { "fs" }, (gchar[]) { "fsx" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "go", N_("_Go"),
	  (const gchar *[]) { (gchar[]) { "go" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-go" }, NULL }
	},

	{ "haskell", N_("_Haskell"),
	  (const gchar *[]) { (gchar[]) { "hs" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-haskell" }, NULL }
	},

	{ "jsp", N_("_JSP"),
	  (const gchar *[]) { (gchar[]) { "jsp" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-jsp" }, NULL }
	},

	{ "lisp", N_("_Lisp"),
	  (const gchar *[]) { (gchar[]) { "cl" }, (gchar[]) { "clisp" },
			      (gchar[]) { "el" }, (gchar[]) { "lsp" },
			      (gchar[]) { "sbcl"}, (gchar[]) { "scom" },
			      NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-emacs-lisp" }, NULL }
	},

	{ "lotus", N_("_Lotus"),
	  (const gchar *[]) { (gchar[]) { "ls" }, NULL },
	  (const gchar *[]) { (gchar[]) { "application/vnd.lotus-1-2-3" }, NULL }
	},

	{ "lua", N_("_Lua"),
	  (const gchar *[]) { (gchar[]) { "lua" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-lua" }, NULL }
	},

	{ "maple", N_("_Maple"),
	  (const gchar *[]) { (gchar[]) { "mpl" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "matlab", N_("_Matlab"),
	  (const gchar *[]) { (gchar[]) { "m" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-matlab" }, NULL }
	},

	{ "maya", N_("_Maya"),
	  (const gchar *[]) { (gchar[]) { "mel" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "oberon", N_("_Oberon"),
	  (const gchar *[]) { (gchar[]) { "ooc" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "objc", N_("_Objective C"),
	  (const gchar *[]) { (gchar[]) { "objc" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-objchdr" },
			      (gchar[]) { "text/x-objcsrc" }, NULL }
	},

	{ "ocaml", N_("_OCaml"),
	  (const gchar *[]) { (gchar[]) { "ml" }, (gchar[]) { "mli" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-ocaml" }, NULL }
	},

	{ "octave", N_("_Octave"),
	  (const gchar *[]) { (gchar[]) { "octave" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "os", N_("_Object Script"),
	  (const gchar *[]) { (gchar[]) { "os" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "pascal", N_("_Pascal"),
	  (const gchar *[]) { (gchar[]) { "pas" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-pascal" }, NULL }
	},

	{ "pov", N_("_POV-Ray"),
	  (const gchar *[]) { (gchar[]) { "pov" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "pro", N_("_Prolog"),
	  (const gchar *[]) { (gchar[]) { "pro" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "r", N_("_R"),
	  (const gchar *[]) { (gchar[]) { "r" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "spec", N_("_RPM Spec"),
	  (const gchar *[]) { (gchar[]) { "spec" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-rpm-spec" }, NULL }
	},

	{ "scala", N_("_Scala"),
	  (const gchar *[]) { (gchar[]) { "scala" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-scala" }, NULL }
	},

	{ "smalltalk", N_("_Smalltalk"),
	  (const gchar *[]) { (gchar[]) { "gst" }, (gchar[]) { "sq" },
			      (gchar[]) { "st" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "tcsh", N_("_TCSH"),
	  (const gchar *[]) { (gchar[]) { "tcsh" }, NULL },
	  (const gchar *[]) { NULL }
	},

	{ "vhd", N_("_VHDL"),
	  (const gchar *[]) { (gchar[]) { "vhd" }, NULL },
	  (const gchar *[]) { (gchar[]) { "text/x-vhdl" }, NULL }
	}
};

Language *
get_default_langauges (gsize *len)
{
	if (len) {
		*len = G_N_ELEMENTS (languages);
	}

	return languages;
}

Language *
get_additinal_languages (gsize *len)
{
	if (len) {
		*len = G_N_ELEMENTS (other_languages);
	}

	return other_languages;
}

const gchar *
get_syntax_for_ext (const gchar *extension)
{
	gint i;
	gint j;

	for (i = 0; i < G_N_ELEMENTS (languages); i++) {

		Language *lang = &languages[i];
		const gchar *ext;

		j = 0;
		ext = lang->extensions[j];
		while (ext) {
			if (g_ascii_strncasecmp (ext, extension, strlen (ext)) == 0) {
				return lang->action_name;
			}

			j++;
			ext = lang->extensions[j];
		}
	}

	for (i = 0; i < G_N_ELEMENTS (other_languages); i++) {

		Language *lang = &other_languages[i];
		const gchar *ext;

		j = 0;
		ext = lang->extensions[j];
		while (ext) {
			if (g_ascii_strncasecmp (ext, extension, strlen (ext)) == 0) {
				return lang->action_name;
			}

			j++;
			ext = lang->extensions[j];
		}
	}

	return NULL;
}

const gchar *
get_syntax_for_mime_type (const gchar *mime_type)
{
	gint i;
	gint j;

	for (i = 0; i < G_N_ELEMENTS (languages); i++) {

		Language *lang = &languages[i];
		const gchar *mt;

		j = 0;
		mt = lang->mime_types[j];
		while (mt) {
			if (g_ascii_strncasecmp (mt, mime_type, strlen (mt)) == 0) {
				return lang->action_name;
			}

			j++;
			mt = lang->mime_types[j];
		}
	}

	for (i = 0; i < G_N_ELEMENTS (other_languages); i++) {

		Language *lang = &other_languages[i];
		const gchar *mt;

		j = 0;
		mt = lang->mime_types[j];
		while (mt) {
			if (g_ascii_strncasecmp (mt, mime_type, strlen (mt)) == 0) {
				return lang->action_name;
			}

			j++;
			mt = lang->mime_types[j];
		}
	}

	return NULL;
}

const gchar **
get_mime_types (void)
{
	G_LOCK (mime_types);
	if (mime_types == NULL) {
		gchar **list;
		gsize array_len;
		gint i, pos;

		array_len = G_N_ELEMENTS (languages);
		pos = 0;

		list = g_malloc (array_len * sizeof (gchar *));

		for (i = 0; i < G_N_ELEMENTS (languages); i++) {
			Language *lang = &languages[i];

			gint j = 0;
			while (lang->mime_types[j] != NULL) {
				if (pos == array_len) {
					array_len += 10;
					list = g_realloc (list, array_len * sizeof (gchar *));
				}

				list[pos] = (gchar *) lang->mime_types[j];
				pos++;
				j++;
			}
		}

		for (i = 0; i < G_N_ELEMENTS (other_languages); i++) {
			Language *lang = &other_languages[i];

			gint j = 0;
			while (lang->mime_types[j] != NULL) {
				if (pos == array_len) {
					array_len += 10;
					list = g_realloc (list, array_len * sizeof (gchar *));
				}

				list[pos] = (gchar *) lang->mime_types[j];
				pos++;
				j++;
			}
		}

		if (pos == array_len) {
			array_len += 1;
			list = g_realloc (list, array_len * sizeof (gchar *));
		}

		/* Ensure the array is null-terminated */
		for (i = pos; i < array_len; i++) {
			list[i] = NULL;
		}

		mime_types = (const gchar **) list;
	}
	G_UNLOCK (mime_types);

	return mime_types;
}
