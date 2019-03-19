#include<epicsExit.h>
#include<epicsString.h>
#include<epicsExport.h>
#include<asynDriver.h>
#include<asynOctet.h>
#include<asynOption.h>
#include<string.h>
#include<stdio.h>
#include<assert.h> 
#include<stdlib.h>
#include<iocsh.h>
#include<cantProceed.h>
#include<curl/curl.h>

typedef struct cjAsynPort {
    asynUser      *pasynUser;
    char          *portName;
    char          *baseURL;
    CURL          *dev;
    int            flags;
#define FLAG_SHUTDOWN                   0x1
    asynInterface  common;
    asynInterface  option;
    asynInterface  octet;
    asynOctet      octet_if;
#define MAXRXDATA    16384
} cjAsynPort_t;

static asynStatus
connectIt(void *drvPvt, asynUser *pasynUser)
{
    cjAsynPort_t *pasyn = (cjAsynPort_t *)drvPvt;

    assert(pasyn);
    assert(!pasyn->dev);
    /* Don't actually *do* anything here!! */
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "Opening connection %s\n", pasyn->portName);
    return pasyn->dev ? asynSuccess : asynError;
}

static void
closeIt(asynUser *pasynUser, cjAsynPort_t *pasyn, const char *why)
{
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "Close %s connection: %s\n", pasyn->portName, why);
    /* Don't actually *do* anything here!! */
    if (pasyn->flags & FLAG_SHUTDOWN)
        pasynManager->exceptionDisconnect(pasynUser);
}

/*
 * asynCommon methods
 */
static void
asynCommonReport(void *drvPvt, FILE *fp, int details)
{
    cjAsynPort_t *pasyn = (cjAsynPort_t *)drvPvt;

    assert(pasyn);
    if (details >= 1) {
        fprintf(fp, "%s --> %s\n", pasyn->portName, pasyn->baseURL);
    }
}

static asynStatus
asynCommonConnect(void *drvPvt, asynUser *pasynUser)
{
    asynStatus status = asynSuccess;

    status = connectIt(drvPvt, pasynUser);
    if (status == asynSuccess)
        pasynManager->exceptionConnect(pasynUser);
    return status;
}

static asynStatus
asynCommonDisconnect(void *drvPvt, asynUser *pasynUser)
{
    cjAsynPort_t *pasyn = (cjAsynPort_t *)drvPvt;

    assert(pasyn);
    closeIt(pasynUser, pasyn, "Disconnect request");
    return asynSuccess;
}

static const struct asynCommon asynCommonMethods = {
    asynCommonReport,
    asynCommonConnect,
    asynCommonDisconnect
};

static asynStatus writeIt(void *drvPvt, asynUser *pasynUser,
    const char *data, size_t numchars,size_t *nbytesTransfered)
{
    cjAsynPort_t *pasyn = (cjAsynPort_t *)drvPvt;
    asynStatus status = asynSuccess;

    assert(pasyn);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s write.\n", pasyn->portName);
    asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, data, numchars,
                "%s write %lu\n", pasyn->portName, (unsigned long)numchars);
    *nbytesTransfered = 0;
    // WORK!!!
    *nbytesTransfered = numchars;
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "wrote %lu to %s, return %s.\n", (unsigned long)*nbytesTransfered,
                                               pasyn->portName,
                                               pasynManager->strStatus(status));
    return status;
}

static asynStatus readIt(void *drvPvt, asynUser *pasynUser,
    char *data, size_t maxchars, size_t *nbytesTransfered, int *gotEom)
{
    cjAsynPort_t *pasyn = (cjAsynPort_t *)drvPvt;
    asynStatus status = asynSuccess;

    assert(pasyn);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s read (max=%lu, timeout=%lg).\n", pasyn->portName, maxchars, pasynUser->timeout);
    if (maxchars <= 0) {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
                  "%s maxchars %d. Why <=0?", pasyn->portName, (int)maxchars);
        return asynError;
    }
    // WORK!!
    *nbytesTransfered = 0;
    if (gotEom) *gotEom = 0;
    return status;
}

