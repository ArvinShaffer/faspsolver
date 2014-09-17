/*! \file itsolver_bsr.c
 *
 *  \brief Iterative solvers for dBSRmat matrices
 */

#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "fasp.h"
#include "fasp_functs.h"
#include "itsolver_util.inl"

/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/**
 * \fn void fasp_set_GS_threads (INT threads,INT its)
 *
 * \brief  Set threads for CPR. Please add it at the begin of Krylov openmp method function 
 *         and after iter++.
 *
 * \param threads  Total threads of sovler
 * \param its      Current its of the Krylov methods
 *
 * \author Feng Chunsheng, Yue Xiaoqiang
 * \date 03/20/2011
 *
 * TODO: Why put it here??? --Chensong
 */

INT THDs_AMG_GS=0;  /**< cpr amg gs smoothing threads      */
INT THDs_CPR_lGS=0; /**< reservoir gs smoothing threads     */
INT THDs_CPR_gGS=0; /**< global matrix gs smoothing threads */

void fasp_set_GS_threads (INT mythreads,
                          INT its)
{
#ifdef _OPENMP 

#if 1

    if (its <=8) {
        THDs_AMG_GS =  mythreads;
        THDs_CPR_lGS = mythreads ;
        THDs_CPR_gGS = mythreads ;
    }
    else if (its <=12) {
        THDs_AMG_GS =  mythreads;
        THDs_CPR_lGS = (6 < mythreads) ? 6 : mythreads;
        THDs_CPR_gGS = (4 < mythreads) ? 4 : mythreads;
    }
    else if (its <=15) {
        THDs_AMG_GS =  (3 < mythreads) ? 3 : mythreads;
        THDs_CPR_lGS = (3 < mythreads) ? 3 : mythreads;
        THDs_CPR_gGS = (2 < mythreads) ? 2 : mythreads;
    }
    else if (its <=18) {
        THDs_AMG_GS =  (2 < mythreads) ? 2 : mythreads;
        THDs_CPR_lGS = (2 < mythreads) ? 2 : mythreads;
        THDs_CPR_gGS = (1 < mythreads) ? 1 : mythreads;
    }
    else {
        THDs_AMG_GS =  1;
        THDs_CPR_lGS = 1;
        THDs_CPR_gGS = 1;
    }

#else

    THDs_AMG_GS =  mythreads;
    THDs_CPR_lGS = mythreads ;
    THDs_CPR_gGS = mythreads ;

#endif
    
#endif // FASP_USE_OPENMP
}

/**
 * \fn INT fasp_solver_dbsr_itsolver (dBSRmat *A, dvector *b, dvector *x, 
 *                                    precond *pc, itsolver_param *itparam)
 *
 * \brief Solve Ax=b by preconditioned Krylov methods for BSR matrices
 *
 * \param A        Pointer to the coeff matrix in dBSRmat format
 * \param b        Pointer to the right hand side in dvector format
 * \param x        Pointer to the approx solution in dvector format
 * \param pc       Pointer to the preconditioning action
 * \param itparam  Pointer to parameters for iterative solvers
 *
 * \return         Number of iterations if succeed
 *
 * \author Zhiyang Zhou, Xiaozhe Hu
 * \date   10/26/2010
 */
