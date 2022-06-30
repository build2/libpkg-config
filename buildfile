# file      : buildfile
# license   : ISC; see accompanying COPYING file

# Glue buildfile that "pulls" all the packages.

import pkgs = {*/ -upstream/}
./: $pkgs
