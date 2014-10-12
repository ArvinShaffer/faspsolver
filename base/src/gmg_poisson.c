/*! \file gmg_poisson.c
 *
 *  \brief GMG method as an iterative solver for Poisson Problem
 */

#include <time.h>
#include <math.h>

#include "fasp.h"
#include "fasp_functs.h"

#include "gmg_util.inl"

/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/**
 * \fn INT fasp_poisson_gmg_1D (REAL *u, REAL *b, INT nx, INT maxlevel,
 *                              REAL rtol, const SHORT prtlvl)
 *
 * \brief Solve Ax=b of Poisson 1D equation by Geometric Multigrid Method
 *
 * \param u         Pointer to the vector of dofs
 * \param b         Pointer to the vector of right hand side
 * \param nx        Number of grids in x direction
 * \param maxlevel  Maximum levels of the multigrid
 * \param rtol      Relative tolerance to judge convergence
 * \param prtlvl    Print level for output
 *
 * \author Ziteng Wang
 * \date   06/07/2013
 */
INT fasp_poisson_gmg_1D (REAL *u,
                         REAL *b,
                         INT nx,
                         INT maxlevel,
                         REAL rtol,
					     const SHORT prtlvl)
{
    const REAL atol = 1.0E-15;
    const INT  max_itr_num = 100;
    
    REAL      *u0, *r0, *b0;
    REAL       norm_r, norm_r0, norm_r1, error, factor;
    INT        i, *level, count = 0;
    REAL       AMG_start, AMG_end;
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_gmg_1D ...... [Start]\n");
    printf("### DEBUG: nx=%d, maxlevel=%d\n", nx, maxlevel);
#endif

	if ( prtlvl > PRINT_NONE ) {
		fasp_gettime(&AMG_start);
		printf("Num of DOF's: %d\n", nx+1);
	}

    // set level
    level = (INT *)malloc((maxlevel+2)*sizeof(REAL));
    level[0] = 0; level[1] = nx+1;
    for (i = 1; i < maxlevel; i++) {
        level[i+1] = level[i]+(level[i]-level[i-1]+1)/2;
    }
	level[maxlevel+1] = level[maxlevel]+1;
    
    // set u0, b0, r0
    u0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    b0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
	r0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    
    fasp_array_set(level[maxlevel], u0, 0.0);
    fasp_array_set(level[maxlevel], b0, 0.0);
	fasp_array_set(level[maxlevel], r0, 0.0);
    
    fasp_array_cp(nx, u, u0);
    fasp_array_cp(nx, b, b0);
    
    // compute initial l2 norm of residue
    fasp_array_set(level[1], r0, 0.0);
    compute_r_1d(u0, b0, r0, 0, level);
    norm_r0 = computenorm(r0, level, 0);
	norm_r1 = norm_r0;
    if (norm_r0 < atol) goto FINISHED;

	if ( prtlvl > PRINT_SOME ){
		printf("-----------------------------------------------------------\n");
		printf("It Num |   ||r||/||b||   |     ||r||      |  Conv. Factor\n");
		printf("-----------------------------------------------------------\n");
	}

    // GMG solver of V-cycle
    while (count < max_itr_num) {
        count++;
        multigriditeration1d(u0, b0, level, 0, maxlevel);
        compute_r_1d(u0, b0, r0, 0, level);
        norm_r = computenorm(r0, level, 0);
		factor = norm_r/norm_r1;
        error = norm_r / norm_r0;
		norm_r1 = norm_r;
		if ( prtlvl > PRINT_SOME ){
			printf("%6d | %13.6e   | %13.6e  | %10.4f\n",count,error,norm_r,factor);
		}
        if (error < rtol || norm_r < atol) break;
    }
    
	if ( prtlvl > PRINT_NONE ){
		if (count >= max_itr_num) {
			printf("### WARNING: V-cycle failed to converge.\n");
		}
		else {
			printf("Num of V-cycle's: %d, Relative Residual = %e.\n", count, error);
		}
	}
    
    // Update u
	fasp_array_cp(level[1], u0, u);

    // print out CPU time if needed
    if ( prtlvl > PRINT_NONE ) {
        fasp_gettime(&AMG_end);
        print_cputime("GMG totally", AMG_end - AMG_start);
    }

#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_gmg_1D ...... [Finish]\n");
#endif

FINISHED:
    free(level);
    free(r0);
    free(u0);
    free(b0);
    
    return count;
}