INT fasp_solver_dbsr_itsolver (dBSRmat *A, 
                               dvector *b, 
                               dvector *x, 
                               precond *pc, 
                               itsolver_param *itparam)
{
    const SHORT print_level = itparam->print_level;    
    const SHORT itsolver_type = itparam->itsolver_type;
    const SHORT stop_type = itparam->stop_type;
    const SHORT restart = itparam->restart;
    const INT   MaxIt = itparam->maxit;
    const REAL  tol = itparam->tol; 
    
    // Local variables
    INT iter;
    REAL solver_start, solver_end, solver_duration;

#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Start]\n", __FUNCTION__);
    printf("### DEBUG: rhs/sol size: %d %d\n", b->row, x->row);
#endif

    fasp_gettime(&solver_start);

    /* Safe-guard checks on parameters */
    ITS_CHECK ( MaxIt, tol );

    switch (itsolver_type) {
        
        case SOLVER_CG:
            if ( print_level>PRINT_NONE ) printf("\nCalling PCG solver (BSR) ...\n");
            iter = fasp_solver_dbsr_pcg(A, b, x, pc, tol, MaxIt, stop_type, print_level);
            break;
    
        case SOLVER_BiCGstab:
            if ( print_level>PRINT_NONE ) printf("\nCalling BiCGstab solver (BSR) ...\n");
            iter = fasp_solver_dbsr_pbcgs(A, b, x, pc, tol, MaxIt, stop_type, print_level);
            break;
    
        case SOLVER_GMRES:
            if ( print_level>PRINT_NONE ) printf("\nCalling GMRES solver (BSR) ...\n");
            iter = fasp_solver_dbsr_pgmres(A, b, x, pc, tol, MaxIt, restart, stop_type, print_level);
            break;
    
        case SOLVER_VGMRES:
            if ( print_level>PRINT_NONE ) printf("\nCalling vGMRES solver (BSR) ...\n");
            iter = fasp_solver_dbsr_pvgmres(A, b, x, pc, tol, MaxIt, restart, stop_type, print_level);
            break;
            
        case SOLVER_VFGMRES:
            if ( print_level>PRINT_NONE ) printf("\nCalling vFGMRes solver (BSR) ...\n");
            iter = fasp_solver_dbsr_pvfgmres(A, b, x, pc, tol, MaxIt, restart, stop_type, print_level);
            break;
            
        default:
            printf("### ERROR: Unknown itertive solver type %d!\n", itsolver_type);
            iter = ERROR_SOLVER_TYPE;
    
    }
    
    if ( (print_level>PRINT_MIN) && (iter >= 0) ) {
        fasp_gettime(&solver_end);
        solver_duration = solver_end - solver_start;
        print_cputime("Iterative method", solver_duration);
    }
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Finish]\n", __FUNCTION__);
#endif

    return iter;
}

/**
 * \fn INT fasp_solver_dbsr_krylov (dBSRmat *A, dvector *b, dvector *x, 
 *                                  itsolver_param *itparam)
 *
 * \brief Solve Ax=b by standard Krylov methods for BSR matrices
 *
 * \param A         Pointer to the coeff matrix in dBSRmat format
 * \param b         Pointer to the right hand side in dvector format
 * \param x         Pointer to the approx solution in dvector format
 * \param itparam   Pointer to parameters for iterative solvers
 *
 * \return          Number of iterations if succeed
 *
 * \author Zhiyang Zhou, Xiaozhe Hu
 * \date   10/26/2010
 */
INT fasp_solver_dbsr_krylov (dBSRmat *A, 
                             dvector *b, 
                             dvector *x, 
                             itsolver_param *itparam)
{
    const SHORT print_level = itparam->print_level;
    INT status = FASP_SUCCESS;
    REAL solver_start, solver_end;
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Start]\n", __FUNCTION__);
#endif

    // solver part
    fasp_gettime(&solver_start);

    status=fasp_solver_dbsr_itsolver(A,b,x,NULL,itparam);

    fasp_gettime(&solver_end);
    
    if ( print_level>PRINT_NONE ) {
        REAL solver_duration = solver_end - solver_start;
        print_cputime("Krylov method totally", solver_duration);
    }
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Finish]\n", __FUNCTION__);
#endif

    return status;
}

