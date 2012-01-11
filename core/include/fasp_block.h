/*
 *  fasp_block.h
 *
 *------------------------------------------------------
 *  Created by Chensong Zhang on 05/21/2010.
 *  Modified by Xiaozhe Hu on 05/28/2010: added precond_block_reservoir_data.
 *  Modified by Xiaozhe Hu on 06/15/2010: modify precond_block_reservoir_data.
 *  Modified by Chensong Zhang on 10/11/2010: added BSR data.
 *------------------------------------------------------
 *
 */

/*! \file fasp_block.h
 *  \brief Main header file for the FASP_BLK package
 *  
 *  Note: Only define macros and data structures, no function decorations. 
 */

#include "fasp.h"

#ifndef __FASPBLK_HEADER__		/*-- allow multiple inclusions --*/
#define __FASPBLK_HEADER__

/*---------------------------*/ 
/*---   Data structures   ---*/
/*---------------------------*/ 

/** 
 * \struct dBSRmat
 * \brief Block sparse row storage matrix of double type.
 *
 * Note: This data structure is adapted from the Intel MKL library. 
 * Refer to http://software.intel.com/sites/products/documentation/hpc/mkl/lin/index.htm
 *
 */
typedef struct dBSRmat{
	
	//! number of rows of sub-blocks in matrix A, M
	int ROW; // use captialized words b/c 
	//! number of cols of block in matrix A, N
	int COL;
	//! number of nonzero sub-blocks in matrix A, NNZ
	int NNZ;
	//! dimension of each sub-block
	int nb; // for the moment, allow nb*nb full block 
	
	//! storage manner for each sub-block           
	int storage_manner; // 1: column-major order; 0: row-major order
	
	//! A real array that contains the elements of the non-zero blocks of 
	//! a sparse matrix. The elements are stored block-by-block in row major 
	//! order. A non-zero block is the block that contains at least one non-zero 
	//! element. All elements of non-zero blocks are stored, even if some of 
	//! them is equal to zero. Within each nonzero block elements are stored 
	//! in row-major order and the size is (NNZ*nb*nb). 
	double *val;
	
	//! integer array of row pointers, the size is ROW+1
	int *IA;
	
	//! Element i of the integer array columns is the number of the column in the
	//! block matrix that contains the i-th non-zero block. The size is NNZ.
	int *JA;
	
} dBSRmat;

/** 
 * \struct block_dCSRmat
 * \brief Block double CSR matrix structure.
 *
 * Block CSR Format in double
 *
 * Note: The starting index of A is 0, other data stuctures also take this convention.  
 */
typedef struct block_dCSRmat{
	
	//! row number of blocks in A, m
	int brow;   
	//! column number of blocks A, n
	int bcol;   
	//! blocks of dCSRmat, point to blocks[brow][bcol]
	dCSRmat **blocks;
	
} block_dCSRmat;

/** 
 * \struct block_iCSRmat
 * \brief Block integer CSR matrix structure.
 *
 * Block CSR Format in integer
 *
 * Note: The starting index of A is 0, other data stuctures also take this convention.  
 */
typedef struct block_iCSRmat{
	
	//! row number of blocks in A, m
	int brow;   
	//! column number of blocks A, n
	int bcol;   
	//! blocks of iCSRmat, point to blocks[brow][bcol]
	iCSRmat **blocks;
	
} block_iCSRmat;

/** 
 * \struct block_dvector
 * \brief Block double vector structure.
 *
 * Block Vector Format in double
 *
 * Note: The starting index of A is 0, other data stuctures also take this convention.  
 */
typedef struct block_dvector{
	
	//! row number of blocks in A, m
	int brow;   
	//! blocks of dvector, point to blocks[brow]
	dvector **blocks;
	
} block_dvector;

/** 
 * \struct block_ivector
 * \brief Block integer vector structure.
 *
 * Block Vector Format in integer
 *
 * Note: The starting index of A is 0, other data stuctures also take this convention.  
 */
typedef struct block_ivector{
	
	//! row number of blocks in A, m
	int brow;   
	//! blocks of dvector, point to blocks[brow]
	ivector **blocks;
	
} block_ivector;

/** 
 * \struct block_Reservoir
 * \brief Block double matrix structure for reservoir simulation.
 *
 */
typedef struct block_Reservoir{
	
	//! reservoir-reservoir block
	dSTRmat ResRes;
	//! reservoir-well block
	dCSRmat ResWel;	
	//! well-reservoir block
	dCSRmat WelRes;
	//! well-well block
	dCSRmat WelWel;
	
} block_Reservoir;

/** 
 * \struct block_BSR
 * \brief Block double matrix structure for reservoir simulation.
 *
 */
