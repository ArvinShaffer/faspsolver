/*! \file sparse_csr.c
 *  \brief Functions for CSR sparse matrices. 
 */

#include <math.h>
#include <time.h>

#include "fasp.h"
#include "fasp_functs.h"

/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/**
 * \fn dCSRmat fasp_dcsr_create (const INT m, const INT n, const INT nnz)
 *
 * \brief Create CSR sparse matrix data memory space
 *
 * \param m    number of rows
 * \param n    number of columns
 * \param nnz  number of nonzeros
 *
 * \return A   the new dCSRmat matrix
 *
 * \author Chensong Zhang 
 * \date 2010/04/06
 */
dCSRmat fasp_dcsr_create (const INT m, 
                          const INT n, 
                          const INT nnz)
{		
	dCSRmat A;
	
	if ( m > 0 ) {
		A.IA = (INT*)fasp_mem_calloc(m+1,sizeof(INT));
	}
	else {
		A.IA = NULL;
	}
	
#if CHMEM_MODE	
	total_alloc_mem += (m+1)*sizeof(INT);
#endif
	
	if ( n > 0 ) {
		A.JA = (INT*)fasp_mem_calloc(nnz,sizeof(INT));
	}
	else {
		A.JA = NULL;
	}	
	
#if CHMEM_MODE		
	total_alloc_mem += nnz*sizeof(INT);
#endif
	
	if ( nnz > 0 ) {
		A.val=(REAL*)fasp_mem_calloc(nnz,sizeof(REAL));
	}
	else {
		A.val = NULL;
	}
	
#if CHMEM_MODE		
	total_alloc_mem += nnz*sizeof(REAL);
#endif
	
	A.row=m; A.col=n; A.nnz=nnz;
	
	return A;
}

/**
 * \fn void fasp_dcsr_alloc (const INT m, const INT n, const INT nnz, dCSRmat *A)
 *
 * \brief Allocate CSR sparse matrix memory space
 *
 * \param m    integer, number of rows
 * \param n    integer, number of columns
 * \param nnz  integer, number of nonzeros
 * \param *A   pointer to the dCSRmat matrix
 *
 * \author Chensong Zhang 
 * \date 2010/04/06
 */
void fasp_dcsr_alloc (const INT m, 
                      const INT n, 
                      const INT nnz, 
                      dCSRmat *A)
{			
	if ( m > 0 ) {
		A->IA=(INT*)fasp_mem_calloc(m+1,sizeof(INT));
	}
	else {
		A->IA = NULL;
	}
	
#if CHMEM_MODE		
	total_alloc_mem += (m+1)*sizeof(INT);
#endif
	
	if ( n > 0 ) {
		A->JA=(INT*)fasp_mem_calloc(nnz,sizeof(INT));
	}
	else {
		A->JA = NULL;
	}	
	
#if CHMEM_MODE		
	total_alloc_mem += nnz*sizeof(INT);
#endif
	
	if ( nnz > 0 ) {
		A->val=(REAL*)fasp_mem_calloc(nnz,sizeof(REAL));
	}
	else {
		A->val = NULL;
	}
	
#if CHMEM_MODE		
	total_alloc_mem += nnz*sizeof(REAL);
#endif
	
	A->row=m; A->col=n; A->nnz=nnz;
	
	return;	
}

/**
 * \fn void fasp_dcsr_free (dCSRmat *A)
 *
 * \brief Free CSR sparse matrix data memeory space
 *
 * \param *A   pointer to the dCSRmat matrix
 *
 * \author Chensong Zhang
 * \date 2010/04/06 
 */
void fasp_dcsr_free (dCSRmat *A)
{			
	if (A==NULL) return;
	
	fasp_mem_free(A->IA);  A->IA  = NULL;
	fasp_mem_free(A->JA);  A->JA  = NULL;
	fasp_mem_free(A->val); A->val = NULL;
}