/**
 * \fn INT fasp_solver_dbsr_krylov_diag (dBSRmat *A, dvector *b, dvector *x, 
 *                                       itsolver_param *itparam)
 *
 * \brief Solve Ax=b by diagonal preconditioned Krylov methods 
 *
 * \param A         Pointer to the coeff matrix in dBSRmat format
 * \param b         Pointer to the right hand side in dvector format
 * \param x         Pointer to the approx solution in dvector format
 * \param itparam   Pointer to parameters for iterative solvers
 *
 * \return the number of iterations
 *
 * \author Zhiyang Zhou, Xiaozhe Hu
 * \date   10/26/2010
 *
 * Modified by Chunsheng Feng, Zheng Li
 * \date   10/15/2012
 */

INT fasp_solver_dbsr_krylov_diag (dBSRmat *A, 
                                  dvector *b, 
                                  dvector *x, 
                                  itsolver_param *itparam)
{
    const SHORT print_level = itparam->print_level;
    INT status = FASP_SUCCESS;
    REAL solver_start, solver_end;
    
    INT nb=A->nb,i,k;
    INT nb2=nb*nb;
    INT ROW=A->ROW; 
    
#ifdef _OPENMP
    // variables for OpenMP
    INT myid, mybegin, myend;
    INT nthreads = FASP_GET_NUM_THREADS();
#endif
    // setup preconditioner
    precond_diagbsr diag;
    fasp_dvec_alloc(ROW*nb2, &(diag.diag));
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Start]\n", __FUNCTION__);
#endif

    // get all the diagonal sub-blocks
#ifdef _OPENMP
    if (ROW > OPENMP_HOLDS) {
#pragma omp parallel for private(myid, mybegin, myend, i, k)
        for (myid=0; myid<nthreads; ++myid) {
            FASP_GET_START_END(myid, nthreads, ROW, &mybegin, &myend);
            for (i = mybegin; i < myend; ++i) {
                for (k = A->IA[i]; k < A->IA[i+1]; ++k) {
                    if (A->JA[k] == i)
                    memcpy(diag.diag.val+i*nb2, A->val+k*nb2, nb2*sizeof(REAL));
                }
            }
        }
    }
    else {
#endif
        for (i = 0; i < ROW; ++i) {
            for (k = A->IA[i]; k < A->IA[i+1]; ++k) {
                if (A->JA[k] == i)
                memcpy(diag.diag.val+i*nb2, A->val+k*nb2, nb2*sizeof(REAL));
            }
        }
#ifdef _OPENMP
    }
#endif
    
    diag.nb=nb;
    
#ifdef _OPENMP
#pragma omp parallel for if(ROW>OPENMP_HOLDS)
#endif
    for (i=0; i<ROW; ++i){
        fasp_blas_smat_inv(&(diag.diag.val[i*nb2]), nb);
    }
    
    precond *pc = (precond *)fasp_mem_calloc(1,sizeof(precond));    
    pc->data = &diag;
    pc->fct  = fasp_precond_dbsr_diag;
    
    // solver part
    fasp_gettime(&solver_start);

    status=fasp_solver_dbsr_itsolver(A,b,x,pc,itparam);

    fasp_gettime(&solver_end);
    
    if ( print_level>PRINT_NONE ) {
        REAL solver_duration = solver_end - solver_start;
        print_cputime("Diag_Krylov method totally", solver_duration);
    }    
    
    // clean up
    fasp_dvec_free(&(diag.diag));

#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Finish]\n", __FUNCTION__);
#endif

    return status;
}

/**
 * \fn INT fasp_solver_dbsr_krylov_ilu (dBSRmat *A, dvector *b, dvector *x, 
 *                                      itsolver_param *itparam, ILU_param *iluparam)
 *
 * \brief Solve Ax=b by ILUs preconditioned Krylov methods
 *
 * \param A         Pointer to the coeff matrix in dBSRmat format
 * \param b         Pointer to the right hand side in dvector format
 * \param x         Pointer to the approx solution in dvector format
 * \param itparam   Pointer to parameters for iterative solvers
 * \param iluparam  Pointer to parameters of ILU
 *
 * \return          Number of iterations if succeed
 *
 * \author Shiquang Zhang, Xiaozhe Hu
 * \date   10/26/2010
 */
