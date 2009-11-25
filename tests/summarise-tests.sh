#!/bin/sh

FAILURES=0;
OIFS="$IFS"
IFS="$IFS<>"

function item_value ()
{
    NAME=$1;
    RAW=$2;
    IVALUE="";

    for ITEM in $RAW;
    do
        VAL=${ITEM#$NAME=};
        if [ x"$VAL" != x"$ITEM" ];
        then
            IVALUE=$VAL;
        fi;
    done;
}

function print_summary ()
{
    if [ $NTESTS -eq 0 ];
    then
        echo "PASS: $GROUP: 0/0 tests passed";
    else
        if [ $NTESTS -gt $PASSED ];
        then
            echo "FAIL: $GROUP : $FAILED/$NTESTS tests failed";
            echo "Warning: $GROUP has failed tests ($FAILED found):";
            for TEST in $FAILED_TESTS;
            do
                echo "$TEST";
            done;
        else
            echo "PASS: $GROUP : $PASSED/$NTESTS tests passed";
        fi;
    fi;
}

LAST_GROUP="*none*";

while read IGNORE TAG VALUES;
do
    VALUES=$(echo -n "$VALUES" | sed -e 's@[<>]\+@ @g; s@/ *$@@; s@"@@g');
    case $TAG in
        testbinary)
            item_value path "$VALUES";
            TNAME="";
            TRVAL="";
            NEW_GROUP=$IVALUE;

            if [ x"$LAST_GROUP" = x"*none*" ];
            then
                GROUP=$NEW_GROUP;
                NTESTS=0;
                PASSED=0;
                FAILED=0;
                FAILED_TESTS="";
            elif [ x"$LAST_GROUP" = x"$NEW_GROUP" ];
            then
                echo -n; # meaningless </testbinary><testbinary> boundary
            else
                print_summary;
                GROUP=$NEW_GROUP;
                NTESTS=0;
                PASSED=0;
                FAILED=0;
                FAILED_TESTS="";
            fi;
            ;;
        testcase)
            item_value path    "$VALUES"; TNAME=$IVALUE;
            item_value skipped "$VALUES"; SKIPPED=$IVALUE;
            if [ x$SKIPPED = x ]; then NTESTS=$((NTESTS + 1)); fi;
            ;;
        status)
            item_value result "$VALUES";
            TRVAL=$IVALUE;
            if [ x$TRVAL = xsuccess ];
            then 
                PASSED=$((PASSED + 1));
            else
                FAILED=$((FAILED + 1));
                FAILED_TESTS=$FAILED_TESTS${FAILED_TESTS:+ }$TRVAL:$TNAME;
            fi;
            TRVAL="";
            ;;
        /testbinary)
            FAILURES=$((FAILURES + FAILED))
            LAST_GROUP=$GROUP;
            ;;
    esac;
done

print_summary;

if [ $FAILURES -gt 0 ];
then
    echo "Test Failures!"
    exit 1;
fi;