/**
 * \fn void fasp_icsr_free (iCSRmat *A)
 *
 * \brief Free CSR sparse matrix data memeory space
 *
 * \param *A   pointer to the iCSRmat matrix
 *
 * \author Chensong Zhang
 * \date 2010/04/06 
 */
void fasp_icsr_free (iCSRmat *A)
{			
	if (A==NULL) return;
	
	fasp_mem_free(A->IA);  A->IA  = NULL;
	fasp_mem_free(A->JA);  A->JA  = NULL;
	fasp_mem_free(A->val); A->val = NULL;
}

/**
 * \fn void fasp_dcsr_init (dCSRmat *A)
 *
 * \brief Initialize CSR sparse matrix
 * 
 * \param *A   pointer to the dCSRmat matrix
 *
 * \author Chensong Zhang
 * \date 2010/04/03  
 */
void fasp_dcsr_init (dCSRmat *A)
{		
	A->row = A->col = A->nnz = 0;
	A->IA  = A->JA  = NULL;
	A->val = NULL;
}

/**
 * \fn dCSRmat fasp_dcsr_perm (dCSRmat *A, INT *P)
 *
 * \brief Apply permutation of A, i.e. Aperm=PAP' by the orders given in P
 *
 * \param *A  pointer to the oringinal dCSRmat matrix
 * \param *P  pointer to orders
 *
 * \return    the new ordered dCSRmat matrix if succeed, NULL if fail
 * 
 * \note   P[i] = k means k-th row and column become i-th row and column
 *
 * \author Shiquan Zhang
 * \date 03/10/2010
 */
dCSRmat fasp_dcsr_perm (dCSRmat *A, 
                        INT *P)
{
	const INT n=A->row,nnz=A->nnz;
	INT *ia=A->IA, *ja=A->JA;
	REAL *Aval=A->val;
	INT i,j,k,jaj,i1,i2,start1,start2;
	
	dCSRmat Aperm = fasp_dcsr_create(n,n,nnz);
	
	// form the tranpose of P
	INT *Pt = (INT*)fasp_mem_calloc(n,sizeof(INT)); 
	for (i=0; i<n; ++i) Pt[P[i]] = i;
	
	// compute IA of P*A (row permutation)
	Aperm.IA[0] = 0;
	for (i=0; i<n; ++i) {
		k = P[i];
		Aperm.IA[i+1] = Aperm.IA[i]+(ia[k+1]-ia[k]);
	}
	
	// perform actual P*A
	for (i=0; i<n; ++i) {
		i1 = Aperm.IA[i]; i2 = Aperm.IA[i+1]-1; 
		k = P[i];
		start1 = ia[k]; start2 = ia[k+1]-1;
		for (j=i1; j<=i2; ++j) {
			jaj = start1+j-i1;
			Aperm.JA[j] = ja[jaj];
			Aperm.val[j] = Aval[jaj];
		}		
	}
	
	// perform P*A*P' (column permutation)
	for (k=0; k<nnz; ++k) {
		j = Aperm.JA[k];
		Aperm.JA[k] = Pt[j];
	}
	
	fasp_mem_free(Pt);
	
	return(Aperm);			
}

/**
 * \fn void fasp_dcsr_sort (dCSRmat *A)
 *
 * \brief Sort each row of A in ascending order w.r.t. column indices.
 *
 * \param *A   pointer to the dCSRmat matrix
 *
 * \author Shiquan Zhang
 * \date 06/10/2010
 */
INT fasp_dcsr_sort (dCSRmat *A)
{
	const INT n=A->col;
	INT i,j,start,row_length;
	INT status = SUCCESS;
	
	// temp memory for sorting rows of A
	INT *index, *ja; 
	REAL *a;
	
	index = (INT*)fasp_mem_calloc(n,sizeof(INT)); 	
	ja    = (INT*)fasp_mem_calloc(n,sizeof(INT)); 	
	a     = (REAL*)fasp_mem_calloc(n,sizeof(REAL)); 	
	
	for (i=0;i<n;++i)
	{
		start=A->IA[i];
		row_length=A->IA[i+1]-start;
		
		for (j=0;j<row_length;++j) index[j]=j;
		
		fasp_aux_iQuickSortIndex(&(A->JA[start]), 0, row_length-1, index);
		
		for (j=0;j<row_length;++j) {
			ja[j]=A->JA[start+index[j]];
			a[j]=A->val[start+index[j]];
		}
		
		for (j=0;j<row_length;++j) {
			A->JA[start+j]=ja[j];
			A->val[start+j]=a[j];
		}
	}
	
	// clean up memory
	fasp_mem_free(index);
	fasp_mem_free(ja);
	fasp_mem_free(a);
	
	return (status);
}

