/*! \file AuxParam.c
 *
 *  \brief Initialize, set, or print input data and parameters
 *
 *  \note This file contains Level-0 (Aux) functions. It requires
 *        AuxInput.c and AuxMessage.c
 */

#include <stdio.h>

#include "fasp.h"
#include "fasp_functs.h"

/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/**
 * \fn void fasp_param_set (int argc, const char *argv [], input_param *iniparam)
 *
 * \brief Read input from command-line arguments
 *
 * \param argc       Number of arg input
 * \param argv       Input arguments
 * \param iniparam   Parameters to be set
 *
 * \author Chensong Zhang
 * \date   12/29/2013
 */
void fasp_param_set (int          argc,
                     const char  *argv[],
                     input_param *iniparam)
{
    int      arg_index   = 1;
    int      print_usage = 0;
    SHORT    status      = FASP_SUCCESS;

    // Option 1. set default input parameters
    fasp_param_input_init(iniparam);

    while ( arg_index < argc ) {

        if ( strcmp(argv[arg_index], "-help") == 0 ) {
            print_usage = 1; break;
        }

        // Option 2. Get parameters from an ini file
        else if ( strcmp(argv[arg_index], "-ini") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Missing ini file name!\n");
                print_usage = 1; break;
            }
            strcpy(iniparam->inifile, argv[arg_index]);
            fasp_param_input(iniparam->inifile,iniparam);
            if ( ++arg_index >= argc ) break;
        }

        // Option 3. Get parameters from command line input
        else if ( strcmp(argv[arg_index], "-print") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting print level (int between 0 and 10).\n");
                print_usage = 1; break;
            }
            iniparam->print_level = atoi(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-output") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting output type (0 or 1).\n");
                print_usage = 1; break;
            }
            iniparam->output_type = atoi(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-solver") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting solver type.\n");
                print_usage = 1; break;
            }
            iniparam->solver_type = atoi(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-precond") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting preconditioner type.\n");
                print_usage = 1; break;
            }
            iniparam->precond_type = atoi(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-maxit") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting max number of iterations.\n");
                print_usage = 1; break;
            }
            iniparam->itsolver_maxit = atoi(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-tol") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting tolerance for itsolver.\n");
                print_usage = 1; break;
            }
            iniparam->itsolver_tol = atof(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-amgmaxit") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting max number of iterations for AMG.\n");
                print_usage = 1; break;
            }
            iniparam->AMG_maxit = atoi(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-amgtol") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting tolerance for AMG.\n");
                print_usage = 1; break;
            }
            iniparam->AMG_tol = atof(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-amgtype") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting AMG type (1, 2, 3).\n");
                print_usage = 1; break;
            }
            iniparam->AMG_type = atoi(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-amgcycle") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting AMG cycle type (1, 2, 3).\n");
                print_usage = 1; break;
            }
            iniparam->AMG_cycle_type = atoi(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-amgcoarsening") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting AMG coarsening type.\n");
                print_usage = 1; break;
            }
            iniparam->AMG_coarsening_type = atoi(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-amginterplation") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting AMG interpolation type.\n");
                print_usage = 1; break;
            }
            iniparam->AMG_interpolation_type = atoi(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-amgsmoother") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting AMG smoother type.\n");
                print_usage = 1; break;
            }
            iniparam->AMG_smoother = atoi(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-amgsthreshold") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting AMG strong threshold.\n");
                print_usage = 1; break;
            }
            iniparam->AMG_strong_threshold = atof(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else if ( strcmp(argv[arg_index], "-amgscouple") == 0 ) {
            arg_index++;
            if ( arg_index >= argc ) {
                printf("### ERROR: Expecting AMG strong coupled threshold.\n");
                print_usage = 1; break;
            }
            iniparam->AMG_strong_coupled = atof(argv[arg_index]);
            if ( ++arg_index >= argc ) break;
        }

        else {
            print_usage = 1;
            break;
        }

    }

    if ( print_usage ) {

        printf("FASP command line options:\n");
        printf("================================================================\n");
        printf("  -ini              [CharValue] : Ini file name\n");
        printf("  -print            [IntValue]  : Print level\n");
        printf("  -output           [IntValue]  : Output to screen or a log file\n");
        printf("  -solver           [IntValue]  : Solver type\n");
        printf("  -precond          [IntValue]  : Preconditioner type\n");
        printf("  -maxit            [IntValue]  : Max number of iterations\n");
        printf("  -tol              [RealValue] : Tolerance for iterative solvers\n");
        printf("  -amgmaxit         [IntValue]  : Max number of AMG iterations\n");
        printf("  -amgtol           [RealValue] : Tolerance for AMG methods\n");
        printf("  -amgtype          [IntValue]  : AMG type\n");
        printf("  -amgcycle         [IntValue]  : AMG cycle type\n");
        printf("  -amgcoarsening    [IntValue]  : AMG coarsening type\n");
        printf("  -amginterpolation [IntValue]  : AMG interpolation type\n");
        printf("  -amgsmoother      [IntValue]  : AMG smoother type\n");
        printf("  -amgsthreshold    [RealValue] : AMG strong threshold\n");
        printf("  -amgscoupled      [RealValue] : AMG strong coupled threshold\n");
        printf("  -help                         : Brief help messages\n");

        exit(ERROR_INPUT_PAR);

    }

    // sanity checks
    status = fasp_param_check(iniparam);

    // if meet unexpected input, stop the program
    fasp_chkerr(status,__FUNCTION__);

}

