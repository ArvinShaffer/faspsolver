/*! \file spcg.c
 *
 *  \brief Preconditioned conjugate gradient method with safe net
 *
 *  Abstract algorithm
 *
 *  PCG method to solve A*x=b is to generate {x_k} to approximate x
 *
 *  Step 0. Given A, b, x_0, M
 *
 *  Step 1. Compute residual r_0 = b-A*x_0 and convergence check;
 *
 *  Step 2. Initialization z_0 = M^{-1}*r_0, p_0=z_0;
 *
 *  Step 3. Main loop ...
 *
 *  FOR k = 0:MaxIt
 *      - get step size alpha = f(r_k,z_k,p_k);
 *      - update solution: x_{k+1} = x_k + alpha*p_k;
 *      - check whether x is NAN;
 *      - perform stagnation check;
 *      - update residual: r_{k+1} = r_k - alpha*(A*p_k);
 *      - if r_{k+1} < r_{best}: save x_{k+1} as x_{best};
 *      - perform residual check;
 *      - obtain p_{k+1} using {p_0, p_1, ... , p_k};
 *      - prepare for next iteration;
 *      - print the result of k-th iteration;
 *  END FOR
 *
 *  Convergence check: norm(r)/norm(b) < tol
 *
 *  Stagnation check:
 *      - IF norm(alpha*p_k)/norm(x_{k+1}) < tol_stag
 *          -# compute r=b-A*x_{k+1};
 *          -# convergence check;
 *          -# IF ( not converged & restart_number < Max_Stag_Check ) restart;
 *      - END IF
 *
 *  Residual check:
 *      - IF norm(r_{k+1})/norm(b) < tol
 *          -# compute the real residual r = b-A*x_{k+1};
 *          -# convergence check;
 *          -# IF ( not converged & restart_number < Max_Res_Check ) restart;
 *      - END IF
 *
 *  Safe net check:
 *      - IF r_{k+1} > r_{best}
 *          -# x_{k+1} = x_{best}
 *      - END IF
 *
 *  \note Refer to Y. Saad 2003
 *        Iterative methods for sparse linear systems (2nd Edition), SIAM
 *
 *  \note See pcg.c for a version without safe net
 *
 */

#include <math.h>

#include "fasp.h"
#include "fasp_functs.h"
#include "itsolver_util.inl"

/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/**
 * \fn INT fasp_solver_dcsr_spcg (dCSRmat *A, dvector *b, dvector *u, precond *pc,
 *                                const REAL tol, const INT MaxIt,
 *                                const SHORT stop_type, const SHORT print_level)
 *
 * \brief Preconditioned conjugate gradient method for solving Au=b with safe net
 *
 * \param A            Pointer to the coefficient matrix
 * \param b            Pointer to the dvector of right hand side
 * \param u            Pointer to the dvector of DOFs
 * \param pc           Pointer to the structure of precondition (precond)
 * \param tol          Tolerance for stopping
 * \param MaxIt        Maximal number of iterations
 * \param stop_type    Stopping criteria type
 * \param print_level  How much information to print out
 *
 * \return             Number of iterations if converged, error message otherwise
 *
 * \author Chensong Zhang
 * \date   03/28/2013
 */
