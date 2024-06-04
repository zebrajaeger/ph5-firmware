Import("env")
import json
import datetime

env = DefaultEnvironment()

PACKAGE_FILE = "package.json"

with open(PACKAGE_FILE, "r") as package:
    pj = json.load(package)
    env.Append(CPPDEFINES={"APP_VERSION" : pj['version'], "APP_NAME" : pj['name'], "BUILD_TIME" : datetime.datetime.now().strftime("%Y-%m-%d")})
