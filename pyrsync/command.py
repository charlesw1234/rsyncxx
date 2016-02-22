from os.path import join as pathjoin, isfile
from zlib import Z_BEST_COMPRESSION
from .supply import normpath, sink, infile, gzinfile, compresser, source
from .supply import signature, delta

class cmdsignature(signature):
    def __init__(self, gzexpand, jsz, rflist, rootdir, nextobj):
        signature.__init__(self, jsz.block, jsz.nsigns, nextobj)
        self.gzexpand = gzexpand
        self.rflist = rflist
        self.rootdir = rootdir

    def pushfiles(self):
        for rfpath in self.rflist:
            fpath = pathjoin(self.rootdir, rfpath)
            if self.gzexpand and fpath.endswith('.gz'): iobj = gzinfile(fpath)
            else: iobj = infile(open(fpath, 'rb'))
            self.push_file(rfpath, iobj)

class cmddelta(delta):
    def __init__(self, gzexpand, jsz, rootdir, next):
        delta.__init__(self, jsz.buf, jsz.buf, next)
        self.gzexpand = gzexpand
        self.rootdir = rootdir

    def make_in(self, rfpath):
        fpath = normpath(pathjoin(self.rootdir, rfpath))
        if not isfile(fpath): return None
        if self.gzexpand and rfpath.endswith('.gz'): return gzinfile(fpath)
        return infile(open(fpath, 'rb'))

class cmdpatch(object):
    def __init__(self, gzexpand, jsz, newpkgobj, oldpkgobj,
                 newrootdir, oldrootdir):
        self.gzexpand = gzexpand
        self.jsz = jsz
        self.newpkgobj = newpkgobj
        self.oldpkgobj = oldpkgobj
        self.newrootdir = newrootdir
        self.oldrootdir = oldrootdir

    def gen(self):
        # connect nodes.
        sinkobj = sink(self.jsz.pystr)
        cobj = compresser(self.jsz.buf, Z_BEST_COMPRESSION, sinkobj)
        dobj = cmddelta(self.gzexpand, self.jsz, self.newrootdir, cobj)
        oldsign = cmdsignature(self.gzexpand, self.jsz,
                               self.oldpkgobj.rflist, self.oldrootdir, dobj)
        # then pushfiles into signature.
        oldsign.pushfiles()
        # push other not exist files.
        for rfpath in set(self.newpkgobj.rflist) - set(self.oldpkgobj.rflist):
            oldsign.push_file(rfpath, source(''))
        oldsign.push_end()

        oldsign.run(self.jsz.buf)
        return sinkobj.get_strlist()