/**
 * \fn void fasp_dcsr_getdiag (INT n, dCSRmat *A, dvector *diag)
 *
 * \brief Get first n diagonal entries of a CSR matrix A
 *
 * \param  n     an interger (if n=0, then get all diagonal entries)
 * \param *A     pointer to dCSRmat CSR matrix
 * \param *diag  pointer to the diagonal as a dvector
 *
 * \author Chensong Zhang
 * \date 05/20/2009
 */
void fasp_dcsr_getdiag (INT n, 
                        dCSRmat *A, 
                        dvector *diag) 
{
	INT i,k,j,ibegin,iend;	
	
	if (n==0) n=MIN(A->row,A->col);
	
	fasp_dvec_alloc(n,diag);
	
	for (i=0;i<n;++i) {
		ibegin=A->IA[i]; iend=A->IA[i+1];
		for (k=ibegin;k<iend;++k) {
			j=N2C(A->JA[N2C(k)]);
			if ((j-i)==0) {
				diag->val[i] = A->val[N2C(k)]; break;
			}
		} // end for k
	} // end for i
	
}

/**
 * \fn INT fasp_dcsr_getcol (const INT n, dCSRmat *A, REAL *col)
 *
 * \brief Get the n-th column of a CSR matrix A
 *
 * \param  n    the index of a column of A (0 <= n <= A.col-1) 
 * \param *A    pointer to dCSRmat CSR matrix
 * \param *col  pointer to the column
 *
 * \return      SUCCESS if succeed, else ERROR (negative value)
 *
 * \author Xiaozhe Hu
 * \date 11/07/2009
 */
INT fasp_dcsr_getcol (const INT n, 
                      dCSRmat *A, 
                      REAL *col) 
{
	INT i,j, row_begin, row_end;
	INT nrow = A->row, ncol = A->col;
	INT status = SUCCESS;
	
	// check the column index n
	if (n < 0 || n >= ncol) 
	{
		printf("getcol_dCSRmat: the column index n is illegal!!\n");
		status = ERROR_MISC;
		goto FINISHED;
	}
	
	// get the column
	for (i=0; i<nrow; ++i) 
	{
		// set the entry to zero
		col[i] = 0.0;
		
		row_begin = A->IA[i]; row_end = A->IA[i+1];
		
		for (j=row_begin; j<row_end; ++j)
		{
			if (A->JA[j] == n)
			{
				col[i] = A->val[j];
			}
		} // end for (j=row_begin; j<row_end; ++j)
		
	} // end for (i=0; i<nrow; ++i)
	
FINISHED:	
	return status;
}

/*!
 * \fn INT fasp_dcsr_diagpref ( dCSRmat *A )
 *
 * \brief Reorder the column and data arrays of a square CSR matrix, 
 *        so that the first entry in each row is the diagonal one.
 *
 * \param *A   pointer to the matrix to be reordered
 *
 * \author Zhiyang Zhou
 * \date 2010/09/09
 *
 * \note Reordering is done in place. 
 * \note Modified by Chensong Zhang on Dec/21/2012
 */
