# Linear Simple Parametric Minimum Cut using HPF

This repository provides an implementation of the **HPF algorithm** to compute the **minimum cut** for a linear simple parametric flow problem. The problem is discretized over a user-specified precision.

## Input Format

The executable reads input from standard input in the following format:

```
c this is a comment
p max numNodes numArcs precision initParam endParam stepParam
n sourceID s
n sinkID t
a from_1 to_1 a_1 b_1
...
a from_m to_m a_m b_m
```

## Compile

A makefile is provided. To compile into an executable just run the command

```
make
```


