#!/usr/bin/env python

import sys

import xml.dom.minidom as minidom

def process(testbinary):
    cases = 0
    passes = 0
    failures = []

    for e in testbinary.childNodes:
        if e.nodeType != e.ELEMENT_NODE or e.localName != 'testcase':
            continue
        if e.hasAttribute('skipped'):
            continue
        path = e.getAttribute("path")

        cases = cases + 1

        status = e.getElementsByTagName('status')[0]

        if status.getAttribute('result') != 'success':
            failures += [ path ]

    return (cases, failures)

doc = minidom.parse(sys.argv[1])

okay = True

tests = {}
for e in doc.childNodes[0].childNodes:
    if e.nodeType != e.ELEMENT_NODE or e.localName != 'testbinary':
        continue
    path = e.getAttribute("path")

    cases, failures  = process(e)
    ocases, ofailures = tests.get (path, [ 0, []])
    tests[path] = [ ocases + cases, ofailures + failures ]

for name, [cases, failures] in tests.iteritems():
    if failures == []:
        result = 'PASS'
    else:
        result = 'FAIL'
        okay = False

    print "%s: %s: %u/%u tests passed" % (result, name, cases - len (failures), cases)
    for f in failures:
        print "\tFailure: %s" % f

if not okay:
    print "Disaster! Calamity!"
    sys.exit(1)