/**
 * \fn INT fasp_poisson_gmg_2D (REAL *u, REAL *b, INT nx, INT ny,
 *                              INT maxlevel, REAL rtol, const SHORT prtlvl)
 *
 * \brief Solve Ax=b of Poisson 2D equation by Geometric Multigrid Method
 *
 * \param u         Pointer to the vector of dofs
 * \param b         Pointer to the vector of right hand side
 * \param nx        Number of grids in x direction
 * \param ny        Number of grids in y direction
 * \param maxlevel  Maximum levels of the multigrid
 * \param rtol      Relative tolerance to judge convergence
 * \param prtlvl    Print level for output
 *
 * \author Ziteng Wang
 * \date   06/07/2013
 */
INT fasp_poisson_gmg_2D (REAL *u,
                         REAL *b,
                         INT nx,
                         INT ny,
                         INT maxlevel,
                         REAL rtol,
                         const SHORT prtlvl)
{
    const REAL atol = 1.0E-15;
    const INT  max_itr_num = 100;

    REAL *u0,*r0,*b0;
    REAL norm_r,norm_r0,norm_r1, error, factor;
    INT i, k, count = 0, *nxk, *nyk, *level;
    REAL AMG_start, AMG_end;
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_gmg_2D ...... [Start]\n");
    printf("### DEBUG: nx=%d, ny=%d, maxlevel=%d\n", nx, ny, maxlevel);
#endif

	if ( prtlvl > PRINT_NONE ) {
		fasp_gettime(&AMG_start);
		printf("Num of DOF's: %d\n", (nx+1)*(ny+1));
	}
	
	// set nxk, nyk
    nxk = (INT *)malloc(maxlevel*sizeof(INT));
	nyk = (INT *)malloc(maxlevel*sizeof(INT));
	nxk[0] = nx+1; nyk[0] = ny+1;
    for(k=1;k<maxlevel;k++){
		nxk[k] = (int) (nxk[k-1]+1)/2;
		nyk[k] = (int) (nyk[k-1]+1)/2;
	}
    
    // set level
    level = (INT *)malloc((maxlevel+1)*sizeof(REAL));
    level[0] = 0; level[1] = (nx+1)*(ny+1);
    for (i = 1; i < maxlevel; i++) {
        level[i+1] = level[i]+(nx/pow(2.0,i)+1)*(ny/pow(2.0,i)+1);
    }
	level[maxlevel+1] = level[maxlevel]+1;
    
    // set u0, b0
    u0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    b0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
	r0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    fasp_array_set(level[maxlevel], u0, 0.0);
    fasp_array_set(level[maxlevel], b0, 0.0);
	fasp_array_set(level[maxlevel], r0, 0.0);
    fasp_array_cp(level[1], u, u0);
    fasp_array_cp(level[1], b, b0);
    
    // compute initial l2 norm of residue
    compute_r_2d(u0, b0, r0, 0, level, nxk, nyk);
    norm_r0 = computenorm(r0, level, 0);
	norm_r1 = norm_r0;
    if (norm_r0 < atol) goto FINISHED;

	if ( prtlvl > PRINT_SOME ){
		printf("-----------------------------------------------------------\n");
		printf("It Num |   ||r||/||b||   |     ||r||      |  Conv. Factor\n");
		printf("-----------------------------------------------------------\n");
	}
    
    // GMG solver of V-cycle
    while (count < max_itr_num) {
        count++;
        multigriditeration2d(u0, b0, level, 0, maxlevel, nxk, nyk);
        compute_r_2d(u0, b0, r0, 0, level, nxk, nyk);
        norm_r = computenorm(r0, level, 0);
        error = norm_r / norm_r0;
		factor = norm_r/norm_r1;
		norm_r1 = norm_r;
		if ( prtlvl > PRINT_SOME ){
			printf("%6d | %13.6e   | %13.6e  | %10.4f\n",count,error,norm_r,factor);
		}
        if (error < rtol || norm_r < atol) break;
    }
    
	if ( prtlvl > PRINT_NONE ){
		if (count >= max_itr_num) {
			printf("### WARNING: V-cycle failed to converge.\n");
		}
		else {
			printf("Num of V-cycle's: %d, Relative Residual = %e.\n", count, error);
		}
	}
    
	// update u
	fasp_array_cp(level[1], u0, u);
   
    // print out CPU time if needed
    if ( prtlvl > PRINT_NONE ) {
        fasp_gettime(&AMG_end);
        print_cputime("GMG totally", AMG_end - AMG_start);
    }

#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_gmg_2D ...... [Finish]\n");
#endif

FINISHED:
    free(level);
    free(nxk);
    free(nyk);
    free(r0);
    free(u0);
    free(b0);
    
    return count;
}

