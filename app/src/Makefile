TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=====================================================
# Path to "NON EPICS" External PACKAGES: USER INCLUDES
#======================================================

# Support Perl C regular expression library and rapidjson
USR_INCLUDES += -I$(PCRE_INCLUDE) -I/reg/g/psdm/sw/conda2/inst/envs/ps-2.0.0/include

#======================================================
# PATH TO "NON EPICS" EXTERNAL PACKAGES: USER LIBRARIES
#======================================================

pcre_DIR = $(PCRE_LIB)
#curl_DIR = /opt/libcurl/lib
# If static, we actually want curl without-ssl, without-zlib.
ifeq '$(T_A)' 'rhel6-x86_64'
curl_DIR = /reg/neh/home/mcbrowne/rhel6/lib
endif
ifeq '$(T_A)' 'rhel7-x86_64'
curl_DIR = /reg/neh/home/mcbrowne/rhel7/lib
endif


# ======================================================
# LINK "NON EPICS" EXTERNAL PACKAGE LIBRARIES STATICALLY
#=======================================================

USR_LIBS_Linux += pcre

#=============================
# Build an ioc application
# Use PROD_IOC_RTEMS for RTEMS only
# Use PROD_IOC_Linux for Linux only
PROD_IOC_Linux = topas

# topas.dbd will be created and installed
DBD += topas.dbd

# topas.dbd will be made up from these files:
topas_DBD += base.dbd
topas_DBD += iocAdmin.dbd
topas_DBD += asSupport.dbd
topas_DBD += asyn.dbd
topas_DBD += drvAsynSerialPort.dbd
topas_DBD += drvAsynIPPort.dbd
topas_DBD += stream.dbd

topas_DBD += drvAsynCurlJSONPort.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
topas_SRCS += topas_registerRecordDeviceDriver.cpp
topas_SRCS_DEFAULT += topasMain.cpp

# Add locally compiled object code
#topas_SRCS += dbSubExample.c

topas_SRCS += drvAsynCurlJSONPort.cpp
topas_SRCS += drvAsynCarbidePacketize.cpp

topas_LIBS += devIocStats
topas_LIBS += autosave
topas_LIBS += asyn
topas_LIBS += stream
topas_LIBS += $(EPICS_BASE_IOC_LIBS)
topas_LIBS += curl
USR_SYS_LIBS += z

topas_LDFLAGS += -L$(curl_DIR)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

