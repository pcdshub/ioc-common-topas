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
        m = re.search('^TOPAS\(BASE=([^,]*),URL="([^"]*)"', l)
        if m:
            topas.append((len(topas), m.group(1), m.group(2)))
    c = pycurl.Curl()
    b = MyBuf()
    mlist = []
    for (n, base, url) in topas:
        c.setopt(c.URL, url + "/Motors/AllProperties")
        c.setopt(c.WRITEFUNCTION, b.callback)
        b.clear()
        c.perform()
        d = json.loads(b.contents)
        for m in d['Motors']:
            mlist.append('MOTOR(BASE=%s,PORT=%s,INDEX=%d,NAME="%s",EGU="%s",LOPR="%g",HOPR="%g")' %
                         (base, n, m['Index'], m['Title'], m['UnitName'],
                          m['MinimalPositionInUnits'], m['MaximalPositionInUnits']))
    with open(sys.argv[1], "w") as f:
        for l in lines:
            if l[:6] != 'MOTOR(':
                f.write("%s\n" % l.strip())
        for l in mlist:
            f.write("%s\n" % l)

