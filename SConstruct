# -*- python -*-
# see LICENSE.md for license.
from glob import glob
from os.path import join as pathjoin, abspath

if ARGUMENTS.get('debug', 0): ccflags = ['-g', '-Wall']
else: ccflags = ['-O3', '-Wall', '-flto', '-Ofast', '-fomit-frame-pointer']
env = Environment(CCFLAGS = ccflags,
                  CXXFLAGS = '-std=c++11',
                  CPPDEFINES = [('_FILE_OFFSET_BITS', 64)],
                  CPPPATH = [abspath('.'), abspath(pathjoin('..', 'rapidjson', 'include'))],
                  PROGSUFFIX = '.exe')
Export('env')
SConscript(pathjoin('rsyncxx', 'SConscript'))
SConscript(pathjoin('pyrsync', 'SConscript'))
