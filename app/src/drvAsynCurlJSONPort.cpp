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
#include"rapidjson/document.h"
#include"rapidjson/error/en.h"

typedef struct cjAsynPort {
    asynUser      *pasynUser;
    char          *portName;
    char          *baseURL;
    CURL          *dev;
    long           curlrc;   /* Last return code! */
    int            flags;
#define FLAG_SHUTDOWN                   0x1
    int            state;
    /*
     * State definitions:
     *     INIT  - We are ready to make a new request.
     *     WBODY - We have a request to make, but are waiting to get the request body.
     *     RCODE - We have made a request and have the response, but are waiting for
     *             streamdevice to read out the return code.
     *     RBODY - We have made a request and have the response, but are waiting for
     *             streamdevice to read out the body.
     */
#define STATE_INIT                      0
#define STATE_WBODY                     1
#define STATE_RCODE                     2
#define STATE_RBODY                     3
    int            req;
#define REQ_GET                         0
#define REQ_POST                        1
#define REQ_POSTV                       2
#define REQ_PUT                         3
#define REQ_PUTV                        4
    asynInterface  common;
    asynInterface  option;
    asynInterface  octet;
    asynOctet      octet_if;
#define MAXTXBUF   16384
#define MAXRXBUF   131072
    char           outbuf[MAXTXBUF];  /* Buffer to "write" to the asyn port */
    char          *rxbuf;
    char           inbuf[MAXRXBUF];   /* Raw read from the asyn port */
    char           rawbuf[MAXRXBUF];  /* Buffer to "read" from the asyn port */
    int            incnt;
    int            incur;
    char           fullURL[MAXTXBUF];
#define MAXFILTER  10
    char          *filters[MAXFILTER];
    int            fcnt;
    int            fcur;
    struct curl_slist *headers;
    rapidjson::Document *d;
} cjAsynPort_t;

static asynStatus
connectIt(void *drvPvt, asynUser *pasynUser)
{
    cjAsynPort_t *pasyn = (cjAsynPort_t *)drvPvt;

    assert(pasyn);
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

extern "C" {
static const struct asynCommon asynCommonMethods = {
    asynCommonReport,
    asynCommonConnect,
    asynCommonDisconnect
};
}

static size_t dropMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    return realsize;
}

static size_t appendMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    cjAsynPort_t *pasyn = (cjAsynPort_t *)userp;
    /* Check buffer size! */
    memcpy(&pasyn->rxbuf[pasyn->incnt], contents, realsize);
    pasyn->incnt += realsize;
    pasyn->rxbuf[pasyn->incnt] = 0;
    return realsize;
}