/**
 * \fn void fasp_param_init (input_param *iniparam,
 *                           itsolver_param *itsparam,
 *                           AMG_param *amgparam,
 *                           ILU_param *iluparam,
 *                           Schwarz_param *schparam)
 *
 * \brief Initialize parameters, global variables, etc
 *
 * \param iniparam      Input parameters
 * \param itsparam      Iterative solver parameters
 * \param amgparam      AMG parameters
 * \param iluparam      ILU parameters
 * \param schparam      Schwarz parameters
 *
 * \author Chensong Zhang
 * \date   2010/08/12
 *
 * Modified by Xiaozhe Hu (01/23/2011): initialize, then set value
 * Modified by Chensong Zhang (09/12/2012): find a bug during debugging in VS08
 * Modified by Chensong Zhang (12/29/2013): rewritten
 */
void fasp_param_init (input_param    *iniparam,
                      itsolver_param *itsparam,
                      AMG_param      *amgparam,
                      ILU_param      *iluparam,
                      Schwarz_param  *schparam)
{
#if CHMEM_MODE
    total_alloc_mem   = 0; // initialize total memeory amount
    total_alloc_count = 0; // initialize alloc count
#endif

    if (itsparam) fasp_param_solver_init(itsparam);

    if (amgparam) fasp_param_amg_init(amgparam);

    if (iluparam) fasp_param_ilu_init(iluparam);

    if (schparam) fasp_param_Schwarz_init(schparam);

    if (iniparam) {
        if (itsparam) fasp_param_solver_set(itsparam,iniparam);
        if (amgparam) fasp_param_amg_set(amgparam,iniparam);
        if (iluparam) fasp_param_ilu_set(iluparam,iniparam);
        if (schparam) fasp_param_Schwarz_set(schparam,iniparam);
    }
    else {
        printf("### WARNING: No input specified. Use default values instead!\n");
    }
}

/**
 * \fn void fasp_param_input_init (input_param *iniparam)
 *
 * \brief Initialize input parameters
 *
 * \param iniparam    Input parameters
 *
 * \author Chensong Zhang
 * \date   2010/03/20
 */
