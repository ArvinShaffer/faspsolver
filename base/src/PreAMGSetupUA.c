/*! \file PreAMGSetupUA.c
 *
 *  \brief Unsmoothed aggregation AMG: SETUP phase
 *
 *  \note This file contains Level-4 (Pre) functions. It requires
 *        AuxArray.c, AuxMemory.c, AuxMessage.c, AuxSmallMat.c, AuxTiming.c,
 *        AuxVector.c, BlaFormat.c, BlaILUSetupBSR.c, BlaILUSetupCSR.c,
 *        BlaSchwarzSetup.c, BlaSmallMat.c, BlaSparseBLC.c, BlaSparseBSR.c,
 *        BlaSparseCSR.c, BlaSpmvBSR.c, BlaSpmvCSR.c, and PreMGRecurAMLI.c
 *
 *  \note Setup A, P, PT and levels using the unsmoothed aggregation algorithm;
 *        Refer to P. Vanek, J. Madel and M. Brezina
 *        Algebraic Multigrid on Unstructured Meshes, 1994
 */

#include <math.h>
#include <time.h>

#include "fasp.h"
#include "fasp_functs.h"

/*---------------------------------*/
/*--  Declare Private Functions  --*/
/*---------------------------------*/

#include "PreAMGAggregationCSR.inl"
#include "PreAMGAggregationBSR.inl"

static SHORT amg_setup_unsmoothP_unsmoothR(AMG_data *, AMG_param *);
static SHORT amg_setup_unsmoothP_unsmoothR_bsr(AMG_data_bsr *, AMG_param *);

/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/**
 * \fn SHORT fasp_amg_setup_ua (AMG_data *mgl, AMG_param *param)
 *
 * \brief Set up phase of unsmoothed aggregation AMG
 *
 * \param mgl    Pointer to AMG data: AMG_data
 * \param param  Pointer to AMG parameters: AMG_param
 *
 * \return       FASP_SUCCESS if successed; otherwise, error information.
 *
 * \author Xiaozhe Hu
 * \date   12/28/2011
 */
SHORT fasp_amg_setup_ua (AMG_data   *mgl,
                         AMG_param  *param)
{
#if DEBUG_MODE > 0
    printf("### DEBUG: %s ...... [Start]\n", __FUNCTION__);
    printf("### DEBUG: nr=%d, nc=%d, nnz=%d\n",
           mgl[0].A.row, mgl[0].A.col, mgl[0].A.nnz);
#endif

    SHORT status = amg_setup_unsmoothP_unsmoothR(mgl, param);

#if DEBUG_MODE > 0
    printf("### DEBUG: %s ...... [Finish]\n", __FUNCTION__);
#endif

    return status;
}

/**
 * \fn INT fasp_amg_setup_ua_bsr (AMG_data_bsr *mgl, AMG_param *param)
 *
 * \brief Set up phase of unsmoothed aggregation AMG (BSR format)
 *
 * \param mgl    Pointer to AMG data: AMG_data_bsr
 * \param param  Pointer to AMG parameters: AMG_param
 *
 * \return       FASP_SUCCESS if successed; otherwise, error information.
 *
 * \author Xiaozhe Hu
 * \date   03/16/2012
 */
SHORT fasp_amg_setup_ua_bsr (AMG_data_bsr   *mgl,
                             AMG_param      *param)
{
#if DEBUG_MODE > 0
    printf("### DEBUG: %s ...... [Start]\n", __FUNCTION__);
#endif

    SHORT status = amg_setup_unsmoothP_unsmoothR_bsr(mgl, param);

#if DEBUG_MODE > 0
    printf("### DEBUG: %s ...... [Finish]\n", __FUNCTION__);
#endif

    return status;
}

/*---------------------------------*/
/*--      Private Functions      --*/
/*---------------------------------*/

