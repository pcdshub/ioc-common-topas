#!$$IOCTOP/bin/$$IF(ARCH,$$ARCH,linux-x86_64)/topas

< envPaths

epicsEnvSet( "ENGINEER" , "$$ENGINEER" )
epicsEnvSet( "IOCSH_PS1", "$$IOCNAME>" )
epicsEnvSet( "IOCENAME",  "$$TRANSLATE(IOCNAME,"a-z_-","A-Z::")" )
epicsEnvSet( "IOCPVROOT", "$$IF(IOCPVROOT,$$IOCPVROOT,$(IOCENAME)")
epicsEnvSet( "LOCATION",  "$$IF(LOCATION,$$LOCATION,$$IOCPVROOT)")
epicsEnvSet( "BASE_NAME", "$$NAME"     )
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
drvAsynCurlJSONPortConfigure(TOPAS$$INDEX, "$$URL")

#define ASYN_TRACE_ERROR     0x0001
#define ASYN_TRACEIO_DEVICE  0x0002
#define ASYN_TRACEIO_FILTER  0x0004
#define ASYN_TRACEIO_DRIVER  0x0008
#define ASYN_TRACE_FLOW      0x0010
$$IF(DEBUG,,#)asynSetTraceMask( "$(USBPORT)", 0, 0x19) # log everything
#define ASYN_TRACEIO_ASCII  0x0001
#define ASYN_TRACEIO_ESCAPE 0x0002
#define ASYN_TRACEIO_HEX    0x0004
$$IF(DEBUG,,#)asynSetTraceIOMask( "$(USBPORT)", 0, 2)  # Escape the strings.

## Asyn record support
dbLoadRecords( "db/asynRecord.db","P=$(BASE_NAME),R=,PORT=TOPAS$$INDEX,ADDR=0,OMAX=0,IMAX=0")
$$ENDLOOP(TOPAS)

#------------------------------------------------------------------------------
# Load record instances

$$LOOP(TOPAS)
dbLoadRecords( "db/topas.db",              	"P=$(BASE_NAME), PORT=TOPAS$$INDEX" )
$$ENDLOOP(TOPAS)
dbLoadRecords( "db/iocSoft.db",			"IOC=$(IOCENAME)" )
dbLoadRecords( "db/save_restoreStatus.db",	"P=$(IOCPVROOT)" )

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