INT fasp_solver_dcsr_spcg (dCSRmat *A,
                           dvector *b,
                           dvector *u,
                           precond *pc,
                           const REAL tol,
                           const INT MaxIt,
                           const SHORT stop_type,
                           const SHORT print_level)
{
    const SHORT  MaxStag = MAX_STAG, MaxRestartStep = MAX_RESTART;
    const INT    m = b->row;
    const REAL   maxdiff = tol*STAG_RATIO; // staganation tolerance
    const REAL   sol_inf_tol = SMALLREAL; // infinity norm tolerance
    
    // local variables
    INT          iter = 0, stag = 1, more_step = 1, restart_step = 1;
    REAL         absres0, absres, relres, reldiff, factor;
    REAL         normr0 = BIGREAL, normu, infnormu;
    REAL         alpha, beta, temp1, temp2;
    
    INT          iter_best = 0; // initial best known iteration
    REAL         absres_best = BIGREAL; // initial best known residual
    
    // allocate temp memory (need 4*m REAL numbers)
    REAL *work = (REAL *)fasp_mem_calloc(5*m,sizeof(REAL));
    REAL *p = work, *z = work+m, *r = z+m, *t = r+m, *u_best = t+m;
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_solver_dcsr_spcg ...... [Start]\n");
    printf("### DEBUG: maxit = %d, tol = %.4le, stop type = %d\n", MaxIt, tol, stop_type);
#endif
    
    // r = b-A*u
    fasp_array_cp(m,b->val,r);
    fasp_blas_dcsr_aAxpy(-1.0,A,u->val,r);
    
    if (pc != NULL)
        pc->fct(r,z,pc->data); /* Apply preconditioner */
    else
        fasp_array_cp(m,r,z); /* No preconditioner */
    
    // compute initial residuals
    switch (stop_type) {
        case STOP_REL_RES:
            absres0 = fasp_blas_array_norm2(m,r);
            normr0  = MAX(SMALLREAL,absres0);
            relres  = absres0/normr0;
            break;
        case STOP_REL_PRECRES:
            absres0 = sqrt(fasp_blas_array_dotprod(m,r,z));
            normr0  = MAX(SMALLREAL,absres0);
            relres  = absres0/normr0;
            break;
        case STOP_MOD_REL_RES:
            absres0 = fasp_blas_array_norm2(m,r);
            normu   = MAX(SMALLREAL,fasp_blas_array_norm2(m,u->val));
            relres  = absres0/normu;
            break;
        default:
            printf("### WARNING: Unrecognized stopping type!\n");
            goto FINISHED;
    }
    
    // if initial residual is small, no need to iterate!
    if ( relres < tol ) goto FINISHED;
    
    // output iteration information if needed
    print_itinfo(print_level,stop_type,iter,relres,absres0,0.0);
    
    fasp_array_cp(m,z,p);
    temp1 = fasp_blas_array_dotprod(m,z,r);
    
    // main PCG loop
    while ( iter++ < MaxIt ) {
        
        // t=A*p
        fasp_blas_dcsr_mxv(A,p,t);
        
        // alpha_k=(z_{k-1},r_{k-1})/(A*p_{k-1},p_{k-1})
        temp2 = fasp_blas_array_dotprod(m,t,p);
        alpha = temp1/temp2;
        
        // u_k=u_{k-1} + alpha_k*p_{k-1}
        fasp_blas_array_axpy(m,alpha,p,u->val);
        
        // r_k=r_{k-1} - alpha_k*A*p_{k-1}
        fasp_blas_array_axpy(m,-alpha,t,r);
        
        // compute residuals
        switch ( stop_type ) {
            case STOP_REL_RES:
                absres = fasp_blas_array_norm2(m,r);
                relres = absres/normr0;
                break;
            case STOP_REL_PRECRES:
                // z = B(r)
                if ( pc != NULL )
                    pc->fct(r,z,pc->data); /* Apply preconditioner */
                else
                    fasp_array_cp(m,r,z); /* No preconditioner */
                absres = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
                relres = absres/normr0;
                break;
            case STOP_MOD_REL_RES:
                absres = fasp_blas_array_norm2(m,r);
                relres = absres/normu;
                break;
        }
        
        // compute reducation factor of residual ||r||
        factor = absres/absres0;
        
        // output iteration information if needed
        print_itinfo(print_level,stop_type,iter,relres,absres,factor);
        
        // safe net check: save the best-so-far solution
        if ( fasp_dvec_isnan(u) ) {
            // If the solution is NAN, restrore the best solution
            absres = BIGREAL;
            goto RESTORE_BESTSOL;
        }
        
        if ( absres < absres_best - maxdiff) {
            absres_best = absres;
            iter_best   = iter;
            fasp_array_cp(m,u->val,u_best);
        }
        
        // Check I: if soultion is close to zero, return ERROR_SOLVER_SOLSTAG
        infnormu = fasp_blas_array_norminf(m, u->val);
        if ( infnormu <= sol_inf_tol ) {
            if (print_level>PRINT_MIN) ITS_ZEROSOL;
            iter = ERROR_SOLVER_SOLSTAG;
            break;
        }
        
        //  Check II: if staggenated, try to restart
        normu   = fasp_blas_dvec_norm2(u);
        
        // compute relative difference
        reldiff = ABS(alpha)*fasp_blas_array_norm2(m,p)/normu;
        if ( (stag <= MaxStag) & (reldiff < maxdiff) ) {
            
            if ( print_level >= PRINT_MORE ) {
                ITS_DIFFRES(reldiff,relres);
                ITS_RESTART;
            }
            
            fasp_array_cp(m,b->val,r);
            fasp_blas_dcsr_aAxpy(-1.0,A,u->val,r);
            
            // compute residuals
            switch ( stop_type ) {
                case STOP_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normr0;
                    break;
                case STOP_REL_PRECRES:
                    // z = B(r)
                    if ( pc != NULL )
                        pc->fct(r,z,pc->data); /* Apply preconditioner */
                    else
                        fasp_array_cp(m,r,z); /* No preconditioner */
                    absres = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
                    relres = absres/normr0;
                    break;
                case STOP_MOD_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normu;
                    break;
            }
            
            if ( print_level >= PRINT_MORE ) ITS_REALRES(relres);
            
            if ( relres < tol )
                break;
            else {
                if ( stag >= MaxStag ) {
                    if ( print_level > PRINT_MIN ) ITS_STAGGED;
                    iter = ERROR_SOLVER_STAG;
                    break;
                }
                fasp_array_set(m,p,0.0);
                ++stag;
                ++restart_step;
            }
        } // end of staggnation check!
        
        // Check III: prevent false convergence
        if ( relres < tol ) {
            
            if ( print_level >= PRINT_MORE ) ITS_COMPRES(relres);
            
            // compute residual r = b - Ax again
            fasp_array_cp(m,b->val,r);
            fasp_blas_dcsr_aAxpy(-1.0,A,u->val,r);
            
            // compute residuals
            switch ( stop_type ) {
                case STOP_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normr0;
                    break;
                case STOP_REL_PRECRES:
                    // z = B(r)
                    if ( pc != NULL )
                        pc->fct(r,z,pc->data); /* Apply preconditioner */
                    else
                        fasp_array_cp(m,r,z); /* No preconditioner */
                    absres = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
                    relres = absres/normr0;
                    break;
                case STOP_MOD_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normu;
                    break;
            }
            
            if ( print_level >= PRINT_MORE ) ITS_REALRES(relres);
            
            // check convergence
            if ( relres < tol ) break;
            
            if ( more_step >= MaxRestartStep ) {
                if (print_level>PRINT_MIN) ITS_ZEROTOL;
                iter = ERROR_SOLVER_TOLSMALL;
                break;
            }
            
            // prepare for restarting the method
            fasp_array_set(m,p,0.0);
            ++more_step;
            ++restart_step;
            
        } // end of safe-guard check!
        
        // save residual for next iteration
        absres0 = absres;
        
        // compute z_k = B(r_k)
        if ( stop_type != STOP_REL_PRECRES ) {
            if ( pc != NULL )
                pc->fct(r,z,pc->data); /* Apply preconditioner */
            else
                fasp_array_cp(m,r,z); /* No preconditioner, B=I */
        }
        
        // compute beta_k = (z_k, r_k)/(z_{k-1}, r_{k-1})
        temp2 = fasp_blas_array_dotprod(m,z,r);
        beta  = temp2/temp1;
        temp1 = temp2;
        
        // compute p_k = z_k + beta_k*p_{k-1}
        fasp_blas_array_axpby(m,1.0,z,beta,p);
        
    } // end of main PCG loop.
    
    // safe net check: restore the best-so-far solution if necessary
    fasp_array_cp(m,b->val,r);
    fasp_blas_dcsr_aAxpy(-1.0,A,u_best,r);
    
    switch ( stop_type ) {
        case STOP_REL_RES:
            absres_best = fasp_blas_array_norm2(m,r);
            break;
        case STOP_REL_PRECRES:
            // z = B(r)
            if ( pc != NULL )
                pc->fct(r,z,pc->data); /* Apply preconditioner */
            else
                fasp_array_cp(m,r,z); /* No preconditioner */
            absres_best = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
            break;
        case STOP_MOD_REL_RES:
            absres_best = fasp_blas_array_norm2(m,r);
            break;
    }
    
RESTORE_BESTSOL:
    if ( iter != iter_best && absres > absres_best + maxdiff ) {
        if ( print_level > PRINT_NONE )
            printf("### WARNING: Restore iteration %d!!!\n", iter_best);
        fasp_array_cp(m,u_best,u->val);
    }
    
FINISHED:  // finish the iterative method
    if ( print_level > PRINT_NONE ) ITS_FINAL(iter,MaxIt,relres);
    
    // clean up temp memory
    fasp_mem_free(work);
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_solver_dcsr_spcg ...... [Finish]\n");
#endif
    
    if ( iter > MaxIt )
        return ERROR_SOLVER_MAXIT;
    else
        return iter;
}