INT fasp_solver_dbsr_krylov_ilu (dBSRmat *A, 
                                 dvector *b, 
                                 dvector *x, 
                                 itsolver_param *itparam, 
                                 ILU_param *iluparam)
{
    const SHORT print_level = itparam->print_level;    
    REAL solver_start, solver_end;
    INT status = FASP_SUCCESS;
    
    ILU_data LU; 
    precond pc; 
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Start]\n", __FUNCTION__);
    printf("### DEBUG: matrix size: %d %d %d\n", A->ROW, A->COL, A->NNZ);
    printf("### DEBUG: rhs/sol size: %d %d\n", b->row, x->row);    
#endif
    
    // ILU setup for whole matrix
    if ( (status = fasp_ilu_dbsr_setup(A,&LU,iluparam))<0 ) goto FINISHED;
    
    // check iludata
    if ( (status = fasp_mem_iludata_check(&LU))<0 ) goto FINISHED;
    
    // set preconditioner 
    pc.data = &LU; pc.fct = fasp_precond_dbsr_ilu;
    
    // solve
    fasp_gettime(&solver_start);
    
    status=fasp_solver_dbsr_itsolver(A,b,x,&pc,itparam);
    
    fasp_gettime(&solver_end);
    
    if ( print_level>PRINT_NONE ) {
        REAL solver_duration = solver_end - solver_start;
        print_cputime("ILUk_Krylov method totally", solver_duration);
    }
    
 FINISHED: 
    fasp_ilu_data_free(&LU);
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Finish]\n", __FUNCTION__);
#endif
        
    return status;    
}

/**
 * \fn INT fasp_solver_dbsr_krylov_amg (dBSRmat *A, dvector *b, dvector *x, 
 *                                      itsolver_param *itparam, AMG_param *amgparam)
 *
 * \brief Solve Ax=b by AMG preconditioned Krylov methods 
 *
 * \param A         Pointer to the coeff matrix in dBSRmat format
 * \param b         Pointer to the right hand side in dvector format
 * \param x         Pointer to the approx solution in dvector format
 * \param itparam   Pointer to parameters for iterative solvers
 * \param amgparam  Pointer to parameters of AMG
 *
 * \return          Number of iterations if succeed
 *
 * \author Xiaozhe Hu
 * \date   03/16/2012
 */
