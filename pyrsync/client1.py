from json import dumps as jdumps
from cStringIO import StringIO
from gzip import GzipFile
from os import remove
from os.path import join as pathjoin, relpath
from .clientbase import rsyncbase
from .supply import normpath, walkdirs

class rsclient(rsyncbase):
    def __init__(self, jsz, jloc, urlbase):
        rsyncbase.__init__(self, jsz, jloc, urlbase)
        self.batch = jloc.get('batch', jsz.batch)
        self.rpathes = jloc.get('rpathes', None)
        self.excrpathes = jloc.get('excrpathes', [])

    def __call__(self):
        self.signprepare()
        yield self.report.setTitle('srv.rflist')
        bodyfobj = StringIO(self.urlpost('rflist', jdumps(self.rpathes)))
        self.srvrflist = []
        for ln in GzipFile('srvrflist', 'rb', fileobj = bodyfobj).readlines():
            self.srvrflist.append(ln.strip())
        self.srvrflist = [ normpath(path) for path in self.srvrflist ]

        self.report.setTitle('traversal')
        self.maxstep = len(self.srvrflist)
        wdobj = walkdirs(self.rootdir, self.rpathes, self.excrpathes)
        fpath = wdobj.next()
        while fpath is not None:
            rfpath = relpath(fpath, self.rootdir)
            while self.srvrflist and rfpath > self.srvrflist[0]:
                if self.doSign(self.srvrflist.pop(0)): yield self.report
            if not self.srvrflist: self.doDel(rfpath)
            elif rfpath < self.srvrflist[0]: self.doDel(rfpath)
            elif rfpath == self.srvrflist[0]:
                if self.doSign(self.srvrflist.pop(0)): yield self.report
            fpath = wdobj.next()
        while self.srvrflist:
            if self.doSign(self.srvrflist.pop(0)): yield self.report

        if self.signbatch(): yield self.report
        self.report.setTitle('result')
        yield self.report.setStep(0, 0)
        yield None

    def signprepare(self):
        rsyncbase.signprepare(self)

    def signbatch(self):
        if not self.signobj.fpathes: return False
        self.report.setStep(self.maxstep - len(self.srvrflist), self.maxstep)
        self.signwork()
        return True

    def signwork(self):
        self.signobj.push_end()
        self.signobj.run(self.jsz.buf)
        signbody = self.signobj.sinkobj.get_string()
        #open('sign.out', 'wb').write(signbody)
        patchbody = self.urlpost('cmp', signbody)
        self.patchflist(patchbody)
        self.signprepare()

    def doDel(self, rfpath):
        remove(pathjoin(self.rootdir, rfpath))
        self.report.incDels()

    def doSign(self, rfpath):
        self.signobj.push_rfpath(rfpath)
        if len(self.signobj.fpathes) >= self.batch:
            return self.signbatch()
        return False