void fasp_param_input_init (input_param *iniparam)
{
    strcpy(iniparam->workdir,"../data/");

    // Input/output
    iniparam->print_level              = PRINT_SOME;
    iniparam->output_type              = 0;

    // Problem information
    iniparam->problem_num              = 10;
    iniparam->solver_type              = SOLVER_CG;
    iniparam->precond_type             = PREC_AMG;
    iniparam->stop_type                = STOP_REL_RES;

    // Solver parameters
    iniparam->itsolver_tol             = 1e-6;
    iniparam->itsolver_maxit           = 500;
    iniparam->restart                  = 25;

    // ILU method parameters
    iniparam->ILU_type                 = ILUk;
    iniparam->ILU_lfil                 = 0;
    iniparam->ILU_droptol              = 0.001;
    iniparam->ILU_relax                = 0;
    iniparam->ILU_permtol              = 0.0;

    // Schwarz method parameters
    iniparam->Schwarz_mmsize           = 200;
	iniparam->Schwarz_maxlvl           = 2;
	iniparam->Schwarz_type             = 1;
	iniparam->Schwarz_blksolver        = SOLVER_DEFAULT;

    // AMG method parameters
    iniparam->AMG_type                 = CLASSIC_AMG;
    iniparam->AMG_levels               = 20;
    iniparam->AMG_cycle_type           = V_CYCLE;
    iniparam->AMG_smoother             = SMOOTHER_GS;
    iniparam->AMG_smooth_order         = CF_ORDER;
    iniparam->AMG_presmooth_iter       = 1;
    iniparam->AMG_postsmooth_iter      = 1;
    iniparam->AMG_relaxation           = 1.0;
    iniparam->AMG_coarse_dof           = 500;
	iniparam->AMG_coarse_solver        = 0;
    iniparam->AMG_tol                  = 1e-6;
    iniparam->AMG_maxit                = 1;
    iniparam->AMG_ILU_levels           = 0;
    iniparam->AMG_Schwarz_levels       = 0;
    iniparam->AMG_coarse_scaling       = OFF; // Require investigation --Chensong
    iniparam->AMG_amli_degree          = 1;
    iniparam->AMG_nl_amli_krylov_type  = 2;

    // Classical AMG specific
    iniparam->AMG_coarsening_type      = 1;
    iniparam->AMG_interpolation_type   = 1;
    iniparam->AMG_max_row_sum          = 0.9;
    iniparam->AMG_strong_threshold     = 0.3;
    iniparam->AMG_truncation_threshold = 0.2;
    iniparam->AMG_aggressive_level     = 0;
    iniparam->AMG_aggressive_path      = 1;

    // Aggregation AMG specific
    iniparam->AMG_aggregation_type     = PAIRWISE;
    iniparam->AMG_quality_bound        = 8.0;
    iniparam->AMG_pair_number          = 2;
    iniparam->AMG_strong_coupled       = 0.25;
    iniparam->AMG_max_aggregation      = 9;
    iniparam->AMG_tentative_smooth     = 0.67;
    iniparam->AMG_smooth_filter        = ON;
}

/**
 * \fn void fasp_param_amg_init (AMG_param *amgparam)
 *
 * \brief Initialize AMG parameters
 *
 * \param amgparam    Parameters for AMG
 *
 * \author Chensong Zhang
 * \date   2010/04/03
 */
void fasp_param_amg_init (AMG_param *amgparam)
{
    // General AMG parameters
    amgparam->AMG_type             = CLASSIC_AMG;
    amgparam->print_level          = PRINT_NONE;
    amgparam->maxit                = 1;
    amgparam->tol                  = 1e-6;
    amgparam->max_levels           = 20;
    amgparam->coarse_dof           = 500;
    amgparam->cycle_type           = V_CYCLE;
    amgparam->smoother             = SMOOTHER_GS;
    amgparam->smooth_order         = CF_ORDER;
    amgparam->presmooth_iter       = 1;
    amgparam->postsmooth_iter      = 1;
    amgparam->coarse_solver        = SOLVER_DEFAULT;
    amgparam->relaxation           = 1.0;
    amgparam->polynomial_degree    = 3;
    amgparam->coarse_scaling       = OFF;
    amgparam->amli_degree          = 2;
    amgparam->amli_coef            = NULL;
    amgparam->nl_amli_krylov_type  = SOLVER_GCG;

    // Classical AMG specific
    amgparam->coarsening_type      = COARSE_RS;
    amgparam->interpolation_type   = INTERP_DIR;
    amgparam->max_row_sum          = 0.9;
    amgparam->strong_threshold     = 0.3;
    amgparam->truncation_threshold = 0.2;
    amgparam->aggressive_level     = 0;
    amgparam->aggressive_path      = 1;

    // Aggregation AMG specific
    amgparam->aggregation_type     = PAIRWISE;
    amgparam->quality_bound        = 10.0;
    amgparam->pair_number          = 2;
    amgparam->strong_coupled       = 0.08;
    amgparam->max_aggregation      = 20;
    amgparam->tentative_smooth     = 0.67;
    amgparam->smooth_filter        = ON;

    // ILU smoother parameters
    amgparam->ILU_type             = ILUk;
    amgparam->ILU_levels           = 0;
    amgparam->ILU_lfil             = 0;
    amgparam->ILU_droptol          = 0.001;
    amgparam->ILU_relax            = 0;

    // Schwarz smoother parameters
    amgparam->Schwarz_levels       = 0; // how many levels will use Schwarz smoother
    amgparam->Schwarz_mmsize       = 200;
    amgparam->Schwarz_maxlvl       = 3; // blocksize -- vertices with smaller distance
    amgparam->Schwarz_type         = 1;
    amgparam->Schwarz_blksolver    = SOLVER_DEFAULT;
}