INT fasp_solver_dbsr_krylov_amg (dBSRmat *A, 
                                 dvector *b, 
                                 dvector *x, 
                                 itsolver_param *itparam, 
                                 AMG_param *amgparam)
{
    //--------------------------------------------------------------
    // Part 1: prepare
    // --------------------------------------------------------------
    //! parameters of iterative method
    const SHORT print_level = itparam->print_level;
    const SHORT max_levels = amgparam->max_levels;
    
    // return variable
    INT status = FASP_SUCCESS;
    
    // data of AMG 
    AMG_data_bsr *mgl=fasp_amg_data_bsr_create(max_levels);
    
    // timing
    REAL setup_start, setup_end, solver_start, solver_end, solver_duration, setup_duration;
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Start]\n", __FUNCTION__);
#endif

    //--------------------------------------------------------------
    //Part 2: set up the preconditioner
    //--------------------------------------------------------------
    fasp_gettime(&setup_start);
    
    // initialize A, b, x for mgl[0]
    mgl[0].A = fasp_dbsr_create(A->ROW, A->COL, A->NNZ, A->nb, A->storage_manner);
    fasp_dbsr_cp(A,  &(mgl[0].A));
    //mgl[0].A.ROW = A->ROW; mgl[0].A.COL = A->COL; mgl[0].A.NNZ = A->NNZ;
    //mgl[0].A.nb = A->nb; mgl[0].A.storage_manner = A->storage_manner;
    //mgl[0].A.IA = A->IA; mgl[0].A.JA = A->JA; mgl[0].A.val = A->val;
    mgl[0].b = fasp_dvec_create(mgl[0].A.ROW*mgl[0].A.nb);
    mgl[0].x = fasp_dvec_create(mgl[0].A.COL*mgl[0].A.nb);
    
    
    switch (amgparam->AMG_type) {
            
        case SA_AMG: // Smoothed Aggregation AMG
            status = fasp_amg_setup_sa_bsr(mgl, amgparam); break;
    
        default:
            status = fasp_amg_setup_ua_bsr(mgl, amgparam); break;
            
    }
    
    if (status < 0) goto FINISHED;
    
    fasp_gettime(&setup_end);
    
    setup_duration = setup_end - setup_start;
    
    precond_data_bsr precdata;
    precdata.print_level = amgparam->print_level;
    precdata.maxit = amgparam->maxit;
    precdata.tol = amgparam->tol;
    precdata.cycle_type = amgparam->cycle_type;
    precdata.smoother = amgparam->smoother;
    precdata.presmooth_iter = amgparam->presmooth_iter;
    precdata.postsmooth_iter = amgparam->postsmooth_iter;
    precdata.coarsening_type = amgparam->coarsening_type;
    precdata.relaxation = amgparam->relaxation;
    precdata.coarse_scaling = amgparam->coarse_scaling;
    precdata.amli_degree = amgparam->amli_degree;
    precdata.amli_coef = amgparam->amli_coef;
    precdata.tentative_smooth = amgparam->tentative_smooth;
    precdata.max_levels = mgl[0].num_levels;
    precdata.mgl_data = mgl;
    precdata.A = A;
    
    if (status < 0) goto FINISHED;
    
    precond prec; 
    prec.data = &precdata; 
    switch (amgparam->cycle_type) {
        case NL_AMLI_CYCLE: // Nonlinear AMLI AMG
            prec.fct = fasp_precond_dbsr_nl_amli;
            break;
        default: // V,W-Cycle AMG
            prec.fct = fasp_precond_dbsr_amg;
            break;
    }
    
    if ( print_level>=PRINT_MIN ) {
        print_cputime("BSR AMG setup", setup_duration);
    }

    //--------------------------------------------------------------
    // Part 3: solver
    //--------------------------------------------------------------
    fasp_gettime(&solver_start);

    status=fasp_solver_dbsr_itsolver(A,b,x,&prec,itparam);
   
    fasp_gettime(&solver_end);
    
    solver_duration = solver_end - solver_start;
    
    if ( print_level>=PRINT_MIN ) {
        print_cputime("BSR AMG Krylov method totally", setup_duration+solver_duration);
    }
    
 FINISHED:
    fasp_amg_data_bsr_free(mgl);    
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Finish]\n", __FUNCTION__);
#endif

    if (status == ERROR_ALLOC_MEM) goto MEMORY_ERROR;
    return status;
    
 MEMORY_ERROR:
    printf("### ERROR: %s cannot allocate memory!\n", __FUNCTION__);
    exit(status);    
}

/**
 * \fn INT fasp_solver_dbsr_krylov_amg_nk (dBSRmat *A, dvector *b, dvector *x,
 *                                         itsolver_param *itparam, AMG_param *amgparam
 *                                         dCSRmat *A_nk, dCSRmat *P_nk, dCSRmat *R_nk)
 *
 * \brief Solve Ax=b by AMG with extra near kernel solve preconditioned Krylov methods
 *
 * \param A         Pointer to the coeff matrix in dBSRmat format
 * \param b         Pointer to the right hand side in dvector format
 * \param x         Pointer to the approx solution in dvector format
 * \param itparam   Pointer to parameters for iterative solvers
 * \param amgparam  Pointer to parameters of AMG
 * \param A_nk      Pointer to the coeff matrix for near kernel space in dBSRmat format
 * \param P_nk      Pointer to the prolongation for near kernel space in dBSRmat format
 * \param R_nk      Pointer to the restriction for near kernel space in dBSRmat format
 *
 * \return          Number of iterations if succeed
 *
 * \author Xiaozhe Hu
 * \date   05/26/2012
 */
