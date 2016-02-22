from os import SEEK_END
from os.path import join as pathjoin, isfile
from zlib import Z_BEST_COMPRESSION
from .supply import jsonattr, normpath
from .supply import source, sink, infile, gzinfile, compresser, delta

class srvdelta(delta):
    def __init__(self, gzexpand, jsz, rootdir, next):
        delta.__init__(self, jsz.buf, jsz.buf, next)
        self.gzexpand = gzexpand
        self.rootdir = rootdir

    def make_in(self, rfpath):
        fpath = normpath(pathjoin(self.rootdir, rfpath))
        if not isfile(fpath): return None
        if self.gzexpand and rfpath.endswith('.gz'): return gzinfile(fpath)
        return infile(open(fpath, 'rb'))

class rsyncappbase(object):
    def __init__(self, jconfig):
        self.jsz = jsonattr(jconfig['rsync.sz'])
        self.jlocs = jconfig['rsync.locs']

    def __call__(self, environ, start_response):
        if environ['REQUEST_METHOD'] == 'GET': body = None
        elif environ['REQUEST_METHOD'] == 'POST':
            try: bodysz = int(environ.get('CONTENT_LENGTH', 0))
            except ValueError: bodysz = 0
            body = environ['wsgi.input'].read(bodysz)
        pinfo = environ['PATH_INFO']
        for loc in self.jlocs:
            if not pinfo.startswith(loc['urlbase']): continue
            mname = 'serve_' + pinfo[len(loc['urlbase']) + 1:]
            member = getattr(self, mname, None)
            if member is None: break
            self.gzexpand = self.__class__.__name__ in loc.get('gzexpand', [])
            return member(body, loc, start_response)
        if pinfo == '/debug': return self.srvdbg(environ, start_response)
        text = 'NOT FOUND'
        start_response('404 NOT FOUND',
                       [('Content-type', 'text/plain'),
                        ('Content-length', str(len(text)))])
        return text

    def serve_cmp(self, signbody, loc, start_response):
        #open('sign.out', 'wb').write(signbody)
        sinkobj = sink(self.jsz.pystr)
        cobj = compresser(self.jsz.buf, Z_BEST_COMPRESSION, sinkobj)
        dobj = srvdelta(self.gzexpand, self.jsz, loc['rootdir'], cobj)
        srcobj = source(signbody, dobj)
        srcobj.run(self.jsz.buf)
        return self.sinkreply(sinkobj, start_response)

    def srvdbg(self, environ, start_response):
        lines = map(lambda item: '[%s]: [%s]\n' % (item[0], item[1]),
                    environ.items())
        return self.strreply(''.join(lines), start_response)

    def strreply(self, body, start_response):
        start_response('200 OK',
                       [('Content-type', 'text/plain'),
                        ('Content-length', str(len(body)))])
        return body

    def octreply(self, body, start_response):
        start_response('200 OK',
                       [('Content-type', 'application/octet-stream'),
                        ('Content-length', str(len(body)))])
        return body

    def sinkreply(self, sinkobj, start_response):
        start_response('200 OK',
                       [('Content-type', 'application/octet-stream'),
                        ('Content-length', str(sinkobj.get_len()))])
        return sinkobj.get_strlist()

    def filereply(self, fpath, start_response):
        if not isfile(fpath):
            text = '%s IS NOT FOUND' % fpath
            start_response('404 NOT FOUND',
                       [('Content-type', 'text/plain'),
                        ('Content-length', str(len(text)))])
            return text
        fobj = open(fpath, 'rb')
        fobj.seek(0, SEEK_END)
        start_response('200 OK',
                       [('Content-type', 'application/octet-stream'),
                        ('Content-length', str(fobj.tell()))])
        fobj.seek(0)
        return fobj