/**
 * \fn void fasp_param_solver_init (itsolver_param *itsparam)
 *
 * \brief Initialize itsolver_param
 *
 * \param itsparam   Parameters for iterative solvers
 *
 * \author Chensong Zhang
 * \date   2010/03/23
 */
void fasp_param_solver_init (itsolver_param *itsparam)
{
    itsparam->print_level   = PRINT_NONE;
    itsparam->itsolver_type = SOLVER_CG;
    itsparam->precond_type  = PREC_AMG;
    itsparam->stop_type     = STOP_REL_RES;
    itsparam->maxit         = 500;
    itsparam->restart       = 25;
    itsparam->tol           = 1e-6;
}

/**
 * \fn void fasp_param_ilu_init (ILU_param *iluparam)
 *
 * \brief Initialize ILU parameters
 *
 * \param iluparam  Parameters for ILU
 *
 * \author Chensong Zhang
 * \date   2010/04/06
 */
void fasp_param_ilu_init (ILU_param *iluparam)
{
    iluparam->print_level  = PRINT_NONE;
    iluparam->ILU_type     = ILUk;
    iluparam->ILU_lfil     = 2;
    iluparam->ILU_droptol  = 0.001;
    iluparam->ILU_relax    = 0;
    iluparam->ILU_permtol  = 0.01;
}

/**
 * \fn void fasp_param_Schwarz_init (Schwarz_param *schparam)
 *
 * \brief Initialize Schwarz parameters
 *
 * \param schparam    Parameters for Schwarz method
 *
 * \author Xiaozhe Hu
 * \date   05/22/2012
 *
 * Modified by Chensong Zhang on 10/10/2014: Add block solver type
 */
void fasp_param_Schwarz_init (Schwarz_param *schparam)
{
    schparam->print_level       = PRINT_NONE;
    schparam->Schwarz_type      = 3;
    schparam->Schwarz_maxlvl    = 2;
    schparam->Schwarz_mmsize    = 200;
    schparam->Schwarz_blksolver = 0;
}

/**
 * \fn void fasp_param_amg_set (AMG_param *param, input_param *iniparam)
 *
 * \brief Set AMG_param from INPUT
 *
 * \param param     Parameters for AMG
 * \param iniparam   Input parameters
 *
 * \author Chensong Zhang
 * \date   2010/03/23
 */
void fasp_param_amg_set (AMG_param   *param,
                         input_param *iniparam)
{
    param->AMG_type    = iniparam->AMG_type;
    param->print_level = iniparam->print_level;

    if (iniparam->solver_type == SOLVER_AMG) {
        param->maxit = iniparam->itsolver_maxit;
        param->tol   = iniparam->itsolver_tol;
    }
    else if (iniparam->solver_type == SOLVER_FMG) {
        param->maxit = iniparam->itsolver_maxit;
        param->tol   = iniparam->itsolver_tol;
    }
    else {
        param->maxit = iniparam->AMG_maxit;
        param->tol   = iniparam->AMG_tol;
    }

    param->max_levels           = iniparam->AMG_levels;
    param->cycle_type           = iniparam->AMG_cycle_type;
    param->smoother             = iniparam->AMG_smoother;
    param->smooth_order         = iniparam->AMG_smooth_order;
    param->relaxation           = iniparam->AMG_relaxation;
    param->coarse_solver        = iniparam->AMG_coarse_solver;
    param->polynomial_degree    = iniparam->AMG_polynomial_degree;
    param->presmooth_iter       = iniparam->AMG_presmooth_iter;
    param->postsmooth_iter      = iniparam->AMG_postsmooth_iter;
    param->coarse_dof           = iniparam->AMG_coarse_dof;
    param->coarse_scaling       = iniparam->AMG_coarse_scaling;
    param->amli_degree          = iniparam->AMG_amli_degree;
    param->amli_coef            = NULL;
    param->nl_amli_krylov_type  = iniparam->AMG_nl_amli_krylov_type;

    param->coarsening_type      = iniparam->AMG_coarsening_type;
    param->interpolation_type   = iniparam->AMG_interpolation_type;
    param->strong_threshold     = iniparam->AMG_strong_threshold;
    param->truncation_threshold = iniparam->AMG_truncation_threshold;
    param->max_row_sum          = iniparam->AMG_max_row_sum;
    param->aggressive_level     = iniparam->AMG_aggressive_level;
    param->aggressive_path      = iniparam->AMG_aggressive_path;

    param->aggregation_type     = iniparam->AMG_aggregation_type;
    param->pair_number          = iniparam->AMG_pair_number;
    param->quality_bound        = iniparam->AMG_quality_bound;
    param->strong_coupled       = iniparam->AMG_strong_coupled;
    param->max_aggregation      = iniparam->AMG_max_aggregation;
    param->tentative_smooth     = iniparam->AMG_tentative_smooth;
    param->smooth_filter        = iniparam->AMG_smooth_filter;
    param->smooth_filter        = iniparam->AMG_smooth_filter;

    param->ILU_levels           = iniparam->AMG_ILU_levels;
    param->ILU_type             = iniparam->ILU_type;
    param->ILU_lfil             = iniparam->ILU_lfil;
    param->ILU_droptol          = iniparam->ILU_droptol;
    param->ILU_relax            = iniparam->ILU_relax;
    param->ILU_permtol          = iniparam->ILU_permtol;

    param->Schwarz_levels       = iniparam->AMG_Schwarz_levels;
	param->Schwarz_mmsize       = iniparam->Schwarz_mmsize;
	param->Schwarz_maxlvl       = iniparam->Schwarz_maxlvl;
	param->Schwarz_type         = iniparam->Schwarz_type;
}

