from base64 import urlsafe_b64encode as b64encode
from hashlib import sha512
from os.path import join as pathjoin, dirname, relpath
from .supply import walkdirs

class package(object):
    def __init__(self, pkgname = None):
        self.pkgname = pkgname
        self.shobj = None
        self.chksum = None
        self.history = []
        self.rflist = []

    def prepare(self):
        self.shobj = sha512()

    def done(self):
        self.chksum = ''.join(b64encode(self.shobj.digest()).split('\n'))
        self.shobj = None

    def load(self, parts):
        self.chksum = parts[0]

    def append(self, rfpath, rootdir):
        self.rflist.append(rfpath)
        if self.shobj is None: return
        fpath = pathjoin(rootdir, rfpath)
        self.shobj.update(open(fpath, 'rb').read())

    def append_history(self, hchksum):
        self.history.append(hchksum)

    def clean_history(self):
        self.history = []

    def save(self, wfp):
        wfp.write('%s\n' % ','.join([self.chksum] + self.history))
        assert(self.pkgname is not None)
        wfp.write('    pn,%s\n' % self.pkgname)
        for version in self.history:
            wfp.write('    h,%s\n' % version)
        for rfpath in self.rflist:
            wfp.write('    rf,%s\n' % rfpath.replace('\\', '/'))

class manifest(dict):
    def __init__(self, jloc):
        self.jloc = jloc
        self.rootdir = jloc['rootdir']
        self.rpathes = jloc.get('rpathes', [''])
        self.manifest = jloc['manifest']
        self.history = jloc.get('history', [])
        self.pkgdict = {}

    def historymanifest(self, hdir):
        assert(hdir in self.history)
        jloc = {
            'rootdir': hdir,
            'manifest': None,
            'history': []
        }
        if self.jloc.has_key('rpathes'):
            jloc['rpathes'] = self.jloc['rpathes']
        return self.__class__(jloc)

    def gen(self):
        wdobj = walkdirs(self.rootdir, self.rpathes)
        fpath = wdobj.next()
        while fpath is not None:
            rfpath = relpath(fpath, self.rootdir)
            pkgname = self.pkgname(rfpath)
            if pkgname not in self.pkgdict:
                self.pkgdict[pkgname] = package(pkgname)
                self.pkgdict[pkgname].prepare()
            self.pkgdict[pkgname].append(rfpath, self.rootdir)
            fpath = wdobj.next()
        for pkgname, pkgobj in self.pkgdict.items():
            pkgobj.done()
            self[pkgobj.chksum] = pkgobj

    def load(self, rfp):
        pkgobj = None
        for ln in rfp.readlines():
            parts = ln.strip().split(',')
            if parts[0] == 'rf':
                assert(pkgobj.pkgname is not None)
                pkgobj.append(parts[1], self.rootdir)
            elif parts[0] == 'pn':
                assert(pkgobj.pkgname is None)
                pkgobj.pkgname = parts[1]
            elif parts[0] == 'h':
                pkgobj.append_history(parts[1])
            else:
                pkgobj = package()
                pkgobj.load(parts)
                self[pkgobj.chksum] = pkgobj

    def save(self, wfp):
        for pkgobj in self.values():
            pkgobj.save(wfp)

    def find(self, chksum):
        for pkgobj in self.values():
            if chksum in pkgobj.history: return pkgobj.chksum
        return None

    def pkgname(self, rfpath):
        # the default strategy of package: by directory.
        # the derived class can override it.
        return dirname(rfpath)

    def findpkgname(self, pkgname):
        for pkgobj in self.values():
            if pkgobj.pkgname == pkgname:
                return pkgobj

    def matchpkgs(self, newmanifest):
        for newchksum, newpkgobj in newmanifest.items():
            oldpkgobj = self.findpkgname(newpkgobj.pkgname)
            if oldpkgobj:
                # diff two pkgname
                assert(newchksum not in oldpkgobj.history)
                yield newpkgobj, oldpkgobj
