Subset c:

variables, pointers, single dimension arrays, global variables, functions

technical: follows aapcs arm64, chaitin briggs reg allocation, pointer folding when dealing w const pointers, peephole optimizations, and more



TODO:

BIG FIX: need to add pointer liveness tracking to CFG gen. after making cfg, go through and do dirty bit tracking, writeback analysis, 