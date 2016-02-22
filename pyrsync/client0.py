from sys import stderr
from cStringIO import StringIO
from gzip import open as gzopen, GzipFile
from os import remove
from os.path import isfile, join as pathjoin, exists as pathexists
from zipfile import ZipFile
from .manifest import manifest
from .clientbase import rsyncbase

class rsclient(rsyncbase):
    def __init__(self, jsz, jloc, urlbase):
        rsyncbase.__init__(self, jsz, jloc, urlbase)
        self.manifest = jloc['manifest']
        self.jloc = jloc
        self.rflist = [] # the unproccessed rfpathes.

    def __call__(self):
        yield self.report.setTitle('cli.manifest')
        self.climfobj = manifest(self.jloc)
        if isfile(self.manifest):
            self.climfobj.load(gzopen(self.manifest, 'rb'))

        yield self.report.setTitle('srv.manifest')
        self.srvmfobj = manifest(self.jloc)
        bodyfobj = StringIO(self.urlget('manifest'))
        try:
            self.srvmfobj.load(GzipFile('srvmanifest', 'rb',
                                        fileobj = bodyfobj))
        except:
            bodyfobj.seek(0)
            raise Exception('Error: with response %s' % bodyfobj.read())

        self.report.setTitle('cli.traversal')
        genobj = self.cli_traversal()
        iterobj = genobj.next()
        while iterobj:
            yield iterobj
            iterobj = genobj.next()

        self.report.setTitle('srv.traversal')
        genobj = self.srv_traversal()
        iterobj = genobj.next()
        while iterobj:
            yield iterobj
            iterobj = genobj.next()

        if self.rflist:
            # remove all files which do not covered by any srvmfobj pacakge.
            yield self.report.setTitle('clean ')
            for rfpath in self.rflist: remove(pathjoin(self.rootdir, rfpath))
            yield self.report.incDels(len(self.rflist))

        self.report.setTitle('result')
        yield self.report.setStep(0, 0)
        yield None

    def cli_traversal(self):
        step = 0
        self.report.branch = None
        for chksum in self.climfobj.keys():
            yield self.report.setStep(step, len(self.climfobj))
            if chksum in self.srvmfobj:
                self.report.branch = 'match'
                # all files in the current packages are not changed.
                assert(self.climfobj[chksum].rflist ==
                       self.srvmfobj[chksum].rflist)
                self.report.incKeeps(len(self.climfobj[chksum].rflist))
                del(self.srvmfobj[chksum])
                step = step + 1
            else:
                chksum0 = self.srvmfobj.find(chksum)
                if chksum0 is None:
                    self.report.branch = 'discard'
                    # orphan package in client, discard it.
                    # remember the rflist, remove them if required.
                    self.rflist.extend(self.climfobj[chksum].rflist)
                    del(self.climfobj[chksum])
                else:
                    self.report.branch = 'patch'
                    # cached patch found, use it.
                    patchbody = self.urlpost('patch', chksum)
                    rflist = self.climfobj[chksum].rflist
                    self.patchflist(patchbody, rflist)
                    del(self.climfobj[chksum])
                    self.climfobj[chksum0] = self.srvmfobj.pop(chksum0)
                    self.climfobj[chksum0].clean_history()
                    step = step + 1
                self.climfobj.save(gzopen(self.manifest, 'wb'))
        yield self.report.setStep(step, len(self.climfobj))
        self.report.branch = None
        yield None

    def srv_traversal(self):
        maxstep = len(self.srvmfobj)
        #self.srvmfobj.save(open('xxx.mf', 'wt'))
        self.report.branch = None
        for chksum in self.srvmfobj.keys():
            yield self.report.setStep(maxstep - len(self.srvmfobj), maxstep)
            rflist = self.srvmfobj[chksum].rflist
            self.signprepare()
            self.signobj.push_rflist(rflist)
            self.signobj.push_end()
            fpathes = self.signobj.fpathes
            if not self.signobj.fpathes: signbody = ''
            else:
                self.signobj.run(self.jsz.buf)
                signbody = self.signobj.sinkobj.get_string()
            self.signobj = None
            limit = self.jsz.signlimit * len(rflist) +\
                    sum(map(lambda rfpath: len(rfpath), rflist))
            if len(signbody) < limit:
                self.report.branch = 'zip'
                # new package or not worth to do cmps, use cached zip directly.
                # clean up firstly.
                rmcnt = 0
                for fpath in fpathes:
                    if not pathexists(fpath): continue
                    remove(fpath)
                    rmcnt = rmcnt + 1
                self.report.incDels(rmcnt)
                zipbody = self.urlpost('zip', chksum)
                self.applyzip(zipbody)
            else:
                self.report.branch = 'cmp'
                # no cache, do signature, patch.
                patchbody = self.urlpost('cmp', signbody)
                self.patchflist(patchbody)
            self.climfobj[chksum] = self.srvmfobj.pop(chksum)
            self.climfobj.save(gzopen(self.manifest, 'wb'))
        assert(self.srvmfobj == {})
        yield self.report.setStep(maxstep, maxstep)
        self.report.branch = None
        yield None

    def applyzip(self, zipbody):
        zipfobj = StringIO(zipbody)
        zipobj = ZipFile(zipfobj, 'r')
        rflist = zipobj.namelist()
        zipobj.extractall(self.rootdir)
        self.rflist = filter(lambda rfpath: rfpath not in rflist, self.rflist)
        zipobj.close()
        zipfobj.close()
        self.report.incNews(len(rflist))
        return rflist