/**
 * \fn void fasp_param_ilu_set (ILU_param *iluparam, input_param *iniparam)
 *
 * \brief Set ILU_param with INPUT
 *
 * \param iluparam    Parameters for ILU
 * \param iniparam     Input parameters
 *
 * \author Chensong Zhang
 * \date   2010/04/03
 */
void fasp_param_ilu_set (ILU_param   *iluparam,
                         input_param *iniparam)
{
    iluparam->print_level = iniparam->print_level;
    iluparam->ILU_type    = iniparam->ILU_type;
    iluparam->ILU_lfil    = iniparam->ILU_lfil;
    iluparam->ILU_droptol = iniparam->ILU_droptol;
    iluparam->ILU_relax   = iniparam->ILU_relax;
    iluparam->ILU_permtol = iniparam->ILU_permtol;
}

/**
 * \fn void fasp_param_Schwarz_set (Schwarz_param *schparam, input_param *iniparam)
 *
 * \brief Set Schwarz_param with INPUT
 *
 * \param schparam    Parameters for Schwarz method
 * \param iniparam     Input parameters
 *
 * \author Xiaozhe Hu
 * \date   05/22/2012
 */
void fasp_param_Schwarz_set (Schwarz_param *schparam,
                             input_param   *iniparam)
{
    schparam->print_level       = iniparam->print_level;
    schparam->Schwarz_type      = iniparam->Schwarz_type;
    schparam->Schwarz_maxlvl    = iniparam->Schwarz_maxlvl;
    schparam->Schwarz_mmsize    = iniparam->Schwarz_mmsize;
    schparam->Schwarz_blksolver = iniparam->Schwarz_blksolver;
}

/**
 * \fn void fasp_param_solver_set (itsolver_param *itsparam, input_param *iniparam)
 *
 * \brief Set itsolver_param with INPUT
 *
 * \param itsparam   Parameters for iterative solvers
 * \param iniparam    Input parameters
 *
 * \author Chensong Zhang
 * \date   2010/03/23
 */
void fasp_param_solver_set (itsolver_param *itsparam,
                            input_param    *iniparam)
{
    itsparam->print_level    = iniparam->print_level;
    itsparam->itsolver_type  = iniparam->solver_type;
    itsparam->precond_type   = iniparam->precond_type;
    itsparam->stop_type      = iniparam->stop_type;
    itsparam->restart        = iniparam->restart;

    if ( itsparam->itsolver_type == SOLVER_AMG ) {
        itsparam->tol   = iniparam->AMG_tol;
        itsparam->maxit = iniparam->AMG_maxit;
    }
    else {
        itsparam->tol   = iniparam->itsolver_tol;
        itsparam->maxit = iniparam->itsolver_maxit;
    }
}

/**
 * \fn void fasp_param_amg_to_prec (precond_data *pcdata, AMG_param *amgparam)
 *
 * \brief Set precond_data with AMG_param
 *
 * \param pcdata      Preconditioning data structure
 * \param amgparam    Parameters for AMG
 *
 * \author Chensong Zhang
 * \date   2011/01/10
 */
