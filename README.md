# ioc-common-topas
This is an EPICS IOC that is used at LCLS. This repo was automatically transferred to github from an internal filesystem repository via the scripts at https://github.com/pcdshub/afs_ioc_migration.

The original filesystem location was /afs/slac.stanford.edu/g/cd/swe/git/repos/package/epics/ioc/common/topas.git.


## Original readme files
### README
The template file for a TOPAS-HP contains declarations for the TOPAS itself
as well as the motors and possible interactions that the device supports.

Two scripts are provided to assist the maintenance of the MOTOR and INTERACTIONS.

At IOC creation time, once a TOPAS is defined, the script children/make_cfg can be
run with the IOC configuration filename as an argument.  This script will talk
to the TOPAS-HP and retrieve motor and interaction information and add it to
the configuration, removing any information that currently is there.

Once the IOC is running, the motor and interaction information will be autosaved,
so as long as the number of motors and interactions is constant, new values can
be overwritten.  To assist with this, the script children/update_cfg can be run
with the IOC configuration file name as an argument.  This script will find the
new information from the TOPAS-HP and store it into the required PVs.

