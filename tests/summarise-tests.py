#!/usr/bin/python

import sys

import xml.dom.minidom as minidom

def process(testbinary):
    cases = 0
    passes = 0

    for e in testbinary.childNodes:
        if e.nodeType != e.ELEMENT_NODE or e.localName != 'testcase':
            continue

        cases = cases + 1

        status = e.getElementsByTagName('status')[0]

        if status.getAttribute('result') == 'success':
            passes = passes + 1

    return (passes, cases)

doc = minidom.parse(sys.argv[1])

okay = True

for e in doc.childNodes[0].childNodes:
    if e.nodeType != e.ELEMENT_NODE or e.localName != 'testbinary':
        continue

    name = e.getAttribute("path")

    passes, total = process(e)

    if passes == total:
        result = 'PASS'
    else:
        result = 'FAIL'
        okay = False

    print "%s: %s: %u/%u tests passed" % (result, name, passes, total)

if not okay:
    print "Disaster! Calamity!"
    sys.exit(1)
