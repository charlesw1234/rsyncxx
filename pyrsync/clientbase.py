from os import makedirs, remove, rename
from os.path import join as pathjoin, isdir, isfile, dirname
from time import time
from requests import Session
from .supply import infile, outfile, gzinfile, gzoutfile, uncompresser
from .supply import signature, patch, source, sink

class rsreport(object):
    def __init__(self, rootdir):
        self.rootdir = rootdir
        self.start = time()
        self.title = None
        self.step = 0
        self.maxstep = 0
        self.ndels = 0
        self.nkeeps = 0
        self.ncmps = 0
        self.nnews = 0
        self.branch = None

    def __str__(self):
        tmstr = self.gettm()
        sstr = '(%u/%u)' % (self.step, self.maxstep)
        ndict = { 'removes': self.ndels,
                  'unchanged': self.nkeeps,
                  'updated': self.ncmps,
                  'created': self.nnews }
        result = '%s: %s%s: %s' % (tmstr, self.title, sstr, repr(ndict))
        if self.branch is None: return result
        return '%s => %s' % (result, self.branch)

    def gettm(self):
        tmval = time() - self.start
        hour = int(tmval) / 3600
        minute = int(tmval) / 60 % 60
        second = tmval - hour * 3600 - minute * 60
        return '%02u:%05.2f' % (minute, second)

    def setTitle(self, title):
        self.title = title
        self.step = 0
        self.maxstep = 0
        return self

    def setStep(self, step, maxstep):
        self.step = step
        self.maxstep = maxstep
        return self

    def incDels(self, val = 1):
        self.ndels = self.ndels + val
        return self

    def incKeeps(self, val = 1):
        self.nkeeps = self.nkeeps + val
        return self

    def incCmps(self, val = 1):
        self.ncmps = self.ncmps + val
        return self

    def incNews(self, val = 1):
        self.nnews = self.nnews + val
        return self

class clisignature(signature):
    def __init__(self, gzexpand, jsz, rootdir):
        self.gzexpand = gzexpand
        self.fpathes = []
        self.sinkobj = sink(jsz.pystr)
        signature.__init__(self, jsz.block, jsz.nsigns, self.sinkobj)
        self.rootdir = rootdir

    def push_rfpath(self, rfpath):
        fpath = pathjoin(self.rootdir, rfpath)
        if not isfile(fpath): iobj = source('')
        else:
            if self.gzexpand and fpath.endswith('.gz'): iobj = gzinfile(fpath)
            else: iobj = infile(open(fpath, 'rb'))
        self.fpathes.append(fpath)
        self.push_file(rfpath, iobj)

    def push_rflist(self, rflist):
        for rfpath in rflist: self.push_rfpath(rfpath)

class clipatch(patch):
    def __init__(self, gzexpand, jsz, rootdir, report):
        patch.__init__(self, jsz.buf)
        self.gzexpand = gzexpand
        self.jsz = jsz
        self.rootdir = rootdir
        self.rflist = []
        self.report = report

    def unchanged(self, rfpath):
        fpath = pathjoin(self.rootdir, rfpath)
        if not isfile(fpath): open(fpath, 'wb').close()
        self.report.incKeeps()

    def make_in(self, rfpath):
        self.rflist.append(rfpath)
        fpath = pathjoin(self.rootdir, rfpath)
        if isfile(fpath):
            self.report.incCmps()
            if self.gzexpand and rfpath.endswith('.gz'):
                return gzinfile(fpath)
            return infile(open(fpath, 'rb'))
        dpath = dirname(fpath)
        if not isdir(dpath): makedirs(dpath)
        self.report.incNews()
        return source('')

    def make_out(self, rfpath):
        newfpath = pathjoin(self.rootdir, rfpath + '.new')
        if self.gzexpand and rfpath.endswith('.gz'):
            return gzoutfile(self.jsz.buf, newfpath)
        return outfile(open(newfpath, 'wb'))

class rsyncbase(object):
    def __init__(self, jsz, jloc, urlbase):
        self.gzexpand = self.__class__.__name__ in jloc.get('gzexpand', [])
        self.jsz = jsz
        self.rootdir = jloc['rootdir']
        self.rpathes = jloc.get('rpathes', None)
        self.report = rsreport(self.rootdir)
        self.urlbase = urlbase + jloc['urlbase']
        self.session = Session()

    def signprepare(self):
        self.signobj = clisignature(self.gzexpand, self.jsz, self.rootdir)

    def patchflist(self, patchbody, rflist = []):
        pobj = clipatch(self.gzexpand, self.jsz, self.rootdir, self.report)
        ucobj = uncompresser(self.jsz.buf, pobj)
        patchsrc = source(patchbody, ucobj)
        patchsrc.run(self.jsz.buf)
        rflist_out = pobj.rflist
        del pobj
        for rfpath in rflist_out:
            fpath = pathjoin(self.rootdir, rfpath)
            if not isfile(fpath + '.new'): continue
            if isfile(fpath): remove(fpath)
            rename(fpath + '.new', fpath)
        for rfpath in rflist:
            if rfpath in rflist_out: continue
            fpath = pathjoin(self.rootdir, rfpath)
            if not isfile(fpath): continue
            remove(fpath)
            self.report.incDels()

    # override by tester.
    def urlget(self, suburl):
        url = '%s/%s' % (self.urlbase, suburl)
        resp = self.session.get(url = url, stream = False)
        return resp.content

    def urlpost(self, suburl, body):
        url = '%s/%s' % (self.urlbase, suburl)
        resp = self.session.post(url = url, data = body, stream = False)
        return resp.content
