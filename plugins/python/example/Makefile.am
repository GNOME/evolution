AM_CPPFLAGS =						\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"		\
	-DEVOLUTION_IMAGESDIR=\""$(imagesdir)"\"	

@EVO_PLUGIN_RULE@

plugin_DATA = \
	hello_python.py	\
	org-gnome-hello-python-ui.xml				\
	org-gnome-hello-python.eplug

liborg_gnome_py_plug_test_la_LIBADD=			\
	$(NO_UNDEFINED_REQUIRED_LIBS)		

liborg_gnome_py_plug_test_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

errordir = $(privdatadir)/errors

BUILDME = org-gnome-hello-python.eplug	\
$(error_i18n)

BUILT_SOURCES =  \
	$(BUILDME)				

EXTRA_DIST =						\
	hello_python.py 					\
	org-gnome-hello-python-ui.xml				\
	org-gnome-hello-python.eplug.xml		

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
