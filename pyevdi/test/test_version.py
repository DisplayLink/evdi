# Copyright (c) 2022 DisplayLink (UK) Ltd.
import PyEvdi

VERSION_MAJOR = 1
VERSION_MINOR = 13

def testVersion():
    assert PyEvdi.version[:-1] == (VERSION_MAJOR, VERSION_MINOR)
