Import('env')
import os
import shutil

privateCredentials =  "src/credentials.private.h"
defaultCredentials =  "src/credentials.default.h"
targetCredentials = "src/credentials.h"

warningHeader = "// !!!!! This file is generated, do not edit !!!!!"

if os.path.isfile(privateCredentials):
    print ("*** use private credentials ***")
    with open(privateCredentials, "r") as f:
        text = f.read()
    text = warningHeader + "\n" + text
    with open(targetCredentials, "w") as f:
        f.write(text)
else: 
    print ("*** use default credentials ***")
    with open(defaultCredentials, "r") as f:
        text = f.read()
    text = warningHeader + "\n" + text
    with open(targetCredentials, "w") as f:
        f.write(text)
