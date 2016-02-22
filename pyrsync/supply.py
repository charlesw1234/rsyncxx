from os import listdir
from os.path import join as pathjoin, normpath as _normpath
from os.path import isdir, isfile, relpath
from pylib.utils import jsonattr
from pyrsync._rsync import *

def normpath(fpath):
    return _normpath(fpath.replace('\\', '/'))

def walkdirs(rootdir, rpathes = None, excrpathes = []):
    if rpathes is None: dirs0 = [rootdir]
    else: dirs0 = map(lambda rpath: pathjoin(rootdir, rpath), rpathes)
    dirs0 = filter(lambda dirpath: isdir(dirpath), dirs0)
    # uniform on windows and linux to sort pathes
    dirs0 = map(lambda path: path.replace('\\', '/'), dirs0)
    dirs0.sort()
    # convert path sep as platform
    dirs0 = map(lambda path: _normpath(path), dirs0)
    while dirs0:
        dirs1 = []
        for dirpath in dirs0:
            flist = listdir(dirpath)
            flist.sort()
            for fname in flist:
                fpath = pathjoin(dirpath, fname)
                if relpath(fpath, rootdir) in excrpathes: continue
                elif isdir(fpath): dirs1.append(fpath)
                elif isfile(fpath): yield fpath
        dirs0 = dirs1
    yield None