/**
 * \fn INT fasp_poisson_gmg_3D (REAL *u, REAL *b, INT nx, INT ny, INT nz,
 *                              INT maxlevel, REAL rtol, const SHORT prtlvl)
 *
 * \brief Solve Ax=b of Poisson 3D equation by Geometric Multigrid Method
 *
 * \param u         Pointer to the vector of dofs
 * \param b         Pointer to the vector of right hand side
 * \param nx        Number of grids in x direction
 * \param ny        Number of grids in y direction
 * \param nz        Number of grids in z direction
 * \param maxlevel  Maximum levels of the multigrid
 * \param rtol      Relative tolerance to judge convergence
 * \param prtlvl    Print level for output
 *
 * \author Ziteng Wang
 * \date   06/07/2013
 */
INT fasp_poisson_gmg_3D (REAL *u,
                         REAL *b,
                         INT nx,
                         INT ny,
                         INT nz,
                         INT maxlevel,
                         REAL rtol,
					     const SHORT prtlvl)
{
    const REAL atol = 1.0E-15;
    const INT  max_itr_num = 100;

    REAL *u0,*r0,*b0;
    REAL norm_r,norm_r0,norm_r1, error, factor;
    INT i, k, count = 0, *nxk, *nyk, *nzk, *level;
	REAL AMG_start, AMG_end;
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_gmg_3D ...... [Start]\n");
    printf("### DEBUG: nx=%d, ny=%d, nz=%d, maxlevel=%d\n", 
			nx, ny, nz, maxlevel);
#endif

	if ( prtlvl > PRINT_NONE ) {
		fasp_gettime(&AMG_start);
		printf("Num of DOF's: %d\n", (nx+1)*(ny+1)*(nz+1));
	}

    // set nxk, nyk, nzk
    nxk = (INT *)malloc(maxlevel*sizeof(INT));
    nyk = (INT *)malloc(maxlevel*sizeof(INT));
    nzk = (INT *)malloc(maxlevel*sizeof(INT));
	nxk[0] = nx+1; nyk[0] = ny+1; nzk[0] = nz+1;
    for(k=1;k<maxlevel;k++){
		nxk[k] = (int) (nxk[k-1]+1)/2;
		nyk[k] = (int) (nyk[k-1]+1)/2;
        nzk[k] = (int) (nyk[k-1]+1)/2;
	}
    
    // set level
    level = (INT *)malloc((maxlevel+2)*sizeof(REAL));
    level[0] = 0; level[1] = (nx+1)*(ny+1)*(nz+1);
    for (i = 1; i < maxlevel; i++) {
        level[i+1] = level[i]+(nx/pow(2.0,i)+1)*(ny/pow(2.0,i)+1)*(nz/pow(2.0,i)+1);
    }
	level[maxlevel+1] = level[maxlevel]+1;
    
    // set u0, b0, r0
    u0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    b0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
	r0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    fasp_array_set(level[maxlevel], u0, 0.0);
    fasp_array_set(level[maxlevel], b0, 0.0);
	fasp_array_set(level[maxlevel], r0, 0.0);
    fasp_array_cp(level[1], u, u0);
    fasp_array_cp(level[1], b, b0);
    
    // compute initial l2 norm of residue
    compute_r_3d(u0, b0, r0, 0, level, nxk, nyk, nzk);
    norm_r0 = computenorm(r0, level, 0);
	norm_r1 = norm_r0;
    if (norm_r0 < atol) goto FINISHED;

	if ( prtlvl > PRINT_SOME ){
		printf("-----------------------------------------------------------\n");
		printf("It Num |   ||r||/||b||   |     ||r||      |  Conv. Factor\n");
		printf("-----------------------------------------------------------\n");
	}

    // GMG solver of V-cycle
    while (count < max_itr_num) {
        count++;
        multigriditeration3d(u0, b0, level, 0, maxlevel, nxk, nyk, nzk);
        compute_r_3d(u0, b0, r0, 0, level, nxk, nyk, nzk);
        norm_r = computenorm(r0, level, 0);
        factor = norm_r/norm_r1;
        error = norm_r / norm_r0;
		norm_r1 = norm_r;
		if ( prtlvl > PRINT_SOME ){
			printf("%6d | %13.6e   | %13.6e  | %10.4f\n",count,error,norm_r,factor);
		}
        if (error < rtol || norm_r < atol) break;
    }
    
	if ( prtlvl > PRINT_NONE ){
		if (count >= max_itr_num) {
			printf("### WARNING: V-cycle failed to converge.\n");
		}
		else {
			printf("Num of V-cycle's: %d, Relative Residual = %e.\n", count, error);
		}
	}
    
	// update u
	fasp_array_cp(level[1], u0, u);
    
	// print out CPU time if needed
    if ( prtlvl > PRINT_NONE ) {
        fasp_gettime(&AMG_end);
        print_cputime("GMG totally", AMG_end - AMG_start);
    }

FINISHED:
    free(level);
    free(nxk);
    free(nyk);
    free(nzk);
    free(r0);
    free(u0);
    free(b0);
    
    return count;
}