static asynStatus flushIt(void *drvPvt,asynUser *pasynUser)
{
    return asynSuccess;
}

/*
 * asynOption methods
 */
static asynStatus
getOption(void *drvPvt, asynUser *pasynUser, const char *key, char *val, int valSize)
{
    cjAsynPort_t *pasyn = (cjAsynPort_t *)drvPvt;

    assert(pasyn);
    /*
     * Look at key, fetch its value and put it into val.  Return asynError/asynSuccess.
     */
    return asynError;
}

static asynStatus
setOption(void *drvPvt, asynUser *pasynUser, const char *key, const char *val)
{
    cjAsynPort_t *pasyn = (cjAsynPort_t *)drvPvt;

    assert(pasyn);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s setOption key %s val %s\n", pasyn->portName, key, val);
    /*
     * Look at key, and put the value where it belongs.  Return asynError/asynSuccess.
     */
    return asynError;
}
static const struct asynOption asynOptionMethods = { setOption, getOption };

static void
final_cleanup (void *arg)
{
    asynStatus status;
    cjAsynPort_t *pasyn = (cjAsynPort_t *)arg;

    if (!pasyn)
        return;
    status=pasynManager->lockPort(pasyn->pasynUser);
    if (status!=asynSuccess)
        asynPrint(pasyn->pasynUser, ASYN_TRACE_ERROR, "%s: cleanup locking error\n", pasyn->portName);
    if (pasyn->dev) {
        pasyn->flags |= FLAG_SHUTDOWN;
        curl_easy_cleanup(pasyn->dev);
        pasyn->dev = NULL;
    }
    curl_global_cleanup();
    if(status==asynSuccess)
        pasynManager->unlockPort(pasyn->pasynUser);
}

static void
cleanup(cjAsynPort_t *pasyn)
{
    if (pasyn) {
        free(pasyn->portName);
        free(pasyn->baseURL);
        if (pasyn->dev) {
            curl_easy_cleanup(pasyn->dev);
            pasyn->dev = NULL;
        }
        free(pasyn);
    }
}

static asynStatus setInputEos(void *drvPvt,asynUser *pasynUser,
                              const char *eos,int eoslen)
{
    cjAsynPort_t *pasyn = (cjAsynPort_t *)drvPvt;
    static int first = 1;
    if (first) {
        printf("WARNING: setInputEos is ignored for port %s\n", pasyn->portName);
        first = 0;
    }
    return asynSuccess;
}

static asynStatus getInputEos(void *drvPvt,asynUser *pasynUser,
                              char *eos, int eossize, int *eoslen)
{
    *eos = 0;
    *eoslen = 0;
    return asynSuccess;
}

static asynStatus setOutputEos(void *drvPvt,asynUser *pasynUser,
                               const char *eos,int eoslen)
{
    cjAsynPort_t *pasyn = (cjAsynPort_t *)drvPvt;
    static int first = 1;
    if (first) {
        printf("WARNING: setOutputEos is ignored for port %s\n", pasyn->portName);
        first = 0;
    }
    return asynSuccess;
}

static asynStatus getOutputEos(void *drvPvt,asynUser *pasynUser,
                               char *eos, int eossize, int *eoslen)
{
    *eos = 0;
    *eoslen = 0;
    return asynSuccess;
}

