#!/bin/bash
# MISRA-C 2012 Static Analysis Script

echo "Running MISRA-C 2012 checks..."

# Run Cppcheck with MISRA addon
cppcheck \
    --enable=all \
    --addon=misra \
    --suppressions-list=scripts/misra-suppressions.txt \
    --xml \
    --xml-version=2 \
    -I include \
    src/ \
    2> reports/misra/cppcheck-result.xml

# Generate HTML report
cppcheck-htmlreport \
    --file=reports/misra/cppcheck-result.xml \
    --report-dir=reports/misra/html \
    --source-dir=.

echo "MISRA report generated: reports/misra/html/index.html"