/**
 * \fn void fasp_poisson_fgmg_1D (REAL *u, REAL *b, INT nx,
 *                                INT maxlevel, REAL rtol, const SHORT prtlvl)
 *
 * \brief Solve Ax=b of Poisson 1D equation by Geometric Multigrid Method
 *        (Full Multigrid)
 *
 * \param u         Pointer to the vector of dofs
 * \param b         Pointer to the vector of right hand side
 * \param nx        Number of grids in x direction
 * \param maxlevel  Maximum levels of the multigrid 
 * \param rtol      Relative tolerance to judge convergence
 * \param prtlvl    Print level for output
 *
 * \author Ziteng Wang
 * \date   06/07/2013
 */
void fasp_poisson_fgmg_1D (REAL *u,
                           REAL *b,
                           INT nx,
                           INT maxlevel,
                           REAL rtol,
						   const SHORT prtlvl)
{
    const REAL  atol = 1.0E-15;
    REAL       *u0,*r0,*b0;
    REAL        norm_r0, norm_r;
    int         i, *level;
	REAL        AMG_start, AMG_end;

#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_fgmg_1D ...... [Start]\n");
    printf("### DEBUG: nx=%d, maxlevel=%d\n", nx, maxlevel);
#endif
    
	if ( prtlvl > PRINT_NONE ) {
		fasp_gettime(&AMG_start);
		printf("Num of DOF's: %d\n", (nx+1));
	}

    // set level
    level = (INT *)malloc((maxlevel+2)*sizeof(REAL));
    level[0] = 0; level[1] = nx+1;
    for (i = 1; i < maxlevel; i++) {
        level[i+1] = level[i]+(level[i]-level[i-1]+1)/2;
    }
	level[maxlevel+1] = level[maxlevel]+1;
    
    // set u0, b0, r0
    u0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    b0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
	r0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    fasp_array_set(level[maxlevel], u0, 0.0);
    fasp_array_set(level[maxlevel], b0, 0.0);
	fasp_array_set(level[maxlevel], r0, 0.0);
    fasp_array_cp(nx, u, u0);
    fasp_array_cp(nx, b, b0);    
    
    // compute initial l2 norm of residue
    fasp_array_set(level[1], r0, 0.0);
    compute_r_1d(u0, b0, r0, 0, level);
    norm_r0 = computenorm(r0, level, 0);
    if (norm_r0 < atol) goto FINISHED;
    
    //  Full GMG solver 
    fullmultigrid_1d(u0, b0, level, maxlevel, nx);
    
    // update u
    fasp_array_cp(level[1], u0, u);

	// print out Relative Residual and CPU time if needed
    if ( prtlvl > PRINT_NONE ) {
        fasp_gettime(&AMG_end);
        print_cputime("FGMG totally", AMG_end - AMG_start);
		compute_r_1d(u0, b0, r0, 0, level);
		norm_r = computenorm(r0, level, 0);
		printf("Relative Residual = %e.\n", norm_r/norm_r0);
    }
    
FINISHED:
    free(level);
    free(r0);
    free(u0);
    free(b0);

#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_fgmg_1D ...... [Finish]\n");
#endif
    
    return;
}

/**
 * \fn void fasp_poisson_fgmg_2D (REAL *u, REAL *b, INT nx, INT ny,
 *                                INT maxlevel, REAL rtol, const SHORT prtlvl)
 *
 * \brief Solve Ax=b of Poisson 2D equation by Geometric Multigrid Method
 *        (Full Multigrid)
 *
 * \param u         Pointer to the vector of dofs
 * \param b         Pointer to the vector of right hand side
 * \param nx        Number of grids in x direction
 * \param ny        Number of grids in Y direction
 * \param maxlevel  Maximum levels of the multigrid 
 * \param rtol      Relative tolerance to judge convergence
 * \param prtlvl    Print level for output
 *
 * \author Ziteng Wang
 * \date   06/07/2013
 */