INT fasp_dcsr_diagpref (dCSRmat *A)
{
	REAL      * A_data    = A -> val;
	INT       * A_i       = A -> IA;
	INT       * A_j       = A -> JA;
	const INT   num_rowsA = A -> row; 
	const INT   num_colsA = A -> col; 
	
    // Local variable
	INT    i, j;
    INT    tempi, row_size;
	REAL   tempd;
	
	// the matrix is NOT square
	if (num_rowsA != num_colsA) return ERROR_INPUT_PAR;
	
	for (i = 0; i < num_rowsA; i ++)
	{
		row_size = A_i[i+1] - A_i[i];
		
		for (j = 0; j < row_size; j ++)
		{
			if (A_j[j] == i)
			{
				if (j != 0)
				{
					tempi  = A_j[0];
					A_j[0] = A_j[j];
					A_j[j] = tempi;
					
					tempd     = A_data[0];
					A_data[0] = A_data[j];
					A_data[j] = tempd;
				}
				break;
			}
			
			// diagonal element is missing
			if (j == row_size-1) return ERROR_MISC;
		}
		
		A_j    += row_size;
		A_data += row_size;
	}
	
	return SUCCESS;
}

/**
 * \fn INT fasp_dcsr_regdiag (dCSRmat *A, REAL value)
 *
 * \brief Regularize diagonal entries of a CSR sparse matrix
 *
 * \param *A     pointr to the dCSRmat matrix
 * \param value  set a value on diag(A) which is too close to zero to "value"
 * 
 * \return       SUCCESS if no diagonal entry is close to zero, else ERROR
 *
 * \author Shiquan Zhang
 * \date 11/07/2009
 */
INT fasp_dcsr_regdiag (dCSRmat *A, 
                       REAL value)
{
	const INT    m = A->row;
	const INT * ia = A->IA, * ja = A->JA;
	REAL      * aj = A->val;
    
    // Local variables
	INT i,j,k,begin_row,end_row;
	INT status=RUN_FAIL;
	
	for (i=0;i<m;++i) {
		begin_row=ia[i],end_row=ia[i+1];
		for (k=begin_row;k<end_row;++k) {
			j=ja[k];
			if (i==j) {
				if (aj[k] < 0.0) goto FINISHED;
				else if (aj[k] < SMALLREAL) aj[k]=value;		
			}
		} // end for k
	} // end for i
	
	status = SUCCESS;
	
FINISHED:	
	return status;
}

/**
 * \fn void fasp_dcsr_cp (dCSRmat *A, dCSRmat *B)
 *
 * \brief copy a dCSRmat to a new one B=A
 *
 * \param *A   pointer to the dCSRmat matrix
 * \param *B   pointer to the dCSRmat matrix
 *
 * \author Chensong Zhang
 * \date 04/06/2010  
 */
void fasp_dcsr_cp (dCSRmat *A, 
                   dCSRmat *B)
{		
	B->row=A->row;
	B->col=A->col;
	B->nnz=A->nnz;
	
	memcpy(B->IA,A->IA,(A->row+1)*sizeof(INT));
	memcpy(B->JA,A->JA,(A->nnz)*sizeof(INT));
	memcpy(B->val,A->val,(A->nnz)*sizeof(REAL));
}

/**
 * \fn iCSRmat fasp_icsr_trans (iCSRmat *A)
 *
 * \brief Find transpose of iCSRmat matrix A
 *
 * \param *A   pointer to the iCSRmat matrix
 *
 * \return     the transpose of iCSRmat matrix A
 *
 * \author Chensong Zhang
 * \date 04/06/2010  
 */
