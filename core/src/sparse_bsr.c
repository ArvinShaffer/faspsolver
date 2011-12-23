/*! \file sparse_bsr.c
 *  \brief Simple operations for BSR format
 *
 */

#include "fasp.h"
#include "fasp_functs.h"

/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/**
 * \fn dBSRmat fasp_dbsr_create (int ROW, int COL, int NNZ, int nb, int storage_manner)
 * \brief Create BSR sparse matrix data memory space
 *
 * \param ROW             integer, number of rows of block
 * \param COL             integer, number of columns of block
 * \param NNZ             integer, number of nonzero blocks
 * \param nb              integer, dimension of exch block
 * \param storage_manner  integer, storage manner for each sub-block 
 * \return A              the new dBSRmat matrix
 *
 * \author Xiaozhe Hu
 * \date 10/26/2010  
 */
dBSRmat fasp_dbsr_create (int ROW, 
													int COL, 
													int NNZ, 
													int nb, 
													int storage_manner)
{		
	dBSRmat A;
	
	if ( ROW > 0 ) {
		A.IA = (int*)fasp_mem_calloc(ROW+1, sizeof(int));
	}
	else {
		A.IA = NULL;
	}
	
#if CHMEM_MODE	
	total_alloc_mem += (2*ROW)*sizeof(int);
#endif
	
	if ( NNZ > 0 ) {
		A.JA = (int*)fasp_mem_calloc(NNZ ,sizeof(int));
	}
	else {
		A.JA = NULL;
	}	
	
#if CHMEM_MODE		
	total_alloc_mem += NNZ*sizeof(int);
#endif
	
	if ( nb > 0 && NNZ > 0) {
		A.val = (double*)fasp_mem_calloc(NNZ*nb*nb, sizeof(double));
	}
	else {
		A.val = NULL;
	}
	
#if CHMEM_MODE		
	total_alloc_mem += (NNZ*nb*nb)*sizeof(double);
#endif
	
	A.ROW = ROW; A.COL = COL; A.NNZ = NNZ; 
	A.nb = nb; A.storage_manner = storage_manner;
	
	return A;
}

/**
 * \fn void fasp_dbsr_alloc (int ROW, int COL, int NNZ, int nb, int storage_manner, dBSRmat *A)
 * \brief Allocate memory space for BSR format sparse matrix
 *
 * \param ROW             integer, number of rows of block
 * \param COL             integer, number of columns of block
 * \param NNZ             integer, number of nonzero blocks
 * \param nb              integer, dimension of exch block
 * \param storage_manner  integer, storage manner for each sub-block 
 * \param *A              pointer to the dBSRmat matrix
 *
 * \author Xiaozhe Hu
 * \date 10/26/2010 
 */
void fasp_dbsr_alloc (int ROW, 
											int COL, 
											int NNZ, 
											int nb, 
											int storage_manner, 
											dBSRmat *A)
{		
	if ( ROW > 0 ) {
		A->IA = (int*)fasp_mem_calloc(ROW+1, sizeof(int));
	}
	else {
		A->IA = NULL;
	}
	
#if CHMEM_MODE	
	total_alloc_mem += (2*ROW)*sizeof(int);
#endif
	
	if ( NNZ > 0 ) {
		A->JA = (int*)fasp_mem_calloc(NNZ ,sizeof(int));
	}
	else {
		A->JA = NULL;
	}	
	
#if CHMEM_MODE		
	total_alloc_mem += NNZ*sizeof(int);
#endif
	
	if ( nb > 0 ) {
		A->val = (double*)fasp_mem_calloc(NNZ*nb*nb, sizeof(double));
	}
	else {
		A->val = NULL;
	}
	
#if CHMEM_MODE		
	total_alloc_mem += (NNZ*nb*nb)*sizeof(double);
#endif
	
	A->ROW = ROW; A->COL = COL; A->NNZ = NNZ; 
	A->nb = nb; A->storage_manner = storage_manner;
	
	return;
}


