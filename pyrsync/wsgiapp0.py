from os.path import join as pathjoin
from .wsgiappbase import rsyncappbase

class rsyncapp(rsyncappbase):
    def __init__(self, jconfig):
        rsyncappbase.__init__(self, jconfig)
        self.zipdir = jconfig['cache.zip']
        self.patchdir = jconfig['cache.patch']

    def serve_manifest(self, body, loc, start_response):
        return self.filereply(loc['manifest'], start_response)

    def serve_zip(self, chksum, loc, start_response):
        return self.filereply(pathjoin(self.zipdir, chksum), start_response)

    def serve_patch(self, chksum, loc, start_response):
        return self.filereply(pathjoin(self.patchdir, chksum), start_response)
