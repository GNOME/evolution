/*
  generic s-exp evaluator class
*/
#ifndef _FILTER_SEXP_H
#define _FILTER_SEXP_H

#include <glib.h>
#include <gtk/gtk.h>
#include <gtkhtml/gtkhtml.h>
#include <gnome-xml/tree.h>

#define FILTER_SEXP(obj)         GTK_CHECK_CAST (obj, filter_sexp_get_type (), FilterSEXP)
#define FILTER_SEXP_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, filter_sexp_get_type (), FilterSEXPClass)
#define FILTER_IS_SEXP(obj)      GTK_CHECK_TYPE (obj, filter_sexp_get_type ())

typedef struct _FilterSEXP      FilterSEXP;
typedef struct _FilterSEXPClass FilterSEXPClass;

typedef struct _FilterSEXPSymbol FilterSEXPSymbol;
typedef struct _FilterSEXPResult FilterSEXPResult;
typedef struct _FilterSEXPTerm FilterSEXPTerm;

typedef struct _FilterSEXPResult *(FilterSEXPFunc)(struct _FilterSEXP *sexp,
						   int argc,
						   struct _FilterSEXPResult **argv,
						   void *data);

typedef struct _FilterSEXPResult *(FilterSEXPIFunc)(struct _FilterSEXP *sexp,
						    int argc,
						    struct _FilterSEXPTerm **argv,
						    void *data);
enum _FilterSEXPResultType {
	FSEXP_RES_ARRAY_PTR=0,	/* type is a ptrarray, what it points to is implementation dependant */
	FSEXP_RES_INT,		/* type is a number */
	FSEXP_RES_STRING,	/* type is a pointer to a single string */
	FSEXP_RES_BOOL,		/* boolean type */
	FSEXP_RES_UNDEFINED	/* unknown type */
};

struct _FilterSEXPResult {
	enum _FilterSEXPResultType type;
	union {
		GPtrArray *ptrarray;
		int number;
		char *string;
		int bool;
	} value;
};

enum _FilterSEXPTermType {
	FSEXP_TERM_INT	= 0,	/* integer literal */
	FSEXP_TERM_BOOL,	/* boolean literal */
	FSEXP_TERM_STRING,	/* string literal */
	FSEXP_TERM_FUNC,	/* normal function, arguments are evaluated before calling */
	FSEXP_TERM_IFUNC,	/* immediate function, raw terms are arguments */
	FSEXP_TERM_VAR,		/* variable reference */
};

struct _FilterSEXPSymbol {
	int type;		/* FSEXP_TERM_FUNC or FSEXP_TERM_VAR */
	char *name;
	void *data;
	union {
		FilterSEXPFunc *func;
		FilterSEXPIFunc *ifunc;
	} f;
};

struct _FilterSEXPTerm {
	enum _FilterSEXPTermType type;
	union {
		char *string;
		int number;
		int bool;
		struct {
			struct _FilterSEXPSymbol *sym;
			struct _FilterSEXPTerm **terms;
			int termcount;
		} func;
		struct _FilterSEXPSymbol *var;
	} value;
};



struct _FilterSEXP {
	GtkObject object;

	GScanner *scanner;	/* for parsing text version */
	FilterSEXPTerm *tree;	/* root of expression tree */
};

struct _FilterSEXPClass {
	GtkObjectClass parent_class;

};

guint		filter_sexp_get_type		(void);
FilterSEXP     *filter_sexp_new			(void);
void		filter_sexp_add_function  	(FilterSEXP *f, int scope, char *name, FilterSEXPFunc *func, void *data);
void		filter_sexp_add_ifunction  	(FilterSEXP *f, int scope, char *name, FilterSEXPIFunc *func, void *data);
void		filter_sexp_add_variable  	(FilterSEXP *f, int scope, char *name, FilterSEXPTerm *value);
void		filter_sexp_remove_symbol	(FilterSEXP *f, int scope, char *name);
int		filter_sexp_set_scope		(FilterSEXP *f, int scope);

void		filter_sexp_input_text		(FilterSEXP *f, char *text, int len);
void		filter_sexp_input_file		(FilterSEXP *f, int fd);


void		filter_sexp_parse		(FilterSEXP *f);
FilterSEXPResult *filter_sexp_eval		(FilterSEXP *f);

FilterSEXPResult *filter_sexp_term_eval		(struct _FilterSEXP *f, struct _FilterSEXPTerm *t);
FilterSEXPResult *filter_sexp_result_new	(int type);
void		filter_sexp_result_free		(struct _FilterSEXPResult *t);

#endif /* _FILTER_SEXP_H */