/**
 * \fn void fasp_dbsr_free (dBSRmat *A)
 * \brief Free memeory space for BSR format sparse matrix
 *
 * \param *A   pointer to the dBSRmat matrix
 *
 * \author Xiaozhe Hu
 * \date 10/26/2010 
 */
void fasp_dbsr_free (dBSRmat *A)
{	
	if (A==NULL) return;
	
	fasp_mem_free(A->IA);
	fasp_mem_free(A->JA);
	fasp_mem_free(A->val);
	
	A->ROW = 0;
	A->COL = 0;
	A->NNZ = 0;
	A->nb = 0;
	A->storage_manner = 0;
}

/**
 * \fn void fasp_dbsr_init (dBSRmat *A)
 * \brief Initialize sparse matrix on structured grid
 *
 * \param *A pointer to the dBSRmat matrix
 *
 * \author Xiaozhe Hu
 * \date 10/26/2010 
 */
void fasp_dbsr_init (dBSRmat *A)
{		
	A->ROW=0;
	A->COL=0;
	A->NNZ=0;
	A->nb=0;
	A->storage_manner=0;
	A->IA=NULL;
	A->JA=NULL;
	A->val=NULL;
}

/*!
 * \fn int fasp_dbsr_diagpref ( dBSRmat *A )
 * \brief Reorder the column and data arrays of a square BSR matrix, 
 *        so that the first entry in each row is the diagonal one.
 *
 * \param *A   pointer to the BSR matrix 
 *
 * \author Xiaozhe Hu
 * \date 03/10/2011
 *
 * \note Reordering is done in place. 
 *
 */
int fasp_dbsr_diagpref (dBSRmat *A)
{
	int    i, j, tempi, row_size;
	
	double *A_data = A -> val;
	int    *A_i    = A -> IA;
	int    *A_j    = A -> JA;
	int     num_rowsA = A -> ROW; 
	int     num_colsA = A -> COL; 
	int     nb = A->nb;
	int	   nb2 = nb*nb;
	
	/* the matrix should be square */
	if (num_rowsA != num_colsA) return -1;
	
	double *tempd = (double*)fasp_mem_calloc(nb2, sizeof(double));
	
	for (i = 0; i < num_rowsA; i ++)
	{
		row_size = A_i[i+1] - A_i[i];
		
		for (j = 0; j < row_size; j ++)
		{
			if (A_j[j] == i)
			{
				if (j != 0)
				{
					// swap index
					tempi  = A_j[0];
					A_j[0] = A_j[j];
					A_j[j] = tempi;
					
					// swap block
					memcpy(tempd, A_data, (nb2)*sizeof(double));
					memcpy(A_data, A_data+j*nb2,  (nb2)*sizeof(double));
					memcpy(A_data+j*nb2, tempd, (nb2)*sizeof(double));
				}
				break;
			}
			
			/* diagonal element is missing */
			if (j == row_size-1) return -2;
		}
		
		A_j    += row_size;
		A_data += row_size*nb2;
	}
	
	//! free tempd
	fasp_mem_free(tempd);
	
	return 0;
}

/**
 * \fn dBSRmat fasp_dbsr_diaginv ( dBSRmat *A )
 * \brief Compute B := D^{-1}*A, where 'D' is the block diagonal part of A.
 * \param *A pointer to the dBSRmat matrix
 * \author Zhou Zhiyang
 *
 * \note: works for general nb (Xiaozhe)
 * \data 2010/10/26
 */
