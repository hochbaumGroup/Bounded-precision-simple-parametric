# Linear Simple Parametric Minimum Cut using HPF

This repository provides an implementation of the **HPF algorithm** to compute the **minimum cut** for a linear simple parametric flow problem. Each source adjacent arc  and sink adjacent arc capacity is a linear function  ``a_i*lambda + b_i``, where ``a_i`` and ``b_i`` are the coefficients of the linear function of the ``i``-th arc, and ``lambda`` is the value of the parameter. The values of ``a_i`` are such that the capacities of source adjacent arcs are non decreasing, and the capacities of sink adjacent arcs are non increasing.

The problem is discretized over a user-specified precision. This means that the program accepts decimal numbers as input, but capacities are truncated to a fixed number of decimal places.

## Input Format

The executable reads input from standard input in the following modified DIMACS format:

```
c < Comment lines >
p < SEQUENCE | INTERVAL > < # vertices > < # arcs > < # decimal places >  [ < # parameters > < parameter1 > < parameter2 > ... < parameterk > ] [ < start interval > < end interval > < step interval> ]
n < source > s
n < sink > t
a < from-node > < to-node > < capacity >
a < source > < to-node > < a_i > < b_i > 
a < from-node > < sink > < a_i > < b_i > 
```

The user can provide a list of parameter as either:

* A sequence of ``p`` parameters, given as a sequence. To choose this option, provide the line ``p SEQUENCE < # vertices > < # arcs > < # decimal places >  < # parameters > < parameter1 > < parameter2 > ... < parameterk > `` in the input.
* A sequence of parameters, given as an interval and a step, to that the min cuts will be computed for values {start, start+step, start+2 * step, ... , end}. To choose this option, provide the line ``p INTERVAL < # vertices > < # arcs > < # decimal places >  [ < start interval > < end interval > < step interval> `` in the input.


## Compile

A makefile is provided. To compile into an executable just run the command

```
make
```