void fasp_icsr_trans (iCSRmat *A, 
                      iCSRmat *AT)
{
	const INT n=A->row, m=A->col, nnz=A->nnz, m1=m-1;
    
    // Local variables
	INT i,j,k,p;
	INT ibegin, iend;
	
#if DEBUG_MODE
	printf("m=%d, n=%d, nnz=%d\n",m,n,nnz);
#endif
	
	AT->row=m; AT->col=n; AT->nnz=nnz;
	
	AT->IA=(INT*)fasp_mem_calloc(m+1,sizeof(INT));
	
#if CHMEM_MODE
	total_alloc_mem += (m+1)*sizeof(INT);
#endif
	
	AT->JA=(INT*)fasp_mem_calloc(nnz,sizeof(INT));
	
#if CHMEM_MODE
	total_alloc_mem += (nnz)*sizeof(REAL);
#endif
	
	if (A->val) { 
		AT->val=(INT*)fasp_mem_calloc(nnz,sizeof(INT)); 
		
#if CHMEM_MODE
		total_alloc_mem += (nnz)*sizeof(INT);
#endif
		
	}
	else { 
        AT->val=NULL; 
    }	
	
	// first pass: find the number of nonzeros in the first m-1 columns of A 
	// Note: these numbers are stored in the array AT.IA from 1 to m-1
	for (i=0;i<m;++i) AT->IA[i] = 0;
	
	for (j=0;j<nnz;++j) {
		i=N2C(A->JA[j]); // column number of A = row number of A'
		if (i<m1) AT->IA[i+2]++;
	}
	
	for (i=2;i<=m;++i) AT->IA[i]+=AT->IA[i-1];
	
	// second pass: form A'
	if (A->val != NULL) {
		for (i=0;i<n;++i) {
			ibegin=A->IA[i], iend=A->IA[i+1];		
			for(p=ibegin;p<iend;p++) {
				j=A->JA[N2C(p)]+1;
				k=AT->IA[N2C(j)];
				AT->JA[N2C(k)]=C2N(i);
				AT->val[N2C(k)]=A->val[N2C(p)];
				AT->IA[j]=k+1;
			} // end for p
		} // end for i
	}
	else {
		for (i=0;i<n;++i) {
			ibegin=A->IA[i], iend=A->IA[i+1];		
			for(p=ibegin;p<iend;p++) {
				j=A->JA[N2C(p)]+1;
				k=AT->IA[N2C(j)];
				AT->JA[N2C(k)]=C2N(i);
				AT->IA[j]=k+1;
			} // end for p
		} // end for i	
	} // end if
}	

/**
 * \fn INT fasp_dcsr_trans (dCSRmat *A, dCSRmat *AT)
 *
 * \brief Find tranpose of dCSRmat matrix A
 *
 * \param *A   pointer to the dCSRmat matrix
 * \param *AT  pointer to the transpose of dCSRmat matrix A (output)
 *
 * \return     SUCCESS if succeeded, ERROR message if failed
 *
 * \author Chensong Zhang
 * \date 04/06/2010   
 */
INT fasp_dcsr_trans (dCSRmat *A, 
                     dCSRmat *AT)
{
	const INT n=A->row, m=A->col, nnz=A->nnz;	
	INT status = SUCCESS;
    
    // Local variables
	INT i,j,k,p;
	
	AT->row=m;
	AT->col=n;
	AT->nnz=nnz;
	
	AT->IA=(INT*)fasp_mem_calloc(m+1,sizeof(INT));
	
#if CHMEM_MODE
	total_alloc_mem += (m+1)*sizeof(INT);
#endif
	
	AT->JA=(INT*)fasp_mem_calloc(nnz,sizeof(INT));
	
#if CHMEM_MODE
	total_alloc_mem += (nnz)*sizeof(INT);
#endif
	
	if (A->val) { 
		AT->val=(REAL*)fasp_mem_calloc(nnz,sizeof(REAL)); 
		
#if CHMEM_MODE
		total_alloc_mem += (nnz)*sizeof(REAL);
#endif
		
	}
	else { AT->val=NULL; }
	
	// first pass: find the number of nonzeros in the first m-1 columns of A 
	// Note: these numbers are stored in the array AT.IA from 1 to m-1
	for (i=0;i<m;++i) AT->IA[i] = 0;
	
	for (j=0;j<nnz;++j) {
		i=N2C(A->JA[j]); // column number of A = row number of A'
		if (i<m-1) AT->IA[i+2]++;
	}
	
	for (i=2;i<=m;++i) AT->IA[i]+=AT->IA[i-1];
	
	// second pass: form A'
	if (A->val) {
		for (i=0;i<n;++i) {
			INT ibegin=A->IA[i], iend1=A->IA[i+1];
			
			for(p=ibegin;p<iend1;p++) {
				j=A->JA[N2C(p)]+1;
				k=AT->IA[N2C(j)];
				AT->JA[N2C(k)]=C2N(i);
				AT->val[N2C(k)]=A->val[N2C(p)];
				AT->IA[j]=k+1;
			} // end for p
		} // end for i
		
	}
	else {
		for (i=0;i<n;++i) {
			INT ibegin=A->IA[i], iend1=A->IA[i+1];
			
			for(p=ibegin;p<iend1;p++) {
				j=A->JA[N2C(p)]+1;
				k=AT->IA[N2C(j)];
				AT->JA[N2C(k)]=C2N(i);
				AT->IA[j]=k+1;
			} // end for p
		} // end of i
		
	} // end if 
	
	return (status);
}	