void fasp_param_amg_to_prec (precond_data *pcdata,
                             AMG_param    *amgparam)
{
    pcdata->AMG_type            = amgparam->AMG_type;
    pcdata->print_level         = amgparam->print_level;
    pcdata->maxit               = amgparam->maxit;
    pcdata->max_levels          = amgparam->max_levels;
    pcdata->tol                 = amgparam->tol;
    pcdata->cycle_type          = amgparam->cycle_type;
    pcdata->smoother            = amgparam->smoother;
    pcdata->smooth_order        = amgparam->smooth_order;
    pcdata->presmooth_iter      = amgparam->presmooth_iter;
    pcdata->postsmooth_iter     = amgparam->postsmooth_iter;
    pcdata->coarsening_type     = amgparam->coarsening_type;
    pcdata->coarse_solver       = amgparam->coarse_solver;
    pcdata->relaxation          = amgparam->relaxation;
    pcdata->polynomial_degree   = amgparam->polynomial_degree;
    pcdata->coarse_scaling      = amgparam->coarse_scaling;
    pcdata->amli_degree         = amgparam->amli_degree;
    pcdata->amli_coef           = amgparam->amli_coef;
    pcdata->nl_amli_krylov_type = amgparam->nl_amli_krylov_type;
    pcdata->tentative_smooth    = amgparam->tentative_smooth;
}

/**
 * \fn void fasp_param_prec_to_amg (AMG_param *amgparam, precond_data *pcdata)
 *
 * \brief Set AMG_param with precond_data
 *
 * \param amgparam    Parameters for AMG
 * \param pcdata      Preconditioning data structure
 *
 * \author Chensong Zhang
 * \date   2011/01/10
 */
void fasp_param_prec_to_amg (AMG_param    *amgparam,
                             precond_data *pcdata)
{
    amgparam->AMG_type            = pcdata->AMG_type;
    amgparam->print_level         = pcdata->print_level;
    amgparam->cycle_type          = pcdata->cycle_type;
    amgparam->smoother            = pcdata->smoother;
    amgparam->smooth_order        = pcdata->smooth_order;
    amgparam->presmooth_iter      = pcdata->presmooth_iter;
    amgparam->postsmooth_iter     = pcdata->postsmooth_iter;
    amgparam->relaxation          = pcdata->relaxation;
    amgparam->polynomial_degree   = pcdata->polynomial_degree;
    amgparam->coarse_solver       = pcdata->coarse_solver;
    amgparam->coarse_scaling      = pcdata->coarse_scaling;
    amgparam->amli_degree         = pcdata->amli_degree;
    amgparam->amli_coef           = pcdata->amli_coef;
    amgparam->nl_amli_krylov_type = pcdata->nl_amli_krylov_type;
    amgparam->tentative_smooth    = pcdata->tentative_smooth;
    amgparam->ILU_levels          = pcdata->mgl_data->ILU_levels;
}

/**
 * \fn void fasp_param_amg_to_prec_bsr (precond_data_bsr *pcdata, AMG_param *amgparam)
 *
 * \brief Set precond_data_bsr with AMG_param
 *
 * \param pcdata      Preconditioning data structure
 * \param amgparam    Parameters for AMG
 *
 * \author Xiaozhe Hu
 * \date   02/06/2012
 */
void fasp_param_amg_to_prec_bsr (precond_data_bsr *pcdata,
                                 AMG_param        *amgparam)
{
    pcdata->AMG_type            = amgparam->AMG_type;
    pcdata->print_level         = amgparam->print_level;
    pcdata->maxit               = amgparam->maxit;
    pcdata->max_levels          = amgparam->max_levels;
    pcdata->tol                 = amgparam->tol;
    pcdata->cycle_type          = amgparam->cycle_type;
    pcdata->smoother            = amgparam->smoother;
    pcdata->smooth_order        = amgparam->smooth_order;
    pcdata->presmooth_iter      = amgparam->presmooth_iter;
    pcdata->postsmooth_iter     = amgparam->postsmooth_iter;
    pcdata->coarse_solver       = amgparam->coarse_solver;
    pcdata->coarsening_type     = amgparam->coarsening_type;
    pcdata->relaxation          = amgparam->relaxation;
    pcdata->coarse_scaling      = amgparam->coarse_scaling;
    pcdata->amli_degree         = amgparam->amli_degree;
    pcdata->amli_coef           = amgparam->amli_coef;
    pcdata->nl_amli_krylov_type = amgparam->nl_amli_krylov_type;
    pcdata->tentative_smooth    = amgparam->tentative_smooth;
}

/**
 * \fn void fasp_param_prec_to_amg_bsr (AMG_param *amgparam, precond_data_bsr *pcdata)
 *
 * \brief Set AMG_param with precond_data
 *
 * \param amgparam    Parameters for AMG
 * \param pcdata      Preconditioning data structure
 *
 * \author Xiaozhe Hu
 * \date   02/06/2012
 */