dBSRmat fasp_dbsr_diaginv (dBSRmat *A)
{
	//! members of A 
	int     ROW = A->ROW;
	int     COL = A->COL;
	int     NNZ = A->NNZ;
	int     nb  = A->nb;
	int    *IA  = A->IA;
	int    *JA  = A->JA;   
	double *val = A->val;
	
	dBSRmat B;
	int    *IAb  = NULL;
	int    *JAb  = NULL;
	double *valb = NULL;
	
	//! Create a dBSRmat 'B'
	B = fasp_dbsr_create(ROW, COL, NNZ, nb, 0);
	
	double *diaginv = NULL;
	
	int nb2  = nb*nb;
	int size = ROW*nb2;
	int i,j,k,m,l;        
	
	IAb  = B.IA;
	JAb  = B.JA;
	valb = B.val;        
	
	memcpy(IAb, IA, (ROW+1)*sizeof(int));
	memcpy(JAb, JA, NNZ*sizeof(int));
	
	//! allocate memory   
	diaginv = (double *)fasp_mem_calloc(size, sizeof(double));         
	
	//! get all the diagonal sub-blocks   
	for (i = 0; i < ROW; ++i)
	{
		for (k = IA[i]; k < IA[i+1]; ++k)
		{
			if (JA[k] == i)
				memcpy(diaginv+i*nb2, val+k*nb2, nb2*sizeof(double));
		}
	}
	
	//! compute the inverses of all the diagonal sub-blocks   
	if (nb > 1)
	{
		for (i = 0; i < ROW; ++i)
		{
			fasp_blas_smat_inv(diaginv+i*nb2, nb);
		}
	}
	else
	{
		for (i = 0; i < ROW; ++i)
		{  
			//! zero-diagonal should be tested previously
			diaginv[i] = 1.0 / diaginv[i];
		}
	}
	
	//! compute D^{-1}*A
	for (i = 0; i < ROW; ++i)
	{
		for (k = IA[i]; k < IA[i+1]; ++k)
		{
			m = k*nb2;
			j = JA[k];
			if (j == i)
			{  
				//! Identity sub-block
				memset(valb+m, 0X0, nb2*sizeof(double));
				for (l = 0; l < nb; l ++) valb[m+l*nb+l] = 1.0;
			}
			else
			{  
				fasp_blas_smat_mul(diaginv+i*nb2, val+m, valb+m, nb);
			}
		}
	}
	
	fasp_mem_free(diaginv);
	
	return (B);
}

/**
 * \fn dBSRmat fasp_dbsr_diaginv2 ( dBSRmat *A, double *diaginv )
 * \brief Compute B := D^{-1}*A, where 'D' is the block diagonal part of A.
 * \param *A pointer to the dBSRmat matrix
 * \param *diaginv pointer to the inverses of all the diagonal blocks
 * \author Zhou Zhiyang
 *
 * \note: works for general nb (Xiaozhe)
 * \data 2010/11/07
 */
dBSRmat fasp_dbsr_diaginv2 (dBSRmat *A, 
														double *diaginv)
{
	//! members of A 
	const int ROW = A->ROW;
	const int COL = A->COL;
	const int NNZ = A->NNZ;
	const int nb  = A->nb, nbp1 = nb+1; 
	const int nb2 = nb*nb;
	
	int    *IA  = A->IA;
	int    *JA  = A->JA;   
	double *val = A->val;
	
	dBSRmat B;
	int    *IAb  = NULL;
	int    *JAb  = NULL;
	double *valb = NULL;
	
	int i,k,m,l,ibegin,iend;
	
	//! Create a dBSRmat 'B'
	B = fasp_dbsr_create(ROW, COL, NNZ, nb, 0);
	IAb  = B.IA;
	JAb  = B.JA;
	valb = B.val;        
	
	memcpy(IAb, IA, (ROW+1)*sizeof(int));
	memcpy(JAb, JA, NNZ*sizeof(int));
	
	//! compute D^{-1}*A
	for (i = 0; i < ROW; ++i)
	{
		ibegin = IA[i]; iend = IA[i+1];
		for (k = ibegin; k < iend; ++k)
		{
			m = k*nb2;
			if (JA[k] != i)
			{  
				fasp_blas_smat_mul(diaginv+i*nb2, val+m, valb+m, nb);
			}
			else
			{  
				//! Identity sub-block
				memset(valb+m, 0X0, nb2*sizeof(double));
				for (l = 0; l < nb; l ++) valb[m+l*nbp1] = 1.0;
			}
		}
	}
	
	return (B);
}

