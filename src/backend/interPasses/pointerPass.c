// simple: decide whether derefs and refs are absolute or local
// only absolute references and dereferences are ones for pointers that point to global variables.
// a pointer can only be declared to be a global address if it is assigned a reference to a global variable
// a pointer passed in a function must be defined as absolute. even if the pointer was relative, any passing to a function requires it to be reduced to an absolute address before passing.
// possibly move some of this logic to 3ac gen, specifically the logic specific to arguments