typedef struct block_BSR{
	
	//! reservoir-reservoir block
	dBSRmat ResRes;
	//! reservoir-well block
	dCSRmat ResWel;	
	//! well-reservoir block
	dCSRmat WelRes;
	//! well-well block
	dCSRmat WelWel;
	
} block_BSR;

/*---------------------------*/ 
/*--- Parameter structures --*/
/*---------------------------*/ 

/**
 * \brief Parameters passed to the preconditioner for generalized Stokes problems
 *
 */
typedef struct precond_Stokes_param {
		
	//! AMG type
	int AMG_type;
	//! print level in AMG preconditioner
	int print_level;
	//! max number of AMG levels
	int max_levels;
	
} precond_Stokes_param;
	
/**
 * \brief Data passed to the preconditioner for generalized Stokes problems
 *
 */
typedef struct precond_Stokes_data {

	//! size of A, B, and whole matrix
	int colA, colB, col;
	
	double beta;
	
	AMG_data *mgl_data; /**< AMG data for presure-presure block */

	//! print level in AMG preconditioner
	int print_level;
	//! max number of AMG levels
	int max_levels;
	//! max number of iterations of AMG preconditioner
	int maxit;
	//! tolerance for AMG preconditioner
	double amg_tol;
	//! AMG cycle type
	int cycle_type;
	//! AMG smoother type
	int smoother;
	//! number of presmoothing
	int presmooth_iter;
	//! number of postsmoothing
	int postsmooth_iter;
	//! coarsening type
	int coarsening_type;
	//! relaxation parameter for SOR smoother
	double relaxation;
	//! switch of scaling of coarse grid correction
	int coarse_scaling;
		
	dCSRmat *M; /**< mass matrix */
	dvector *diag_M; /**< diagonal of mass matrix M */ 
	dCSRmat *P; /**< Poisson matrix for pressure*/	
	
	//! temporary work space
	double *w; /*<  temporary work space for other usage */
	
} precond_Stokes_data;

/**
 * \brief Data passed to the preconditioner for preconditioning reservoir simulation problems
 *
 * This is needed for the Black oil model with wells
 */
typedef struct precond_block_reservoir_data {
	
	//! basic data: original matrix
	block_Reservoir *A; /**< problem data in block_Reservoir format */
	block_dCSRmat *Abcsr; /**< problem data in block_dCSRmat format */
	dCSRmat *Acsr; /**< problem data in CSR format */
	
	//! data of ILU decomposition
	int ILU_lfil; /**< level of fill-in for structured ILU(k) */
	dSTRmat *LU; /**< LU matrix for ResRes block */
	ILU_data *LUcsr; /**< LU matrix for ResRes block */
	
	//! data of AMG
	AMG_data *mgl_data; /**< AMG data for presure-presure block */
	
	//! parameters for AMG solver
	//! print level in AMG preconditioner
	int print_level;
	//! max number of iterations of AMG preconditioner
	int maxit_AMG;
	//! max number of AMG levels
	int max_levels;
	//! tolerance for AMG preconditioner
	double amg_tol;
	//! AMG cycle type
	int cycle_type;
	//! AMG smoother type
	int smoother;
	//! number of presmoothing
	int presmooth_iter;
	//! number of postsmoothing
	int postsmooth_iter;
	//! coarsening type
	int coarsening_type;
	//! relaxation parameter for SOR smoother
	double relaxation;
	//! switch of scaling of coarse grid correction
	int coarse_scaling;
	
	//! parameters for krylov method used for blocks
	int maxit; /*< max number of iterations */
	int tol; /*< tolerance */ 
	int restart; /* number of iterations for restart */
	
	//! inverse of the schur complement (-I - Awr*Arr^{-1}*Arw)^{-1}, Arr may be replaced by LU
	double *invS;
	
	//! Diag(PS) * inv(Diag(SS)) 
	dvector *DPSinvDSS;
	
	//! Data for FASP
	int scaled;
	ivector *perf_idx;
	
	dSTRmat *RR;  /*< Diagonal scaled reservoir block */
	dCSRmat *WW; /*< Argumented well block */
	dCSRmat *PP; /*< pressure block after diagonal scaling */
	dSTRmat *SS; /*< saturation block after diaogonal scaling */
	
	precond_diagstr *diag; /*< store the diagonal inverse for diagonal scaling */
	
	dvector *diaginv; /*< store the inverse of the diagonals for GS/block GS smoother (whole reservoir matrix) */
	ivector *pivot; //! store the pivot for the GS/block GS smoother (whole reservoir matrix)
	dvector *diaginvS; //! store the inverse of the diagonals for GS/block GS smoother (saturation block)
	ivector *pivotS; //! store the pivot for the GS/block GS smoother (saturation block)
	ivector *order;
	
	//! temporary work space
	dvector r; /*< temporary dvector used to store and restore the residual */
	double *w; /*<  temporary work space for other usage */
	
} precond_block_reservoir_data;