/**
 * \fn dBSRmat fasp_dbsr_diaginv3 (dBSRmat *A, double *diaginv)
 * \brief Compute B := D^{-1}*A, where 'D' is the block diagonal part of A.
 * \param *A pointer to the dBSRmat matrix
 * \param *diaginv pointer to the inverses of all the diagonal blocks
 * \author Xiaozhe Hu
 *
 * \note: works for general nb (Xiaozhe)
 * \date 12/25/2010
 */
dBSRmat fasp_dbsr_diaginv3 (dBSRmat *A, 
														double *diaginv)
{
	//! members of A 
	int     ROW = A->ROW;
	int     COL = A->COL;
	int     NNZ = A->NNZ;
	int     nb  = A->nb;
	int    *IA  = A->IA;
	int    *JA  = A->JA;   
	double *val = A->val;
	
	dBSRmat B;
	int    *IAb  = NULL;
	int    *JAb  = NULL;
	double *valb = NULL;
	
	int nb2  = nb*nb;
	int i,j,k,m;    
	
	//! Create a dBSRmat 'B'
	B = fasp_dbsr_create(ROW, COL, NNZ, nb, 0);
	
	IAb  = B.IA;
	JAb  = B.JA;
	valb = B.val;        
	
	memcpy(IAb, IA, (ROW+1)*sizeof(int));
	memcpy(JAb, JA, NNZ*sizeof(int));
	
	
	switch (nb)
	{
		case 2:
			//! main loop 
			for (i = 0; i < ROW; ++i)
			{
				//! get the diagonal sub-blocks
				for (k = IA[i]; k < IA[i+1]; ++k)
				{
					if (JA[k] == i)
					{
						m = k*4;
						memcpy(diaginv+i*4, val+m, 4*sizeof(double));
						fasp_smat_identity_nc2(valb+m);
					}
				}
				
				//! compute the inverses of the diagonal sub-blocks 
				fasp_blas_smat_inv_nc2(diaginv+i*4);
				
				//! compute D^{-1}*A
				for (k = IA[i]; k < IA[i+1]; ++k)
				{
					m = k*4;
					j = JA[k];
					if (j != i) fasp_blas_smat_mul_nc2(diaginv+i*4, val+m, valb+m);
				}
			}// end of main loop
			
			break;	
			
		case 3:
			//! main loop 
			for (i = 0; i < ROW; ++i)
			{
				//! get the diagonal sub-blocks
				for (k = IA[i]; k < IA[i+1]; ++k)
				{
					if (JA[k] == i)
					{
						m = k*9;
						memcpy(diaginv+i*9, val+m, 9*sizeof(double));
						fasp_smat_identity_nc3(valb+m);
					}
				}
				
				//! compute the inverses of the diagonal sub-blocks 
				fasp_blas_smat_inv_nc3(diaginv+i*9);
				
				//! compute D^{-1}*A
				for (k = IA[i]; k < IA[i+1]; ++k)
				{
					m = k*9;
					j = JA[k];
					if (j != i) fasp_blas_smat_mul_nc3(diaginv+i*9, val+m, valb+m);
				}
			}// end of main loop
			
			break;
			
		case 5: 
			//! main loop 
			for (i = 0; i < ROW; ++i)
			{
				//! get the diagonal sub-blocks
				for (k = IA[i]; k < IA[i+1]; ++k)
				{
					if (JA[k] == i)
					{
						m = k*25;
						memcpy(diaginv+i*25, val+m, 25*sizeof(double));
						fasp_smat_identity_nc5(valb+m);
					}
				}
				
				//! compute the inverses of the diagonal sub-blocks 
				fasp_blas_smat_inv_nc5(diaginv+i*25);
				
				//! compute D^{-1}*A
				for (k = IA[i]; k < IA[i+1]; ++k)
				{
					m = k*25;
					j = JA[k];
					if (j != i) fasp_blas_smat_mul_nc5(diaginv+i*25, val+m, valb+m);
				}
			}// end of main loop
			
			break;
			
		case 7:
			//! main loop 
			for (i = 0; i < ROW; ++i)
			{
				//! get the diagonal sub-blocks
				for (k = IA[i]; k < IA[i+1]; ++k)
				{
					if (JA[k] == i)
					{
						m = k*49;
						memcpy(diaginv+i*49, val+m, 49*sizeof(double));
						fasp_smat_identity_nc7(valb+m);
					}
				}
				
				//! compute the inverses of the diagonal sub-blocks 
				fasp_blas_smat_inv_nc7(diaginv+i*49);
				
				//! compute D^{-1}*A
				for (k = IA[i]; k < IA[i+1]; ++k)
				{
					m = k*49;
					j = JA[k];
					if (j != i) fasp_blas_smat_mul_nc7(diaginv+i*49, val+m, valb+m);
				}
			}// end of main loop
			
			break;
			
		default:
			//! main loop 
			for (i = 0; i < ROW; ++i)
			{
				//! get the diagonal sub-blocks
				for (k = IA[i]; k < IA[i+1]; ++k)
				{
					if (JA[k] == i)
					{
						m = k*nb2;
						memcpy(diaginv+i*nb2, val+m, nb2*sizeof(double));
						fasp_smat_identity(valb+m, nb, nb2);
					}
				}
				
				//! compute the inverses of the diagonal sub-blocks 
				fasp_blas_smat_inv(diaginv+i*nb2, nb);
				
				//! compute D^{-1}*A
				for (k = IA[i]; k < IA[i+1]; ++k)
				{
					m = k*nb2;
					j = JA[k];
					if (j != i) fasp_blas_smat_mul(diaginv+i*nb2, val+m, valb+m, nb);
				}
			}// end of main loop
			
			break;
	}
	
	return (B);
}