epicsShareFunc int drvAsynCurlJSONPortConfigure(const char *portName, char *baseURL)
{
    cjAsynPort_t *pasyn;
    static int firsttime = 1;
    asynStatus status;

    if (firsttime) {
        int r = curl_global_init(CURL_GLOBAL_ALL);
        if (r) {
            printf("Cannot initialize libusb: error %d\n", r);
            return -1;
        }
        firsttime = 0;
    }

    if (portName == NULL) {
        printf("Port name missing.\n");
        return -1;
    }
    if (baseURL == NULL) {
        printf("Base URL missing.\n");
        return -1;
    }
    pasyn = (cjAsynPort_t *)callocMustSucceed(1, sizeof(cjAsynPort_t), "drvAsynCurlJSONPortConfigure()");
    pasyn->portName = epicsStrDup(portName);
    pasyn->baseURL = epicsStrDup(baseURL);
    pasyn->dev = curl_easy_init();
    pasyn->flags = 0;

    pasyn->common.interfaceType = asynCommonType;
    pasyn->common.pinterface  = (void *)&asynCommonMethods;
    pasyn->common.drvPvt = pasyn;
    pasyn->option.interfaceType = asynOptionType;
    pasyn->option.pinterface  = (void *)&asynOptionMethods;
    pasyn->option.drvPvt = pasyn;

    if (pasynManager->registerPort(pasyn->portName,
                                   ASYN_CANBLOCK,
                                   1, /* auto connect */
                                   0, /* medium priority */
                                   0) /* default stack size */ != asynSuccess) {
        printf("drvAsynCurlJSONPortConfigure: Can't register myself.\n");
        cleanup(pasyn);
        return -1;
    }
    status = pasynManager->registerInterface(pasyn->portName,&pasyn->common);
    if(status != asynSuccess) {
        printf("drvAsynCurlJSONPortConfigure: Can't register common.\n");
        cleanup(pasyn);
        return -1;
    }
    status = pasynManager->registerInterface(pasyn->portName,&pasyn->option);
    if(status != asynSuccess) {
        printf("drvAsynCurlJSONPortConfigure: Can't register option.\n");
        cleanup(pasyn);
        return -1;
    }

    pasyn->octet.interfaceType = asynOctetType;
    pasyn->octet.pinterface  = (void *)&pasyn->octet_if;
    pasyn->octet.drvPvt = pasyn;
    pasyn->octet_if.read = readIt;
    pasyn->octet_if.write = writeIt;
    pasyn->octet_if.flush = flushIt;
    pasyn->octet_if.setInputEos = setInputEos;
    pasyn->octet_if.setOutputEos = setOutputEos;
    pasyn->octet_if.getInputEos = getInputEos;
    pasyn->octet_if.getOutputEos = getOutputEos;
    status = pasynOctetBase->initialize(pasyn->portName,&pasyn->octet, 0, 0, 1);
    if(status != asynSuccess) {
        printf("drvAsynCurlJSONPortConfigure: pasynOctetBase->initialize failed.\n");
        cleanup(pasyn);
        return -1;
    }

    pasyn->pasynUser = pasynManager->createAsynUser(0,0);
    status = pasynManager->connectDevice(pasyn->pasynUser,pasyn->portName,-1);
    if(status != asynSuccess) {
        printf("connectDevice failed %s\n",pasyn->pasynUser->errorMessage);
        cleanup(pasyn);
        return -1;
    }

    /*
     * Register for socket cleanup
     */
    epicsAtExit(final_cleanup, pasyn);
    return 0;
}

/*
 * IOC shell command registration
 */
static const iocshArg drvAsynCurlJSONPortConfigureArg0 = { "port name",iocshArgString};
static const iocshArg drvAsynCurlJSONPortConfigureArg1 = { "base URL",iocshArgString};
static const iocshArg *drvAsynCurlJSONPortConfigureArgs[] = {
    &drvAsynCurlJSONPortConfigureArg0, &drvAsynCurlJSONPortConfigureArg1};
static const iocshFuncDef drvAsynCurlJSONPortConfigureFuncDef =
                      {"drvAsynCurlJSONPortConfigure",2,drvAsynCurlJSONPortConfigureArgs};
static void drvAsynCurlJSONPortConfigureCallFunc(const iocshArgBuf *args)
{
    drvAsynCurlJSONPortConfigure(args[0].sval, args[1].sval);
}

/*
 * This routine is called before multitasking has started, so there's
 * no race condition in the test/set of firstTime.
 */
static void
drvAsynCurlJSONPortRegisterCommands(void)
{
    static int firstTime = 1;
    if (firstTime) {
        iocshRegister(&drvAsynCurlJSONPortConfigureFuncDef,drvAsynCurlJSONPortConfigureCallFunc);
        firstTime = 0;
    }
}
epicsExportRegistrar(drvAsynCurlJSONPortRegisterCommands);
