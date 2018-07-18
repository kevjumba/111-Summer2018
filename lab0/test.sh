#!/bin/sh
echo "Smoke Tests for Lab 0 started"
echo "Smoke Test 1: Read and Write from File"
echo "word1" > input.txt
./lab0 --input=input.txt --output=output.txt;
if [[ $? -ne 0 ]]
then 
    echo "Error: Unable to read and write from file. Exit code: $$?"
    exit 1
fi
diff -q input.txt output.txt > /dev/null;
if [[ $status ]] 
then 
    echo "Error: Text was successfully copied but input and output do not match."; \
    exit 1;
fi
rm -f input.txt output.txt;
echo "Smoke Test 2: Invalid Argument";
./lab0 --arg1 &> /dev/null
if [[ $? -ne 1 ]] 
then
    echo "Error: Invalid argument not caught.";
    exit 1;
fi 
echo "Smoke Test 3: Invalid file";
./lab0 --input=file &> /dev/null
if [[ $? -ne 2 ]]; then 
    echo "Error: Invalid file not caught";
    exit 1;
fi

echo "Smoke Test 4: No write access to output"; \
touch output.txt;
chmod -w output.txt;
 
echo "word2" | ./lab0 --output=output.txt &> /dev/null

if [[ $? -ne 3 ]];
then
    echo "Error: Did not catch invalid write access to output"
    exit 1;
fi
rm -f output.txt

echo "Smoke Test 5: No read access to file"
echo "input" > input.txt
chmod -r input.txt
./lab0 --input=input.txt > output.txt &>/dev/null
if [ $? -ne 2 ]; 
then 
	echo "Error: incorrect handling of un-openable file. Should exit with 2 instead of $$? ."
	exit 1
fi
rm -f input.txt output.txt ;

echo "Smoke Test 6: Catch handler";
./lab0 --segfault --catch &> /dev/null
if [[ $? -ne 4 ]];
then
    echo "Error: Segmentation fault correctly triggered but wasn't correctly caught by --catch flag.";
    exit 1;
fi

echo "All Smoke Tests Passed";