/* 
 * \fn void fasp_dcsr_transpose (INT *row[2], INT *col[2], REAL *val[2], 
 *                               INT *nn, INT *tniz)
 *
 * \brief Transpose of an IJ matrix.
 *
 * \param *row[2]  pointers of the rows of the matrix and its transpose
 * \param *col[2]  pointers of the columns of the matrix and its transpose
 * \param *val[2]  pointers to the values of the matrix and its transpose
 * \param *nn      number of rows and columns of the matrix
 * \param *tniz    number of the nonzeros in the matrices A and A'
 *
 * \note This subroutine transpose in IJ format IN ORDER. 
 */
void fasp_dcsr_transpose (INT *row[2], 
                          INT *col[2], 
                          REAL *val[2], 
                          INT *nn, 
                          INT *tniz)
{
    const INT nca=nn[1]; // number of columns
	
    INT *izc    = (INT *)fasp_mem_calloc(nn[1],sizeof(INT));	
    INT *izcaux = (INT *)fasp_mem_calloc(nn[1],sizeof(INT));
	
    // Local variables
	INT i,m,itmp;
    
	// first pass: to set order right	
    for (i=0;i<tniz[0];++i) izc[N2C(col[0][i])]++;
	
    izcaux[0]=0;
    for (i=1;i<nca;++i) izcaux[i]=izcaux[i-1]+izc[i-1];
	
	// second pass: form transpose
	memset(izc,0,nca*sizeof(INT));
	
    for (i=0;i<tniz[0];++i) {
		m=col[0][i]; 
		itmp=izcaux[m]+izc[m];
		row[1][itmp]=m;
		col[1][itmp]=row[0][i];
		val[1][itmp]=val[0][i];
		izc[m]++;
    }
	
	fasp_mem_free(izc);
	fasp_mem_free(izcaux);
}

/**
 * \fn INT fasp_dcsr_compress (dCSRmat *A, dCSRmat *B, REAL dtol)
 *
 * \brief Compress a CSR matrix A and store in CSR matrix B by
 *        dropping small entries abs(aij)<=dtol
 *
 * \param *A    pointer to dCSRmat CSR matrix
 * \param *B    pointer to dCSRmat CSR matrix
 * \param dtol  drop tolerance
 *
 * \return      SUCCESS if succeeded, or ERROR message if failed
 *
 * \author Shiquan Zhang
 * \date 03/10/2010
 */
INT fasp_dcsr_compress (dCSRmat *A, 
                        dCSRmat *B, 
                        REAL dtol)
{
	INT status = SUCCESS;
	INT i, j, k;
	INT ibegin,iend1;	
	
	INT *index=(INT*)fasp_mem_calloc(A->nnz,sizeof(INT));
	
	B->row=A->row; B->col=A->col;
	
	B->IA=(INT*)fasp_mem_calloc(A->row+1,sizeof(INT));
	
	B->IA[0]=A->IA[0];
	
	// first pass: determine the size of B
	k=0;
	for (i=0;i<A->row;++i) {
		ibegin=A->IA[i]; iend1=A->IA[i+1];	
		for (j=ibegin;j<iend1;++j)
			if (ABS(A->val[N2C(j)])>dtol) {
				index[k]=N2C(j);
				++k;
			} /* end of j */
		B->IA[i+1]=C2N(k);
	} /* end of i */
	B->nnz=k;
	
	B->JA=(INT*)fasp_mem_calloc(B->nnz,sizeof(INT));	
	B->val=(REAL*)fasp_mem_calloc(B->nnz,sizeof(REAL));
	
	// second pass: generate the index and element to B
	for (i=0;i<B->nnz;++i) {
		B->JA[i]=A->JA[index[i]];
		B->val[i]=A->val[index[i]]; 
	}  
	
	fasp_mem_free(index);
	
	return (status);
}

