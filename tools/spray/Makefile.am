MAINTAINERCLEANFILES = Makefile.in

if !ONLY_C_CODE
SUBDIRS = 

bin_PROGRAMS=spray

spray_SOURCES= spray.cc \
			cgetopt.h \
			cgetopt.cc \
			cudpsend.h \
			cudpsend.cc \
			cudprecv.h \
			cudprecv.cc \
			cudpmsg.h \
			cudpmsg.cc \
			ctimeval.h \
			ctimeval.cc
			
			
spray_LDADD=$(top_builddir)/src/rofl/librofl.la -lpthread
endif
