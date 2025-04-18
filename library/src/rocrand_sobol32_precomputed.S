/*
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/*
Auto-generated file. Do not edit!
Generated by tools/AsmEmbedSymbol.cmake
*/

#if defined(_WIN32) || defined(__CYGWIN__)
    /* PE/COFF format */
    .section .rdata
#else
    /* ELF format */
    .section .note.GNU-stack,"",@progbits
    .type "rocrand_h_sobol32_direction_vectors",@object
    .section .rodata
#endif
    .globl "rocrand_h_sobol32_direction_vectors"
    .p2align 8
rocrand_h_sobol32_direction_vectors:
    .incbin "rocrand_sobol32_precomputed.bin"
#if !(defined(_WIN32) || defined(__CYGWIN__))
    .size rocrand_h_sobol32_direction_vectors, . - rocrand_h_sobol32_direction_vectors
#endif
#if defined(_WIN32) || defined(__CYGWIN__)
    /* dllexport */
    .section .drectve
	.ascii " /EXPORT:rocrand_h_sobol32_direction_vectors,DATA"
#endif