void fasp_param_prec_to_amg_bsr (AMG_param        *amgparam,
                                 precond_data_bsr *pcdata)
{
    amgparam->AMG_type            = pcdata->AMG_type;
    amgparam->print_level         = pcdata->print_level;
    amgparam->cycle_type          = pcdata->cycle_type;
    amgparam->smoother            = pcdata->smoother;
    amgparam->smooth_order        = pcdata->smooth_order;
    amgparam->presmooth_iter      = pcdata->presmooth_iter;
    amgparam->postsmooth_iter     = pcdata->postsmooth_iter;
    amgparam->relaxation          = pcdata->relaxation;
    amgparam->coarse_solver       = pcdata->coarse_solver;
    amgparam->coarse_scaling      = pcdata->coarse_scaling;
    amgparam->amli_degree         = pcdata->amli_degree;
    amgparam->amli_coef           = pcdata->amli_coef;
    amgparam->nl_amli_krylov_type = pcdata->nl_amli_krylov_type;
    amgparam->tentative_smooth    = pcdata->tentative_smooth;
    amgparam->ILU_levels          = pcdata->mgl_data->ILU_levels;
}

/**
 * \fn void fasp_param_amg_print (AMG_param *param)
 *
 * \brief Print out AMG parameters
 *
 * \param param   Parameters for AMG
 *
 * \author Chensong Zhang
 * \date   2010/03/22
 */
void fasp_param_amg_print (AMG_param *param)
{

    if ( param ) {

        printf("\n       Parameters in AMG_param\n");
        printf("-----------------------------------------------\n");

        printf("AMG print level:                   %d\n", param->print_level);
        printf("AMG max num of iter:               %d\n", param->maxit);
        printf("AMG type:                          %d\n", param->AMG_type);
        printf("AMG tolerance:                     %.2e\n", param->tol);
        printf("AMG max levels:                    %d\n", param->max_levels);
        printf("AMG cycle type:                    %d\n", param->cycle_type);
        printf("AMG coarse solver type:            %d\n", param->coarse_solver);
        printf("AMG scaling of coarse correction:  %d\n", param->coarse_scaling);
        printf("AMG smoother type:                 %d\n", param->smoother);
        printf("AMG smoother order:                %d\n", param->smooth_order);
        printf("AMG num of presmoothing:           %d\n", param->presmooth_iter);
        printf("AMG num of postsmoothing:          %d\n", param->postsmooth_iter);

        if ( param->smoother == SMOOTHER_SOR  ||
             param->smoother == SMOOTHER_SSOR ||
             param->smoother == SMOOTHER_GSOR ||
             param->smoother == SMOOTHER_SGSOR ) {
            printf("AMG relax factor:                  %.4f\n", param->relaxation);
        }

        if ( param->smoother == SMOOTHER_POLY ) {
            printf("AMG polynomial smoother degree:    %d\n", param->polynomial_degree);
        }

        if ( param->cycle_type == AMLI_CYCLE ) {
            printf("AMG AMLI degree of polynomial:     %d\n", param->amli_degree);
        }

        if ( param->cycle_type == NL_AMLI_CYCLE ) {
            printf("AMG Nonlinear AMLI Krylov type:    %d\n", param->nl_amli_krylov_type);
        }

        switch (param->AMG_type) {
            case CLASSIC_AMG:
                printf("AMG coarsening type:               %d\n", param->coarsening_type);
                printf("AMG interpolation type:            %d\n", param->interpolation_type);
                printf("AMG dof on coarsest grid:          %d\n", param->coarse_dof);
                printf("AMG strong threshold:              %.4f\n", param->strong_threshold);
                printf("AMG truncation threshold:          %.4f\n", param->truncation_threshold);
                printf("AMG max row sum:                   %.4f\n", param->max_row_sum);
                printf("AMG aggressive levels:             %d\n", param->aggressive_level);
                printf("AMG aggressive path:               %d\n", param->aggressive_path);
                break;

            default: // SA_AMG or UA_AMG
                printf("Aggregation type:                  %d\n", param->aggregation_type);
                if ( param->aggregation_type == PAIRWISE ) {
                    printf("Aggregation number of pairs:       %d\n", param->pair_number);
                    printf("Aggregation quality bound:         %.2f\n", param->quality_bound);
                }
                if ( param->aggregation_type == VMB ) {
                    printf("Aggregation AMG strong coupling:   %.4f\n", param->strong_coupled);
                    printf("Aggregation AMG max aggregation:   %d\n", param->max_aggregation);
                    printf("Aggregation AMG tentative smooth:  %.4f\n", param->tentative_smooth);
                    printf("Aggregation AMG smooth filter:     %d\n", param->smooth_filter);
                }
                break;
        }

        if (param->ILU_levels>0) {
            printf("AMG ILU smoother level:            %d\n", param->ILU_levels);
            printf("AMG ILU type:                      %d\n", param->ILU_type);
            printf("AMG ILU level of fill-in:          %d\n", param->ILU_lfil);
            printf("AMG ILU drop tol:                  %e\n", param->ILU_droptol);
            printf("AMG ILU relaxation:                %f\n", param->ILU_relax);
        }

        if (param->Schwarz_levels>0){
            printf("AMG Schwarz smoother level:        %d\n", param->Schwarz_levels);
            printf("AMG Schwarz type:                  %d\n", param->Schwarz_type);
            printf("AMG Schwarz forming block level:   %d\n", param->Schwarz_maxlvl);
            printf("AMG Schwarz maximal block size:    %d\n", param->Schwarz_mmsize);
        }

        printf("-----------------------------------------------\n\n");

    }
    else {
        printf("### WARNING: param has not been set!\n");
    } // end if (param)

}

