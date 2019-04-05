import sys, pycurl, json, os, re
from psp import Pv

class MyBuf:
    def __init__(self):
        self.contents = ''
    def callback(self, buf):
        self.contents = self.contents + buf.replace('\r', '\n')
    def clear(self):
        self.contents = ''

def PvPut(pv, val):
    print("%s --> %s" % (pv, str(val)))
    Pv.put(pv, val)

if __name__ == '__main__':
    cfgfile = sys.argv[1]
    with open(sys.argv[1]) as f:
        lines = f.readlines()
    topas = []
    for l in lines:
        m = re.search('^TOPAS\(BASE=([^,]*),URL="([^"]*)"', l)
        if m:
            topas.append((len(topas), m.group(1), m.group(2)))
    c = pycurl.Curl()
    b = MyBuf()
    mlist = []
    ilist = []
    for (n, base, url) in topas:
        c.setopt(c.URL, url + "/Motors/AllProperties")
        c.setopt(c.WRITEFUNCTION, b.callback)
        b.clear()
        c.perform()
        d = json.loads(b.contents)
        for m in d['Motors']:
            PvPut("%s:m%d:SET_POSITION.DESC" % (base, m['Index']), m['Title'])
            PvPut("%s:m%d:SET_POSITION.EGU" % (base, m['Index']), m['UnitName'])
            PvPut("%s:m%d:SET_POSITION.LOPR" % (base, m['Index']), m['MinimalPositionInUnits'])
            PvPut("%s:m%d:SET_POSITION.HOPR" % (base, m['Index']), m['MaximalPositionInUnits'])
        c.setopt(c.URL, url + "/Optical/WavelengthControl/ExpandedInteractions")
        b.clear()
        c.perform()
        d = json.loads(b.contents)
        ct = ["ZR","ON","TW","TH","FR","FV","SX","SV","EI","NI","TE","EL","TV","TT","FT","FF"]
        for (i, id) in enumerate(d):
            PvPut("%s:i%d:NAME" % (base, i+1), id['Type'])
            PvPut("%s:i%d:MIN" % (base, i+1), id['OutputRange']['From'])
            PvPut("%s:i%d:MAX" % (base, i+1), id['OutputRange']['To'])