INT fasp_solver_dbsr_krylov_amg_nk (dBSRmat *A,
                                 dvector *b,
                                 dvector *x,
                                 itsolver_param *itparam,
                                 AMG_param *amgparam,
                                 dCSRmat *A_nk,
                                 dCSRmat *P_nk,
                                 dCSRmat *R_nk)
{
    //--------------------------------------------------------------
    // Part 1: prepare
    // --------------------------------------------------------------
    //! parameters of iterative method
    const SHORT print_level = itparam->print_level;
    const SHORT max_levels = amgparam->max_levels;
    
    // return variable
    INT status = FASP_SUCCESS;
    
    // data of AMG
    AMG_data_bsr *mgl=fasp_amg_data_bsr_create(max_levels);
    
    // timing
    REAL setup_start, setup_end, solver_start, solver_end, solver_duration, setup_duration;
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Start]\n", __FUNCTION__);
#endif
    
    //--------------------------------------------------------------
    //Part 2: set up the preconditioner
    //--------------------------------------------------------------
    fasp_gettime(&setup_start);
    
    // initialize A, b, x for mgl[0]
    mgl[0].A = fasp_dbsr_create(A->ROW, A->COL, A->NNZ, A->nb, A->storage_manner);
    fasp_dbsr_cp(A,  &(mgl[0].A));
    //mgl[0].A.ROW = A->ROW; mgl[0].A.COL = A->COL; mgl[0].A.NNZ = A->NNZ;
    //mgl[0].A.nb = A->nb; mgl[0].A.storage_manner = A->storage_manner;
    //mgl[0].A.IA = A->IA; mgl[0].A.JA = A->JA; mgl[0].A.val = A->val;
    mgl[0].b = fasp_dvec_create(mgl[0].A.ROW*mgl[0].A.nb);
    mgl[0].x = fasp_dvec_create(mgl[0].A.COL*mgl[0].A.nb);
    
    // near kernel space
    //mgl[0].A_nk = A_nk;
    mgl[0].A_nk = NULL;
    mgl[0].P_nk = P_nk;
    mgl[0].R_nk = R_nk;
    
    switch (amgparam->AMG_type) {
            
        case SA_AMG: // Smoothed Aggregation AMG
            status = fasp_amg_setup_sa_bsr(mgl, amgparam); break;
            
        default:
            status = fasp_amg_setup_ua_bsr(mgl, amgparam); break;
            
    }
    
    if (status < 0) goto FINISHED;
    
    fasp_gettime(&setup_end);
    
    setup_duration = setup_end - setup_start;
    
    precond_data_bsr precdata;
    precdata.print_level = amgparam->print_level;
    precdata.maxit = amgparam->maxit;
    precdata.tol = amgparam->tol;
    precdata.cycle_type = amgparam->cycle_type;
    precdata.smoother = amgparam->smoother;
    precdata.presmooth_iter = amgparam->presmooth_iter;
    precdata.postsmooth_iter = amgparam->postsmooth_iter;
    precdata.coarsening_type = amgparam->coarsening_type;
    precdata.relaxation = amgparam->relaxation;
    precdata.coarse_scaling = amgparam->coarse_scaling;
    precdata.amli_degree = amgparam->amli_degree;
    precdata.amli_coef = amgparam->amli_coef;
    precdata.tentative_smooth = amgparam->tentative_smooth;
    precdata.max_levels = mgl[0].num_levels;
    precdata.mgl_data = mgl;
    precdata.A = A;
    