void fasp_poisson_fgmg_2D (REAL *u,
                           REAL *b,
                           INT nx,
                           INT ny,
                           INT maxlevel,
                           REAL rtol,
						   const SHORT prtlvl)
{
    const REAL atol = 1.0E-15;
    REAL *u0,*r0,*b0;
    REAL norm_r0, norm_r;
    INT i, k, *nxk, *nyk, *level;
	REAL       AMG_start, AMG_end;

#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_fgmg_2D ...... [Start]\n");
    printf("### DEBUG: nx=%d, ny=%d, maxlevel=%d\n", nx, ny, maxlevel);
#endif
    
	if ( prtlvl > PRINT_NONE ) {
		fasp_gettime(&AMG_start);
		printf("Num of DOF's: %d\n", (nx+1)*(ny+1));
	}

    // set nxk, nyk
    nxk = (INT *)malloc(maxlevel*sizeof(INT));
	nyk = (INT *)malloc(maxlevel*sizeof(INT));
    
	nxk[0] = nx+1; nyk[0] = ny+1;
    for(k=1;k<maxlevel;k++){
		nxk[k] = (int) (nxk[k-1]+1)/2;
		nyk[k] = (int) (nyk[k-1]+1)/2;
	}
    
    // set level
    level = (INT *)malloc((maxlevel+2)*sizeof(REAL));
    level[0] = 0; level[1] = (nx+1)*(ny+1);
    for (i = 1; i < maxlevel; i++) {
        level[i+1] = level[i]+(nx/pow(2.0,i)+1)*(ny/pow(2.0,i)+1);
    }
	level[maxlevel+1] = level[maxlevel] + 1;
    
    // set u0, b0, r0
    u0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    b0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
	r0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    fasp_array_set(level[maxlevel], u0, 0.0);
    fasp_array_set(level[maxlevel], b0, 0.0);
	fasp_array_set(level[maxlevel], r0, 0.0);
    fasp_array_cp(level[1], u, u0);
    fasp_array_cp(level[1], b, b0);    
    
    // compute initial l2 norm of residue
    fasp_array_set(level[1], r0, 0.0);
    compute_r_2d(u0, b0, r0, 0, level, nxk, nyk);
    norm_r0 = computenorm(r0, level, 0);
    if (norm_r0 < atol) goto FINISHED;
    
    // FMG solver
    fullmultigrid_2d(u0, b0, level, maxlevel, nxk, nyk);
    
	// update u
	fasp_array_cp(level[1], u0, u);

	// print out Relative Residual and CPU time if needed
    if ( prtlvl > PRINT_NONE ) {
        fasp_gettime(&AMG_end);
        print_cputime("FGMG totally", AMG_end - AMG_start);
		compute_r_2d(u0, b0, r0, 0, level, nxk, nyk);
		norm_r = computenorm(r0, level, 0);
		printf("Relative Residual = %e.\n", norm_r/norm_r0);
    }
    
FINISHED:
    free(level);
    free(nxk);
    free(nyk);
    free(r0);
    free(u0);
    free(b0);

#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_fgmg_2D ...... [Finish]\n");
#endif    
    
    return;
}

/**
 * \fn void fasp_poisson_fgmg_3D (REAL *u, REAL *b, INT nx, INT ny, INT nz,
 *                                INT maxlevel, REAL rtol, const SHORT prtlvl)
 *
 * \brief Solve Ax=b of Poisson 3D equation by Geometric Multigrid Method
 *        (Full Multigrid)
 *
 * \param u         Pointer to the vector of dofs
 * \param b         Pointer to the vector of right hand side
 * \param nx        Number of grids in x direction
 * \param ny        NUmber of grids in y direction
 * \param nz        NUmber of grids in z direction
 * \param maxlevel  Maximum levels of the multigrid 
 * \param rtol      Relative tolerance to judge convergence
 * \param prtlvl    Print level for output
 *
 * \author Ziteng Wang
 * \date   06/07/2013
 */
