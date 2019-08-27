#include<epicsExit.h>
#include<epicsString.h>
#include<epicsExport.h>
#include<asynDriver.h>
#include<asynOctet.h>
#include<asynOctetSyncIO.h>
#include<asynOption.h>
#include<string.h>
#include<stdio.h>
#include<assert.h> 
#include<stdlib.h>
#include<iocsh.h>
#include<cantProceed.h>
#include<curl/curl.h>
#include"rapidjson/document.h"
#include"rapidjson/error/en.h"

typedef struct cpAsynPort {
    char          *portName;
    char          *rawPortName;
    asynInterface  common;
    asynInterface  option;
    asynInterface  octet;
    asynOctet      octet_if;
    asynUser      *user;
    unsigned char  buf[65536];
    size_t         len;
    int            start;
} cpAsynPort_t;

static asynStatus
connectIt(void *drvPvt, asynUser *pasynUser)
{
    cpAsynPort_t *pasyn = (cpAsynPort_t *)drvPvt;

    assert(pasyn);
    /* Don't actually *do* anything here!! */
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "Opening connection %s\n", pasyn->portName);
    pasynManager->exceptionConnect(pasynUser);
    return asynSuccess;
}

static void
closeIt(asynUser *pasynUser, cpAsynPort_t *pasyn, const char *why)
{
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "Close %s connection: %s\n", pasyn->portName, why);
    /* Don't actually *do* anything here!! */
}

/*********************************************************
 *
 * asynCommon methods.
 *
 */
static void
asynCommonReport(void *drvPvt, FILE *fp, int details)
{
    cpAsynPort_t *pasyn = (cpAsynPort_t *)drvPvt;

    assert(pasyn);
    if (details >= 1) {
        fprintf(fp, "%s --> %s\n", pasyn->portName, pasyn->rawPortName);
    }
}

static asynStatus
asynCommonConnect(void *drvPvt, asynUser *pasynUser)
{
    asynStatus status = asynSuccess;

    status = connectIt(drvPvt, pasynUser);
    return status;
}

static asynStatus
asynCommonDisconnect(void *drvPvt, asynUser *pasynUser)
{
    cpAsynPort_t *pasyn = (cpAsynPort_t *)drvPvt;

    assert(pasyn);
    closeIt(pasynUser, pasyn, "Disconnect request");
    return asynSuccess;
}

extern "C" {
static const struct asynCommon asynCommonMethods = {
    asynCommonReport,
    asynCommonConnect,
    asynCommonDisconnect
};
}

/*********************************************************
 *
 * asynOptions methods.
 *
 */
static asynStatus
getOption(void *drvPvt, asynUser *pasynUser, const char *key, char *val, int valSize)
{
    cpAsynPort_t *pasyn = (cpAsynPort_t *)drvPvt;

    assert(pasyn);
    /*
     * Look at key, fetch its value and put it into val.  Return asynError/asynSuccess.
     */
    return asynError;
}

static asynStatus
setOption(void *drvPvt, asynUser *pasynUser, const char *key, const char *val)
{
    cpAsynPort_t *pasyn = (cpAsynPort_t *)drvPvt;

    assert(pasyn);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s setOption key %s val %s\n", pasyn->portName, key, val);
    /*
     * Look at key, and put the value where it belongs.  Return asynError/asynSuccess.
     */
    return asynError;
}

extern "C" {
static const struct asynOption asynOptionMethods = { setOption, getOption };
}

/*********************************************************
 *
 * asynOctet methods.
 *
 */

static asynStatus writeIt(void *ppvt, asynUser *pasynUser,
    const char *data, size_t numchars, size_t *nbytesTransfered)
{
    cpAsynPort_t *pasyn = (cpAsynPort_t *)ppvt;

    return pasynOctetSyncIO->write(pasyn->user, data, numchars, 
                                   pasynUser->timeout, nbytesTransfered);
}

/* crc code stolen from streamdevice for ccitt16 */
static unsigned int ccitt16(const unsigned char* data, unsigned long len)
{
    unsigned long crc = 0xffff;

    // x^16 + x^12 + x^5 + x^0 (0x1021)
    const static unsigned short table[256] = {
      0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
      0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
      0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
      0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
      0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
      0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
      0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
      0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
      0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
      0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
      0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
      0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
      0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
      0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
      0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
      0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
      0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
      0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
      0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
      0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
      0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
      0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
      0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
      0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
      0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
      0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
      0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
      0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
      0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
      0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
      0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
      0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0 };

    while (len--) crc = table[((crc>>8) ^ *data++) & 0xFF] ^ (crc << 8);
    return htole16(crc & 0xffff);
}