#if WITH_UMFPACK // use UMFPACK directly
    dCSRmat A_tran;
    fasp_dcsr_trans(A_nk, &A_tran);
    fasp_dcsr_sort(&A_tran);
    precdata.A_nk = &A_tran;
#else
    precdata.A_nk = A_nk;
#endif    
    precdata.P_nk = P_nk;
    precdata.R_nk = R_nk;
    
    if (status < 0) goto FINISHED;
    
    precond prec;
    prec.data = &precdata;
    
    prec.fct = fasp_precond_dbsr_amg_nk;
    
    if ( print_level>=PRINT_MIN ) {
        print_cputime("BSR AMG setup", setup_duration);
    }
    
    //--------------------------------------------------------------
    // Part 3: solver
    //--------------------------------------------------------------
    fasp_gettime(&solver_start);
    
    status=fasp_solver_dbsr_itsolver(A,b,x,&prec,itparam);
    
    fasp_gettime(&solver_end);
    
    solver_duration = solver_end - solver_start;
    
    if ( print_level>=PRINT_MIN ) {
        print_cputime("BSR AMG NK Krylov method totally", setup_duration+solver_duration);
    }
    
FINISHED:
    fasp_amg_data_bsr_free(mgl);
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Finish]\n", __FUNCTION__);
#endif

#if WITH_UMFPACK // use UMFPACK directly
    fasp_dcsr_free(&A_tran);
#endif
    if (status == ERROR_ALLOC_MEM) goto MEMORY_ERROR;
    return status;
    
MEMORY_ERROR:
    printf("### ERROR: %s cannot allocate memory!\n", __FUNCTION__);
    exit(status);
}

/**
 * \fn INT fasp_solver_dbsr_krylov_nk_amg (dBSRmat *A, dvector *b, dvector *x,
 *                                      itsolver_param *itparam, AMG_param *amgparam,
 *                                      const INT nk_dim, dvector *nk)
 *
 * \brief Solve Ax=b by AMG preconditioned Krylov methods
 *
 * \param A         Pointer to the coeff matrix in dBSRmat format
 * \param b         Pointer to the right hand side in dvector format
 * \param x         Pointer to the approx solution in dvector format
 * \param itparam   Pointer to parameters for iterative solvers
 * \param amgparam  Pointer to parameters of AMG
 * \param nk_dim    Dimension of the near kernel spaces
 * \param nk        Pointer to the near kernal spaces
 *
 * \return          Number of iterations if succeed
 *
 * \author Xiaozhe Hu
 * \date   05/27/2012
 */
