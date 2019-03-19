TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=====================================================
# Path to "NON EPICS" External PACKAGES: USER INCLUDES
#======================================================

# Support Perl C regular expression library
USR_INCLUDES += -I$(PCRE_INCLUDE)

# Support for libusb
USR_INCLUDES += -I/usr/include/libusb-1.0
#USR_INCLUDES += -lusb-1.0

#======================================================
# PATH TO "NON EPICS" EXTERNAL PACKAGES: USER LIBRARIES
#======================================================

pcre_DIR = $(PCRE_LIB)
usb-1.0_DIR = /usr/lib64

# ======================================================
# LINK "NON EPICS" EXTERNAL PACKAGE LIBRARIES STATICALLY
#=======================================================

USR_LIBS_Linux += pcre
USR_LIBS_Linux += usb-1.0 
#USR_LIBS += pcre

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
#topas_DBD += asynRecord.dbd
topas_DBD += drvAsynSerialPort.dbd
topas_DBD += drvAsynIPPort.dbd
topas_DBD += stream.dbd
topas_DBD += asynInterposeDribble.dbd

topas_DBD += drvAsynCurlJSONPort.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
topas_SRCS += topas_registerRecordDeviceDriver.cpp
topas_SRCS_DEFAULT += topasMain.cpp

# Add locally compiled object code
#topas_SRCS += dbSubExample.c

topas_SRCS += drvAsynCurlJSONPort.c

topas_LIBS += devIocStats
topas_LIBS += autosave
topas_LIBS += asyn
topas_LIBS += stream
topas_LIBS += $(EPICS_BASE_IOC_LIBS)

#ifneq '$(T_A)' 'RTEMS-beatnik'
#PROD_SYS_LIBS_DEFAULT += pcre
#SHRLIB_DEPLIB_DIRS += $(PCRE_LIB)
#endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

