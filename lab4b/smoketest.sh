#!/bin/bash
#NAME: Kevin Zhang
#EMAIL: kevin.zhang.13499@gmail.com
#ID: 104939334
./lab4b --log=log.txt --period=3 --scale="C" <<-EOF
SCALE=F
PERIOD=1
START
STOP
OFF
EOF
if [ $? -ne 0 ]
then
	echo "ERROR: Program did not successfully run"
else
	echo "Program successfully runs with no errors."
	echo "Smoke test passed"
fi
rm -f log.txt