/**
 * \fn void fasp_param_ilu_print (ILU_param *param)
 *
 * \brief Print out ILU parameters
 *
 * \param param    Parameters for ILU
 *
 * \author Chensong Zhang
 * \date   2011/12/20
 */
void fasp_param_ilu_print (ILU_param *param)
{
    if ( param ) {

        printf("\n       Parameters in ILU_param\n");
        printf("-----------------------------------------------\n");
        printf("ILU print level:                   %d\n",   param->print_level);
        printf("ILU type:                          %d\n",   param->ILU_type);
        printf("ILU level of fill-in:              %d\n",   param->ILU_lfil);
        printf("ILU relaxation factor:             %.4f\n", param->ILU_relax);
        printf("ILU drop tolerance:                %.2e\n", param->ILU_droptol);
        printf("ILU permutation tolerance:         %.2e\n", param->ILU_permtol);
        printf("-----------------------------------------------\n\n");

    }
    else {
        printf("### WARNING: param has not been set!\n");
    }
}

/**
 * \fn void fasp_param_Schwarz_print (Schwarz_param *param)
 *
 * \brief Print out Schwarz parameters
 *
 * \param param    Parameters for Schwarz
 *
 * \author Xiaozhe Hu
 * \date   05/22/2012
 */
void fasp_param_Schwarz_print (Schwarz_param *param)
{
    if ( param ) {

        printf("\n       Parameters in Schwarz_param\n");
        printf("-----------------------------------------------\n");
        printf("Schwarz print level:               %d\n",   param->print_level);
        printf("Schwarz type:                      %d\n",   param->Schwarz_type);
        printf("Schwarz forming block level:       %d\n",   param->Schwarz_maxlvl);
        printf("Schwarz maximal block size:        %d\n",   param->Schwarz_mmsize);
        printf("Schwarz block solver type:         %d\n",   param->Schwarz_blksolver);
        printf("-----------------------------------------------\n\n");

    }
    else {
        printf("### WARNING: param has not been set!\n");
    }
}

/**
 * \fn void fasp_param_solver_print (itsolver_param *param)
 *
 * \brief Print out itsolver parameters
 *
 * \param param    Paramters for iterative solvers
 *
 * \author Chensong Zhang
 * \date   2011/12/20
 */
void fasp_param_solver_print (itsolver_param *param)
{
    if ( param ) {

        printf("\n       Parameters in itsolver_param\n");
        printf("-----------------------------------------------\n");

        printf("Solver print level:                %d\n", param->print_level);
        printf("Solver type:                       %d\n", param->itsolver_type);
        printf("Solver precond type:               %d\n", param->precond_type);
        printf("Solver max num of iter:            %d\n", param->maxit);
        printf("Solver tolerance:                  %.2e\n", param->tol);
        printf("Solver stopping type:              %d\n", param->stop_type);

        if (param->itsolver_type==SOLVER_GMRES ||
            param->itsolver_type==SOLVER_VGMRES) {
            printf("Solver restart number:             %d\n", param->restart);
        }

        printf("-----------------------------------------------\n\n");

    }
    else {
        printf("### WARNING: param has not been set!\n");
    }
}

/*---------------------------------*/
/*--        End of File          --*/
/*---------------------------------*/