INT fasp_solver_dbsr_krylov_nk_amg (dBSRmat *A,
                                 dvector *b,
                                 dvector *x,
                                 itsolver_param *itparam,
                                 AMG_param *amgparam,
                                 const INT nk_dim,
                                 dvector *nk)
{
    //--------------------------------------------------------------
    // Part 1: prepare
    // --------------------------------------------------------------
    //! parameters of iterative method
    const SHORT print_level = itparam->print_level;
    const SHORT max_levels = amgparam->max_levels;
    
    // local variable
    INT i;
    
    // return variable
    INT status = FASP_SUCCESS;
    
    // data of AMG
    AMG_data_bsr *mgl=fasp_amg_data_bsr_create(max_levels);
    
    // timing
    REAL setup_start, setup_end, solver_start, solver_end, solver_duration, setup_duration;
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Start]\n", __FUNCTION__);
#endif

    //--------------------------------------------------------------
    //Part 2: set up the preconditioner
    //--------------------------------------------------------------
    fasp_gettime(&setup_start);
    
    // initialize A, b, x for mgl[0]
    mgl[0].A = fasp_dbsr_create(A->ROW, A->COL, A->NNZ, A->nb, A->storage_manner);
    fasp_dbsr_cp(A,  &(mgl[0].A));
    //mgl[0].A.ROW = A->ROW; mgl[0].A.COL = A->COL; mgl[0].A.NNZ = A->NNZ;
    //mgl[0].A.nb = A->nb; mgl[0].A.storage_manner = A->storage_manner;
    //mgl[0].A.IA = A->IA; mgl[0].A.JA = A->JA; mgl[0].A.val = A->val;
    mgl[0].b = fasp_dvec_create(mgl[0].A.ROW*mgl[0].A.nb);
    mgl[0].x = fasp_dvec_create(mgl[0].A.COL*mgl[0].A.nb);
    
    /*-----------------------*/
    /*-- setup null spaces --*/
    /*-----------------------*/
    
    // null space for whole Jacobian
    mgl[0].near_kernel_dim   = nk_dim;
    mgl[0].near_kernel_basis = (REAL **)fasp_mem_calloc(mgl->near_kernel_dim, sizeof(REAL*));
     
    for ( i=0; i < mgl->near_kernel_dim; ++i ) mgl[0].near_kernel_basis[i] = nk[i].val;
    
    switch (amgparam->AMG_type) {
            
        case SA_AMG: // Smoothed Aggregation AMG
            status = fasp_amg_setup_sa_bsr(mgl, amgparam); break;
            
        default:
            status = fasp_amg_setup_ua_bsr(mgl, amgparam); break;
            
    }
    
    if (status < 0) goto FINISHED;
    
    fasp_gettime(&setup_end);
    
    setup_duration = setup_end - setup_start;
    
    precond_data_bsr precdata;
    precdata.print_level = amgparam->print_level;
    precdata.maxit = amgparam->maxit;
    precdata.tol = amgparam->tol;
    precdata.cycle_type = amgparam->cycle_type;
    precdata.smoother = amgparam->smoother;
    precdata.presmooth_iter = amgparam->presmooth_iter;
    precdata.postsmooth_iter = amgparam->postsmooth_iter;
    precdata.coarsening_type = amgparam->coarsening_type;
    precdata.relaxation = amgparam->relaxation;
    precdata.coarse_scaling = amgparam->coarse_scaling;
    precdata.amli_degree = amgparam->amli_degree;
    precdata.amli_coef = amgparam->amli_coef;
    precdata.tentative_smooth = amgparam->tentative_smooth;
    precdata.max_levels = mgl[0].num_levels;
    precdata.mgl_data = mgl;
    precdata.A = A;
    
    if (status < 0) goto FINISHED;
    
    precond prec;
    prec.data = &precdata;
    switch (amgparam->cycle_type) {
        case NL_AMLI_CYCLE: // Nonlinear AMLI AMG
            prec.fct = fasp_precond_dbsr_nl_amli;
            break;
        default: // V,W-Cycle AMG
            prec.fct = fasp_precond_dbsr_amg;
            break;
    }
    
    if ( print_level>=PRINT_MIN ) {
        print_cputime("BSR AMG setup", setup_duration);
    }
    
    //--------------------------------------------------------------
    // Part 3: solver
    //--------------------------------------------------------------
    fasp_gettime(&solver_start);
    
    status=fasp_solver_dbsr_itsolver(A,b,x,&prec,itparam);
    
    fasp_gettime(&solver_end);
    
    solver_duration = solver_end - solver_start;
    
    if ( print_level>=PRINT_MIN ) {
        print_cputime("BSR AMG Krylov method totally", setup_duration+solver_duration);
    }
    
FINISHED:
    fasp_amg_data_bsr_free(mgl);
    
#if DEBUG_MODE
    printf("### DEBUG: %s ...... [Finish]\n", __FUNCTION__);
#endif

    if (status == ERROR_ALLOC_MEM) goto MEMORY_ERROR;
    return status;
    
MEMORY_ERROR:
    printf("### ERROR: %s cannot allocate memory!\n", __FUNCTION__);
    exit(status);
}

/*---------------------------------*/
/*--        End of File          --*/
/*---------------------------------*/
