## Please update this variable if any new macros are created
MACROS= aclocal-include.m4 gnome-guile-checks.m4 \
	gnome-objc-checks.m4 gnome-x-checks.m4 \
	gnome-support.m4 gnome.m4 

EXTRA_DIST=$(MACROS) autogen.sh macros.dep
MAINTAINERCLEANFILES=macros.dep

@MAINT@macros.dep: Makefile.am
@MAINT@	@echo '$$(top_srcdir)/aclocal.m4: $(MACROS:%=macros/%)' > $@
