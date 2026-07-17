# lam_test.py

import sys, os
sys.path.insert(0, os.path.normpath('..'))

from camlib import *

crate_number = 1
n, a = 3, 0
data = 0

status = COPEN()
if status != 0:
    print(f"ERROR: COPEN(): {os.strerror(status)}")
    sys.exit(-1)
CSETCR(crate_number)
CGENZ()

# enable LAM (F26)
f = 26
q, x, data, _ = CAMAC(n, a, f)
print("NAF:%d,%d,%d, data:%06x, q:%d, x:%d" % (n,a,f, data, q, x))

# clear LAM (F10)
f = 9
q,x,data,_ = CAMAC(n, a, f)
print("NAF:%d,%d,%d, data:%06x, q:%d, x:%d" % (n,a,f, data, q, x))


for i in range(16):
    sys.stdout.write("Waiting for a LAM ...");
    result = CWLAM(timeout=10)

    if result == 0:
        sys.stdout.write("OK.\n")
    else:
        sys.stdout.write("timed out: result=%d\n" % result)

    a, f = 0, 0
    q,x,data,_ = CAMAC(n, a, f)
    print("NAF:%d,%d,%d, data:%06x, q:%d, x:%d" % (n,a,f, data, q, x))

    # clear LAM (F10)
    f = 9;
    q,x,data,_ = CAMAC(n, a, f)
    print("NAF:%d,%d,%d, data:%06x, q:%d, x:%d" % (n,a,f, data, q, x))
    
CCLOSE()