/**
 * \fn INT fasp_dcsr_compress_inplace (dCSRmat *A, REAL dtol)
 *
 * \brief Compress a CSR matrix A IN PLACE by
 *        dropping small entries abs(aij)<=dtol
 *
 * \param *A    pointer to dCSRmat CSR matrix
 * \param dtol  drop tolerance
 *
 * \author Xiaozhe Hu
 * \data 12/25/2010
 * \note this can be modified for filtering!!!
 */
INT fasp_dcsr_compress_inplace (dCSRmat *A, 
                                REAL dtol)
{
	const INT row=A->row;
	const INT nnz=A->nnz;
	
	INT i, j, k;
	INT ibegin,iend=A->IA[0];	
	INT status = SUCCESS;
	
	k=0;
	for (i=0;i<row;++i) {
		ibegin=iend; iend=A->IA[i+1];	
		for (j=ibegin;j<iend;++j)
			if (ABS(A->val[N2C(j)])>dtol) {
				A->JA[N2C(k)] = A->JA[N2C(j)];
				A->val[N2C(k)] = A->val[N2C(j)];
				++k;
			} /* end of j */
		A->IA[i+1]=C2N(k);
	} /* end of i */
	
	if (k<=nnz) 
	{
		A->nnz=k;
		A->JA = (INT*)fasp_mem_realloc(A->JA, k*sizeof(INT));
		A->val = (REAL *)fasp_mem_realloc(A->val, k*sizeof(REAL));
	}
	else 
	{
		status = RUN_FAIL;
		printf("### ERROR: Size of compressed matrix is larger than the original!!\n");
	}
	
	return (status);
}

/**
 * \fn INT fasp_dcsr_shift (dCSRmat *A, INT offset)
 *
 * \brief Reindex a REAL matrix in CSR format to make the index starting from 0 or 1
 *
 * \param *A        pointer to CSR matrix
 * \param  offset   size of offset (1 or -1)
 *
 * \return SUCCESS if succeed
 *
 * \author Chensong Zhang
 * \date 04/06/2010  
 */
INT fasp_dcsr_shift (dCSRmat *A, 
                     INT offset)
{
	const INT nnz=A->nnz;
	const INT n=A->row;
	INT i, *ai=A->IA, *aj=A->JA;
	
	if (offset == 0) offset = ISTART;
	
	for (i=0; i<=n; ++i) ai[i]+=offset;
	
	for (i=0;i<nnz;++i) aj[i]+=offset;
	
	return SUCCESS;
}

/**
 * \fn void fasp_dcsr_symdiagscale (dCSRmat *A, dvector *diag)
 *
 * \brief Symmetric diagonal scaling D^{-1/2}AD^{-1/2}
 *
 * \param *A      pointer to the dCSRmat matrix
 * \param *diag  pointer to the diagonal entries
 * \return void
 *
 * \author Xiaozhe Hu
 * \date 01/31/2011
 */