void fasp_poisson_fgmg_3D (REAL *u,
                           REAL *b,
                           INT nx,
                           INT ny,
                           INT nz,
                           INT maxlevel,
                           REAL rtol,
						   const SHORT prtlvl)
{
    const REAL  atol = 1.0E-15;
    REAL       *u0,*r0,*b0;
    REAL        norm_r0, norm_r;
    int         i, k, *nxk, *nyk, *nzk, *level;
	REAL        AMG_start, AMG_end;
   
#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_fgmg_3D ...... [Start]\n");
    printf("### DEBUG: nx=%d, ny=%d, nz=%d, maxlevel=%d\n", 
			nx, ny, nz, maxlevel);
#endif
    
	if ( prtlvl > PRINT_NONE ) {
		fasp_gettime(&AMG_start);
		printf("Num of DOF's: %d\n", (nx+1)*(ny+1)*(nz+1));
	}
    // set nxk, nyk, nzk
    nxk = (INT *)malloc(maxlevel*sizeof(INT));
    nyk = (INT *)malloc(maxlevel*sizeof(INT));
    nzk = (INT *)malloc(maxlevel*sizeof(INT));
    
	nxk[0] = nx+1; nyk[0] = ny+1; nzk[0] = nz+1;
    for(k=1;k<maxlevel;k++){
		nxk[k] = (int) (nxk[k-1]+1)/2;
		nyk[k] = (int) (nyk[k-1]+1)/2;
        nzk[k] = (int) (nyk[k-1]+1)/2;     
	}
    
    // set level
    level = (INT *)malloc((maxlevel+2)*sizeof(REAL));
    level[0] = 0; level[1] = (nx+1)*(ny+1)*(nz+1);
    for (i = 1; i < maxlevel; i++) {
        level[i+1] = level[i]+(nx/pow(2.0,i)+1)*(ny/pow(2.0,i)+1)*(nz/pow(2.0,i)+1);
    }
	level[maxlevel+1] = level[maxlevel]+1;
    
    // set u0, b0, r0
    u0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    b0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
	r0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    fasp_array_set(level[maxlevel], u0, 0.0);
    fasp_array_set(level[maxlevel], b0, 0.0);
	fasp_array_set(level[maxlevel], r0, 0.0);
    fasp_array_cp(level[1], u, u0);
    fasp_array_cp(level[1], b, b0);
    
    // compute initial l2 norm of residue
    compute_r_3d(u0, b0, r0, 0, level, nxk, nyk, nzk);
    norm_r0 = computenorm(r0, level, 0);
    if (norm_r0 < atol) goto FINISHED;
    
    // FMG
    fullmultigrid_3d(u0, b0, level, maxlevel, nxk, nyk, nzk);
    
	// update u
	fasp_array_cp(level[1], u0, u);

    if ( prtlvl > PRINT_NONE ) {
        fasp_gettime(&AMG_end);
        print_cputime("FGMG totally", AMG_end - AMG_start);
		compute_r_3d(u0, b0, r0, 0, level, nxk, nyk, nzk);
		norm_r = computenorm(r0, level, 0);
		printf("Relative Residual = %e.\n", norm_r/norm_r0);
    }
    
FINISHED:
    free(level);
    free(nxk);
    free(nyk);
    free(nzk);
    free(r0);
    free(u0);
    free(b0);

#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_fgmg_3D ...... [Finish]\n");
#endif    

    return;
}

/**
 * \fn INT fasp_poisson_pcg_gmg_1D (REAL *u, REAL *b, INT nx,
 *                                  INT maxlevel, REAL rtol, const SHORT prtlvl)
 *
 * \brief Solve Ax=b of Poisson 1D equation by Geometric Multigrid Method
 *        (GMG preconditioned Conjugate Gradient method)
 *
 * \param u         Pointer to the vector of dofs
 * \param b         Pointer to the vector of right hand side
 * \param nx        Number of grids in x direction
 * \param maxlevel  Maximum levels of the multigrid 
 * \param rtol      Relative tolerance to judge convergence
 * \param prtlvl    Print level for output
 *
 * \author Ziteng Wang
 * \date   06/07/2013
 */
INT fasp_poisson_pcg_gmg_1D (REAL *u,
                             REAL *b,
                             INT nx,
                             INT maxlevel,
                             REAL rtol,
							 const SHORT prtlvl)
{
    const REAL atol = 1.0E-15;
    const INT  max_itr_num = 100;

    REAL      *u0,*r0,*b0;
    REAL       norm_r0;
    int        i, *level, iter = 0;
	REAL       AMG_start, AMG_end;
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_pcg_gmg_1D ...... [Start]\n");
    printf("### DEBUG: nx=%d, maxlevel=%d\n", nx, maxlevel);
#endif
    
	if ( prtlvl > PRINT_NONE ) {
		fasp_gettime(&AMG_start);
		printf("Num of DOF's: %d\n", (nx+1));
	}
    // set level
    level = (INT *)malloc((maxlevel+2)*sizeof(REAL));
    level[0] = 0; level[1] = nx+1;
    for (i = 1; i < maxlevel; i++) {
        level[i+1] = level[i]+(level[i]-level[i-1]+1)/2;
    }
	level[maxlevel+1] = level[maxlevel]+1;
    
    // set u0, b0
    u0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    b0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
	r0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    fasp_array_set(level[maxlevel], u0, 0.0);
    fasp_array_set(level[maxlevel], b0, 0.0);
	fasp_array_set(level[maxlevel], r0, 0.0);
    fasp_array_cp(nx, u, u0);
    fasp_array_cp(nx, b, b0);    
    
    // compute initial l2 norm of residue
    fasp_array_set(level[1], r0, 0.0);
    compute_r_1d(u, b, r0, 0, level);
    norm_r0 = computenorm(r0, level, 0);
    if (norm_r0 < atol) goto FINISHED;

    // Preconditioned CG method
    iter = pcg_1d(u0, b0, level, maxlevel, nx, rtol, max_itr_num, prtlvl);
    
    // Update u
	fasp_array_cp(level[1], u0, u);

    // print out CPU time if needed
    if ( prtlvl > PRINT_NONE ) {
        fasp_gettime(&AMG_end);
        print_cputime("GMG_PCG totally", AMG_end - AMG_start);
    }
    
FINISHED:
    free(level);
    free(r0);
    free(u0);
    free(b0);

#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_pcg_gmg_1D ...... [Finish]\n");
#endif
    
    return iter;
}    

