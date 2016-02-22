from json import loads as jloads
from cStringIO import StringIO
from gzip import GzipFile
from os.path import relpath
from .wsgiappbase import rsyncappbase
from .supply import walkdirs

class rsyncapp(rsyncappbase):
    def serve_rflist(self, body, loc, start_response):
        rpathes = jloads(body)
        bodyfobj = StringIO()
        gzfobj = GzipFile('srvrflist', 'wb', fileobj = bodyfobj)
        if rpathes == []: rpathes = loc.get('rpathes', None)
        wdobj = walkdirs(loc['rootdir'], rpathes)
        fpath = wdobj.next()
        while fpath:
            gzfobj.write(relpath(fpath, loc['rootdir']) + '\n')
            fpath = wdobj.next()
        gzfobj.close()
        return self.octreply(bodyfobj.getvalue(), start_response)
