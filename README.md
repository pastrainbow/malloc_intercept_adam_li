# Task 1, Malloc Debug - Adam Li

The library is built using `gcc`.

## Build the library

Run the following command in the project directory:


```bash
gcc -shared -fPIC malloc_debug.c -o malloc_debug.so -ldl
```
The fPIC flag allows the code to be loaded into different memory locations, thus allowing the library to be shared.
The ldl flag links the dynamic linking loader library, giving us dlsym(), which
is necessary for the malloc interception.

## Use the library
Like the task description, to intercept malloc while running the seq program:

```bash
LD_PRELOAD=./malloc_debug.so seq 1 5
```