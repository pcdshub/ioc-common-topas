#!$$IOCTOP/bin/$$IF(ARCH,$$ARCH,linux-x86_64)/topas

< envPaths

epicsEnvSet( "ENGINEER" , "$$ENGINEER" )
epicsEnvSet( "IOCSH_PS1", "$$IOCNAME>" )
epicsEnvSet( "IOCPVROOT", "$$IOC_PV"   )
epicsEnvSet( "LOCATION",  "$$IF(LOCATION,$$LOCATION,$$IOC_PV)")
epicsEnvSet( "IOC_TOP",   "$$IOCTOP"   )
epicsEnvSet( "TOP",       "$$TOP"      )
epicsEnvSet( "STREAM_PROTOCOL_PATH", "$(IOC_TOP)/app/srcProtocol" )

cd( "$(IOC_TOP)" )

# Run common startup commands for linux soft IOC's
< /reg/d/iocCommon/All/pre_linux.cmd

# Register all support components
dbLoadDatabase("dbd/topas.dbd")
topas_registerRecordDeviceDriver(pdbbase)

#------------------------------------------------------------------------------
# Asyn support

# Set this to enable LOTS of stream module diagnostics
#var streamDebug 1

$$LOOP(TOPAS)
drvAsynCurlJSONPortConfigure(TOPAS$$INDEX, "http://$$HOST:$$IF(PORT,$$PORT,8004)/$$DEVICE/v0/PublicAPI")

