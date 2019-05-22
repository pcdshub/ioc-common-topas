import sys, pycurl, json, os, re

class MyBuf:
    def __init__(self):
        self.contents = ''
    def callback(self, buf):
        self.contents = self.contents + buf.replace('\r', '\n')
    def clear(self):
        self.contents = ''

if __name__ == '__main__':
    cfgfile = sys.argv[1]
    with open(sys.argv[1]) as f:
        lines = f.readlines()
    topas = []
    for l in lines:
        m = re.search('^TOPAS\(BASE=([^,]*),HOST=([^,]*),DEVICE=([^,]*)', l)
        if m:
            topas.append((len(topas), m.group(1), m.group(2), m.group(3)))
    c = pycurl.Curl()
    b = MyBuf()
    mlist = []
    ilist = []
    slist = []
    plist = []
    for (n, base, host, device) in topas:
        url = "http://%s:8004/%s/v0/PublicAPI" % (host, device)
        c.setopt(c.URL, url + "/Motors/AllProperties")
        c.setopt(c.WRITEFUNCTION, b.callback)
        b.clear()
        c.perform()
        d = json.loads(b.contents)
        for m in d['Motors']:
            mlist.append('MOTOR(BASE=%s,PORT=%s,ID=%d,NAME="%s",EGU="%s",LOPR="%g",HOPR="%g")' %
                         (base, n, m['Index'], m['Title'], m['UnitName'],
                          m['MinimalPositionInUnits'], m['MaximalPositionInUnits']))
        c.setopt(c.URL, url + "/Optical/WavelengthControl/ExpandedInteractions")
        b.clear()
        c.perform()
        d = json.loads(b.contents)
        ct = ["ZR","ON","TW","TH","FR","FV","SX","SV","EI","NI","TE","EL","TV","TT","FT","FF"]
        for (i, id) in enumerate(d):
            ilist.append('INTERACTION(BASE=%s,ID=%d,NAME="%s",FN=%sST,FV=%sVL,MIN="%g",MAX="%g")' %
                         (base, i+1, id['Type'], ct[i+1], ct[i+1],
                          id['OutputRange']['From'], id['OutputRange']['To']))
        url = "http://%s:35300/V0/Diags" % host
        c.setopt(c.URL, url)
        c.setopt(c.WRITEFUNCTION, b.callback)
        b.clear()
        c.perform()
        d = json.loads(b.contents)
        for s in d:
            plist.append('POWERMETER(BASE=%s,ID=%s,HOST=%s,PORT=%s,NAME="%s",DEBUG=)' %
                         (base, s['Index'], host, s['PowerMeter']['RestPort'], s['Name']))
            slist.append('SPECTROMETER(BASE=%s,ID=%s,HOST=%s,PORT=%s,NAME="%s",DEBUG=)' %
                         (base, s['Index'], host, s['Spectrometer']['RestPort'], s['Name']))

    with open(sys.argv[1], "w") as f:
        for l in lines:
            if (l[:6] != 'MOTOR(' and l[:12] != 'INTERACTION(' and 
                l[:11] != 'POWERMETER(' and l[:13] != 'SPECTROMETER('):
                f.write("%s\n" % l.strip())
        for l in mlist:
            f.write("%s\n" % l)
        for l in ilist:
            f.write("%s\n" % l)
        for l in plist:
            f.write("%s\n" % l)
        for l in slist:
            f.write("%s\n" % l)