/**
 * \fn dBSRmat fasp_dbsr_diaginv4 (dBSRmat *A, double *diaginv)
 * \brief Compute B := D^{-1}*A, where 'D' is the block diagonal part of A.
 * \param *A pointer to the dBSRmat matrix (A is preordered that the first block of each row is the diagonal block!!)
 * \param *diaginv pointer to the inverses of all the diagonal blocks
 * \author Xiaozhe Hu
 * \date 03/12/2011
 *
 * \note: works for general nb (Xiaozhe)
 * \note A is preordered that the first block of each row is the diagonal block!!
 */
dBSRmat fasp_dbsr_diaginv4 (dBSRmat *A, 
														double *diaginv)
{
	//! members of A 
	int     ROW = A->ROW;
	int     COL = A->COL;
	int     NNZ = A->NNZ;
	int     nb  = A->nb;
	int    *IA  = A->IA;
	int    *JA  = A->JA;   
	double *val = A->val;
	
	dBSRmat B;
	int    *IAb  = NULL;
	int    *JAb  = NULL;
	double *valb = NULL;
	
	int nb2  = nb*nb;
	int i,j,k,m;  
	int ibegin, iend;  
	
	//! Create a dBSRmat 'B'
	B = fasp_dbsr_create(ROW, COL, NNZ, nb, 0);
	
	IAb  = B.IA;
	JAb  = B.JA;
	valb = B.val;        
	
	memcpy(IAb, IA, (ROW+1)*sizeof(int));
	memcpy(JAb, JA, NNZ*sizeof(int));
	
	
	switch (nb)
	{
		case 2:
			//! main loop 
			for (i = 0; i < ROW; ++i)
			{
				ibegin = IA[i]; iend = IA[i+1];
				//! get the diagonal sub-blocks (It is the first block of each row)
				m = ibegin*4;
				memcpy(diaginv+i*4, val+m, 4*sizeof(double));
				fasp_smat_identity_nc2(valb+m);
				
				//! compute the inverses of the diagonal sub-blocks 
				fasp_blas_smat_inv_nc2(diaginv+i*9);
				
				//! compute D^{-1}*A
				for (k = ibegin+1; k < iend; ++k)
				{
					m = k*4;
					j = JA[k];
					fasp_blas_smat_mul_nc2(diaginv+i*4, val+m, valb+m);
				}
			}// end of main loop
			
			break;	
			
		case 3:
			//! main loop 
			for (i = 0; i < ROW; ++i)
			{
				ibegin = IA[i]; iend = IA[i+1];
				//! get the diagonal sub-blocks (It is the first block of each row)
				m = ibegin*9;
				memcpy(diaginv+i*9, val+m, 9*sizeof(double));
				fasp_smat_identity_nc3(valb+m);
				
				//! compute the inverses of the diagonal sub-blocks 
				fasp_blas_smat_inv_nc3(diaginv+i*9);
				
				//! compute D^{-1}*A
				for (k = ibegin+1; k < iend; ++k)
				{
					m = k*9;
					j = JA[k];
					fasp_blas_smat_mul_nc3(diaginv+i*9, val+m, valb+m);
				}
			}// end of main loop
			
			break;
			
		case 5: 
			//! main loop 
			for (i = 0; i < ROW; ++i)
			{
				//! get the diagonal sub-blocks
				ibegin = IA[i]; iend = IA[i+1];
				m = ibegin*25;
				memcpy(diaginv+i*25, val+m, 25*sizeof(double));
				fasp_smat_identity_nc5(valb+m);
				
				//! compute the inverses of the diagonal sub-blocks 
				fasp_blas_smat_inv_nc5(diaginv+i*25);
				
				//! compute D^{-1}*A
				for (k = ibegin+1; k < iend; ++k)
				{
					m = k*25;
					j = JA[k];
					fasp_blas_smat_mul_nc5(diaginv+i*25, val+m, valb+m);
				}
			}// end of main loop
			
			break;
			
		case 7:
			//! main loop 
			for (i = 0; i < ROW; ++i)
			{
				//! get the diagonal sub-blocks
				ibegin = IA[i]; iend = IA[i+1];	
				m = ibegin*49;
				memcpy(diaginv+i*49, val+m, 49*sizeof(double));
				fasp_smat_identity_nc7(valb+m);
				
				//! compute the inverses of the diagonal sub-blocks 
				fasp_blas_smat_inv_nc7(diaginv+i*49);
				
				//! compute D^{-1}*A
				for (k = ibegin+1; k < iend; ++k)
				{
					m = k*49;
					j = JA[k];
					fasp_blas_smat_mul_nc7(diaginv+i*49, val+m, valb+m);
				}
			}// end of main loop
			
			break;
			
		default:
			//! main loop 
			for (i = 0; i < ROW; ++i)
			{
				//! get the diagonal sub-blocks
				ibegin = IA[i]; iend = IA[i+1];
				m = ibegin*nb2;
				memcpy(diaginv+i*nb2, val+m, nb2*sizeof(double));
				fasp_smat_identity(valb+m, nb, nb2);
				
				//! compute the inverses of the diagonal sub-blocks 
				fasp_blas_smat_inv(diaginv+i*nb2, nb);
				
				//! compute D^{-1}*A
				for (k = ibegin+1; k < iend; ++k)
				{
					m = k*nb2;
					j = JA[k];
					fasp_blas_smat_mul(diaginv+i*nb2, val+m, valb+m, nb);
				}
			}// end of main loop
			
			break;
	}
	
	return (B);
}


/*!
 * \fn fasp_dbsr_getdiag ( int n, dBSRmat *A, double *diag )
 * \brief Abstract the diagonal blocks of a BSR matrix.
 * \param dSTRmat *B pointer to the 'dBSRmat' type matrix
 * \param *diag pointer to array which stores the diagonal blocks in row by row manner
 * \author Zhou Zhiyang
 *
 * \note: works for general nb (Xiaozhe)
 * \date 2010/10/26 
 */
void fasp_dbsr_getdiag (int n, 
												dBSRmat *A, 
												double *diag )
{
	int i,k;
	
	int nb2 = A->nb*A->nb;
	
	for (i = 0; i < n; ++i)
	{
		for (k = A->IA[i]; k < A->IA[i+1]; ++k)
		{
			if (A->JA[k] == i) {
				memcpy(diag+i*nb2, A->val+k*nb2, nb2*sizeof(double));
				break;
			}
		}
	}
	
}

/*---------------------------------*/
/*--        End of File          --*/
/*---------------------------------*/
