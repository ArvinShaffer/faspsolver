/*! \file parameters.c
 *  \brief Initialize, set, or print out data and parameters
 *
 */

#include "fasp.h"
#include "fasp_functs.h"

#include <stdio.h>

/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/**
 * \fn void fasp_param_init (char *inputfile, input_param *inparam, 
 *                           itsolver_param *itparam, AMG_param *amgparam, 
 *                           ILU_param *iluparam)
 *
 * \brief Initialize parameters, global variables, etc
 *
 * \param *inputfile   filename of the input file
 * \param *inparam     input parameters
 * \param *itparam     iterative solver parameters
 * \param *amgparam    AMG parameters
 * \param *iluparam    ILU parameters
 *
 * \author Chensong Zhang
 * \date 2010/08/12 
 *
 * \modified by Xiaozhe Hu (01/23/2011): first initialize then set value, 
 *              make sure all the parameters are well-defined. 
 */
void fasp_param_init (char *inputfile, 
                      input_param *inparam, 
                      itsolver_param *itparam, 
                      AMG_param *amgparam, 
                      ILU_param *iluparam)
{	
	total_alloc_mem   = 0; // initialize total memeory amount
	total_alloc_count = 0; // initialize alloc count
	
	if (itparam)  fasp_param_solver_init(itparam);
	if (amgparam) fasp_param_amg_init(amgparam);
	if (iluparam) fasp_param_ilu_init(iluparam);	
	
	if (inputfile) {
		fasp_param_input(inputfile,inparam);
		if (itparam)  fasp_param_solver_set(itparam,inparam);
		if (amgparam) fasp_param_amg_set(amgparam,inparam);
		if (iluparam) fasp_param_ilu_set(iluparam,inparam);	
	}
}	

/**
 * \fn void fasp_param_input_init (input_param *Input)
 *
 * \brief Initialize input parameters
 *
 * \param Input: pointer to input_param
 *
 * \author Chensong Zhang
 * \date 2010/03/20 
 */
void fasp_param_input_init (input_param *Input)
{
	strcpy(Input->workdir,"data/");
	
	Input->print_level = PRINT_NONE;
	Input->output_type = 0;
	
	Input->problem_num = 10;	
	Input->itsolver_type = SOLVER_CG;
	Input->precond_type = PREC_AMG;
	Input->stop_type = STOP_REL_RES;
	
	Input->itsolver_tol = 1e-8;
	Input->itsolver_maxit = 500;
	Input->restart = 25;
	
	Input->ILU_type = ILUs;
	Input->ILU_lfil = 0;
	Input->ILU_droptol = 0.001;
	Input->ILU_relax = 0;
	Input->ILU_permtol = 0.0;
	
	Input->AMG_type = CLASSIC_AMG;
	Input->AMG_levels = 12;
	Input->AMG_cycle_type = V_CYCLE;
	Input->AMG_smoother = GS;
	Input->AMG_presmooth_iter = 2;
	Input->AMG_postsmooth_iter = 2;
	Input->AMG_relaxation = 1.0;
	Input->AMG_coarse_dof = 500;		
	Input->AMG_tol = 1e-4*Input->itsolver_tol;
	Input->AMG_maxit = 1;
	Input->AMG_ILU_levels = 0;
	Input->AMG_coarse_scaling = OFF;	
	
	Input->AMG_coarsening_type = 1;	
	Input->AMG_interpolation_type = 1;			
	Input->AMG_strong_threshold = 0.6; 
	Input->AMG_truncation_threshold = 0.4;
	Input->AMG_max_row_sum = 0.9;
	
	Input->AMG_strong_coupled = 0.08;
	Input->AMG_max_aggregation = 9;
	Input->AMG_tentative_smooth = 0.67;
	Input->AMG_smooth_filter = ON;
}

/**
 * \fn void fasp_param_amg_init (AMG_param *param)
 *
 * \brief Initialize AMG parameters
 *
 * \param param   pointer to AMG_param
 *
 * \author Chensong Zhang
 * \date 2010/04/03
 */
void fasp_param_amg_init (AMG_param *param)
{
	param->AMG_type = CLASSIC_AMG;
	param->print_level = PRINT_NONE;
	param->max_iter = 1;
	param->tol = 1e-8;
	
	param->max_levels = 12;	
	param->coarse_dof = 500;
	param->cycle_type = V_CYCLE;	
	param->smoother = GS;
	param->smooth_order = NO_ORDER;
	param->presmooth_iter = 2;
	param->postsmooth_iter = 2;
	param->relaxation = 1.0;
	param->coarse_scaling = OFF;
	param->amli_degree = 0;
	param->amli_coef = NULL;
	
	param->coarsening_type = 1;
	param->interpolation_type = 1;
	
	param->strong_threshold = 0.3;
	param->truncation_threshold = 0.4;
	param->max_row_sum = 0.9;
	
	param->strong_coupled = 0.08;
	param->max_aggregation = 9;
	param->tentative_smooth = 0.0; //0.67; edit by Fengchunsheng  2011/03/15
	param->smooth_filter = OFF;
	
	param->ILU_type = ILUs; 
	param->ILU_levels = 0;
	param->ILU_lfil = 0;
	param->ILU_droptol = 0.001;
	param->ILU_relax = 0;
}

