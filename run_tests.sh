#!/bin/bash
set -u
CXX=${CXX:-g++-13}
FLAGS=${FLAGS:--O2 -std=c++17}
PASS=0; FAIL=0
for d in data/one data/two data/three data/four data/five data/six; do
  name=$(basename $d)
  bin=build/test_$name
  if ! $CXX $FLAGS -I src -o $bin $d/code.cpp 2> build/compile_$name.log; then
    echo "[$name] COMPILE ERROR"
    cat build/compile_$name.log | head -20
    FAIL=$((FAIL+1))
    continue
  fi
  timeout 60 $bin > build/out_$name.txt 2>&1
  rc=$?
  if [ $rc -ne 0 ]; then
    echo "[$name] RUNTIME ERROR (rc=$rc)"
    FAIL=$((FAIL+1))
    continue
  fi
  if diff -w build/out_$name.txt $d/answer.txt > build/diff_$name.log 2>&1; then
    echo "[$name] PASS"
    PASS=$((PASS+1))
  else
    echo "[$name] WRONG ANSWER"
    head -5 build/diff_$name.log
    FAIL=$((FAIL+1))
  fi
done
echo "PASS=$PASS FAIL=$FAIL"
