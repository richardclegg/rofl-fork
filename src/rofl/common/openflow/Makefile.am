MAINTAINERCLEANFILES = Makefile.in

SUBDIRS = extensions messages experimental

noinst_LTLIBRARIES = libopenflow.la

libopenflow_la_SOURCES= cflowentry.h cofaclist.h cofactionset.h cofbclist.h cofctl.h cofctlImpl.h cofinlist.h cofmatch.h cofport.h coxmlist.h openflow.h openflow13.h openflow12.h openflow11.h openflow10.h cgroupentry.h cofaction.h cofbucket.h cofdpt.h cofdptImpl.h cofinst.h cofportlist.h coxmatch.h openflow_rofl.h openflow_rofl_exceptions.h cofstats.h cofdescstats.h cofflowstats.h cofaggrstats.h coftablestats.h cofportstats.h cofqueuestats.h cofgroupstats.h cofgroupdescstats.h cofgroupfeaturesstats.h cofqueueprop.h cofqueueproplist.h cofpacketqueue.h cofpacketqueuelist.h cofmeterband.h cofmeterbandset.h \
	 cflowentry.cc cofaclist.cc cofactionset.cc cofbclist.cc cofctlImpl.cc cofinlist.cc cofmatch.cc cofport.cc coxmlist.cc cgroupentry.cc cofaction.cc cofbucket.cc cofdptImpl.cc cofinst.cc cofportlist.cc coxmatch.cc cofstats.cc cofdescstats.cc cofflowstats.cc cofaggrstats.cc coftablestats.cc cofportstats.cc cofqueuestats.cc cofgroupstats.cc cofgroupdescstats.cc cofgroupfeaturesstats.cc cofqueueprop.cc cofqueueproplist.cc cofpacketqueue.cc cofpacketqueuelist.cc cofmeterband.cc cofmeterbandset.cc

libopenflow_la_LIBADD= extensions/libopenflow_extensions.la messages/libopenflow_messages.la experimental/libopenflow_experimental.la

library_includedir=$(includedir)/rofl/common/openflow
library_include_HEADERS = cflowentry.h cofaclist.h cofactionset.h cofbclist.h cofctl.h cofctlImpl.h cofinlist.h \
			  cofmatch.h cofport.h coxmlist.h openflow.h openflow_common.h openflow13.h openflow12.h openflow11.h openflow10.h openflow_experimental.h \
		   cgroupentry.h  cofaction.h       cofbucket.h  cofdpt.h  cofdptImpl.h cofinst.h     cofportlist.h  coxmatch.h   openflow_rofl.h openflow_rofl_exceptions.h cofstats.h cofdescstats.h cofflowstats.h cofaggrstats.h coftablestats.h cofportstats.h cofqueuestats.h cofgroupstats.h cofgroupdescstats.h cofgroupfeaturesstats.h cofqueueprop.h cofqueueproplist.h cofpacketqueue.h cofpacketqueuelist.h cofmeterband.h cofmeterbandset.h
