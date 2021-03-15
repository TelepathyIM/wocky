#!/bin/env python

import sys
import re

rex = re.compile('.*_wocky_signals_marshal_([A-Z0-9]*__[A-Z0-9_]*).*')

src = sys.argv[1:-1]
out = sys.argv[-1]

proto = set()

for fn in src:
  f = open(fn)
  for line in f:
    for m in rex.finditer(line):
      proto.add(m.group(1))
  f.close()

with open(out,'w') as f:
  for call in proto:
    f.write(call.replace('__',':').replace('_',',') + "\n")

