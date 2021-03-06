# multithreading-intepreter
Intepreter that executes SimpleScript programs with multithreading. University project [no.4] for Concurrent Programming (Fall 2017).

## Description
Execute programs written in a custom programming language called SimpleScript. Those programs will be run from command line and may have multiple arguments. The intepreter can run multiple programs at the same time using multithreading. Programs with same variable name will have same memory on the system, so the intepreter is responsible to manage common memory.

## SimpleScript syntax
![SimpleScript syntax](https://github.com/pnikitakis/multithreading-intepreter/blob/main/SimpleScript_syntax.png)

## Prerequisites
- C
- Make (sudo apt install make)
- GCC (sudo apt install gcc)

## How to run
1. On terminal run `make` to build the executable C program.  
2. Start the intepreter with `./hw4b`.
3. To run a program enter `run inputFile.txt [arg0] [arg1] [argN]`
4. Enter `list` to see every program's status and ID.
5. Enter `kill [ID]` to stop a program.
6. Enter `exit` to stop the intepreter.

## Input tests
1. `init.txt` is a dummy program to make sure everything is working. It test if variables are storing value correctly.
2. `sum.txt` is a program that takes as input **N** a number and return the sum of every number from 1 up to **N**.
3. `producer.txt` and `consumer.txt` are the the classic multithreading problem of producer/consumer, where the one side add(produce) items(+N values) on a common buffer(variable) and the other side decrease(consume) those items(values).

## Authors
- [Panagiotis Nikitakis](https://www.linkedin.com/in/panagiotis-nikitakis/)
- [Christos Axelos](https://linkedin.com/in/christos-axelos-748386149)

## Course website
[ECE321 Concurrent Programming](https://www.e-ce.uth.gr/studies/undergraduate/courses/ece321/?lang=en)  
Assigment instructions can be found [here](https://github.com/pnikitakis/multithreading-intepreter/blob/main/assigment_instructions_GR.pdf) **in Greek**.
