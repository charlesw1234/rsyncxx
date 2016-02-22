from json import load as jload
from os.path import dirname, join as pathjoin
from .wsgiapp0 import rsyncapp

basedir = dirname(dirname(__file__))
application = rsyncapp(jload(open(pathjoin(basedir, 'config.json'), 'rt')))