/**
 * \fn void fasp_param_solver_init (itsolver_param *pdata)
 *
 * \brief Initialize itsolver_param
 *
 * \param Input: pointer to itsolver_param
 *
 * \author Chensong Zhang
 * \date 2010/03/23 
 */
void fasp_param_solver_init (itsolver_param *pdata)
{
	pdata->print_level   = 0;
	pdata->itsolver_type = SOLVER_CG;
	pdata->precond_type  = PREC_AMG;
	pdata->stop_type     = STOP_REL_RES;	
	pdata->maxit         = 500;
	pdata->restart       = 25;
	pdata->tol           = 1e-8;	
}

/**
 * \fn void fasp_param_ilu_init (ILU_param *param)
 *
 * \brief Initialize ILU parameters
 *
 * \param param   pointer to ILU_param
 *
 * \author Chensong Zhang
 * \date 2010/04/06 
 */
void fasp_param_ilu_init (ILU_param *param)
{
	param->print_level  = 0;
	param->ILU_type     = ILUs;
	param->ILU_lfil     = 2;
	param->ILU_droptol  = 0.001;
	param->ILU_relax    = 0;
	param->ILU_permtol  = 0.01;	
}

/**
 * \fn void fasp_param_amg_set (AMG_param *param, input_param *Input)
 *
 * \brief Set AMG_param
 *
 * \param param   pointer to AMG_param
 * \param Input   pointer to Input_param
 *
 * \author Chensong Zhang
 * \date 2010/03/23 
 */
void fasp_param_amg_set (AMG_param *param, input_param *Input)
{	
	param->AMG_type    = Input->AMG_type;
	param->print_level = Input->print_level;
	
	if (Input->itsolver_type == SOLVER_AMG) {
		param->max_iter = Input->itsolver_maxit;
		param->tol      = Input->itsolver_tol;
	}
	else if (Input->itsolver_type == SOLVER_FMG) {
		param->max_iter = Input->itsolver_maxit;
		param->tol      = Input->itsolver_tol;
	}
	else {
		param->max_iter = Input->AMG_maxit;
		param->tol      = Input->AMG_tol; 
	}
	
	param->max_levels      = Input->AMG_levels;	
	param->cycle_type      = Input->AMG_cycle_type;	
	param->smoother        = Input->AMG_smoother;
	param->relaxation      = Input->AMG_relaxation;
	param->presmooth_iter  = Input->AMG_presmooth_iter;
	param->postsmooth_iter = Input->AMG_postsmooth_iter;
	param->coarse_dof      = Input->AMG_coarse_dof;
	param->coarse_scaling  = Input->AMG_coarse_scaling;
	param->amli_degree     = Input->AMG_amli_degree;
	param->amli_coef       = NULL;
	
	param->coarsening_type      = Input->AMG_coarsening_type;
	param->interpolation_type   = Input->AMG_interpolation_type;
	param->strong_threshold     = Input->AMG_strong_threshold;
	param->truncation_threshold = Input->AMG_truncation_threshold;
	param->max_row_sum          = Input->AMG_max_row_sum;
	
	param->strong_coupled   = Input->AMG_strong_coupled;
	param->max_aggregation  = Input->AMG_max_aggregation;
	param->tentative_smooth = Input->AMG_tentative_smooth;
	param->smooth_filter    = Input->AMG_smooth_filter;
	
	param->ILU_levels  = Input->AMG_ILU_levels;
	param->ILU_type    = Input->ILU_type;
	param->ILU_lfil    = Input->ILU_lfil;
	param->ILU_droptol = Input->ILU_droptol;
	param->ILU_relax   = Input->ILU_relax;
	param->ILU_permtol = Input->ILU_permtol;
}

/**
 * \fn void fasp_param_ilu_set (ILU_param *param, input_param *Input)
 *
 * \brief Set ILU_param
 *
 * \param param   pointer to ILU_param
 * \param Input   pointer to Input_param
 *
 * \author Chensong Zhang
 * \date 2010/04/03 
 */
void fasp_param_ilu_set (ILU_param *param, input_param *Input)
{	
	param->print_level = Input->print_level;
	param->ILU_type    = Input->ILU_type;
	param->ILU_lfil    = Input->ILU_lfil;
	param->ILU_droptol = Input->ILU_droptol;
	param->ILU_relax   = Input->ILU_relax;
	param->ILU_permtol = Input->ILU_permtol;
}

/**
 * \fn void fasp_param_solver_set (itsolver_param *itparam, input_param *Input)
 *
 * \brief Set itsolver_param
 *
 * \param itparam  pointer to itsolver_param
 * \param Input    pointer to itput_param
 *
 * \author Chensong Zhang
 * \date 2010/03/23 
 */
