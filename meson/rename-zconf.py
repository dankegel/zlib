#!/usr/bin/env python3
import os
import os.path
import sys

oldname = sys.argv[1]

if os.path.exists(oldname):
  newname = oldname + '.included'
  print("Renaming %s to '%s' because this file is included with zlib" % (oldname, newname))
  print("and because otherwise it would override the copy generated in the build directory.")
  os.rename(oldname, newname)