/**
 * \fn INT fasp_poisson_pcg_gmg_2D (REAL *u, REAL *b, INT nx, INT ny, 
 *                                  INT maxlevel, REAL rtol, const SHORT prtlvl)
 *
 * \brief Solve Ax=b of Poisson 2D equation by Geometric Multigrid Method
 *        (GMG preconditioned Conjugate Gradient method)
 *
 * \param u         Pointer to the vector of dofs
 * \param b         Pointer to the vector of right hand side
 * \param nx        Number of grids in x direction
 * \param ny        Number of grids in y direction
 * \param maxlevel  Maximum levels of the multigrid 
 * \param rtol      Relative tolerance to judge convergence
 * \param prtlvl    Print level for output
 *
 * \author Ziteng Wang
 * \date   06/07/2013
 */
INT fasp_poisson_pcg_gmg_2D (REAL *u,
                             REAL *b,
                             INT nx,
                             INT ny,
                             INT maxlevel,
                             REAL rtol,
							 const SHORT prtlvl)
{
    const REAL atol = 1.0E-15;
    const INT  max_itr_num = 100;

    REAL      *u0,*r0,*b0;
    REAL       norm_r0;
    int        i, k, *nxk, *nyk, *level, iter = 0;
	REAL       AMG_start, AMG_end;
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_pcg_gmg_2D ...... [Start]\n");
    printf("### DEBUG: nx=%d, ny=%d, maxlevel=%d\n", nx, ny, maxlevel);
#endif
    
	if ( prtlvl > PRINT_NONE ) {
		fasp_gettime(&AMG_start);
		printf("Num of DOF's: %d\n", (nx+1)*(ny+1));
	}
    // set nxk, nyk
    nxk = (INT *)malloc(maxlevel*sizeof(INT));
	nyk = (INT *)malloc(maxlevel*sizeof(INT));
    
	nxk[0] = nx+1; nyk[0] = ny+1; 
    for(k=1;k<maxlevel;k++){
		nxk[k] = (int) (nxk[k-1]+1)/2;
		nyk[k] = (int) (nyk[k-1]+1)/2;
	}
    
    // set level
    level = (INT *)malloc((maxlevel+2)*sizeof(REAL));
    level[0] = 0; level[1] = (nx+1)*(ny+1);
    for (i = 1; i < maxlevel; i++) {
        level[i+1] = level[i]+(nx/pow(2.0,i)+1)*(ny/pow(2.0,i)+1);
    }
	level[maxlevel+1] = level[maxlevel]+1;
    
    // set u0, b0, r0
    u0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    b0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
	r0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    fasp_array_set(level[maxlevel], u0, 0.0);
    fasp_array_set(level[maxlevel], b0, 0.0);
	fasp_array_set(level[maxlevel], r0, 0.0);
    fasp_array_cp(level[1], u, u0);
    fasp_array_cp(level[1], b, b0);    
    
    // compute initial l2 norm of residue
    fasp_array_set(level[1], r0, 0.0);
    compute_r_2d(u0, b0, r0, 0, level, nxk, nyk);
    norm_r0 = computenorm(r0, level, 0);
    if (norm_r0 < atol) goto FINISHED;
    
    // Preconditioned CG method
    iter = pcg_2d(u0, b0, level, maxlevel, nxk, 
				  nyk, rtol, max_itr_num, prtlvl);
    
	// update u
	fasp_array_cp(level[1], u0, u);

    // print out CPU time if needed
    if ( prtlvl > PRINT_NONE ) {
        fasp_gettime(&AMG_end);
        print_cputime("GMG_PCG totally", AMG_end - AMG_start);
    }
    
FINISHED:
    free(level);
    free(nxk);
    free(nyk);
    free(r0);
    free(u0);
    free(b0);

#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_pcg_gmg_1D ...... [Finish]\n");
#endif
    
    return iter;
}