void fasp_param_solver_set (itsolver_param *itparam, input_param *Input)
{
	itparam->print_level    = Input->print_level;
	itparam->itsolver_type  = Input->itsolver_type;
	itparam->precond_type   = Input->precond_type;
	itparam->stop_type      = Input->stop_type;
	itparam->restart        = Input->restart;
	
	if (itparam->itsolver_type == SOLVER_AMG) {
		itparam->tol   = Input->AMG_tol; 
		itparam->maxit = Input->AMG_maxit;	
	}
	else {
		itparam->tol   = Input->itsolver_tol; 
		itparam->maxit = Input->itsolver_maxit;
	}
}	

/**
 * \fn void fasp_param_amg_print (AMG_param *param)
 *
 * \brief Print out AMG parameters
 *
 * \param param: pointer to AMG_param
 *
 * \author Chensong Zhang
 * \date 2010/03/22 
 */
void fasp_param_amg_print (AMG_param *param)
{
	
	if ( param ) {
        
        printf("\n       Parameters in AMG_param\n");
        printf("-----------------------------------------------\n");
        
		printf("AMG print level:                   %d\n", param->print_level);
		printf("AMG max num of iter:               %d\n", param->max_iter);
		printf("AMG type:                          %d\n", param->AMG_type);
		printf("AMG tolerance:                     %.2e\n", param->tol);
		printf("AMG max levels:                    %d\n", param->max_levels);	
		printf("AMG cycle type:                    %d\n", param->cycle_type);	
		printf("AMG scaling of coarse correction:  %d\n", param->coarse_scaling);
		printf("AMG smoother type:                 %d\n", param->smoother);
		printf("AMG num of presmoothing:           %d\n", param->presmooth_iter);
		printf("AMG num of postsmoothing:          %d\n", param->postsmooth_iter);
		
        if (param->smoother==SOR  || param->smoother==SSOR ||
            param->smoother==GSOR || param->smoother==SGSOR) {
            printf("AMG relax factor:                  %.4f\n", param->relaxation);            
        }
        
		if(param->cycle_type == AMLI_CYCLE) {
            printf("AMG AMLI degree of polynomial:     %d\n", param->amli_degree);
		}
        
        switch (param->AMG_type) {
            case CLASSIC_AMG:
                printf("AMG coarsening type:               %d\n", param->coarsening_type);
                printf("AMG interpolation type:            %d\n", param->interpolation_type);
                printf("AMG dof on coarsest grid:          %d\n", param->coarse_dof);
                printf("AMG strong threshold:              %.4f\n", param->strong_threshold);
                printf("AMG truncation threshold:          %.4f\n", param->truncation_threshold);
                printf("AMG max row sum:                   %.4f\n", param->max_row_sum);
                break;
                
            default: // SA_AMG or UA_AMG
                printf("Aggregation AMG strong coupling:   %.4f\n", param->strong_coupled);
                printf("Aggregation AMG max aggregation:   %d\n", param->max_aggregation);
                printf("Aggregation AMG tentative smooth:  %.4f\n", param->tentative_smooth);
                printf("Aggregation AMG smooth filter:     %d\n", param->smooth_filter);
                break;
        }
		
        if (param->ILU_levels>0) {
            printf("AMG ILU type:                      %d\n", param->ILU_type); 
            printf("AMG ILU level:                     %d\n", param->ILU_levels);
            printf("AMG ILU level of fill-in:          %d\n", param->ILU_lfil);
            printf("AMG ILU drop tol:                  %e\n", param->ILU_droptol);
            printf("AMG ILU relaxation:                %f\n", param->ILU_relax);            
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
 * \param param: pointer to ILU_param
 *
 * \author Chensong Zhang
 * \date 2011/12/20 
 */
void fasp_param_ilu_print (ILU_param *param)
{
	if ( param ) {
        
        printf("\n       Parameters in ILU_param\n");
        printf("-----------------------------------------------\n");
        
		printf("ILU print level:                   %d\n", param->print_level);
		printf("ILU type:                          %d\n", param->ILU_type);
		printf("ILU level of fill-in:              %d\n", param->ILU_lfil);
		printf("ILU relaxation factor:             %.4f\n", param->ILU_relax);	        
		printf("ILU drop tolerance:                %.2e\n", param->ILU_droptol);	
		printf("ILU permutation tolerance:         %.2e\n", param->ILU_permtol);	
        
        printf("-----------------------------------------------\n\n");
        
    }
    else {
		printf("### WARNING: param has not been set!\n");
	} // end if (param)	
}

/**
 * \fn void fasp_param_solver_print (itsolver_param *param)
 *
 * \brief Print out itsolver parameters
 *
 * \param param: pointer to itsolver_param
 *
 * \author Chensong Zhang
 * \date 2011/12/20 
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
	} // end if (param)	    
}

/*---------------------------------*/
/*--        End of File          --*/
/*---------------------------------*/