static asynStatus readIt(void *ppvt, asynUser *pasynUser,
    char *data, size_t maxchars, size_t *nbytesTransfered, int *eomReason)
{
    cpAsynPort_t *pasyn = (cpAsynPort_t *)ppvt;
    asynStatus status;
    size_t i;

    if (pasyn->len == 0) {
        asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s doing read.\n", pasyn->portName);
        status = pasynOctetSyncIO->read(pasyn->user, (char *)pasyn->buf,
                                        sizeof(pasyn->buf), pasynUser->timeout,
                                        &i, eomReason);
        if (!(status == asynSuccess || status == asynTimeout) || i == 0) {
            asynPrint(pasynUser, ASYN_TRACE_FLOW, "Interpose interface returning because of bad read status = %d\n",
                      (int) status);
            return status;
        }
        asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s read %d bytes.\n", pasyn->portName, (int) i);
        pasyn->len = i;
        pasyn->start = 0;
    }
    for (;;) {
        /* Skip to the next start of packet byte */
        while (pasyn->buf[pasyn->start] != 0xaa && pasyn->len) {
            pasyn->start++;
            pasyn->len--;
        }
        i = (pasyn->len < 3) ? 0 : (pasyn->buf[pasyn->start+1] +
                                            (pasyn->buf[pasyn->start+2] << 8));
        asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s found start of packet at %d, length = %d\n",
                  pasyn->portName, pasyn->start, (int)(i+5));
        if (pasyn->len < i + 5) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR, "%s: Partial packet received, flushing!\n",
                      pasyn->portName);
            pasyn->len = 0;
            *nbytesTransfered = 0;
            status = asynError;
            break;
        } else {
            unsigned int crc = ccitt16(&pasyn->buf[pasyn->start], i + 3);
            if (pasyn->buf[pasyn->start + i + 3] == (crc & 0xff) &&
                 pasyn->buf[pasyn->start + i + 4] == ((crc >> 8) & 0xff)) {
                asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s found %d byte packet at offset %d.\n",
                          pasyn->portName, (int)(i + 5), pasyn->start);
                memcpy(data, &pasyn->buf[pasyn->start], i + 3);
                memcpy(&data[i+3], "MCBEOL", 7);
                *nbytesTransfered = i + 9;
                pasyn->start += i + 5;
                pasyn->len -= i + 5;
                asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, data, *nbytesTransfered,
                            "%s read %d\n", pasyn->portName, (int)*nbytesTransfered);
                status = asynSuccess;
                break;
            } else {
                asynPrint(pasynUser, ASYN_TRACE_ERROR, "%s: Bad CRC received, flushing!\n",
                          pasyn->portName);
                *nbytesTransfered = 0;
                pasyn->start += 1; /* Maybe 0xaa is a data byte? */
                pasyn->len -= 1;
                status = asynError;
            }
        }
    }
    if (eomReason)
        *eomReason = 0;
    return status;
}

static asynStatus flushIt(void *ppvt, asynUser *pasynUser)
{
    cpAsynPort_t *pasyn = (cpAsynPort_t *)ppvt;
    
    return pasynOctetSyncIO->flush(pasyn->user);
}

static void
cleanup(cpAsynPort_t *pasyn)
{
    if (pasyn) {
        free(pasyn->portName);
        free(pasyn->rawPortName);
        free(pasyn);
    }
}

epicsShareFunc int drvAsynCarbidePacketizeConfigure(const char *portName, char *rawPortName)
{
    cpAsynPort_t *pasyn;
    asynStatus status;

    if (portName == NULL || rawPortName == NULL) {
        printf("Port name missing.\n");
        return -1;
    }
    pasyn = (cpAsynPort_t *)callocMustSucceed(1, sizeof(cpAsynPort_t), "drvAsynCarbidePacketizeConfigure()");
    status = pasynOctetSyncIO->connect(rawPortName, 0, &pasyn->user, NULL);
    if (status != asynSuccess) {
        printf("Cannot connect to port %s!\n", rawPortName);
        return -1;
    }
    pasyn->portName = epicsStrDup(portName);
    pasyn->rawPortName = epicsStrDup(rawPortName);

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
        printf("drvAsynCarbidePacketizeConfigure: Can't register myself.\n");
        cleanup(pasyn);
        return -1;
    }
    status = pasynManager->registerInterface(pasyn->portName,&pasyn->common);
    if(status != asynSuccess) {
        printf("drvAsynCarbidePacketizeConfigure: Can't register common.\n");
        cleanup(pasyn);
        return -1;
    }
    status = pasynManager->registerInterface(pasyn->portName,&pasyn->option);
    if(status != asynSuccess) {
        printf("drvAsynCarbidePacketizeConfigure: Can't register option.\n");
        cleanup(pasyn);
        return -1;
    }

    pasyn->octet.interfaceType = asynOctetType;
    pasyn->octet.pinterface  = (void *)&pasyn->octet_if;
    pasyn->octet.drvPvt = pasyn;
    pasyn->octet_if.read = readIt;
    pasyn->octet_if.write = writeIt;
    pasyn->octet_if.flush = flushIt;
    status = pasynOctetBase->initialize(pasyn->portName, &pasyn->octet, 0, 0, 1);
    if(status != asynSuccess) {
        printf("drvAsynCarbidePacketizeConfigure: pasynOctetBase->initialize failed.\n");
        cleanup(pasyn);
        return -1;
    }

    printf("drvAsynCarbidePacketizeConfigure: port %s created (raw port %s)\n", portName, rawPortName);
    return 0;
}
