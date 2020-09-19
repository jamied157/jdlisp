# JDLisp
This is my own code from completing [Build your own lisp](http://www.buildyourownlisp.com/), it contains the implementation of the code from the book as well as a few additions I added when completing the exercises in the book. This involves a more expansive set of builtin functions to the language, a decimal type that supports floating point calculations and a separate boolean type. These types support operations between themselves and the base number type.

## Compilation
To compile on Mac and Linux, run 
```
cc -std=c99 -Wall parsing.c mpc.c -ledit -lm -o parsing
```
On Windows
```
cc -std=c99 -Wall parsing.c mpc.c -o parsing

```

## Example usage

Run `./jdlisp` to start the interactive prompt. Some examples of things you can run (taken from the standard library):
```
; Fibonacci
(fun {fib n} {
  select
    { (== n 0) 0 }
    { (== n 1) 1 }
    { otherwise (+ (fib (- n 1)) (fib (- n 2))) }
})


; Insert new atom to the left of first occurance of old in lat
(fun {insertL new old lat} {
        if (== lat {})
                {{}}
                {if (== (eval (head lat)) old)
                        {cons new lat}
                        {cons (eval (head lat)) (insertL new old (tail lat))}
                }
})
```

Look through the standard library stdlib.jdl for more examples. This library is loaded in every time the interactive prompt is run.


The MPC library is taken from https://github.com/orangeduck/mpc