/**
 * \fn INT fasp_poisson_pcg_gmg_3D (REAL *u, REAL *b, INT nx, INT ny, INT nz,
 *                                  INT maxlevel, REAL rtol, const SHORT prtlvl)
 *
 * \brief Solve Ax=b of Poisson 3D equation by Geometric Multigrid Method
 *        (GMG preconditioned Conjugate Gradient method)
 *
 * \param u         Pointer to the vector of dofs
 * \param b         Pointer to the vector of right hand side
 * \param nx        Number of grids in x direction
 * \param ny        Number of grids in y direction
 * \param nz        Number of grids in z direction
 * \param maxlevel  Maximum levels of the multigrid 
 * \param rtol      Relative tolerance to judge convergence
 * \param prtlvl    Print level for output
 *
 * \author Ziteng Wang
 * \date   06/07/2013
 */
INT fasp_poisson_pcg_gmg_3D (REAL *u,
                             REAL *b,
                             INT nx,
                             INT ny,
                             INT nz,
                             INT maxlevel,
                             REAL rtol,
							 const SHORT prtlvl)
{
    const REAL atol = 1.0E-15;
    const INT  max_itr_num = 100;

    REAL      *u0,*r0,*b0;
    REAL       norm_r0;
    int        i, k, *nxk, *nyk, *nzk, *level, iter = 0;
	REAL       AMG_start, AMG_end;

#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_pcg_gmg_2D ...... [Start]\n");
    printf("### DEBUG: nx=%d, ny=%d, nz=%d, maxlevel=%d\n", 
			nx, ny, nz, maxlevel);
#endif
    
	if ( prtlvl > PRINT_NONE ) {
		fasp_gettime(&AMG_start);
		printf("Num of DOF's: %d\n", (nx+1)*(ny+1)*(nz+1));
	}
    
    // set nxk, nyk, nzk
    nxk = (INT *)malloc(maxlevel*sizeof(INT));
    nyk = (INT *)malloc(maxlevel*sizeof(INT));
    nzk = (INT *)malloc(maxlevel*sizeof(INT));
    
	nxk[0] = nx+1; nyk[0] = ny+1; nzk[0] = nz+1;
    for(k=1;k<maxlevel;k++){
		nxk[k] = (int) (nxk[k-1]+1)/2;
		nyk[k] = (int) (nyk[k-1]+1)/2;
        nzk[k] = (int) (nyk[k-1]+1)/2;     
	}
    
    // set level
    level = (INT *)malloc((maxlevel+2)*sizeof(REAL));
    level[0] = 0; level[1] = (nx+1)*(ny+1)*(nz+1);
    for (i = 1; i < maxlevel; i++) {
        level[i+1] = level[i]+(nx/pow(2.0,i)+1)*(ny/pow(2.0,i)+1)*(nz/pow(2.0,i)+1);
    }
	level[maxlevel+1] = level[maxlevel]+1;
    
    // set u0, b0
    u0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    b0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
	r0 = (REAL *)malloc(level[maxlevel]*sizeof(REAL));
    fasp_array_set(level[maxlevel], u0, 0.0);
    fasp_array_set(level[maxlevel], b0, 0.0);
	fasp_array_set(level[maxlevel], r0, 0.0);
    fasp_array_cp(level[1], u, u0);
    fasp_array_cp(level[1], b, b0);
    
    // compute initial l2 norm of residue
    compute_r_3d(u0, b0, r0, 0, level, nxk, nyk, nzk);
    norm_r0 = computenorm(r0, level, 0);
    if (norm_r0 < atol) goto FINISHED;
    
    // Preconditioned CG method
    iter = pcg_3d(u0, b0, level, maxlevel, nxk, 
				  nyk, nzk, rtol, max_itr_num, prtlvl);
    
	// update u
	fasp_array_cp(level[1], u0, u);

    // print out CPU time if needed
    if ( prtlvl > PRINT_NONE ) {
        fasp_gettime(&AMG_end);
        print_cputime("GMG_PCG totally", AMG_end - AMG_start);
    }
    
FINISHED:
    free(level);
    free(nxk);
    free(nyk);
    free(nzk);
    free(r0);
    free(u0);
    free(b0);

#if DEBUG_MODE
    printf("### DEBUG: fasp_poisson_pcg_gmg_3D ...... [Finish]\n");
#endif
    
    return iter;
}

/*---------------------------------*/
/*--        End of File          --*/
/*---------------------------------*/
