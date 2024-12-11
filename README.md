# Linear Simple Parametric Minimum Cut using HPF

This repository provides an implementation of the **HPF algorithm** to compute the **minimum cut** for a linear simple parametric flow problem. The problem is discretized over a user-specified precision, which represents the number of decimal places used in each calculation.

## Input Format

The executable reads input from standard input in the following format:

```
c < Comment lines >
p < SEQUENCE | INTERVAL > < # vertices > < # arcs > < # decimal places >  [ < # parameters > < parameter1 > < parameter2 > ... < parameterk > ] [ < start interval > < end interval > < step interval> ]
n sourceID s
n sinkID t
a < from-node > < to-node > < capacity >
a < source > < to-node > < a_capacity > < b_capacity > 
a < from-node > < sink > < a_capacity > < b_capacity > 
```
Where the user can pick between providing a sequence of lambda values; or an interval, such that the min cuts will be computed for values {start, start+step, start+2*step, ... , end}

For each source adjacent and sink adjacent arc, the capacity is computed as ``a_capacity*lambda + b_capacity``. The values of ``a_capacity`` are such that the capacities of source adjacent arcs are non decreasing, and the capacities of sink adjacent arcs are non increasing.


## Compile

A makefile is provided. To compile into an executable just run the command

```
make
```