/**
 * \fn static SHORT amg_setup_unsmoothP_unsmoothR (AMG_data *mgl, AMG_param *param)
 *
 * \brief Setup phase of plain aggregation AMG, using unsmoothed P and unsmoothed A
 *
 * \param mgl    Pointer to AMG_data
 * \param param  Pointer to AMG_param
 *
 * \return       FASP_SUCCESS if succeed, error otherwise
 *
 * \author Xiaozhe Hu
 * \date   02/21/2011
 *
 * Modified by Chunsheng Feng, Xiaoqiang Yue on 05/27/2012.
 * Modified by Chensong Zhang on 05/10/2013: adjust the structure.
 * Modified by Chensong Zhang on 07/26/2014: handle coarsening errors.
 * Modified by Chensong Zhang on 09/23/2014: check coarse spaces.
 * Modified by Zheng Li on 01/13/2015: adjust coarsening stop criterion.
 * Modified by Zheng Li on 03/22/2015: adjust coarsening ratio.
 */
static SHORT amg_setup_unsmoothP_unsmoothR (AMG_data   *mgl,
                                            AMG_param  *param)
{
    const SHORT prtlvl     = param->print_level;
    const SHORT cycle_type = param->cycle_type;
    const SHORT csolver    = param->coarse_solver;
    const SHORT min_cdof   = MAX(param->coarse_dof,50);
    const INT   m          = mgl[0].A.row;

    // empiric value
    const REAL  cplxmax    = 3.0;
    const REAL  xsi        = 0.6;
    INT   icum = 1.0;
    REAL  eta, fracratio;

    // local variables
    SHORT         max_levels = param->max_levels, lvl = 0, status = FASP_SUCCESS;
    INT           i;
    REAL          setup_start, setup_end;
    ILU_param     iluparam;
    Schwarz_param swzparam;

#if DEBUG_MODE > 0
    printf("### DEBUG: %s ...... [Start]\n", __FUNCTION__);
    printf("### DEBUG: nr=%d, nc=%d, nnz=%d\n",
           mgl[0].A.row, mgl[0].A.col, mgl[0].A.nnz);
#endif

    fasp_gettime(&setup_start);

    // level info (fine: 0; coarse: 1)
    ivector *vertices = (ivector *)fasp_mem_calloc(max_levels,sizeof(ivector));

    // each level stores the information of the number of aggregations
    INT *num_aggs = (INT *)fasp_mem_calloc(max_levels,sizeof(INT));

    // each level stores the information of the strongly coupled neighborhoods
    dCSRmat *Neighbor = (dCSRmat *)fasp_mem_calloc(max_levels,sizeof(dCSRmat));

    // Initialize level information
    for ( i = 0; i < max_levels; ++i ) num_aggs[i] = 0;

    mgl[0].near_kernel_dim   = 1;
    mgl[0].near_kernel_basis = (REAL **)fasp_mem_calloc(mgl->near_kernel_dim,sizeof(REAL*));

    for ( i = 0; i < mgl->near_kernel_dim; ++i ) {
        mgl[0].near_kernel_basis[i] = (REAL *)fasp_mem_calloc(m,sizeof(REAL));
        fasp_array_set(m, mgl[0].near_kernel_basis[i], 1.0);
    }

    // Initialize ILU parameters
    mgl->ILU_levels = param->ILU_levels;
    if ( param->ILU_levels > 0 ) {
        iluparam.print_level = param->print_level;
        iluparam.ILU_lfil    = param->ILU_lfil;
        iluparam.ILU_droptol = param->ILU_droptol;
        iluparam.ILU_relax   = param->ILU_relax;
        iluparam.ILU_type    = param->ILU_type;
    }

    // Initialize Schwarz parameters
    mgl->Schwarz_levels = param->Schwarz_levels;
    if ( param->Schwarz_levels > 0 ) {
        swzparam.Schwarz_mmsize = param->Schwarz_mmsize;
        swzparam.Schwarz_maxlvl = param->Schwarz_maxlvl;
        swzparam.Schwarz_type   = param->Schwarz_type;
        swzparam.Schwarz_blksolver = param->Schwarz_blksolver;
    }

    // Initialize AMLI coefficients
    if ( cycle_type == AMLI_CYCLE ) {
        const INT amlideg = param->amli_degree;
        param->amli_coef = (REAL *)fasp_mem_calloc(amlideg+1,sizeof(REAL));
        REAL lambda_max = 2.0, lambda_min = lambda_max/4;
        fasp_amg_amli_coef(lambda_max, lambda_min, amlideg, param->amli_coef);
    }

#if DIAGONAL_PREF
    fasp_dcsr_diagpref(&mgl[0].A); // reorder each row to make diagonal appear first
#endif

    /*----------------------------*/
    /*--- checking aggregation ---*/
    /*----------------------------*/

    // Pairwise matching algorithm requires diagonal preference ordering
    if ( param->aggregation_type == PAIRWISE ) {
        param->pair_number = MIN(param->pair_number, max_levels);
        fasp_dcsr_diagpref(&mgl[0].A);
    }

    // Main AMG setup loop
    while ( (mgl[lvl].A.row > min_cdof) && (lvl < max_levels-1) ) {

#if DEBUG_MODE > 2
        printf("### DEBUG: level = %d, row = %d, nnz = %d\n",
               lvl, mgl[lvl].A.row, mgl[lvl].A.nnz);
#endif

        /*-- Setup ILU decomposition if necessary */
        if ( lvl < param->ILU_levels ) {
            status = fasp_ilu_dcsr_setup(&mgl[lvl].A, &mgl[lvl].LU, &iluparam);
            if ( status < 0 ) {
                if ( prtlvl > PRINT_MIN ) {
                    printf("### WARNING: ILU setup on level-%d failed!\n", lvl);
                    printf("### WARNING: Disable ILU for level >= %d.\n", lvl);
                }
                param->ILU_levels = lvl;
            }
        }

        /*-- Setup Schwarz smoother if necessary */
        if ( lvl < param->Schwarz_levels ) {
            mgl[lvl].Schwarz.A=fasp_dcsr_sympart(&mgl[lvl].A);
            fasp_dcsr_shift(&(mgl[lvl].Schwarz.A), 1);
            fasp_Schwarz_setup(&mgl[lvl].Schwarz, &swzparam);
        }

        /*-- Aggregation --*/
        switch ( param->aggregation_type ) {

            case VMB: // VMB aggregation

                status = aggregation_vmb(&mgl[lvl].A, &vertices[lvl], param,
                                         lvl+1, &Neighbor[lvl], &num_aggs[lvl]);

                /*-- Choose strength threshold adaptively --*/
                if ( num_aggs[lvl]*4 > mgl[lvl].A.row )
                    param->strong_coupled /= 2;
                else if ( num_aggs[lvl]*1.25 < mgl[lvl].A.row )
                    param->strong_coupled *= 2;

                break;

            default: // pairwise matching aggregation

                status = aggregation_pairwise(mgl, param, lvl, vertices,
                                              &num_aggs[lvl]);

                break;
        }

        // Check 1: Did coarsening step succeed?
        if ( status < 0 ) {
            // When error happens, stop at the current multigrid level!
            if ( prtlvl > PRINT_MIN ) {
                printf("### WARNING: Forming aggregates on level-%d failed!\n", lvl);
            }
            status = FASP_SUCCESS; 
            fasp_ivec_free(&vertices[lvl]);
            fasp_dcsr_free(&Neighbor[lvl]);
            break;
        }

        /*-- Form Prolongation --*/
        form_tentative_p(&vertices[lvl], &mgl[lvl].P, mgl[0].near_kernel_basis,
                         lvl+1, num_aggs[lvl]);

        // Check 2: Is coarse sparse too small?
        if ( mgl[lvl].P.col < MIN_CDOF ) {
            fasp_ivec_free(&vertices[lvl]);
            fasp_dcsr_free(&Neighbor[lvl]);
            break;
        }

        // Check 3: Does this coarsening step too aggressive?
        if ( mgl[lvl].P.row > mgl[lvl].P.col * MAX_CRATE ) {
            if ( prtlvl > PRINT_MIN ) {
                printf("### WARNING: Coarsening might be too aggressive!\n");
                printf("### WARNING: Fine level = %d, coarse level = %d. Discard!\n",
                       mgl[lvl].P.row, mgl[lvl].P.col);
            }
            fasp_ivec_free(&vertices[lvl]);
            fasp_dcsr_free(&Neighbor[lvl]);
            break;
        }

        /*-- Form restriction --*/
        fasp_dcsr_trans(&mgl[lvl].P, &mgl[lvl].R);

        /*-- Form coarse level stiffness matrix --*/
        fasp_blas_dcsr_rap_agg(&mgl[lvl].R, &mgl[lvl].A, &mgl[lvl].P,
                               &mgl[lvl+1].A);

        fasp_dcsr_free(&Neighbor[lvl]);
        fasp_ivec_free(&vertices[lvl]);

        ++lvl;

#if DIAGONAL_PREF
        fasp_dcsr_diagpref(&mgl[lvl].A); // reorder each row to make diagonal appear first
#endif

        // Check 4: Is this coarsening ratio too small?
        if ( (REAL)mgl[lvl].P.col > mgl[lvl].P.row * MIN_CRATE ) {
            param->quality_bound *= 2.0;
        }
        
    } // end of the main while loop

    // Setup coarse level systems for direct solvers
    switch (csolver) {

#if WITH_MUMPS
        case SOLVER_MUMPS: {
            // Setup MUMPS direct solver on the coarsest level
            mgl[lvl].mumps.job = 1;
            fasp_solver_mumps_steps(&mgl[lvl].A, &mgl[lvl].b, &mgl[lvl].x, &mgl[lvl].mumps);
            break;
        }
#endif

#if WITH_UMFPACK
        case SOLVER_UMFPACK: {
            // Need to sort the matrix A for UMFPACK to work
            dCSRmat Ac_tran;
            Ac_tran = fasp_dcsr_create(mgl[lvl].A.row, mgl[lvl].A.col, mgl[lvl].A.nnz);
            fasp_dcsr_transz(&mgl[lvl].A, NULL, &Ac_tran);
            // It is equivalent to do transpose and then sort
            //     fasp_dcsr_trans(&mgl[lvl].A, &Ac_tran);
            //     fasp_dcsr_sort(&Ac_tran);
            fasp_dcsr_cp(&Ac_tran, &mgl[lvl].A);
            fasp_dcsr_free(&Ac_tran);
            mgl[lvl].Numeric = fasp_umfpack_factorize(&mgl[lvl].A, 0);
            break;
        }
#endif

#if WITH_PARDISO
        case SOLVER_PARDISO: {
             fasp_dcsr_sort(&mgl[lvl].A);
             fasp_pardiso_factorize(&mgl[lvl].A, &mgl[lvl].pdata, 0);
             break;
         }
#endif

        default:
            // Do nothing!
            break;
    }

    // setup total level number and current level
    mgl[0].num_levels = max_levels = lvl+1;
    mgl[0].w          = fasp_dvec_create(m);

    for ( lvl = 1; lvl < max_levels; ++lvl) {
        INT mm = mgl[lvl].A.row;
        mgl[lvl].num_levels = max_levels;
        mgl[lvl].b          = fasp_dvec_create(mm);
        mgl[lvl].x          = fasp_dvec_create(mm);

        mgl[lvl].cycle_type     = cycle_type; // initialize cycle type!
        mgl[lvl].ILU_levels     = param->ILU_levels - lvl; // initialize ILU levels!
        mgl[lvl].Schwarz_levels = param->Schwarz_levels -lvl; // initialize Schwarz!

        if ( cycle_type == NL_AMLI_CYCLE )
            mgl[lvl].w = fasp_dvec_create(3*mm);
        else
            mgl[lvl].w = fasp_dvec_create(2*mm);
    }

    // setup for cycle type of unsmoothed aggregation
    eta = xsi/((1-xsi)*(cplxmax-1));
    mgl[0].cycle_type = 1;
    mgl[max_levels-1].cycle_type = 0;

    for (lvl = 1; lvl < max_levels-1; ++lvl) {
        fracratio = (REAL)mgl[lvl].A.nnz/mgl[0].A.nnz;
        mgl[lvl].cycle_type = (INT)(pow((REAL)xsi,(REAL)lvl)/(eta*fracratio*icum));
        // safe-guard step: make cycle type >= 1 and <= 2
        mgl[lvl].cycle_type = MAX(1, MIN(2, mgl[lvl].cycle_type));
        icum = icum * mgl[lvl].cycle_type;
    }

    if ( prtlvl > PRINT_NONE ) {
        fasp_gettime(&setup_end);
        print_amgcomplexity(mgl,prtlvl);
        print_cputime("Unsmoothed aggregation setup", setup_end - setup_start);
    }

    fasp_mem_free(Neighbor);
    fasp_mem_free(vertices);
    fasp_mem_free(num_aggs);

#if DEBUG_MODE > 0
    printf("### DEBUG: %s ...... [Finish]\n", __FUNCTION__);
#endif

    return status;
}