static asynStatus parseRequest(cjAsynPort_t *pasyn, asynUser *pasynUser)
{
    char *s, *t;
    asynStatus status;

    if ((s = index(pasyn->outbuf, ':')) == NULL) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, "Malformed request without :!\n");
        return asynError;
    }
    *s++ = 0;
    if (!strcmp(pasyn->outbuf, "GET"))
        pasyn->req = REQ_GET;
    else if (!strcmp(pasyn->outbuf, "PUT"))
        pasyn->req = REQ_PUT;
    else if (!strcmp(pasyn->outbuf, "PUTV"))
        pasyn->req = REQ_PUTV;
    else if (!strcmp(pasyn->outbuf, "POST"))
        pasyn->req = REQ_POST;
    else if (!strcmp(pasyn->outbuf, "POSTV"))
        pasyn->req = REQ_POSTV;
    else {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, "Malformed request with bad type %s!\n",
                  pasyn->outbuf);
        return asynError;
    }

    pasyn->fcnt = 0;
    pasyn->fcur = 0;
    t = s;
    if ((t = index(t, ':')) != NULL) {
        do {
            *t++ = 0;
            pasyn->filters[pasyn->fcnt++] = t;
            t = index(t, ',');
        } while (t);
    }

    pasyn->incnt = 0;
    pasyn->incur = -1;

    curl_easy_reset(pasyn->dev);
    sprintf(pasyn->fullURL, "%s/%s", pasyn->baseURL, s);
    curl_easy_setopt(pasyn->dev, CURLOPT_URL, pasyn->fullURL);
    curl_easy_setopt(pasyn->dev, CURLOPT_HTTPHEADER, pasyn->headers);
    //curl_easy_setopt(pasyn->dev, CURLOPT_VERBOSE, 1L);

    switch (pasyn->req) {
    case REQ_GET:
        curl_easy_setopt(pasyn->dev, CURLOPT_WRITEFUNCTION, appendMemoryCallback);
        curl_easy_setopt(pasyn->dev, CURLOPT_WRITEDATA, (void *)pasyn);
        pasyn->rxbuf = pasyn->fcnt ? pasyn->rawbuf : pasyn->inbuf;
        pasyn->state = STATE_RCODE;
        status = (curl_easy_perform(pasyn->dev) == CURLE_OK) ? asynSuccess : asynError;
        curl_easy_getinfo(pasyn->dev, CURLINFO_RESPONSE_CODE, &pasyn->curlrc);
        if (!pasyn->fcnt) {
            pasyn->inbuf[pasyn->incnt++] = '\r';
            pasyn->inbuf[pasyn->incnt++] = '\n';
        }
        return status;
    case REQ_PUT:
        curl_easy_setopt(pasyn->dev, CURLOPT_POSTFIELDS, "");
        curl_easy_setopt(pasyn->dev, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(pasyn->dev, CURLOPT_WRITEFUNCTION, 
                         pasyn->fcnt ? appendMemoryCallback : dropMemoryCallback);
        curl_easy_setopt(pasyn->dev, CURLOPT_WRITEDATA, (void *)pasyn);
        if (pasyn->fcnt) {
            pasyn->rxbuf = pasyn->rawbuf;
            pasyn->state = STATE_RCODE;
        }
        status = (curl_easy_perform(pasyn->dev) == CURLE_OK) ? asynSuccess : asynError;
        curl_easy_getinfo(pasyn->dev, CURLINFO_RESPONSE_CODE, &pasyn->curlrc);
        return status;
    case REQ_PUTV:
        curl_easy_setopt(pasyn->dev, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(pasyn->dev, CURLOPT_WRITEFUNCTION, 
                         pasyn->fcnt ? appendMemoryCallback : dropMemoryCallback);
        curl_easy_setopt(pasyn->dev, CURLOPT_WRITEDATA, (void *)pasyn);
        if (pasyn->fcnt)
            pasyn->rxbuf = pasyn->rawbuf;
        pasyn->state = STATE_WBODY;
        return asynSuccess;
    case REQ_POST:
        curl_easy_setopt(pasyn->dev, CURLOPT_POSTFIELDS, "");
        curl_easy_setopt(pasyn->dev, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(pasyn->dev, CURLOPT_WRITEFUNCTION, 
                         pasyn->fcnt ? appendMemoryCallback : dropMemoryCallback);
        curl_easy_setopt(pasyn->dev, CURLOPT_WRITEDATA, (void *)pasyn);
        if (pasyn->fcnt) {
            pasyn->rxbuf = pasyn->rawbuf;
            pasyn->state = STATE_RBODY;
        }
        status = (curl_easy_perform(pasyn->dev) == CURLE_OK) ? asynSuccess : asynError;
        curl_easy_getinfo(pasyn->dev, CURLINFO_RESPONSE_CODE, &pasyn->curlrc);
        return status;
    case REQ_POSTV:
        curl_easy_setopt(pasyn->dev, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(pasyn->dev, CURLOPT_WRITEFUNCTION, 
                         pasyn->fcnt ? appendMemoryCallback : dropMemoryCallback);
        curl_easy_setopt(pasyn->dev, CURLOPT_WRITEDATA, (void *)pasyn);
        if (pasyn->fcnt)
            pasyn->rxbuf = pasyn->rawbuf;
        pasyn->state = STATE_WBODY;
        return asynSuccess;
    }
    return asynError; /* We never get here, but let's make the compiler happy. */
}

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
    *nbytesTransfered = numchars; /* We'll take it all in any case! */
    switch (pasyn->state) {
    case STATE_INIT:
        /*
         * We assume we have the entire request: TYPE:URLTAIL:LIST_OF_JSON_FIELDS
         */
        if (numchars < 2 || data[numchars - 2] != '\r' || data[numchars - 1] != '\n') {
            asynPrint(pasynUser, ASYN_TRACE_ERROR, "Malformed request without EOM chars!\n");
            return asynError;
        }
        memcpy(pasyn->outbuf, data, numchars - 2); /* Strip off \r\n */
        pasyn->outbuf[numchars - 2] = 0;
        status = parseRequest(pasyn, pasynUser);
        break;
    case STATE_WBODY:
        /* Is this really the entire write?!? */
        curl_easy_setopt(pasyn->dev, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(pasyn->dev, CURLOPT_POSTFIELDSIZE, numchars);
        status = (curl_easy_perform(pasyn->dev) == CURLE_OK) ? asynSuccess : asynError;
        curl_easy_getinfo(pasyn->dev, CURLINFO_RESPONSE_CODE, &pasyn->curlrc);
        pasyn->state = STATE_RCODE;
        break;
    case STATE_RCODE:
    case STATE_RBODY:
        asynPrint(pasynUser, ASYN_TRACE_ERROR, "Trying to write in read state?\n");
        status = asynError;
        break;
    }
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "wrote %lu to %s, return %s.\n", (unsigned long)*nbytesTransfered,
                                               pasyn->portName,
                                               pasynManager->strStatus(status));
    return status;
}

static int printNumber(const rapidjson::Value *v, char *output)
{
    /* OK, this is kind of painful. */
    if (v->IsDouble())
        sprintf(output, "%.15lg", v->GetDouble());
    else if (v->IsInt())
        sprintf(output, "%d", v->GetInt());
    else if (v->IsUint())
        sprintf(output, "%u", v->GetUint());
    else if (v->IsInt64())
        sprintf(output, "%ld", v->GetInt64());
    else if (v->IsUint64())
        sprintf(output, "%lu", v->GetUint64());
    return strlen(output);
}

static int parseJSON(rapidjson::Document *d, char *name, char *output)
{
    char *s = name, *t = NULL;
    rapidjson::Value *v = d;
    do {
        t = index(s, '.');
        if (t)
            *t = 0;
        if (!v->HasMember(s))
            return -1;
        v = &(*v)[s];
        if (t)
            *t++ = '.';
        s = t;
    } while (s);
    switch (v->GetType()) {
    case rapidjson::kNullType:
        sprintf(output, "%s null\r\n", name);
        break;
    case rapidjson::kFalseType:
        sprintf(output, "%s false\r\n", name);
        break;
    case rapidjson::kTrueType:
        sprintf(output, "%s true\r\n", name);
        break;
    case rapidjson::kStringType:
        sprintf(output, "%s \"%s\"\r\n", name, v->GetString());
        break;
    case rapidjson::kNumberType:
        s = output;
        sprintf(output, "%s ", name);
        s += strlen(output);
        s += printNumber(v, s);
        *s++ = '\r';
        *s++ = '\n';
        *s++ = 0;
        break;
    case rapidjson::kArrayType:
        s = output;
        sprintf(output, "%s", name);
        s += strlen(output);
        for (rapidjson::Value::ConstValueIterator itr = v->Begin(); itr != v->End(); ++itr) {
            *s++ = ' ';
            s += printNumber(itr, s);
        }
        *s++ = '\r';
        *s++ = '\n';
        *s++ = 0;
        break;
    case rapidjson::kObjectType:
        /* We don't support returning objects! */
    default:
        return -1;
    }
    return strlen(output);
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
    *nbytesTransfered = 0;
    switch (pasyn->state) {
    case STATE_INIT:
    case STATE_WBODY:
        /* We just don't read anything here. */
        break;
    case STATE_RCODE:
        sprintf(data, "%03ld\r\n", pasyn->curlrc);
        *nbytesTransfered = 5;
        if (pasyn->req == REQ_GET || pasyn->fcnt)
            pasyn->state = STATE_RBODY;
        else
            pasyn->state = STATE_INIT;
        break;
    case STATE_RBODY:
        if (pasyn->incur == -1) { /* First time through the loop! */
            if (pasyn->fcnt) {    /* We're parsing JSON */
                if (pasyn->d)
                    delete pasyn->d;
                pasyn->d = new rapidjson::Document();
                pasyn->d->Parse(pasyn->rawbuf);
                if (pasyn->d->HasParseError()) {
                    asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                              "JSON Parse error: %s, location %d\n",
                              rapidjson::GetParseError_En(pasyn->d->GetParseError()),
                              (int)pasyn->d->GetErrorOffset());
                    status = asynError;
                    break;
                }
                if (!pasyn->d->IsObject()) {
                    /* If we don't have a JSON object, just pass it through and hope @mismatch
                       handles it. */
                    pasyn->fcnt = 0;
                    memcpy(pasyn->inbuf, pasyn->rawbuf, pasyn->incnt);
                    pasyn->inbuf[pasyn->incnt++] = '\r';
                    pasyn->inbuf[pasyn->incnt++] = '\n';
                    pasyn->incur = 0;
                } else
                    pasyn->incnt = 0;
            } else {               /* We're just passing the body up to streamdevice */
                if (pasyn->incnt == 0) { /* If we didn't read anything, we're done! */
                    pasyn->state = STATE_INIT;
                    break;
                } else
                    pasyn->incur = 0;
            }
        }
        if (!pasyn->incnt) {  /* Nothing in our buffer, get something! */
            pasyn->incnt = parseJSON(pasyn->d, pasyn->filters[pasyn->fcur], pasyn->inbuf);
            if (pasyn->incnt < 0) {
                asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                          "JSON does not contain %s\n",
                          pasyn->filters[pasyn->fcur]);
                status = asynError;
                pasyn->state = STATE_INIT;  /* Abort, abort!! */
                break;
            }
            pasyn->incur = 0;
        }
        if (maxchars >= (size_t)(pasyn->incnt - pasyn->incur)) { /* Full buffer! */
            memcpy(data, &pasyn->inbuf[pasyn->incur], pasyn->incnt - pasyn->incur);
            *nbytesTransfered = pasyn->incnt - pasyn->incur;
            pasyn->incnt = 0;
            if (++pasyn->fcur >= pasyn->fcnt)  /* Done!! */
                pasyn->state = STATE_INIT;
        } else { /* Partial buffer */
            memcpy(data, &pasyn->inbuf[pasyn->incur], maxchars);
            *nbytesTransfered = maxchars;
            pasyn->incur += maxchars;
        }
        asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, data, (int)*nbytesTransfered,
                    "read %d\n", (int)*nbytesTransfered);
        break;
    }
    if (gotEom)
        *gotEom = 0;
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

extern "C" {
static const struct asynOption asynOptionMethods = { setOption, getOption };
}

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
        if (pasyn->d) {
            delete pasyn->d;
            pasyn->d = NULL;
        }
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
                               const char *eos, int eoslen)
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
    int cnt;
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
    cnt = strlen(pasyn->baseURL);
    if (pasyn->baseURL[cnt - 1] == '/')
        pasyn->baseURL[cnt - 1] = 0;   /* Delete trailing '/'! */
    pasyn->dev = curl_easy_init();
    pasyn->headers = NULL;
    pasyn->headers = curl_slist_append(pasyn->headers, "Expect:");
    pasyn->headers = curl_slist_append(pasyn->headers, "Content-Type: application/json");
    pasyn->flags = STATE_INIT;
    pasyn->flags = 0;
    pasyn->d = NULL;

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
extern "C" {
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
}