/**
 * \fn INT fasp_solver_bdcsr_spcg (block_dCSRmat *A, dvector *b, dvector *u, precond *pc,
 *                                 const REAL tol, const INT MaxIt,
 *                                 const SHORT stop_type, const SHORT print_level)
 *
 * \brief Preconditioned conjugate gradient method for solving Au=b with safe net
 *
 * \param A            Pointer to the coefficient matrix
 * \param b            Pointer to the dvector of right hand side
 * \param u            Pointer to the dvector of DOFs
 * \param pc           Pointer to the structure of precondition (precond)
 * \param tol          Tolerance for stopping
 * \param MaxIt        Maximal number of iterations
 * \param stop_type    Stopping criteria type
 * \param print_level  How much information to print out
 *
 * \return             Number of iterations if converged, error message otherwise
 *
 * \author Chensong Zhang
 * \date   03/28/2013
 */
INT fasp_solver_bdcsr_spcg (block_dCSRmat *A,
                            dvector *b,
                            dvector *u,
                            precond *pc,
                            const REAL tol,
                            const INT MaxIt,
                            const SHORT stop_type,
                            const SHORT print_level)
{
    const SHORT  MaxStag = MAX_STAG, MaxRestartStep = MAX_RESTART;
    const INT    m = b->row;
    const REAL   maxdiff = tol*STAG_RATIO; // staganation tolerance
    const REAL   sol_inf_tol = SMALLREAL; // infinity norm tolerance
    
    // local variables
    INT          iter = 0, stag = 1, more_step = 1, restart_step = 1;
    REAL         absres0, absres, relres, reldiff, factor;
    REAL         normr0 = BIGREAL, normu, infnormu;
    REAL         alpha, beta, temp1, temp2;
    
    INT          iter_best = 0; // initial best known iteration
    REAL         absres_best = BIGREAL; // initial best known residual
    
    // allocate temp memory (need 4*m REAL numbers)
    REAL *work = (REAL *)fasp_mem_calloc(5*m,sizeof(REAL));
    REAL *p = work, *z = work+m, *r = z+m, *t = r+m, *u_best = t+m;
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_solver_bdcsr_spcg ...... [Start]\n");
    printf("### DEBUG: maxit = %d, tol = %.4le, stop type = %d\n", MaxIt, tol, stop_type);
#endif
    
    // r = b-A*u
    fasp_array_cp(m,b->val,r);
    fasp_blas_bdcsr_aAxpy(-1.0,A,u->val,r);
    
    if (pc != NULL)
        pc->fct(r,z,pc->data); /* Apply preconditioner */
    else
        fasp_array_cp(m,r,z); /* No preconditioner */
    
    // compute initial residuals
    switch (stop_type) {
        case STOP_REL_RES:
            absres0 = fasp_blas_array_norm2(m,r);
            normr0  = MAX(SMALLREAL,absres0);
            relres  = absres0/normr0;
            break;
        case STOP_REL_PRECRES:
            absres0 = sqrt(fasp_blas_array_dotprod(m,r,z));
            normr0  = MAX(SMALLREAL,absres0);
            relres  = absres0/normr0;
            break;
        case STOP_MOD_REL_RES:
            absres0 = fasp_blas_array_norm2(m,r);
            normu   = MAX(SMALLREAL,fasp_blas_array_norm2(m,u->val));
            relres  = absres0/normu;
            break;
        default:
            printf("### WARNING: Unrecognized stopping type!\n");
            goto FINISHED;
    }
    
    // if initial residual is small, no need to iterate!
    if ( relres < tol ) goto FINISHED;
    
    // output iteration information if needed
    print_itinfo(print_level,stop_type,iter,relres,absres0,0.0);
    
    fasp_array_cp(m,z,p);
    temp1 = fasp_blas_array_dotprod(m,z,r);
    
    // main PCG loop
    while ( iter++ < MaxIt ) {
        
        // t=A*p
        fasp_blas_bdcsr_mxv(A,p,t);
        
        // alpha_k=(z_{k-1},r_{k-1})/(A*p_{k-1},p_{k-1})
        temp2 = fasp_blas_array_dotprod(m,t,p);
        alpha = temp1/temp2;
        
        // u_k=u_{k-1} + alpha_k*p_{k-1}
        fasp_blas_array_axpy(m,alpha,p,u->val);
        
        // r_k=r_{k-1} - alpha_k*A*p_{k-1}
        fasp_blas_array_axpy(m,-alpha,t,r);
        
        // compute residuals
        switch ( stop_type ) {
            case STOP_REL_RES:
                absres = fasp_blas_array_norm2(m,r);
                relres = absres/normr0;
                break;
            case STOP_REL_PRECRES:
                // z = B(r)
                if ( pc != NULL )
                    pc->fct(r,z,pc->data); /* Apply preconditioner */
                else
                    fasp_array_cp(m,r,z); /* No preconditioner */
                absres = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
                relres = absres/normr0;
                break;
            case STOP_MOD_REL_RES:
                absres = fasp_blas_array_norm2(m,r);
                relres = absres/normu;
                break;
        }
        
        // compute reducation factor of residual ||r||
        factor = absres/absres0;
        
        // output iteration information if needed
        print_itinfo(print_level,stop_type,iter,relres,absres,factor);
        
        // safe net check: save the best-so-far solution
        if ( fasp_dvec_isnan(u) ) {
            // If the solution is NAN, restrore the best solution
            absres = BIGREAL;
            goto RESTORE_BESTSOL;
        }
        
        if ( absres < absres_best - maxdiff) {
            absres_best = absres;
            iter_best   = iter;
            fasp_array_cp(m,u->val,u_best);
        }
        
        // Check I: if soultion is close to zero, return ERROR_SOLVER_SOLSTAG
        infnormu = fasp_blas_array_norminf(m, u->val);
        if ( infnormu <= sol_inf_tol ) {
            if (print_level>PRINT_MIN) ITS_ZEROSOL;
            iter = ERROR_SOLVER_SOLSTAG;
            break;
        }
        
        //  Check II: if staggenated, try to restart
        normu   = fasp_blas_dvec_norm2(u);
        
        // compute relative difference
        reldiff = ABS(alpha)*fasp_blas_array_norm2(m,p)/normu;
        if ( (stag <= MaxStag) & (reldiff < maxdiff) ) {
            
            if ( print_level >= PRINT_MORE ) {
                ITS_DIFFRES(reldiff,relres);
                ITS_RESTART;
            }
            
            fasp_array_cp(m,b->val,r);
            fasp_blas_bdcsr_aAxpy(-1.0,A,u->val,r);
            
            // compute residuals
            switch ( stop_type ) {
                case STOP_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normr0;
                    break;
                case STOP_REL_PRECRES:
                    // z = B(r)
                    if ( pc != NULL )
                        pc->fct(r,z,pc->data); /* Apply preconditioner */
                    else
                        fasp_array_cp(m,r,z); /* No preconditioner */
                    absres = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
                    relres = absres/normr0;
                    break;
                case STOP_MOD_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normu;
                    break;
            }
            
            if ( print_level >= PRINT_MORE ) ITS_REALRES(relres);
            
            if ( relres < tol )
                break;
            else {
                if ( stag >= MaxStag ) {
                    if ( print_level > PRINT_MIN ) ITS_STAGGED;
                    iter = ERROR_SOLVER_STAG;
                    break;
                }
                fasp_array_set(m,p,0.0);
                ++stag;
                ++restart_step;
            }
        } // end of staggnation check!
        
        // Check III: prevent false convergence
        if ( relres < tol ) {
            
            if ( print_level >= PRINT_MORE ) ITS_COMPRES(relres);
            
            // compute residual r = b - Ax again
            fasp_array_cp(m,b->val,r);
            fasp_blas_bdcsr_aAxpy(-1.0,A,u->val,r);
            
            // compute residuals
            switch ( stop_type ) {
                case STOP_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normr0;
                    break;
                case STOP_REL_PRECRES:
                    // z = B(r)
                    if ( pc != NULL )
                        pc->fct(r,z,pc->data); /* Apply preconditioner */
                    else
                        fasp_array_cp(m,r,z); /* No preconditioner */
                    absres = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
                    relres = absres/normr0;
                    break;
                case STOP_MOD_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normu;
                    break;
            }
            
            if ( print_level >= PRINT_MORE ) ITS_REALRES(relres);
            
            // check convergence
            if ( relres < tol ) break;
            
            if ( more_step >= MaxRestartStep ) {
                if (print_level>PRINT_MIN) ITS_ZEROTOL;
                iter = ERROR_SOLVER_TOLSMALL;
                break;
            }
            
            // prepare for restarting the method
            fasp_array_set(m,p,0.0);
            ++more_step;
            ++restart_step;
            
        } // end of safe-guard check!
        
        // save residual for next iteration
        absres0 = absres;
        
        // compute z_k = B(r_k)
        if ( stop_type != STOP_REL_PRECRES ) {
            if ( pc != NULL )
                pc->fct(r,z,pc->data); /* Apply preconditioner */
            else
                fasp_array_cp(m,r,z); /* No preconditioner, B=I */
        }
        
        // compute beta_k = (z_k, r_k)/(z_{k-1}, r_{k-1})
        temp2 = fasp_blas_array_dotprod(m,z,r);
        beta  = temp2/temp1;
        temp1 = temp2;
        
        // compute p_k = z_k + beta_k*p_{k-1}
        fasp_blas_array_axpby(m,1.0,z,beta,p);
        
    } // end of main PCG loop.
    
    // safe net check: restore the best-so-far solution if necessary
    fasp_array_cp(m,b->val,r);
    fasp_blas_bdcsr_aAxpy(-1.0,A,u_best,r);
    
    switch ( stop_type ) {
        case STOP_REL_RES:
            absres_best = fasp_blas_array_norm2(m,r);
            break;
        case STOP_REL_PRECRES:
            // z = B(r)
            if ( pc != NULL )
                pc->fct(r,z,pc->data); /* Apply preconditioner */
            else
                fasp_array_cp(m,r,z); /* No preconditioner */
            absres_best = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
            break;
        case STOP_MOD_REL_RES:
            absres_best = fasp_blas_array_norm2(m,r);
            break;
    }
    
RESTORE_BESTSOL:
    if ( iter != iter_best && absres > absres_best + maxdiff ) {
        if ( print_level > PRINT_NONE )
            printf("### WARNING: Restore iteration %d!!!\n", iter_best);
        fasp_array_cp(m,u_best,u->val);
    }

FINISHED:  // finish the iterative method
    if ( print_level > PRINT_NONE ) ITS_FINAL(iter,MaxIt,relres);
    
    // clean up temp memory
    fasp_mem_free(work);
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_solver_bdcsr_spcg ...... [Finish]\n");
#endif
    
    if ( iter > MaxIt )
        return ERROR_SOLVER_MAXIT;
    else
        return iter;
}

