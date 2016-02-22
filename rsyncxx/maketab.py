#!/usr/bin/python
from json import load as jload
from sys import argv
from jinja2 import Environment

env = Environment()
tgt = argv[1]
src = argv[2]
jdesc = jload(open(argv[3], 'rt'))
temp = env.from_string(open(src, 'rt').read())

prefix = jdesc['prefix']
kwargs = { 'prefix': prefix, 'kinds': [], 'cmds': [], 'graphstrs': [],
           'pkinds0': [], 'pkinds1': [], 'pkinds2': [] }
kinds = jdesc['graph'].keys()
kinds.sort()
kindvalue = 0
cmdvalue = 0
maxcmdparams = 0
entrance_cmd = None
for kind in kinds:
    kdict = { 'name': 'k%s_%s' % (prefix, kind),
              'value': kindvalue }
    kwargs['kinds'].append(kdict)
    kindvalue = kindvalue + 1
    if kind not in jdesc['params']:
        pkargs = { 'name': kind, 'cmd': cmdvalue }
        if kind == jdesc['entrance']: entrance_cmd = cmdvalue
        cdict = { 'value': cmdvalue,
                  'hexvalue': '%02X' % cmdvalue,
                  'kind': 'k%s_%s' % (prefix, kind),
                  'immed': 0,
                  'param1': 0,
                  'param2': 0 }
        kwargs['cmds'].append(cdict)
        kwargs['pkinds0'].append(pkargs)
        cmdvalue = cmdvalue + 1
    elif len(jdesc['params'][kind]) == 1:
        pkargs = { 'name': kind,
                   'start0': cmdvalue }
        for value in range(jdesc['params'][kind][0]):
            cdict = { 'value': cmdvalue,
                      'hexvalue': '%02X' % cmdvalue,
                      'kind': 'k%s_%s' % (prefix, kind),
                      'immed': value,
                      'param1': 0,
                      'param2': 0 }
            kwargs['cmds'].append(cdict)
            cmdvalue = cmdvalue + 1
        pkargs['start1'] = cmdvalue
        for nbytes in (1, 2, 4, 8):
            cdict = { 'value': cmdvalue,
                      'hexvalue': '%02X' % cmdvalue,
                      'kind': 'k%s_%s' % (prefix, kind),
                      'immed': 0,
                      'param1': nbytes,
                      'param2': 0 }
            kwargs['cmds'].append(cdict)
            cmdvalue = cmdvalue + 1
        kwargs['pkinds1'].append(pkargs)
        maxcmdparams = max(1, maxcmdparams)
    elif len(jdesc['params'][kind]) == 2:
        pkargs = { 'name': kind, 'start1': cmdvalue }
        for nbytes0 in (1, 2, 4, 8):
            for nbytes1 in (1, 2, 4, 8):
                cdict = { 'value': cmdvalue,
                          'hexvalue': '%02X' % cmdvalue,
                          'kind': 'k%s_%s' % (prefix, kind),
                          'immed': 0,
                          'param1': nbytes0,
                          'param2': nbytes1 }
                kwargs['cmds'].append(cdict)
                cmdvalue = cmdvalue + 1
        kwargs['pkinds2'].append(pkargs)
        maxcmdparams = max(2, maxcmdparams)
    gstr = ['.'] * len(kinds)
    for tgtkind in jdesc['graph'][kind]:
        gstr[kinds.index(tgtkind)] = 'X'
    kwargs['graphstrs'].append(''.join(gstr))
kwargs['maxcmd'] = cmdvalue
kwargs['maxcmdlen'] = 1 + maxcmdparams * 8
kwargs['entrance'] = entrance_cmd

open(tgt, 'wt').write(temp.render(**kwargs))
