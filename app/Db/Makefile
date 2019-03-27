TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install)
# databases, templates, substitutions like this
DB += topas.db topas_motor.db

#----------------------------------------------------
# Create and install (or just install)
# Archive files, templates, substitutions like this
#
ARCHIVE += topas-ioc.archive
ARCHIVE += topas.tpl-arch

#----------------------------------------------------
# Create and install (or just install)
# Autosave request files, templates, substitutions like this
#
REQ += topas-ioc.req
REQ += topas.tpl-req

# Nothing to build for autosave, just install from $AUTOSAVE/db
DB_INSTALLS += $(AUTOSAVE)/db/save_restoreStatus.db
# Nothing to build for asyn, just install from $ASYN/db
DB_INSTALLS += $(ASYN)/db/asynRecord.db
# Nothing to build for iocAdmin, just install from $IOCADMIN/db
DB_INSTALLS += $(IOCADMIN)/db/iocSoft.db

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