void fasp_dcsr_symdiagscale (dCSRmat *A, 
                             dvector *diag)
{
	// information about matrix A
	const INT n = A->row;
	INT *IA = A->IA;
	INT *JA = A->JA;
	REAL *val = A->val;
	
	// local variables
	unsigned INT i, j, k, row_start, row_end;
	
	if (diag->row != n)
	{
		printf("### ERROR: size of diagonal %d and size of matrix %d do not match!!", 
               diag->row, n);
		exit(ERROR_MISC);
	}
	
	// work space
	REAL *work = (REAL *)fasp_mem_calloc(n, sizeof(REAL));
	
	// square root of diagonal entries
	for (i=0; i<n; i++) work[i] = sqrt(diag->val[i]);
	
	// main loop
	for (i=0; i<n; i++)
	{
		row_start = IA[i]; row_end = IA[i+1];
		for (j=row_start; j<row_end; j++)
		{
			k = JA[j];
			val[j] = val[j]/(work[i]*work[k]);
		}
	}
	
	// free work space
	if (work) fasp_mem_free(work);
	
	return;
}

/*-----------------------------------omp--------------------------------------*/

/**
 * \fn void fasp_dcsr_getdiag_omp (INT n, dCSRmat *A, dvector *diag, INT nthreads, INT openmp_holds) 
 * \brief Get first n diagonal entries of a CSR matrix A
 *
 * \param  n     an interger (if n=0, then get all diagonal entries)
 * \param *A     pointer to dCSRmat CSR matrix
 * \param *diag  pointer to the diagonal as a dvector
 * \param nthreads number of threads
 * \param openmp_holds threshold of parallelization
 *
 * \author Feng Chunsheng, Yue Xiaoqiang
 * \date 03/01/2011
 */
void fasp_dcsr_getdiag_omp (INT n, 
                            dCSRmat *A, 
                            dvector *diag, 
                            INT nthreads, 
                            INT openmp_holds) 
{
#if FASP_USE_OPENMP
	INT i,k,j,ibegin,iend;	
	
	if (n==0) n=MIN(A->row,A->col);
	
	fasp_dvec_alloc(n,diag);
	
	if (n > openmp_holds) {
		INT myid, mybegin, myend;
#pragma omp parallel private(myid, mybegin, myend, i, ibegin, iend, k, j) //num_threads(nthreads)
		{
			myid = omp_get_thread_num();
			FASP_GET_START_END(myid, nthreads, n, mybegin, myend);
			for (i=mybegin; i<myend; i++)
			{
				ibegin=A->IA[i]; iend=A->IA[i+1];
				for (k=ibegin;k<iend;++k) {
					j=N2C(A->JA[N2C(k)]);
					if ((j-i)==0) {
						diag->val[i] = A->val[N2C(k)]; break;
					} // end if
				} // end for k
			} // end for i
		}
	}
	else {
		for (i=0;i<n;++i) {
			ibegin=A->IA[i]; iend=A->IA[i+1];
			for (k=ibegin;k<iend;++k) {
				j=N2C(A->JA[N2C(k)]);
				if ((j-i)==0) {
					diag->val[i] = A->val[N2C(k)]; break;
				} // end if
			} // end for k
		} // end for i
	}
#endif
}

/**
 * \fn void fasp_dcsr_cp_omp (dCSRmat *A, dCSRmat *B, INT nthreads, INT openmp_holds)
 * \brief copy a dCSRmat to a new one B=A
 *
 * \param *A   pointer to the dCSRmat matrix
 * \param *B   pointer to the dCSRmat matrix
 * \param nthreads number of threads
 * \param openmp_holds threshold of parallelization
 *
 * \author Feng Chunsheng, Yue Xiaoqiang
 * \date 03/01/2011
 */
void fasp_dcsr_cp_omp (dCSRmat *A, 
                       dCSRmat *B, 
                       INT nthreads, 
                       INT openmp_holds)
{
#if FASP_USE_OPENMP
	B->row=A->row;
	B->col=A->col;
	B->nnz=A->nnz;
	
	fasp_iarray_cp_omp(A->row+1, A->IA, B->IA, nthreads, openmp_holds);
	fasp_iarray_cp_omp(A->nnz, A->JA, B->JA, nthreads, openmp_holds);
	fasp_array_cp_omp(A->nnz, A->val, B->val, nthreads, openmp_holds);
#endif
}

/*---------------------------------*/
/*--        End of File          --*/
/*---------------------------------*/
