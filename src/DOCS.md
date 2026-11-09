Subset c:

variables, pointers, single dimension arrays, global variables, functions

technical: follows aapcs arm64, chaitin briggs reg allocation, pointer folding when dealing w const pointers, peephole optimizations, and more



TODO:

BIG FIX: need to add pointer liveness tracking to CFG gen. 
	1. run through all quads before CFG gen and mark any vregs that are referenced
	2. assign every referenced variable a stack slot
	3. at any definition of a referenced variable immediately write to stack slot
	4. at any read load first
	5. then, you can run redundant load/store elimination passes
	6. after all of this, if you can prove a reference wont escape a function(its local) you can go back to treating it as a normal vreg
	7. IF YOU REALLY WANNA, add strict type aliasing