/**
 * \fn INT fasp_solver_dstr_spcg (dSTRmat *A, dvector *b, dvector *u, precond *pc,
 *                                const REAL tol, const INT MaxIt,
 *                                const SHORT stop_type, const SHORT print_level)
 *
 * \brief Preconditioned conjugate gradient method for solving Au=b with safe net
 *
 * \param A            Pointer to the coefficient matrix
 * \param b            Pointer to the dvector of right hand side
 * \param u            Pointer to the dvector of DOFs
 * \param MaxIt        Maximal number of iterations
 * \param tol          Tolerance for stopping
 * \param pc           Pointer to the structure of precondition (precond)
 * \param print_level  How much information to print out
 * \param stop_type    Stopping criteria type
 *
 * \return             Number of iterations if converged, error message otherwise
 *
 * \author Chensong Zhang
 * \date   03/28/2013
 */
INT fasp_solver_dstr_spcg (dSTRmat *A,
                           dvector *b,
                           dvector *u,
                           precond *pc,
                           const REAL tol,
                           const INT MaxIt,
                           const SHORT stop_type,
                           const SHORT print_level)
{
    const SHORT  MaxStag = MAX_STAG, MaxRestartStep = MAX_RESTART;
    const INT    m = b->row;
    const REAL   maxdiff = tol*STAG_RATIO; // staganation tolerance
    const REAL   sol_inf_tol = SMALLREAL; // infinity norm tolerance
    
    // local variables
    INT          iter = 0, stag = 1, more_step = 1, restart_step = 1;
    REAL         absres0, absres, relres, reldiff, factor;
    REAL         normr0 = BIGREAL, normu, infnormu;
    REAL         alpha, beta, temp1, temp2;
    
    INT          iter_best = 0; // initial best known iteration
    REAL         absres_best = BIGREAL; // initial best known residual
    
    // allocate temp memory (need 4*m REAL numbers)
    REAL *work = (REAL *)fasp_mem_calloc(5*m,sizeof(REAL));
    REAL *p = work, *z = work+m, *r = z+m, *t = r+m, *u_best = t+m;
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_solver_dstr_spcg ...... [Start]\n");
    printf("### DEBUG: maxit = %d, tol = %.4le, stop type = %d\n", MaxIt, tol, stop_type);
#endif
    
    // r = b-A*u
    fasp_array_cp(m,b->val,r);
    fasp_blas_dstr_aAxpy(-1.0,A,u->val,r);
    
    if (pc != NULL)
        pc->fct(r,z,pc->data); /* Apply preconditioner */
    else
        fasp_array_cp(m,r,z); /* No preconditioner */
    
    // compute initial residuals
    switch (stop_type) {
        case STOP_REL_RES:
            absres0 = fasp_blas_array_norm2(m,r);
            normr0  = MAX(SMALLREAL,absres0);
            relres  = absres0/normr0;
            break;
        case STOP_REL_PRECRES:
            absres0 = sqrt(fasp_blas_array_dotprod(m,r,z));
            normr0  = MAX(SMALLREAL,absres0);
            relres  = absres0/normr0;
            break;
        case STOP_MOD_REL_RES:
            absres0 = fasp_blas_array_norm2(m,r);
            normu   = MAX(SMALLREAL,fasp_blas_array_norm2(m,u->val));
            relres  = absres0/normu;
            break;
        default:
            printf("### WARNING: Unrecognized stopping type!\n");
            goto FINISHED;
    }
    
    // if initial residual is small, no need to iterate!
    if ( relres < tol ) goto FINISHED;
    
    // output iteration information if needed
    print_itinfo(print_level,stop_type,iter,relres,absres0,0.0);
    
    fasp_array_cp(m,z,p);
    temp1 = fasp_blas_array_dotprod(m,z,r);
    
    // main PCG loop
    while ( iter++ < MaxIt ) {
        
        // t=A*p
        fasp_blas_dstr_mxv(A,p,t);
        
        // alpha_k=(z_{k-1},r_{k-1})/(A*p_{k-1},p_{k-1})
        temp2 = fasp_blas_array_dotprod(m,t,p);
        alpha = temp1/temp2;
        
        // u_k=u_{k-1} + alpha_k*p_{k-1}
        fasp_blas_array_axpy(m,alpha,p,u->val);
        
        // r_k=r_{k-1} - alpha_k*A*p_{k-1}
        fasp_blas_array_axpy(m,-alpha,t,r);
        
        // compute residuals
        switch ( stop_type ) {
            case STOP_REL_RES:
                absres = fasp_blas_array_norm2(m,r);
                relres = absres/normr0;
                break;
            case STOP_REL_PRECRES:
                // z = B(r)
                if ( pc != NULL )
                    pc->fct(r,z,pc->data); /* Apply preconditioner */
                else
                    fasp_array_cp(m,r,z); /* No preconditioner */
                absres = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
                relres = absres/normr0;
                break;
            case STOP_MOD_REL_RES:
                absres = fasp_blas_array_norm2(m,r);
                relres = absres/normu;
                break;
        }
        
        // compute reducation factor of residual ||r||
        factor = absres/absres0;
        
        // output iteration information if needed
        print_itinfo(print_level,stop_type,iter,relres,absres,factor);
        
        // safe net check: save the best-so-far solution
        if ( fasp_dvec_isnan(u) ) {
            // If the solution is NAN, restrore the best solution
            absres = BIGREAL;
            goto RESTORE_BESTSOL;
        }
        
        if ( absres < absres_best - maxdiff) {
            absres_best = absres;
            iter_best   = iter;
            fasp_array_cp(m,u->val,u_best);
        }
        
        // Check I: if soultion is close to zero, return ERROR_SOLVER_SOLSTAG
        infnormu = fasp_blas_array_norminf(m, u->val);
        if ( infnormu <= sol_inf_tol ) {
            if (print_level>PRINT_MIN) ITS_ZEROSOL;
            iter = ERROR_SOLVER_SOLSTAG;
            break;
        }
        
        //  Check II: if staggenated, try to restart
        normu   = fasp_blas_dvec_norm2(u);
        
        // compute relative difference
        reldiff = ABS(alpha)*fasp_blas_array_norm2(m,p)/normu;
        if ( (stag <= MaxStag) & (reldiff < maxdiff) ) {
            
            if ( print_level >= PRINT_MORE ) {
                ITS_DIFFRES(reldiff,relres);
                ITS_RESTART;
            }
            
            fasp_array_cp(m,b->val,r);
            fasp_blas_dstr_aAxpy(-1.0,A,u->val,r);
            
            // compute residuals
            switch ( stop_type ) {
                case STOP_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normr0;
                    break;
                case STOP_REL_PRECRES:
                    // z = B(r)
                    if ( pc != NULL )
                        pc->fct(r,z,pc->data); /* Apply preconditioner */
                    else
                        fasp_array_cp(m,r,z); /* No preconditioner */
                    absres = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
                    relres = absres/normr0;
                    break;
                case STOP_MOD_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normu;
                    break;
            }
            
            if ( print_level >= PRINT_MORE ) ITS_REALRES(relres);
            
            if ( relres < tol )
                break;
            else {
                if ( stag >= MaxStag ) {
                    if ( print_level > PRINT_MIN ) ITS_STAGGED;
                    iter = ERROR_SOLVER_STAG;
                    break;
                }
                fasp_array_set(m,p,0.0);
                ++stag;
                ++restart_step;
            }
        } // end of staggnation check!
        
        // Check III: prevent false convergence
        if ( relres < tol ) {
            
            if ( print_level >= PRINT_MORE ) ITS_COMPRES(relres);
            
            // compute residual r = b - Ax again
            fasp_array_cp(m,b->val,r);
            fasp_blas_dstr_aAxpy(-1.0,A,u->val,r);
            
            // compute residuals
            switch ( stop_type ) {
                case STOP_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normr0;
                    break;
                case STOP_REL_PRECRES:
                    // z = B(r)
                    if ( pc != NULL )
                        pc->fct(r,z,pc->data); /* Apply preconditioner */
                    else
                        fasp_array_cp(m,r,z); /* No preconditioner */
                    absres = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
                    relres = absres/normr0;
                    break;
                case STOP_MOD_REL_RES:
                    absres = fasp_blas_array_norm2(m,r);
                    relres = absres/normu;
                    break;
            }
            
            if ( print_level >= PRINT_MORE ) ITS_REALRES(relres);
            
            // check convergence
            if ( relres < tol ) break;
            
            if ( more_step >= MaxRestartStep ) {
                if (print_level>PRINT_MIN) ITS_ZEROTOL;
                iter = ERROR_SOLVER_TOLSMALL;
                break;
            }
            
            // prepare for restarting the method
            fasp_array_set(m,p,0.0);
            ++more_step;
            ++restart_step;
            
        } // end of safe-guard check!
        
        // save residual for next iteration
        absres0 = absres;
        
        // compute z_k = B(r_k)
        if ( stop_type != STOP_REL_PRECRES ) {
            if ( pc != NULL )
                pc->fct(r,z,pc->data); /* Apply preconditioner */
            else
                fasp_array_cp(m,r,z); /* No preconditioner, B=I */
        }
        
        // compute beta_k = (z_k, r_k)/(z_{k-1}, r_{k-1})
        temp2 = fasp_blas_array_dotprod(m,z,r);
        beta  = temp2/temp1;
        temp1 = temp2;
        
        // compute p_k = z_k + beta_k*p_{k-1}
        fasp_blas_array_axpby(m,1.0,z,beta,p);
        
    } // end of main PCG loop.
    
    // safe net check: restore the best-so-far solution if necessary
    fasp_array_cp(m,b->val,r);
    fasp_blas_dstr_aAxpy(-1.0,A,u_best,r);
    
    switch ( stop_type ) {
        case STOP_REL_RES:
            absres_best = fasp_blas_array_norm2(m,r);
            break;
        case STOP_REL_PRECRES:
            // z = B(r)
            if ( pc != NULL )
                pc->fct(r,z,pc->data); /* Apply preconditioner */
            else
                fasp_array_cp(m,r,z); /* No preconditioner */
            absres_best = sqrt(ABS(fasp_blas_array_dotprod(m,z,r)));
            break;
        case STOP_MOD_REL_RES:
            absres_best = fasp_blas_array_norm2(m,r);
            break;
    }
    
RESTORE_BESTSOL:
    if ( iter != iter_best && absres > absres_best + maxdiff ) {
        if ( print_level > PRINT_NONE )
            printf("### WARNING: Restore iteration %d!!!\n", iter_best);
        fasp_array_cp(m,u_best,u->val);
    }

FINISHED:  // finish the iterative method
    if ( print_level > PRINT_NONE ) ITS_FINAL(iter,MaxIt,relres);
    
    // clean up temp memory
    fasp_mem_free(work);
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_solver_dstr_spcg ...... [Finish]\n");
#endif
    
    if ( iter > MaxIt )
        return ERROR_SOLVER_MAXIT;
    else
        return iter;
}

/*---------------------------------*/
/*--        End of File          --*/
/*---------------------------------*/