/**
 * \fn static SHORT amg_setup_unsmoothP_unsmoothR_bsr (AMG_data_bsr *mgl,
 *                                                     AMG_param *param)
 *
 * \brief Set up phase of plain aggregation AMG, using unsmoothed P and unsmoothed A
 *        in BSR format
 *
 * \param mgl    Pointer to AMG data: AMG_data_bsr
 * \param param  Pointer to AMG parameters: AMG_param
 *
 * \return       FASP_SUCCESS if succeed, error otherwise
 *
 * \author Xiaozhe Hu
 * \date   03/16/2012
 *
 * Modified by Chensong Zhang on 05/10/2013: adjust the structure.
 */
static SHORT amg_setup_unsmoothP_unsmoothR_bsr (AMG_data_bsr   *mgl,
                                                AMG_param      *param)
{
    
    const SHORT prtlvl   = param->print_level;
    const SHORT csolver  = param->coarse_solver;
    const SHORT min_cdof = MAX(param->coarse_dof,50);
    const INT   m        = mgl[0].A.ROW;
    const INT   nb       = mgl[0].A.nb;

    SHORT     max_levels=param->max_levels;
    SHORT     i, lvl=0, status=FASP_SUCCESS;
    REAL      setup_start, setup_end;
    
    AMG_data *mgl_csr=fasp_amg_data_create(max_levels);
    
    dCSRmat temp1, temp2;

#if DEBUG_MODE > 0
    printf("### DEBUG: %s ...... [Start]\n", __FUNCTION__);
    printf("### DEBUG: nr=%d, nc=%d, nnz=%d\n",
           mgl[0].A.ROW, mgl[0].A.COL, mgl[0].A.NNZ);
#endif

    fasp_gettime(&setup_start);

    /*-----------------------*/
    /*--local working array--*/
    /*-----------------------*/

    // level info (fine: 0; coarse: 1)
    ivector *vertices = (ivector *)fasp_mem_calloc(max_levels, sizeof(ivector));

    //each elvel stores the information of the number of aggregations
    INT *num_aggs = (INT *)fasp_mem_calloc(max_levels, sizeof(INT));

    // each level stores the information of the strongly coupled neighborhoods
    dCSRmat *Neighbor = (dCSRmat *)fasp_mem_calloc(max_levels, sizeof(dCSRmat));

    for ( i=0; i<max_levels; ++i ) num_aggs[i] = 0;

    /*-----------------------*/
    /*-- setup null spaces --*/
    /*-----------------------*/

    // null space for whole Jacobian
    /*
     mgl[0].near_kernel_dim   = 1;
     mgl[0].near_kernel_basis = (REAL **)fasp_mem_calloc(mgl->near_kernel_dim, sizeof(REAL*));

     for ( i=0; i < mgl->near_kernel_dim; ++i ) mgl[0].near_kernel_basis[i] = NULL;
     */

    /*-----------------------*/
    /*-- setup ILU param   --*/
    /*-----------------------*/
    
    // initialize ILU parameters
    mgl->ILU_levels = param->ILU_levels;
    ILU_param iluparam;

    if ( param->ILU_levels > 0 ) {
        iluparam.print_level = param->print_level;
        iluparam.ILU_lfil    = param->ILU_lfil;
        iluparam.ILU_droptol = param->ILU_droptol;
        iluparam.ILU_relax   = param->ILU_relax;
        iluparam.ILU_type    = param->ILU_type;
    }
    
    /*----------------------------*/
    /*--- checking aggregation ---*/
    /*----------------------------*/
    if (param->aggregation_type == PAIRWISE)
        param->pair_number = MIN(param->pair_number, max_levels);
    
    // Main AMG setup loop
    while ( (mgl[lvl].A.ROW > min_cdof) && (lvl < max_levels-1) ) {

        /*-- setup ILU decomposition if necessary */
        if ( lvl < param->ILU_levels ) {
            status = fasp_ilu_dbsr_setup(&mgl[lvl].A, &mgl[lvl].LU, &iluparam);
            if ( status < 0 ) {
                if ( prtlvl > PRINT_MIN ) {
                    printf("### WARNING: ILU setup on level-%d failed!\n", lvl);
                    printf("### WARNING: Disable ILU for level >= %d.\n", lvl);
                }
                param->ILU_levels = lvl;
            }
        }
        
        /*-- get the diagonal inverse --*/
        mgl[lvl].diaginv = fasp_dbsr_getdiaginv(&mgl[lvl].A);
        
        /*-- Aggregation --*/
        //mgl[lvl].PP =  fasp_dbsr_getblk_dcsr(&mgl[lvl].A);
        mgl[lvl].PP = fasp_dbsr_Linfinity_dcsr(&mgl[lvl].A);  // TODO: Try different way to form the scalar block!!  -- Xiaozhe
        
        switch ( param->aggregation_type ) {

            case VMB: // VMB aggregation

                status = aggregation_vmb(&mgl[lvl].PP, &vertices[lvl], param,
                                         lvl+1, &Neighbor[lvl], &num_aggs[lvl]);

                /*-- Choose strength threshold adaptively --*/
                if ( num_aggs[lvl]*4 > mgl[lvl].PP.row )
                    param->strong_coupled /= 4;
                else if ( num_aggs[lvl]*1.25 < mgl[lvl].PP.row )
                    param->strong_coupled *= 1.5;

                break;

            default: // pairwise matching aggregation

                //mgl_csr[lvl].A=fasp_dcsr_create(mgl[lvl].PP.row,mgl[lvl].PP.col,mgl[lvl].PP.nnz);
                //fasp_dcsr_cp(&mgl[lvl].PP, &mgl_csr[lvl].A);
                mgl_csr[lvl].A = mgl[lvl].PP;

                status = aggregation_pairwise(mgl_csr, param, lvl, vertices, &num_aggs[lvl]);

                // TODO: Need to design better algorithm for this part -- Xiaozhe

               break;
        }

        if ( status < 0 ) {
            // When error happens, force solver to use the current multigrid levels!
            if ( prtlvl > PRINT_MIN ) {
                printf("### WARNING: Forming aggregates on level-%d failed!\n", lvl);
            }
            status = FASP_SUCCESS; break;
        }
        
        /* -- Form Prolongation --*/
        //form_tentative_p_bsr(&vertices[lvl], &mgl[lvl].P, &mgl[0], lvl+1, num_aggs[lvl]);

        if ( lvl == 0 && mgl[0].near_kernel_dim >0 ) {
            form_tentative_p_bsr1(&vertices[lvl], &mgl[lvl].P, &mgl[0], lvl+1,
                                  num_aggs[lvl], mgl[0].near_kernel_dim,
                                  mgl[0].near_kernel_basis);
        }
        else {

            form_boolean_p_bsr(&vertices[lvl], &mgl[lvl].P, &mgl[0], lvl+1,
                               num_aggs[lvl]);
        }
        
        /*-- Form resitriction --*/
        fasp_dbsr_trans(&mgl[lvl].P, &mgl[lvl].R);

        /*-- Form coarse level stiffness matrix --*/
        fasp_blas_dbsr_rap(&mgl[lvl].R, &mgl[lvl].A, &mgl[lvl].P, &mgl[lvl+1].A);
        
        /* -- Form extra near kernal space if needed --*/
        if (mgl[lvl].A_nk != NULL){

            mgl[lvl+1].A_nk = (dCSRmat *)fasp_mem_calloc(1, sizeof(dCSRmat));
            mgl[lvl+1].P_nk = (dCSRmat *)fasp_mem_calloc(1, sizeof(dCSRmat));
            mgl[lvl+1].R_nk = (dCSRmat *)fasp_mem_calloc(1, sizeof(dCSRmat));

            temp1 = fasp_format_dbsr_dcsr(&mgl[lvl].R);
            fasp_blas_dcsr_mxm(&temp1, mgl[lvl].P_nk, mgl[lvl+1].P_nk);
            fasp_dcsr_trans(mgl[lvl+1].P_nk, mgl[lvl+1].R_nk);
            temp2 = fasp_format_dbsr_dcsr(&mgl[lvl+1].A);
            fasp_blas_dcsr_rap(mgl[lvl+1].R_nk, &temp2, mgl[lvl+1].P_nk, mgl[lvl+1].A_nk);
            fasp_dcsr_free(&temp1);
            fasp_dcsr_free(&temp2);

        }
        
        fasp_dcsr_free(&Neighbor[lvl]);
        fasp_ivec_free(&vertices[lvl]);

        ++lvl;
    }
    
    // Setup coarse level systems for direct solvers (BSR version)
    switch (csolver) {

#if WITH_MUMPS
        case SOLVER_MUMPS: {
            // Setup MUMPS direct solver on the coarsest level
            mgl[lvl].Ac = fasp_format_dbsr_dcsr(&mgl[lvl].A);
            mgl[lvl].mumps.job = 1;
            fasp_solver_mumps_steps(&mgl[lvl].Ac, &mgl[lvl].b, &mgl[lvl].x, &mgl[lvl].mumps);
            break;
        }
#endif

#if WITH_UMFPACK
        case SOLVER_UMFPACK: {
            // Need to sort the matrix A for UMFPACK to work
            mgl[lvl].Ac = fasp_format_dbsr_dcsr(&mgl[lvl].A);
            dCSRmat Ac_tran;
            fasp_dcsr_trans(&mgl[lvl].Ac, &Ac_tran);
            fasp_dcsr_sort(&Ac_tran);
            fasp_dcsr_cp(&Ac_tran, &mgl[lvl].Ac);
            fasp_dcsr_free(&Ac_tran);
            mgl[lvl].Numeric = fasp_umfpack_factorize(&mgl[lvl].Ac, 0);
            break;
        }
#endif

#if WITH_SuperLU
        case SOLVER_SUPERLU: {
            /* Setup SuperLU direct solver on the coarsest level */
            mgl[lvl].Ac = fasp_format_dbsr_dcsr(&mgl[lvl].A);
        }
#endif

#if WITH_PARDISO
        case SOLVER_PARDISO: {
            mgl[lvl].Ac = fasp_format_dbsr_dcsr(&mgl[lvl].A);
            fasp_dcsr_sort(&mgl[lvl].Ac);
            fasp_pardiso_factorize(&mgl[lvl].Ac, &mgl[lvl].pdata, 0);
            break;
         }
#endif

        default:
            // Do nothing!
            break;
    }
    

    // setup total level number and current level
    mgl[0].num_levels = max_levels = lvl+1;
    mgl[0].w = fasp_dvec_create(3*m*nb);

    if (mgl[0].A_nk != NULL){

#if WITH_UMFPACK
        // Need to sort the matrix A_nk for UMFPACK
        fasp_dcsr_trans(mgl[0].A_nk, &temp1);
        fasp_dcsr_sort(&temp1);
        fasp_dcsr_cp(&temp1, mgl[0].A_nk);
        fasp_dcsr_free(&temp1);
#endif

    }

    for ( lvl = 1; lvl < max_levels; lvl++ ) {
        const INT mm = mgl[lvl].A.ROW*nb;
        mgl[lvl].num_levels = max_levels;
        mgl[lvl].b          = fasp_dvec_create(mm);
        mgl[lvl].x          = fasp_dvec_create(mm);
        mgl[lvl].w          = fasp_dvec_create(3*mm);
        mgl[lvl].ILU_levels = param->ILU_levels - lvl; // initialize ILU levels!

        if (mgl[lvl].A_nk != NULL){

#if WITH_UMFPACK
            // Need to sort the matrix A_nk for UMFPACK
            fasp_dcsr_trans(mgl[lvl].A_nk, &temp1);
            fasp_dcsr_sort(&temp1);
            fasp_dcsr_cp(&temp1, mgl[lvl].A_nk);
            fasp_dcsr_free(&temp1);
#endif

        }

    }
    

    if ( prtlvl > PRINT_NONE ) {
        fasp_gettime(&setup_end);
        print_amgcomplexity_bsr(mgl,prtlvl);
        print_cputime("Unsmoothed aggregation (BSR) setup", setup_end - setup_start);
    }
    

    fasp_mem_free(vertices);
    fasp_mem_free(num_aggs);
    fasp_mem_free(Neighbor);

#if DEBUG_MODE > 0
    printf("### DEBUG: %s ...... [Finish]\n", __FUNCTION__);
#endif

    return status;
}

/*---------------------------------*/
/*--        End of File          --*/
/*---------------------------------*/