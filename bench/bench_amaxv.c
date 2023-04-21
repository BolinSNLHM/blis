/*

   BLIS
   An object-based framework for developing high-performance BLAS-like
   libraries.

   Copyright (C) 2021-2022, Advanced Micro Devices, Inc. All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    - Neither the name of The University of Texas nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "blis.h"

// Benchmark application to process aocl logs generated by BLIS library.
#ifndef DT
#define DT BLIS_DOUBLE
#endif

#define AOCL_MATRIX_INITIALISATION

//#define BLIS_ENABLE_CBLAS

/* For BLIS since logs are collected at BLAS interfaces
 * we disable cblas interfaces for this benchmark application
 */

/* #ifdef BLIS_ENABLE_CBLAS */
/* #define CBLAS */
/* #endif */

int main( int argc, char** argv )
{
    obj_t x, idx;
    dim_t p_inc = 0; // to keep track of number of inputs
    num_t dt, dt_idx;
    char  dt_ch;
    int   r, n_repeats;

    double   dtime;
    double   dtime_save;
    double   gflops;

    FILE* fin  = NULL;
    FILE* fout = NULL;

    n_repeats = N_REPEAT;  // This macro will get from Makefile.

    dt = DT;
    dt_idx = BLIS_INT;

    if (argc < 3)
    {
        printf("Usage: ./bench_amaxv_XX.x input.csv output.csv\n");
        exit(1);
    }
    fin = fopen(argv[1], "r");
    if (fin == NULL)
    {
        printf("Error opening the file %s\n", argv[1]);
        exit(1);
    }
    fout = fopen(argv[2], "w");
    if (fout == NULL)
    {
        printf("Error opening output file %s\n", argv[2]);
        exit(1);
    }

    fprintf(fout, "Dt\t n\t incx\t gflops\n");

    dim_t n;
    inc_t incx;
    char tmp[256]; // to store function name, line no present in logs.

    // {S,D,C,Z} {n incx}
    while (fscanf(fin, "%s %c " INT_FS INT_FS " \n",
        tmp, &dt_ch, &n, &incx) == 4)
    {

#ifdef PRINT
        fprintf (stdout, "Input = %s %c %ld %ld %6.3f\n",
                 tmp, dt_ch, n, incx, gflops);
#endif
        if (dt_ch == 'D' || dt_ch == 'd') dt = BLIS_DOUBLE;
        else if (dt_ch == 'Z' || dt_ch == 'z') dt = BLIS_DCOMPLEX;
        else if (dt_ch == 'S' || dt_ch == 's') dt = BLIS_FLOAT;
        else if (dt_ch == 'C' || dt_ch == 'c') dt = BLIS_SCOMPLEX;
        else
        {
            printf("Invalid data type %c\n", dt_ch);
            continue;
        }

        // Create objects with required sizes and strides.
        //
        // Given a vector of length n, i?amax returns the zero-based index index of the
        // element of vector x that contains the largest absolute value (or, in
        // the complex domain, the largest complex modulus).
        //
        //  where:
        //      n is lenght of vector
        //      x is an n-element vector, Array size at least (1+(n-1)*abs(incx)).

        bli_obj_create( dt, n, 1, incx, 1, &x );
        bli_obj_create( dt_idx, 1, 1, 0, 0, &idx );

#ifdef AOCL_MATRIX_INITIALISATION
        bli_randm( &x );
#endif

        dtime_save = DBL_MAX;

#ifdef PRINT
            bli_printm( "x", &x, "%4.1f", "" );
#endif

        inc_t max_index = -1;
        for ( r = 0; r < n_repeats; ++r )
        {
            dtime = bli_clock();

#ifdef BLIS
            bli_amaxv( &x,
                       &idx);
#else

            f77_int nn     = bli_obj_length( &x );
            f77_int incx   = bli_obj_vector_inc( &x );

            if ( bli_is_float( dt ) )
            {
                float*  xp     = bli_obj_buffer( &x );

#ifdef CBLAS
                max_index = cblas_isamax( nn,
                                          xp,
                                          incx );
#else
                max_index = isamax_( &nn,
                                     xp,
                                     &incx );
#endif
            }
            else if (bli_is_double( dt ) )
            {
                double*  xp    = bli_obj_buffer( &x );

#ifdef CBLAS
                max_index = cblas_idamax( nn,
                                          xp,
                                          incx );
#else
                max_index = idamax_( &nn,
                                     xp,
                                     &incx );
#endif
            }
            else if ( bli_is_scomplex( dt ) )
            {
                scomplex*  xp  = bli_obj_buffer( &x );

#ifdef CBLAS
                max_index = cblas_icamax( nn,
                                          xp,
                                          incx );
#else
                max_index = icamax_(  &nn,
                                      xp,
                                      &incx );
#endif
            }
            else if ( bli_is_dcomplex( dt ) )
            {
                dcomplex*  xp  = bli_obj_buffer( &x );

#ifdef CBLAS
                max_index = cblas_izamax( nn,
                                          xp,
                                          incx );
#else
                max_index = izamax_(  &nn,
                                      xp,
                                      &incx );
#endif
            }
#endif

            dtime_save = bli_clock_min_diff( dtime_save, dtime );
        }

        gflops = ( 1.0 * n ) / dtime_save / 1.0e9;
        if ( bli_obj_is_complex( &x ) ) gflops *= 2.0;

#ifdef PRINT
            printf( "data_iamaxv_%s", BLAS );
            printf( "[ %4lu %7.2f ], Max Index = %d;\n",
                ( unsigned long )n, gflops, max_index );
            exit(1);
#endif

        printf( "data_amaxv_%s", BLAS );

        p_inc++;
        printf("( %2lu, 1:4 ) = [ %4lu %7.2f ], Max Index = %ld;\n",
               (unsigned long)(p_inc),
               (unsigned long)n,
                gflops, max_index);

        fprintf (fout, "%s %c %ld %ld %ld %6.3f\n", tmp, dt_ch, n, incx, max_index, gflops);

        fflush(fout);

        bli_obj_free( &x );
        bli_obj_free( &idx );
    }

    //bli_finalize();
    fclose(fin);
    fclose(fout);

    return 0;
}
