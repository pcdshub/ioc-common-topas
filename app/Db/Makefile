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
DB += topas.db topas_motor.db topas_inter.db topas_power.db topas_spec.db
DB += carbide_reg.db carbide.db

#----------------------------------------------------
# Create and install (or just install)
# Archive files, templates, substitutions like this
#
ARCHIVE += topas-ioc.archive
ARCHIVE += topas.tpl-arch
ARCHIVE += carbide_reg.arch carbide.tpl-arch

#----------------------------------------------------
# Create and install (or just install)
# Autosave request files, templates, substitutions like this
#
REQ += topas-ioc.req
REQ += topas.tpl-req
REQ += topas_motor.tpl-req
REQ += topas_inter.tpl-req
REQ += carbide_reg.req carbide.tpl-req

# Nothing to build for autosave, just install from $AUTOSAVE/db
DB_INSTALLS += $(AUTOSAVE)/db/save_restoreStatus.db
# Nothing to build for asyn, just install from $ASYN/db
DB_INSTALLS += $(ASYN)/db/asynRecord.db
# Nothing to build for iocAdmin, just install from $IOCADMIN/db
DB_INSTALLS += $(IOCADMIN)/db/iocSoft.db

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE


MACRO       = /reg/g/pcds/controls/macro/expand

carbide_reg.db.d: ../O.Common/carbide_reg.db
	touch carbide_reg.db.d

../O.Common/carbide_reg.db: ../CARBIDE.cfg ../CARBIDE.db
	csh -c "$(MACRO) -c ../CARBIDE.cfg ../CARBIDE.db $@"

../O.Common/carbide_reg.arch: ../CARBIDE.cfg ../CARBIDE.arch
	csh -c "$(MACRO) -c ../CARBIDE.cfg ../CARBIDE.arch $@"

../O.Common/carbide_reg.req: ../CARBIDE.cfg ../CARBIDE.req
	csh -c "$(MACRO) -c ../CARBIDE.cfg ../CARBIDE.req $@"

../CARBIDE.cfg ../CARBIDE.db ../CARBIDE.arch ../CARBIDE.req:

realclean:
	-rm -rf O.* *~
	-rm -f carbide_reg.{db,arch,req}