/** 
 * \brief Data passed to the preconditioner for block diagonal preconditioning.
 *
 * This is needed for the diagnoal block preconditioner.
 */
typedef struct {
	
	dCSRmat  *A; /**< problem data, the sparse matrix */
	dvector  *r; /**< problem data, the right-hand side vector */
	
	dCSRmat **Ablock; /**< problem data, the blocks */
	ivector **row_idx; /**< problem data, row indices */
	ivector **col_idx; /**< problem data, col indices */
	
	AMG_param *amgparam; /**< parameters for AMG */
	dCSRmat  **Aarray; /**< data generated in the setup phase */	
	
} precond_block_data;

/**
 * \brief Data passed to the preconditioner for preconditioning reservoir simulation problems
 *
 * This is needed for the Black oil model with wells
 */
typedef struct precond_FASP_blkoil_data{
	
	//-------------------------------------------------------------------------------
	//! Part 1: Basic data
	//-------------------------------------------------------------------------------
	block_BSR *A; /**< whole jacobian system in block_BSRmat */
	
	//-------------------------------------------------------------------------------
	//! Part 2: Data for CPR-like preconditioner for reservoir block
	//-------------------------------------------------------------------------------
	//! diagonal scaling for reservoir block
	int scaled; /*< scaled = 1 means the the following RR block is diagonal scaled */
	dvector *diaginv_noscale; /*< inverse of diagonal blocks for diagonal scaling */
	dBSRmat *RR;  /*< reservoir block */
	
	//! neighborhood and reordering of reservoir block
	ivector *neigh; /*< neighbor information of the reservoir block */
	ivector *order; /*< ordering of the reservoir block */
	
	//! data for GS/bGS smoother for saturation block
	dBSRmat *SS; /*< saturation block */
	dvector *diaginv_S; /*< inverse of the diagonal blocks for GS/block GS smoother for saturation block */
	ivector *pivot_S; /*< pivot for the GS/block GS smoother for saturation block */
	
	//! data of AMG for pressure block
	//dCSRmat *PP; /*< pressure block */
	AMG_data *mgl_data; /**< AMG data for presure-presure block */
	//! parameters for AMG solver
	//! print level in AMG preconditioner
	int print_level;
	//! max number of iterations of AMG preconditioner
	int maxit_AMG;
	//! max number of AMG levels
	int max_levels;
	//! tolerance for AMG preconditioner
	double amg_tol;
	//! AMG cycle type
	int cycle_type;
	//! AMG smoother type
	int smoother;
	//! number of presmoothing
	int presmooth_iter;
	//! number of postsmoothing
	int postsmooth_iter;
	//! coarsening type
	int coarsening_type;
	//! relaxation parameter for SOR smoother
	double relaxation;
	//! switch of scaling of coarse grid correction
	int coarse_scaling;
	//! degree of the polynomial used by AMLI cycle
	int amli_degree;
	//! coefficients of the polynomial used by AMLI cycle
	double *amli_coef;
	//! relaxation parameter for smoothing the tentative prolongation
	double tentative_smooth;
	
	//! data of GS/bGS smoother for reservoir block
	dvector *diaginv; /*< inverse of the diagonal blocks for GS/bGS smoother for reservoir block */
	ivector *pivot; /*< pivot for the GS/bGS smoother for the reservoir matrix */
	//! data of ILU for reservoir block
	ILU_data *LU;
	
	//! data for the argumented well block
	ivector *perf_idx; /*< index of blocks which have perforation */
	ivector *perf_neigh; /*< index of blocks which are neighbors of perforations (include perforations) */
	dCSRmat *WW; /*< Argumented well block */
	//! data for direct solver for argumented well block
	void *Numeric;
	
	//! inverse of the schur complement (-I - Awr*Arr^{-1}*Arw)^{-1}, Arr may be replaced by LU
	double *invS;
	
	//! parameters for krylov method used for blocks
	int maxit; /*< max number of iterations */
	int tol; /*< tolerance */ 
	int restart; /* number of iterations for restart */
	
	//! temporary work space
	dvector r; /*< temporary dvector used to store and restore the residual */
	double *w; /*<  temporary work space for other usage */
	
} precond_FASP_blkoil_data;

#endif /* end if for __FASPBLK_HEADER__ */

/*---------------------------------*/
/*--        End of File          --*/
/*---------------------------------*/