#define ASYN_TRACE_ERROR     0x0001
#define ASYN_TRACEIO_DEVICE  0x0002
#define ASYN_TRACEIO_FILTER  0x0004
#define ASYN_TRACEIO_DRIVER  0x0008
#define ASYN_TRACE_FLOW      0x0010
$$IF(DEBUG,,#)asynSetTraceMask( "TOPAS$$INDEX", 0, 0x19) # log everything
#define ASYN_TRACEIO_ASCII  0x0001
#define ASYN_TRACEIO_ESCAPE 0x0002
#define ASYN_TRACEIO_HEX    0x0004
$$IF(DEBUG,,#)asynSetTraceIOMask( "TOPAS$$INDEX", 0, 2)  # Escape the strings.

## Asyn record support
dbLoadRecords( "db/asynRecord.db","P=$$BASE,R=,PORT=TOPAS$$INDEX,ADDR=0,OMAX=0,IMAX=0")
$$ENDLOOP(TOPAS)

$$LOOP(CARBIDE)
drvAsynCurlJSONPortConfigure(CBD$$INDEX, "http://$$HOST:$$IF(PORT,$$PORT,20010)/v$$IF(API,$$API,0)")
drvAsynIPPortConfigure("CBDRAW$$INDEX", "$$HOST:$$IF(RAWPORT,$$RAWPORT,20016) TCP", 0, 0, 0 )

#define ASYN_TRACE_ERROR     0x0001
#define ASYN_TRACEIO_DEVICE  0x0002
#define ASYN_TRACEIO_FILTER  0x0004
#define ASYN_TRACEIO_DRIVER  0x0008
#define ASYN_TRACE_FLOW      0x0010
$$IF(DEBUG,,#)asynSetTraceMask( "CBD$$INDEX", 0, 0x19) # log everything
$$IF(RAWDEBUG,,#)asynSetTraceMask( "CBDRAW$$INDEX", 0, 0x19) # log everything
#define ASYN_TRACEIO_ASCII  0x0001
#define ASYN_TRACEIO_ESCAPE 0x0002
#define ASYN_TRACEIO_HEX    0x0004
$$IF(DEBUG,,#)asynSetTraceIOMask( "CBD$$INDEX", 0, 2)  # Escape the strings.
$$IF(RAWDEBUG,,#)asynSetTraceIOMask( "CBDRAW$$INDEX", 0, 2)  # Escape the strings.

## Asyn record support
dbLoadRecords( "db/asynRecord.db","P=$$BASE,R=CBD,PORT=CBD$$INDEX,ADDR=0,OMAX=0,IMAX=0")
dbLoadRecords( "db/asynRecord.db","P=$$BASE,R=RAW,PORT=CBDRAW$$INDEX,ADDR=0,OMAX=0,IMAX=0")
$$ENDLOOP(CARBIDE)

$$LOOP(POWERMETER)
drvAsynCurlJSONPortConfigure(POWER$$INDEX, "http://$$HOST:$$PORT/V2")
$$IF(DEBUG,,#)asynSetTraceMask("POWER$$INDEX", 0, 0x19) # log everything
$$IF(DEBUG,,#)asynSetTraceIOMask("POWER$$INDEX", 0, 2)  # Escape the strings.
$$ENDLOOP(POWERMETER)
$$LOOP(SPECTROMETER)
drvAsynCurlJSONPortConfigure(SPEC$$INDEX, "http://$$HOST:$$PORT/V2")
$$IF(DEBUG,,#)asynSetTraceMask("SPEC$$INDEX", 0, 0x19) # log everything
$$IF(DEBUG,,#)asynSetTraceIOMask("SPEC$$INDEX", 0, 2)  # Escape the strings.
$$ENDLOOP(SPECTROMETER)

#------------------------------------------------------------------------------
# Load record instances

$$LOOP(TOPAS)
dbLoadRecords( "db/topas.db",              	"BASE=$$BASE, PORT=TOPAS$$INDEX" )
$$ENDLOOP(TOPAS)
$$LOOP(CARBIDE)
dbLoadRecords( "db/carbide.db",            	"BASE=$$BASE, PORT=CBD$$INDEX" )
dbLoadRecords( "db/carbide_reg.db",            	"BASE=$$BASE, PORT=CBDRAW$$INDEX" )
$$ENDLOOP(CARBIDE)
$$LOOP(MOTOR)
dbLoadRecords( "db/topas_motor.db", "BASE=$$BASE,PORT=TOPAS$$PORT,ID=$$ID,NAME=$$NAME,EGU=$$EGU,LOPR=$$LOPR,HOPR=$$HOPR" )
$$ENDLOOP(MOTOR)
$$LOOP(INTERACTION)
dbLoadRecords( "db/topas_inter.db", "BASE=$$BASE,ID=$$ID,NAME=$$NAME,FN=$$FN,FV=$$FV,MIN=$$MIN,MAX=$$MAX" )
$$ENDLOOP(INTERACTION)
$$LOOP(POWERMETER)
dbLoadRecords( "db/topas_power.db", "BASE=$$BASE,ID=$$ID,NAME=$$NAME,PORT=POWER$$INDEX" )
$$ENDLOOP(POWERMETER)
$$LOOP(SPECTROMETER)
dbLoadRecords( "db/topas_spec.db", "BASE=$$BASE,ID=$$ID,NAME=$$NAME,PORT=SPEC$$INDEX" )
$$ENDLOOP(SPECTROMETER)
dbLoadRecords( "db/iocSoft.db",			"IOC=$(IOCPVROOT)" )
dbLoadRecords( "db/save_restoreStatus.db",	"P=$(IOCPVROOT):" )

#------------------------------------------------------------------------------
# Setup autosave

set_savefile_path( "$(IOC_DATA)/$(IOC)/autosave" )
set_requestfile_path( "$(TOP)/autosave" )
save_restoreSet_status_prefix( "$(IOCPVROOT):" )
save_restoreSet_IncompleteSetsOk( 1 )
save_restoreSet_DatedBackupFiles( 1 )

# Just restore the settings
set_pass0_restoreFile( "$(IOC).sav" )
set_pass1_restoreFile( "$(IOC).sav" )


# Initialize the IOC and start processing records
iocInit()

# Start autosave backups
create_monitor_set( "$(IOC).req", 5, "" )

# All IOCs should dump some common info after initial startup.
< /reg/d/iocCommon/All/post_linux.cmd
