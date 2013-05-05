#! /bin/bash
python testsuite/breakdancer/engine_test.py > .generated_breakdancer_testsuite.h
exitval=$?
if [ $exitval -ne 0 ]
then
    rm .generated_breakdancer_testsuite.h
fi
exit $exitval
