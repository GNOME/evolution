/*
  generic s-exp evaluator class
*/
#ifndef _E_SEXP_H
#define _E_SEXP_H

#include <setjmp.h>
#include <time.h>
#include <glib.h>

#ifdef E_SEXP_IS_G_OBJECT
#include <glib-object.h>
#endif

#ifdef E_SEXP_IS_G_OBJECT
#define E_TYPE_SEXP            (e_sexp_get_type ())
#define E_SEXP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SEXP, ESExp))
#define E_SEXP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SEXP, ESExpClass))
#define IS_E_SEXP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SEXP))
#define IS_E_SEXP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_SEXP))
#define E_SEXP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_SEXP, ESExpClass))
#else
#define E_TYPE_SEXP            (0)
#define E_SEXP(obj)            ((struct _ESExp *) (obj))
#define E_SEXP_CLASS(klass)    ((struct _ESExpClass *) (klass))
#define IS_E_SEXP(obj)         (1)
#define IS_E_SEXP_CLASS(obj)   (1)
#define E_SEXP_GET_CLASS(obj)  (NULL)
#endif

typedef struct _ESExp      ESExp;
typedef struct _ESExpClass ESExpClass;

typedef struct _ESExpSymbol ESExpSymbol;
typedef struct _ESExpResult ESExpResult;
typedef struct _ESExpTerm ESExpTerm;

typedef struct _ESExpResult *(ESExpFunc)(struct _ESExp *sexp, int argc,
					 struct _ESExpResult **argv,
					 void *data);

typedef struct _ESExpResult *(ESExpIFunc)(struct _ESExp *sexp, int argc,
					  struct _ESExpTerm **argv,
					  void *data);

enum _ESExpResultType {
	ESEXP_RES_ARRAY_PTR=0,	/* type is a ptrarray, what it points to is implementation dependant */
	ESEXP_RES_INT,		/* type is a number */
	ESEXP_RES_STRING,	/* type is a pointer to a single string */
	ESEXP_RES_BOOL,		/* boolean type */
	ESEXP_RES_TIME,		/* time_t type */
	ESEXP_RES_UNDEFINED	/* unknown type */
};

struct _ESExpResult {
	enum _ESExpResultType type;
	union {
		GPtrArray *ptrarray;
		int number;
		char *string;
		int bool;
		time_t time;
	} value;
};

enum _ESExpTermType {
	ESEXP_TERM_INT	= 0,	/* integer literal */
	ESEXP_TERM_BOOL,	/* boolean literal */
	ESEXP_TERM_STRING,	/* string literal */
	ESEXP_TERM_TIME,	/* time_t literal (number of seconds past the epoch) */
	ESEXP_TERM_FUNC,	/* normal function, arguments are evaluated before calling */
	ESEXP_TERM_IFUNC,	/* immediate function, raw terms are arguments */
	ESEXP_TERM_VAR,		/* variable reference */
};

struct _ESExpSymbol {
	int type;		/* ESEXP_TERM_FUNC or ESEXP_TERM_VAR */
	char *name;
	void *data;
	union {
		ESExpFunc *func;
		ESExpIFunc *ifunc;
	} f;
};

struct _ESExpTerm {
	enum _ESExpTermType type;
	union {
		char *string;
		int number;
		int bool;
		time_t time;
		struct {
			struct _ESExpSymbol *sym;
			struct _ESExpTerm **terms;
			int termcount;
		} func;
		struct _ESExpSymbol *var;
	} value;
};



struct _ESExp {
#ifdef E_SEXP_IS_G_OBJECT
	GObject parent_object;
#else
	int refcount;
#endif
	GScanner *scanner;	/* for parsing text version */
	ESExpTerm *tree;	/* root of expression tree */
	
	/* private stuff */
	jmp_buf failenv;
	char *error;
	
	/* TODO: may also need a pool allocator for term strings, so we dont lose them
	   in error conditions? */
	struct _EMemChunk *term_chunks;
	struct _EMemChunk *result_chunks;
};

struct _ESExpClass {
#ifdef E_SEXP_IS_G_OBJECT
	GObjectClass parent_class;
#else
	int dummy;
#endif
};

#ifdef E_SEXP_IS_G_OBJECT
GType           e_sexp_get_type		(void);
#endif
ESExp 	       *e_sexp_new		(void);
#ifdef E_SEXP_IS_G_OBJECT
#define         e_sexp_ref(f)           g_object_ref (f)
#define         e_sexp_unref(f)         g_object_unref (f)
#else
void		e_sexp_ref		(ESExp *f);
void		e_sexp_unref		(ESExp *f);
#endif
void		e_sexp_add_function  	(ESExp *f, int scope, char *name, ESExpFunc *func, void *data);
void		e_sexp_add_ifunction  	(ESExp *f, int scope, char *name, ESExpIFunc *func, void *data);
void		e_sexp_add_variable  	(ESExp *f, int scope, char *name, ESExpTerm *value);
void		e_sexp_remove_symbol	(ESExp *f, int scope, char *name);
int		e_sexp_set_scope	(ESExp *f, int scope);

void		e_sexp_input_text	(ESExp *f, const char *text, int len);
void		e_sexp_input_file	(ESExp *f, int fd);


int		e_sexp_parse		(ESExp *f);
ESExpResult    *e_sexp_eval		(ESExp *f);

ESExpResult    *e_sexp_term_eval	(struct _ESExp *f, struct _ESExpTerm *t);
ESExpResult    *e_sexp_result_new	(struct _ESExp *f, int type);
void		e_sexp_result_free	(struct _ESExp *f, struct _ESExpResult *t);

/* used in normal functions if they have to abort, to free their arguments */
void		e_sexp_resultv_free	(struct _ESExp *f, int argc, struct _ESExpResult **argv);

/* utility functions for creating s-exp strings. */
void		e_sexp_encode_bool	(GString *s, gboolean state);
void		e_sexp_encode_string	(GString *s, const char *string);

/* only to be called from inside a callback to signal a fatal execution error */
void		e_sexp_fatal_error	(struct _ESExp *f, char *why, ...);

/* return the error string */
const char     *e_sexp_error		(struct _ESExp *f);

#endif /* _E_SEXP_H */
