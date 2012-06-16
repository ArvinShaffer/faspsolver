/*! \file interpolation.c
 *  \brief Interpolation operators for AMG
 */

#include <math.h>
#include <time.h>
#include <omp.h>

#include "fasp.h"
#include "fasp_functs.h"

static SHORT invden(INT nn, REAL *mat, REAL *invmat);
static SHORT get_block(dCSRmat *A, INT m, INT n, INT *rows, INT *cols, REAL *Aloc, INT *mask);
static SHORT gentisquare_nomass(dCSRmat *A, INT mm, INT *Ii, REAL *ima, INT *mask);
static SHORT getinonefull(INT **mat, REAL **matval, INT *lengths, INT mm, INT *Ii, REAL *ima);
static SHORT orderone(INT **mat, REAL **matval, INT *lengths);
static SHORT genintval(dCSRmat *A, INT **itmat, REAL **itmatval, INT ittniz, INT *isol, INT numiso, INT nf, INT nc);
static SHORT getiteval(dCSRmat *A, dCSRmat *it);
static void interp_RS(dCSRmat *A, ivector *vertices, dCSRmat *Ptr, AMG_param *param);
void interp_EM(dCSRmat *A, ivector *vertices, dCSRmat *Ptr, AMG_param *param);
static void interp_RS2(dCSRmat *A, ivector *vertices, dCSRmat *Ptr, AMG_param *param);
static void interp_RS1(dCSRmat *A, ivector *vertices, dCSRmat *Ptr, AMG_param *param, INT *icor_ysk);
static void fasp_get_icor_ysk(INT nrows, INT ncols, INT *CoarseIndex, INT nbl_ysk, INT nbr_ysk, INT *CF_marker, INT *icor_ysk);
INT fasp_BinarySearch(INT *list, INT value, INT list_length);
static void fasp_get_nbl_nbr_ysk(dCSRmat *A, INT *nbl_ptr, INT *nbr_ptr);
static void fasp_mod_coarse_index(INT nrows, INT *CoarseIndex);
static void interp_STD (dCSRmat *A, ivector *vertices, dCSRmat *P, iCSRmat *S, AMG_param *param);
/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/**
 * \fn SHORT fasp_amg_interp (dCSRmat *A, ivector *vertices, dCSRmat *P, AMG_param *param)
 *
 * \brief Generate interpolation P 
 *
 * \param A          pointer to the stiffness matrix
 * \param vertices   pointer to the indicator of CF split node is on fine (current) or coarse grid
 * \param P          pointer to the dCSRmat matrix of resulted interpolation
 * \param param      pointer to AMG parameters
 *
 * \return           SUCCESS or error message
 *
 * \author Xuehai Huang, Chensong Zhang
 * \date 04/04/2010 
 *
 * \note: modified by Xiaozhe Hu: add S as input -- 05/23/2012
 */
SHORT fasp_amg_interp (dCSRmat *A, 
                       ivector *vertices, 
                       dCSRmat *P, 
					   iCSRmat *S,
                       AMG_param *param)
{
    INT interp_type=param->interpolation_type;
	const INT coarsening_type = param->coarsening_type;
    INT status = SUCCESS;
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_amg_interp ...... [Start]\n");
#endif
    // make sure standard interpolaiton is used for aggressive coarsening
    if (coarsening_type == 4) interp_type = INTERP_STD;
    
    /*-- Standard interpolation operator --*/
    switch (interp_type) {
    case INTERP_REG: // Standard interpolation
        interp_RS(A, vertices, P, param); 
        break;            
    case INTERP_ENG_MIN: // Energy min interpolation in C
        interp_EM(A, vertices, P, param);
        break;
    case INTERP_STD: // standard interpolation
		interp_STD(A, vertices, P, S, param);
    default:
        fasp_chkerr(ERROR_AMG_INTERP_TYPE, "fasp_amg_interp");
        break;
    }
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_amg_interp ...... [Finish]\n");
#endif
    
    if (status==SUCCESS) return status;
    else exit(status);
}


/*---------------------------------*/
/*--      Private Functions      --*/
/*---------------------------------*/

/**
 * \fn static SHORT invden (INT nn, REAL *mat, REAL *invmat)
 *
 * \brief the routine is to find the inverse of a dense matrix
 *
 * \param nn      Size of the matrix
 * \param mat     pointer to the full matrix
 * \param invmat  pointer to the full inverse matrix
 *
 * \return        SUCCESS or error message
 * 
 * \note this routine works for symmetric matrix.
 *
 * \author Xuehai Huang
 * \date   04/04/2009 
 */
static SHORT invden (INT nn, 
                     REAL *mat, 
                     REAL *invmat)
{
    INT    i,j;
    SHORT  status = SUCCESS;
    
    INT  *pivot=(INT *)fasp_mem_calloc(nn,sizeof(INT));    
    REAL *rhs=(REAL *)fasp_mem_calloc(nn,sizeof(REAL));    
    REAL *sol=(REAL *)fasp_mem_calloc(nn,sizeof(REAL));
    
    fasp_smat_lu_decomp(mat,pivot,nn);
    
    for (i=0;i<nn;++i) {
        for (j=0;j<nn;++j) rhs[j]=0.;
        rhs[i]=1.;
        fasp_smat_lu_solve(mat,rhs,pivot,sol,nn);
        for (j=0;j<nn;++j) invmat[i*nn+j]=sol[j];
    }
    
    fasp_mem_free(pivot);
    fasp_mem_free(rhs);
    fasp_mem_free(sol);
    
    return status;
}

/**
 * \fn static SHORT get_block (dCSRmat *A, INT m, INT n, INT *rows, INT *cols, REAL *Aloc, INT *mask)
 *
 * \brief Get a local block from a CSR sparse matrix
 *
 * \param A     pointer to a sparse matrix
 * \param m     Number of rows of the local block matrix
 * \param n     Number of columns of the local block matrix
 * \param rows  Indices to local rows
 * \param cols  Indices to local columns
 * \param Aloc  Local dense matrix saved as an array
 * \param mask  Working array, which should be a negative number initially
 *
 * \return      SUCCESS or error message
 *
 * \author Xuehai Huang
 * \date   04/04/2009 
 */
static SHORT get_block (dCSRmat *A, 
                        INT m, 
                        INT n, 
                        INT *rows, 
                        INT *cols, 
                        REAL *Aloc, 
                        INT *mask)
{
    unsigned INT i, j, k, iloc;
    
    for ( i=0; i<m; ++i ) {
        for ( j=0; j<n; ++j ) {
            Aloc[i*n+j] = 0.0; // initialize Aloc
        }    
    }
    
    for ( j=0; j<n; ++j ) {
        mask[N2C(cols[j])] = j; // initialize mask, mask stores C indices 0,1,... 
    }    
    
    for ( i=0; i<m; ++i ) {
        iloc=N2C(rows[i]);
        for ( k=A->IA[iloc]; k<A->IA[iloc+1]; ++k ) {
            j = N2C(A->JA[N2C(k)]);
            if (mask[j]>=0) Aloc[i*n+mask[j]]=A->val[k];
        } /* end for k */
    } /* enf for i */
    
    for ( j=0; j<n; ++j ) mask[N2C(cols[j])] = -1; // re-initialize mask
    
    return SUCCESS;
}

/**
 * \fn static SHORT gentisquare_nomass (dCSRmat *A, INT mm, INT *Ii, REAL *ima, INT *mask)
 *
 * \brief given the row indices and col indices, to find a block submatrix and get its inverse
 *
 * \param A     pointer to the whole matrix
 * \param mm    Size of the submatrix
 * \param Ii    Integer array, to store the indices of row (also col)
 * \param ima   pointer to the inverse of the full submatrix, the storage is row by row
 * \param mask  Working array
 *
 * \return      SUCCESS or error message
 *
 * \author Xuehai Huang
 * \date   04/04/2010 
 */
static SHORT gentisquare_nomass (dCSRmat *A, 
                                 INT mm, 
                                 INT *Ii, 
                                 REAL *ima, 
                                 INT *mask)
{
    SHORT status = SUCCESS;
    
    REAL *ms = (REAL *)fasp_mem_calloc(mm*mm,sizeof(REAL));    
    
    status = get_block(A,mm,mm,Ii,Ii,ms,mask);
    status = invden(mm,ms,ima);
    
    fasp_mem_free(ms);
    return status;
}

/**
 * \fn static SHORT getinonefull (INT **mat, REAL **matval, INT *lengths, INT mm, INT *Ii, REAL *ima)
 *
 * \brief Add a small submatrix to a big matrix with respect to its row and cols in the big matrix
 *
 * \param mat      pointer pointing to the structure of the matrix
 * \param matval   pointer pointing to the values according to the structure
 * \param lengths  2d array, the second entry is the lengths of matval
 * \param mm       Number of the rows (also the columns) 
 * \param Ii       pointer to the array to store the relative position of the rows and cols
 * \param ima      pointer to the full submatrix, the sequence is row by row
 *
 * \return         SUCCESS or error message
 *
 * \author Xuehai Huang
 * \date   04/04/2009 
 */
static SHORT getinonefull (INT **mat,
                           REAL **matval, 
                           INT *lengths, 
                           INT mm, 
                           INT *Ii, 
                           REAL *ima)
{
    INT tniz,i,j;
    
    tniz=lengths[1];
    for (i=0;i<mm;++i) {
        for (j=0;j<mm;++j) {    
            mat[0][tniz+i*mm+j]=Ii[i];
            mat[1][tniz+i*mm+j]=Ii[j];
            matval[0][tniz+i*mm+j]=ima[i*mm+j];
        }
    }
    lengths[1]=tniz+mm*mm;
    
    return SUCCESS;
}

/**
 * \fn static SHORT orderone (INT **mat, REAL **matval, INT *lengths)
 *
 * \brief Order a cluster of entries in a sequence
 *
 * \param *mat      pointer to the relative position of the entries
 * \param *matval   pointer to the values corresponding to the position
 * \param lengths   INT array, to store the number of rows, number of cols and number of nonzerow
 *
 * \return          SUCCESS or error message
 *
 * \author Xuehai Huang
 * \date   04/04/2009 
 */
static SHORT orderone (INT **mat, 
                       REAL **matval, 
                       INT *lengths)
//    lengths[0] for the number of rows
//    lengths[1] for the number of cols
//    lengths[2] for the number of nonzeros
{
    INT *rows[2],*cols[2],nns[2],tnizs[2];
    REAL *vals[2];
    SHORT status = SUCCESS;
    INT tniz,i;
    
    nns[0]=lengths[0];
    nns[1]=lengths[1];
    tnizs[0]=lengths[2];
    tniz=lengths[2];
    
    rows[0]=(INT *)fasp_mem_calloc(tniz,sizeof(INT));    
    cols[0]=(INT *)fasp_mem_calloc(tniz,sizeof(INT));    
    vals[0]=(REAL *)fasp_mem_calloc(tniz,sizeof(REAL));
    
    for (i=0;i<tniz;++i) {
        rows[0][i]=mat[0][i];
        cols[0][i]=mat[1][i];
        vals[0][i]=matval[0][i];
    }
    
    rows[1]=(INT *)fasp_mem_calloc(tniz,sizeof(INT));    
    cols[1]=(INT *)fasp_mem_calloc(tniz,sizeof(INT));    
    vals[1]=(REAL *)fasp_mem_calloc(tniz,sizeof(REAL));
    
    fasp_dcsr_transpose(rows,cols,vals,nns,tnizs);
    
    // all the nonzeros with same col are gathering together
    
    for (i=0;i<tniz;++i) {
        rows[0][i]=rows[1][i];
        cols[0][i]=cols[1][i];
        vals[0][i]=vals[1][i];
    }
    tnizs[1]=nns[0];
    nns[0]=nns[1];
    nns[1]=tnizs[1];
    tnizs[1]=tnizs[0];
    fasp_dcsr_transpose(rows,cols,vals,nns,tnizs);
    
    // all the nozeros with same col and row are gathering togheter    
    
    for (i=0;i<tniz;++i) {
        rows[0][i]=rows[1][i];
        cols[0][i]=cols[1][i];
        vals[0][i]=vals[1][i];
    }
    tnizs[1]=nns[0];
    nns[0]=nns[1];
    nns[1]=tnizs[1];
    tnizs[1]=tnizs[0];
    
    tniz=tnizs[0];
    for (i=0;i<tniz-1;++i) {
        if (rows[0][i]==rows[0][i+1]&&cols[0][i]==cols[0][i+1]) {
            vals[0][i+1]+=vals[0][i];
            rows[0][i]=nns[0];
            cols[0][i]=nns[1];
        }
    }
    nns[0]=nns[0]+1;
    nns[1]=nns[1]+1;
    
    fasp_dcsr_transpose(rows,cols,vals,nns,tnizs);
    
    for (i=0;i<tniz;++i) {
        rows[0][i]=rows[1][i];
        cols[0][i]=cols[1][i];
        vals[0][i]=vals[1][i];
    }
    tnizs[1]=nns[0];
    nns[0]=nns[1];
    nns[1]=tnizs[1];
    tnizs[1]=tnizs[0];
    
    fasp_dcsr_transpose(rows,cols,vals,nns,tnizs);
    
    for (i=0;i<tniz;++i) {
        rows[0][i]=rows[1][i];
        cols[0][i]=cols[1][i];
        vals[0][i]=vals[1][i];
    }
    tnizs[1]=nns[0];
    nns[0]=nns[1];
    nns[1]=tnizs[1];
    tnizs[1]=tnizs[0];
    
    tniz=0;
    for (i=0;i<tnizs[0];++i)
        if (rows[0][i]<nns[0]-1) tniz++;
    
    for (i=0;i<tniz;++i) {
        mat[0][i]=rows[0][i];
        mat[1][i]=cols[0][i];
        matval[0][i]=vals[0][i];
    }
    nns[0]=nns[0]-1;
    nns[1]=nns[1]-1;
    lengths[0]=nns[0];
    lengths[1]=nns[1];
    lengths[2]=tniz;
    
    fasp_mem_free(rows[0]);
    fasp_mem_free(rows[1]);
    fasp_mem_free(cols[0]);
    fasp_mem_free(cols[1]);
    fasp_mem_free(vals[0]);
    fasp_mem_free(vals[1]);
    
    return(status);
}


/**
 * \fn static SHORT genintval (dCSRmat *A, INT **itmat, REAL **itmatval, INT ittniz, INT *isol, 
 *                             INT numiso, INT nf, INT nc) 
 *
 * \brief Given the structure of the interpolation, to get the evaluation of the interpolation
 *
 * \param A         pointer to the dCSRmat matrix
 * \param itmat     pointer to the structure of the interpolation
 * \param itmatval  pointer to the evaluation of the interpolation
 * \param isol      ???
 * \param numiso    ???
 * \param ittniz    Length of interpolation
 * \param nf        Number of fine-level nodes
 * \param nc        Number of coarse-level nodes
 *
 * \return          SUCCESS or error message
 *
 * \author Xuehai Huang
 * \date   10/29/2010  
 *
 * \note 
 *  nf=number fine, nc= n coarse    
 *  Suppose that the structure of the interpolation is known. 
 *  It is N*m matrix, N>m, recorded in itmat.
 *  We record its row index and col index, note that the same col indices gather together
 *  the itma and itmatval have a special data structure
 *  to be exact, the same columns gather together
 *  itmat[0] record the column number, and itmat[1] record the row number.
 */
static SHORT genintval (dCSRmat *A, 
                        INT **itmat, 
                        REAL **itmatval, 
                        INT ittniz, 
                        INT *isol, 
                        INT numiso, 
                        INT nf, 
                        INT nc) 
{
    INT *Ii=NULL, *mask=NULL;
    REAL *ima=NULL, *pex=NULL, **imas=NULL;
    INT **mat=NULL;
    REAL **matval;
    INT lengths[3];
    dCSRmat T;
    INT tniz;
    dvector sol, rhs;
    
    INT mm,sum,i,j,k;
    INT *iz,*izs,*izt,*izts;
    SHORT status=SUCCESS;
    
    itsolver_param itparam;
    itparam.print_level    = PRINT_NONE;
    itparam.itsolver_type  = SOLVER_CG;
    itparam.stop_type      = STOP_REL_RES;
    itparam.tol            = 1e-3; 
    itparam.maxit          = 100;
    itparam.restart        = 100;
    
    mask=(INT *)fasp_mem_calloc(nf,sizeof(INT));    
    iz=(INT *)fasp_mem_calloc(nc,sizeof(INT));    
    izs=(INT *)fasp_mem_calloc(nc,sizeof(INT));    
    izt=(INT *)fasp_mem_calloc(nf,sizeof(INT));    
    izts=(INT *)fasp_mem_calloc(nf,sizeof(INT));
    
    for (i=0;i<nf;++i) mask[i]=-1;
    
    for (i=0;i<nc;++i) iz[i]=0;
    
    for (i=0;i<ittniz;++i) iz[itmat[0][i]]++;
    
    izs[0]=0;
    for (i=1;i<nc;++i) izs[i]=izs[i-1]+iz[i-1];
    
    for (sum=i=0;i<nc;++i) sum+=iz[i]*iz[i];
    
    imas=(REAL **)fasp_mem_calloc(nc,sizeof(REAL *));
    
    for (i=0;i<nc;++i) {
        imas[i]=(REAL *)fasp_mem_calloc(iz[i]*iz[i],sizeof(REAL));
    }
    
    mat=(INT **)fasp_mem_calloc(2,sizeof(INT *));
    mat[0]=(INT *)fasp_mem_calloc((sum+numiso),sizeof(INT));    
    mat[1]=(INT *)fasp_mem_calloc((sum+numiso),sizeof(INT));    
    matval=(REAL **)fasp_mem_calloc(1,sizeof(REAL *));    
    matval[0]=(REAL *)fasp_mem_calloc(sum+numiso,sizeof(REAL));
    
    lengths[1]=0;
    
    for (i=0;i<nc;++i) {    
    
        mm=iz[i]; 
        Ii=(INT *)fasp_mem_realloc(Ii,mm*sizeof(INT));
    
        for (j=0;j<mm;++j) Ii[j]=itmat[1][izs[i]+j];
    
        ima=(REAL *)fasp_mem_realloc(ima,mm*mm*sizeof(REAL));
    
        gentisquare_nomass(A,mm,Ii,ima,mask);
    
        getinonefull(mat,matval,lengths,mm,Ii,ima);
    
        for (j=0;j<mm*mm;++j) imas[i][j]=ima[j];
    }
    
    for (i=0;i<numiso;++i) {
        mat[0][sum+i]=isol[i];
        mat[1][sum+i]=isol[i];
        matval[0][sum+i]=1.0;
    }
    
    lengths[0]=nf;
    lengths[2]=lengths[1]+numiso;
    lengths[1]=nf;
    orderone(mat,matval,lengths);
    tniz=lengths[2];
    
    sol.row=nf;
    sol.val=(REAL*)fasp_mem_calloc(nf,sizeof(REAL));
    
    for (i=0;i<nf;++i) izt[i]=0;
    
    for (i=0;i<tniz;++i) izt[mat[0][i]]++;
    
    T.IA=(INT*)fasp_mem_calloc((nf+1),sizeof(INT));
    
    T.row=nf;
    T.col=nf;
    T.nnz=tniz;
    T.IA[0]=0;
    for (i=1;i<nf+1;++i) T.IA[i]=T.IA[i-1]+izt[i-1];
    
    T.JA=(INT*)fasp_mem_calloc(tniz,sizeof(INT));
    
    for (j=0;j<tniz;++j) T.JA[j]=mat[1][j];
    
    T.val=(REAL*)fasp_mem_calloc(tniz,sizeof(REAL));
    
    for (j=0;j<tniz;++j) T.val[j]=matval[0][j];
    
    rhs.val=(REAL*)fasp_mem_calloc(nf,sizeof(REAL));
    
    for (i=0;i<nf;++i) rhs.val[i]=1.0;
    rhs.row=nf;
    
    fasp_solver_dcsr_krylov_diag(&T,&rhs,&sol,&itparam);
    
    for (i=0;i<nc;++i) {
        mm=iz[i];
    
        ima=(REAL *)fasp_mem_realloc(ima,mm*mm*sizeof(REAL));
    
        pex=(REAL *)fasp_mem_realloc(pex,mm*sizeof(REAL));
    
        Ii=(INT *)fasp_mem_realloc(Ii,mm*sizeof(INT));
    
        for (j=0;j<mm;++j) Ii[j]=itmat[1][izs[i]+j];
    
        for (j=0;j<mm*mm;++j) ima[j]=imas[i][j];
    
        for (k=0;k<mm;++k) {
            for (pex[k]=j=0;j<mm;++j) pex[k]+=ima[k*mm+j]*sol.val[Ii[j]];
        }
        for (j=0;j<mm;++j) itmatval[0][izs[i]+j]=pex[j];
    
    }
    
    fasp_mem_free(ima);
    fasp_mem_free(pex);
    fasp_mem_free(Ii);
    fasp_mem_free(mask);
    fasp_mem_free(iz);
    fasp_mem_free(izs);
    fasp_mem_free(izt);
    fasp_mem_free(izts);
    fasp_mem_free(mat[0]);
    fasp_mem_free(mat[1]);
    fasp_mem_free(matval[0]);
    
    return status;    
}

/**
 * \fn static SHORT getiteval (dCSRmat *A, dCSRmat *it)
 *
 * \brief Given a coarsening (in the form of an interpolation operator), inherit the structure,
 *        get new evaluation
 *
 * \param A    pointer to the dCSRmat matrix
 * \param it   pointer to the interpolation matrix
 *
 * \author Xuehai Huang, Chensong Zhang
 * \date   10/29/2010  
 */
static SHORT getiteval (dCSRmat *A, 
                        dCSRmat *it)
{
    INT nf,nc,ittniz;
    INT *itmat[2];
    REAL **itmatval;
    INT *rows[2],*cols[2];
    REAL *vals[2];
    INT nns[2],tnizs[2];
    INT i,j,numiso;
    INT *isol;
    SHORT status = SUCCESS;
    
    nf=A->row;
    nc=it->col;
    ittniz=it->IA[nf]; 
    
    itmat[0]=(INT *)fasp_mem_calloc(ittniz,sizeof(INT));    
    itmat[1]=(INT *)fasp_mem_calloc(ittniz,sizeof(INT));    
    itmatval=(REAL **)fasp_mem_calloc(1,sizeof(REAL *));    
    itmatval[0]=(REAL *)fasp_mem_calloc(ittniz,sizeof(REAL));    
    isol=(INT *)fasp_mem_calloc(nf,sizeof(INT));
    
    numiso=0;
    for (i=0;i<nf;++i) {
        if (it->IA[i]==it->IA[i+1]) {
            isol[numiso]=i;
            numiso++;
        }
    }
    
    for (i=0;i<nf;++i) {
        for (j=it->IA[i];j<it->IA[i+1];++j) itmat[0][j]=i;
    }
    
    for (j=0;j<ittniz;++j) itmat[1][j]=it->JA[j];
    
    for (j=0;j<ittniz;++j) itmatval[0][j]=it->val[j];
    
    rows[0]=(INT *)fasp_mem_calloc(ittniz,sizeof(INT));    
    cols[0]=(INT *)fasp_mem_calloc(ittniz,sizeof(INT));    
    vals[0]=(REAL *)fasp_mem_calloc(ittniz,sizeof(REAL));
    
    for (i=0;i<ittniz;++i) {
        rows[0][i]=itmat[0][i];
        cols[0][i]=itmat[1][i];
        vals[0][i]=itmat[0][i];
    }
    
    nns[0]=nf;
    nns[1]=nc;
    tnizs[0]=ittniz;
    
    rows[1]=(INT *)fasp_mem_calloc(ittniz,sizeof(INT));    
    cols[1]=(INT *)fasp_mem_calloc(ittniz,sizeof(INT));    
    vals[1]=(REAL *)fasp_mem_calloc(ittniz,sizeof(REAL));
    
    fasp_dcsr_transpose(rows,cols,vals,nns,tnizs);
    for (i=0;i<ittniz;++i) {
        itmat[0][i]=rows[1][i];
        itmat[1][i]=cols[1][i];
        itmatval[0][i]=vals[1][i];
    }
    genintval(A,itmat,itmatval,ittniz,isol,numiso,nf,nc);
    
    for (i=0;i<ittniz;++i) {
        rows[0][i]=itmat[0][i];
        cols[0][i]=itmat[1][i];
        vals[0][i]=itmatval[0][i];
    }
    nns[0]=nc;
    nns[1]=nf;
    tnizs[0]=ittniz;
    
    fasp_dcsr_transpose(rows,cols,vals,nns,tnizs);
    
    for (i=0;i<ittniz;++i) it->val[i]=vals[1][i];
    
    fasp_mem_free(isol);
    fasp_mem_free(itmat[0]); 
    fasp_mem_free(itmat[1]);
    fasp_mem_free(itmatval[0]); 
    fasp_mem_free(itmatval);
    fasp_mem_free(rows[0]);
    fasp_mem_free(rows[1]);
    fasp_mem_free(cols[0]);
    fasp_mem_free(cols[1]);
    fasp_mem_free(vals[0]);
    fasp_mem_free(vals[1]);
    
    return status;
}

/**
 * \fn static void interp_EM (dCSRmat *A, ivector *vertices, dCSRmat *P, AMG_param *param)
 *
 * \brief Energy-min interpolation 
 *
 * \param A          pointer to the stiffness matrix
 * \param vertices   pointer to the indicator of CF split node is on fine or coarse grid
 * \param P          pointer to the dCSRmat matrix of resulted interpolation
 * \param param      pointer to AMG parameters 
 *
 * \author Shuo Zhang, Xuehai Huang
 * \date   04/04/2010 
 *
 * \note Ref J. Xu and L. Zikatanov
 *           On An Energy Minimazing Basis in Algebraic Multigrid Methods
 *           Computing and visualization in sciences, 2003
 */
void interp_EM (dCSRmat *A, 
                ivector *vertices, 
                dCSRmat *P, 
                AMG_param *param)
{
    INT i, j, index;
    INT *vec=vertices->val;    
    INT *CoarseIndex=(INT*)fasp_mem_calloc(vertices->row,sizeof(INT));
    
    for (index=i=0;i<vertices->row;++i) {
        if (vec[i]==1) {
            CoarseIndex[i]=index;
            index++;
        }
    }
    
    for (i=0;i<P->nnz;++i) {
        j=P->JA[i];
        P->JA[i]=CoarseIndex[j];
    }
    
    // clean up memory
    fasp_mem_free(CoarseIndex);    
    
    // main part 
    getiteval(A, P); 
    
    return;
}

/**
 * \fn static void interp_RS (dCSRmat *A, ivector *vertices, dCSRmat *Ptr, AMG_param *param)
 *
 * \brief Direct interpolation 
 *
 * \param A         pointer to the stiffness matrix
 * \param vertices  pointer to the indicator of CF split node is on fine or coarse grid
 * \param Ptr       pointer to the dCSRmat matrix of resulted interpolation
 * \param param     pointer to AMG parameters
 *
 * \author Xuehai Huang
 * \date   01/31/2009  
 * 
 *
 * \note Ref P479, U. Trottenberg, C. W. Oosterlee, and A. Sch¨uller. Multigrid. 
 *             Academic Press Inc., San Diego, CA, 2001. 
 *           With contributions by A. Brandt, P. Oswald and K. St¨uben.
 */

static void interp_RS( dCSRmat *A, 
		               ivector *vertices, 
				       dCSRmat *Ptr, 
				       AMG_param *param )
{
    REAL epsilon_tr = param->truncation_threshold;
    REAL amN, amP, apN, apP;
    REAL alpha, beta, aii=0;
    INT *vec = vertices->val;
    INT countPplus, diagindex;
    
    unsigned INT i,j,k,l,index=0;
    INT begin_row, end_row;
    INT myid;
    INT mybegin;
    INT myend;
    INT stride_i;
    
	INT nthreads, use_openmp;

	if(!FASP_USE_OPENMP || A->row <= OPENMP_HOLDS){
		use_openmp = FALSE;
	}
	else{
		use_openmp = TRUE;
        nthreads = FASP_GET_NUM_THREADS();
	}
    
    /** Generate interpolation P */
    dCSRmat P=fasp_dcsr_create(Ptr->row,Ptr->col,Ptr->nnz);
    
    /** step 1: Find first the structure IA of P */
    fasp_iarray_cp(P.row+1, Ptr->IA, P.IA);
    
    /** step 2: Find the structure JA of P */
    fasp_iarray_cp(P.nnz, Ptr->JA, P.JA);
    
    /** step 3: Fill the data of P */
    if (use_openmp) {
        stride_i = A->row/nthreads;
#pragma omp parallel private(myid,mybegin,myend,i,begin_row,end_row,diagindex,aii,amN,amP,apN,apP,countPplus,j,k,alpha,beta,l) num_threads(nthreads)
        {
            myid = omp_get_thread_num();
            mybegin = myid*stride_i;
            if(myid < nthreads-1) myend = mybegin+stride_i;
            else myend = A->row;
            for (i=mybegin; i<myend; ++i){
				begin_row=A->IA[i]; end_row=A->IA[i+1]-1;    
				for(diagindex=begin_row;diagindex<=end_row;diagindex++){
                        if (A->JA[diagindex]==i) {
                            aii=A->val[diagindex];
                            break;
                        }
				}
				if(vec[i]==0){  // if node i is on fine grid 
					amN=0, amP=0, apN=0, apP=0,  countPplus=0;
					for(j=begin_row;j<=end_row;++j){
						if(j==diagindex) continue;
						for(k=Ptr->IA[i];k<Ptr->IA[i+1];++k) {
							if(Ptr->JA[k]==A->JA[j]) break;
						}
						if(A->val[j]>0) {
							apN+=A->val[j];
							if(k<Ptr->IA[i+1]) {
								apP+=A->val[j];
								countPplus++;
							}
						}
						else {
							amN+=A->val[j];
							if(k<Ptr->IA[i+1]) {
								amP+=A->val[j];
							}
						}
					} // j
                            
					alpha=amN/amP;
					if(countPplus>0) {
						beta=apN/apP;
					}
					else {
						beta=0;
						aii+=apN;
					}
					for(j=P.IA[i];j<P.IA[i+1];++j){
						k=P.JA[j];
						for(l=A->IA[i];l<A->IA[i+1];l++){
							if(A->JA[l]==k) break;
						}
						if(A->val[l]>0){
							P.val[j]=-beta*A->val[l]/aii;
						}
						else {
							P.val[j]=-alpha*A->val[l]/aii;
						}
					}
				}
				else if(vec[i]==2) // if node i is a special fine node
				{
                        
				}
				else {// if node i is on coarse grid 
					P.val[P.IA[i]]=1;
				}
			}
        }
    }
    else {
        for(i=0;i<A->row;++i){
			begin_row=A->IA[i]; end_row=A->IA[i+1]-1;    
			for(diagindex=begin_row;diagindex<=end_row;diagindex++) {
				if (A->JA[diagindex]==i) {
					aii=A->val[diagindex];
					break;
				}
			}
			if(vec[i]==0){  // if node i is on fine grid 
				amN=0, amP=0, apN=0, apP=0,  countPplus=0;
				for(j=begin_row;j<=end_row;++j){
					if(j==diagindex) continue;
					for(k=Ptr->IA[i];k<Ptr->IA[i+1];++k) {
						if(Ptr->JA[k]==A->JA[j]) break;
					}
					if(A->val[j]>0) {
						apN+=A->val[j];
						if(k<Ptr->IA[i+1]) {
							apP+=A->val[j];
							countPplus++;
						}
					}
					else {
						amN+=A->val[j];
						if(k<Ptr->IA[i+1]) {
							amP+=A->val[j];
						}
					}
				} // j
                        
				alpha=amN/amP;
				if(countPplus>0) {
					beta=apN/apP;
				}
				else {
					beta=0;
					aii+=apN;
				}
				for(j=P.IA[i];j<P.IA[i+1];++j) {
					k=P.JA[j];
					for(l=A->IA[i];l<A->IA[i+1];l++){
						if(A->JA[l]==k) break;
					}
					if(A->val[l]>0) {
						P.val[j]=-beta*A->val[l]/aii;
					}
					else {
						P.val[j]=-alpha*A->val[l]/aii;
					}
				}
			}
			else if(vec[i]==2)  // if node i is a special fine node
			{
                    
			}
			else {// if node i is on coarse grid 
				P.val[P.IA[i]]=1;
			}
		}
	}
    
    fasp_mem_free(Ptr->IA);
    fasp_mem_free(Ptr->JA);
    fasp_mem_free(Ptr->val);    
#if 1
    INT *CoarseIndex=(INT*)fasp_mem_calloc(A->row, sizeof(INT));
#else
    INT *CoarseIndex=(INT*)fasp_mem_calloc(vertices->row, sizeof(INT));
#endif
    
#if CHMEM_MODE
#if 1
    total_alloc_mem += (A->row)*sizeof(INT);
#else
    total_alloc_mem += (vertices->row)*sizeof(INT);
#endif
#endif
    
    index=0;
#if 1
    for(i=0;i<A->row;++i)
#else
        for(i=0;i<vertices->row;++i)
#endif
            {
                if(vec[i]==1)
                    {
                        CoarseIndex[i]=index;
                        index++;
                    }
            }

	if(!FASP_USE_OPENMP || P.IA[P.row] <= OPENMP_HOLDS){
		use_openmp = FALSE;
	}
	else{
		use_openmp = TRUE;
        nthreads = FASP_GET_NUM_THREADS();
	}
    if (use_openmp) {
        stride_i = P.IA[P.row]/nthreads;
#pragma omp parallel private(myid,mybegin,myend,i,j) num_threads(nthreads)
        {
            myid = omp_get_thread_num();
            mybegin = myid*stride_i;
            if(myid < nthreads-1) myend = mybegin+stride_i;
            else myend = P.IA[P.row];
            for (i=mybegin; i<myend; ++i)
                {
                    j=P.JA[i];
                    P.JA[i]=CoarseIndex[j];
                }
        }
    }
    else {
        for(i=0;i<P.IA[P.row];++i)
            {
                j=P.JA[i];
                P.JA[i]=CoarseIndex[j];
            }
    }
    fasp_mem_free(CoarseIndex);
    
    /** Truncation of interpolation */
    REAL mMin, pMax;
    REAL mSum, pSum;
    REAL mTruncedSum, pTruncedSum;
    INT mTruncCount, pTruncCount;
    INT num_lost=0;
    
    Ptr->val=(REAL*)fasp_mem_calloc(P.IA[Ptr->row],sizeof(REAL));
#if CHMEM_MODE
    total_alloc_mem += (P.IA[Ptr->row])*sizeof(REAL);
#endif
    Ptr->JA=(INT*)fasp_mem_calloc(P.IA[Ptr->row],sizeof(INT));    
#if CHMEM_MODE
    total_alloc_mem += (P.IA[Ptr->row])*sizeof(INT);
#endif
    Ptr->IA=(INT*)fasp_mem_calloc(Ptr->row+1, sizeof(INT));
#if CHMEM_MODE
    total_alloc_mem += (Ptr->row+1)*sizeof(INT);
#endif
    
    INT index1=0, index2=0;
    for(i=0;i<P.row;++i)
        {
            mMin=0;
            pMax=0;
            mSum=0;
            pSum=0;
            mTruncedSum=0;
            pTruncedSum=0;
            mTruncCount=0;
            pTruncCount=0;
    
            Ptr->IA[i]-=num_lost;
    
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            mSum+=P.val[j];
                            if(P.val[j]<mMin)
                                {
                                    mMin=P.val[j];
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            pSum+=P.val[j];
                            if(P.val[j]>pMax)
                                {
                                    pMax=P.val[j];
                                }
                        }
                }
    
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            if(P.val[j]>mMin*epsilon_tr)
                                {
                                    mTruncCount++;
                                }
                            else
                                {
                                    num_lost--;
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            if(P.val[j]<pMax*epsilon_tr)
                                {
                                    pTruncCount++;
                                }
                            else
                                {
                                    num_lost--;
                                }
                        }
                }
    
            // step 2: Find the structure JA and fill the data A of Ptr
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            if(!(P.val[j]>mMin*epsilon_tr))
                                {
                                    Ptr->JA[index1]=P.JA[j];
                                    mTruncedSum+=P.val[j];
                                    index1++;
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            if(!(P.val[j]<pMax*epsilon_tr))
                                {
                                    Ptr->JA[index1]=P.JA[j];
                                    pTruncedSum+=P.val[j];
                                    index1++;
                                }
                        }
                }
    
            // step 3: Fill the data A of Ptr
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            if(!(P.val[j]>mMin*epsilon_tr))
                                {
                                    Ptr->val[index2]=P.val[j]/mTruncedSum*mSum;
                                    index2++;
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            if(!(P.val[j]<pMax*epsilon_tr))
                                {
                                    Ptr->val[index2]=P.val[j]/pTruncedSum*pSum;
                                    index2++;
                                }
                        }
                }
        }
    Ptr->IA[P.row]-=num_lost;
    Ptr->nnz=Ptr->IA[Ptr->row];
    
    Ptr->JA=(INT*)fasp_mem_realloc(Ptr->JA, Ptr->IA[Ptr->row]*sizeof(INT));    
    Ptr->val=(REAL*)fasp_mem_realloc(Ptr->val, Ptr->IA[Ptr->row]*sizeof(REAL));
    
    fasp_mem_free(P.IA);
    fasp_mem_free(P.JA);
    fasp_mem_free(P.val);

}

/**
 * \fn INT fasp_amg_interp1(dCSRmat *A, ivector *vertices, dCSRmat *P, AMG_param *param)
 * \brief Generate interpolation P 
 *
 * \param A          pointer to the stiffness matrix
 * \param vertices   pointer to the indicator of CF split node is on fine(current level) or coarse grid (fine: 0; coarse: 1)
 * \param P          pointer to the dCSRmat matrix of resulted interpolation
 * \param param      pointer to AMG parameters
 * \return            SUCCESS or error message
 *
 * \date 03/01/2011
 */
INT fasp_amg_interp1( dCSRmat *A, 
                      ivector *vertices, 
                      dCSRmat *P, 
                      AMG_param *param, 
					  iCSRmat *S,
                      INT *icor_ysk)
{
    INT status = SUCCESS;
    INT interp_type=param->interpolation_type;
	const INT coarsening_type = param->coarsening_type;
    
#if DEBUG_MODE
    printf("fasp_amg_interp ...... [Start]\n");
#endif
    
	// make sure standard interpolaiton is used for aggressive coarsening
	if (coarsening_type == 4) interp_type = INTERP_STD;

    /*-- Standard interpolation operator --*/
    switch (interp_type) {
	case INTERP_REG: // Direct interpolation
        interp_RS1(A, vertices, P, param, icor_ysk);
		break;
	case INTERP_ENG_MIN: // Energy min interpolation 
        interp_EM(A, vertices, P, param);
        break;
	case INTERP_STD: // standard interpolation
		interp_STD(A, vertices, P, S, param);
		break;
    default:
		fasp_chkerr(ERROR_AMG_INTERP_TYPE, "fasp_amg_interp");
		break;
    }
    
#if DEBUG_MODE
    printf("fasp_amg_interp ...... [Finish]\n");
#endif
    
    if (status<0) exit(status);
    else return status;
}

/**
 * \fn static void fasp_get_nbl_nbr_ysk(dCSRmat *A, INT *nbl_ptr, INT *nbr_ptr)
 * \brief Get the bandwidth of the matrix
 *
 * \param A         pointer to the stiffness matrix
 * \param nbl_ptr   the left bandwidth
 * \param nbr_ptr   the right bandwidth
 *
 * \date 03/01/2011
 */
static void fasp_get_nbl_nbr_ysk( dCSRmat *A, 
		                          INT *nbl_ptr, 
								  INT *nbr_ptr )
{
#if FASP_USE_OPENMP
    INT *IA = A->IA;
    INT *JA = A->JA;
    INT myid, mybegin, myend, nthreads;
    INT max_l, max_r;
    INT i, end_row_A, j;
	
    nthreads = FASP_GET_NUM_THREADS();

    INT *max_left_right = (INT *)fasp_mem_calloc(2*nthreads, sizeof(INT));

#pragma omp parallel for private(myid,mybegin,myend,max_l,max_r,i,end_row_A,j)
    for (myid = 0; myid < nthreads; myid ++)
        {
            FASP_GET_START_END(myid, nthreads, A->row, mybegin, myend);
            max_l = 0;
            max_r = 0;
            for (i = mybegin; i < myend; i ++) {
                end_row_A = IA[i+1];
                for (j = IA[i]; j < end_row_A; j ++) {
                    max_l = MAX(i-JA[j], max_l);
                    max_r = MAX(JA[j]-i, max_r);
                }
            }
            max_left_right[myid*2] = max_l;
            max_left_right[myid*2+1] = max_r;
        }
    max_l = max_left_right[0];
    max_r = max_left_right[1];
    for (i = 1; i < nthreads; i ++) {
        max_l = MAX(max_l, max_left_right[i*2]);
        max_r = MAX(max_r, max_left_right[i*2+1]);
    }
    fasp_mem_free(max_left_right);
    *nbl_ptr = max_l;
    *nbr_ptr = max_r;
#endif
}

/**
 * \fn static void fasp_mod_coarse_index( INT nrows, INT *CoarseIndex)
 * \brief Modify CoarseIndex
 *
 * \param nrows         length of CoarseIndex
 * \param CoarseIndex  pointer to index of coarse poINTs
 * \param  nthreads     number of threads
 * \param openmp_holds  threshold of parallelization
 *
 * \date 03/01/2011
 */
static void fasp_mod_coarse_index( INT nrows, 
		                           INT *CoarseIndex)
{
#if FASP_USE_OPENMP
    INT myid, mybegin, myend, i, nthreads;

    nthreads = FASP_GET_NUM_THREADS();

#pragma omp parallel for private(myid,mybegin,myend,i)
    for (myid = 0; myid < nthreads; myid ++)
        {
            FASP_GET_START_END(myid, nthreads, nrows, mybegin, myend);
            if (myid == 0)
                {
                    mybegin ++;
                }
            for (i = mybegin; i < myend; i ++)
                {
                    if (CoarseIndex[i] < CoarseIndex[i-1])
                        {
                            CoarseIndex[i] = CoarseIndex[i-1];
                        }
                }
        }
#endif
}

/**
 * \fn INT fasp_BinarySearch(INT *list, INT value, INT list_length)
 * \brief Binary Search
 *
 * \date 03/01/2011
 */
INT fasp_BinarySearch (INT *list, 
		               INT value, 
					   INT list_length)
{
    INT not_found = 1;
    INT low, high, m;
    
    low = 0;
    high = list_length - 1;
    while (not_found && low <= high)
        {
            m = (low + high) / 2;
            if (value < list[m])
                {
                    high = m - 1;
                }
            else if (value > list[m])
                {
                    low = m + 1;
                }
            else
                {
                    not_found = 0;
                    return m;
                }
        }
    
    return -1;
}

/**
 * \fn static void fasp_get_icor_ysk
 * \brief Get Icor_ysk
 *
 * \date 03/01/2011
 */
static void fasp_get_icor_ysk(INT nrows, 
		                      INT ncols, 
							  INT *CoarseIndex, 
							  INT nbl_ysk, 
							  INT nbr_ysk, 
							  INT *CF_marker, 
							  INT *icor_ysk)
{
#if FASP_USE_OPENMP
    INT myid, FiveMyid, mybegin, myend, min_A, max_A, i, first_f_node, min_P, max_P, myend_minus_one;
    INT lengthAA = 0, lengthPP = 0;
    INT nthreads; 
    
    nthreads = FASP_GET_NUM_THREADS();

#pragma omp parallel for private(myid,FiveMyid,mybegin,myend,min_A,max_A,i,first_f_node,min_P,max_P,myend_minus_one) reduction(+: lengthAA,lengthPP)
    for (myid = 0; myid < nthreads; myid ++)
        {
            FiveMyid = myid * 5;
            FASP_GET_START_END(myid, nthreads, ncols, mybegin, myend);
            icor_ysk[FiveMyid] = mybegin;
            if (mybegin == myend) {
                lengthAA = 0;
                lengthPP = 0;
                icor_ysk[FiveMyid+1] = 0;
                icor_ysk[FiveMyid+3] = 0;
            }
            else {
                first_f_node = fasp_BinarySearch(CoarseIndex, mybegin, nrows);
                for (i = first_f_node; i > -1; i --) {
                    if (CoarseIndex[i] != mybegin) {
                        break;
                    }
                }
                min_A = i + 1;
                min_A = MAX(0, min_A-2*nbl_ysk);
                myend_minus_one = myend - 1;
                first_f_node = fasp_BinarySearch(CoarseIndex, myend_minus_one, nrows);
                for (i = first_f_node; i > -1; i --) {
                    if (CoarseIndex[i] != myend_minus_one) {
                        max_A = i;
                        break;
                    }
                }
                max_A = MIN(nrows, max_A+2*nbr_ysk+1);
                lengthAA = max_A - min_A + 2;
                icor_ysk[FiveMyid+1] = lengthAA;
                icor_ysk[FiveMyid+2] = min_A;
                for (i = min_A; i >= 0; i --) {
                    if (CF_marker[i] == 0) {
                        first_f_node = i;
                        break;
                    }
                }
                if (i == -1) {
                    min_P = 0;
                }
                else {
                    first_f_node -= nbl_ysk;
                    if (first_f_node <= 0) {
                        min_P = 0;
                    }
                    else {
                        for (i = first_f_node; i >= 0; i --) {
                            if (CF_marker[i] == 1) {
                                min_P = CoarseIndex[i];
                                break;
                            }
                        }
                        if (i == -1) {
                            min_P = 0;
                        }
                    }
                }
                for (i = max_A-1; i < nrows; i ++) {
                    if (CF_marker[i] == 0) {
                        first_f_node = i;
                        break;
                    }
                }
                if (i == nrows) {
                    max_P = ncols;
                }
                else {
                    first_f_node += nbr_ysk;
                    if (first_f_node >= nrows) {
                        max_P = ncols;
                    }
                    else {
                        for (i = first_f_node; i < nrows; i ++) {
                            if (CF_marker[i] == 1) {
                                max_P = CoarseIndex[i] + 1;
                                break;
                            }
                        }
                        if (i == nrows) {
                            max_P = ncols;
                        }
                    }
                }
                lengthPP = max_P - min_P + 2;
                icor_ysk[FiveMyid+3] = lengthPP;
                icor_ysk[FiveMyid+4] = min_P;
            }
        }
    icor_ysk[5*nthreads] = lengthAA;
    icor_ysk[5*nthreads+1] = lengthPP;
#endif
}

/**
 * \fn static void interp_RS1(dCSRmat *A, ivector *vertices, dCSRmat *Ptr, AMG_param *param)
 * \brief Direct interpolation 
 *
 * \param A         pointer to the stiffness matrix
 * \param vertices  pointer to the indicator of CF split node is on fine(current level) or coarse grid (fine: 0; coarse: 1)
 * \param Ptr       pointer to the dCSRmat matrix of resulted interpolation
 * \param param     pointer to AMG parameters
 *
 * Refer to P479, U. Trottenberg, C. W. Oosterlee, and A. Sch¨uller. Multigrid. 
 *              Academic Press Inc., San Diego, CA, 2001. 
 *          With contributions by A. Brandt, P. Oswald and K. St¨uben.
 */

static void interp_RS1(dCSRmat *A, 
		               ivector *vertices, 
					   dCSRmat *Ptr, 
					   AMG_param *param, 
					   INT *icor_ysk)
{
    REAL epsilon_tr = param->truncation_threshold;
    REAL amN, amP, apN, apP;
    REAL alpha, beta, aii=0;
    INT *vec = vertices->val;
    INT countPplus, diagindex;
    
    unsigned INT i,j,k,l,index=0;
    INT begin_row, end_row;
    INT myid;
    INT mybegin;
    INT myend;
    INT stride_i;
    INT *indexs = NULL;
    INT shift;
    INT nbl_ysk, nbr_ysk;
   
	INT nthreads, use_openmp;

	if(!FASP_USE_OPENMP || A->row <= OPENMP_HOLDS){
		use_openmp = FALSE;
	}
	else{
		use_openmp = TRUE;
        nthreads = FASP_GET_NUM_THREADS();
	}
    
    /** Generate interpolation P */
    dCSRmat P=fasp_dcsr_create(Ptr->row,Ptr->col,Ptr->nnz);
    
    /** step 1: Find first the structure IA of P */
    fasp_iarray_cp(P.row+1, Ptr->IA, P.IA);
    
    /** step 2: Find the structure JA of P */
    fasp_iarray_cp(P.nnz, Ptr->JA, P.JA);
    
    /** step 3: Fill the data of P */
    if (use_openmp) {
#pragma omp parallel for private(myid,mybegin,myend,i,begin_row,end_row,diagindex,aii,amN,amP,apN,apP,countPplus,j,k,alpha,beta,l)
        for (myid = 0; myid < nthreads; myid++ )
            {
                FASP_GET_START_END(myid, nthreads, A->row, mybegin, myend);
                for (i=mybegin; i<myend; ++i)
                    {
                        begin_row=A->IA[i]; end_row=A->IA[i+1]-1;    
    
                        for(diagindex=begin_row;diagindex<=end_row;diagindex++) {
                            if (A->JA[diagindex]==i) {
                                aii=A->val[diagindex];
                                break;
                            }
                        }
    
                        if(vec[i]==0)  // if node i is on fine grid 
                            {
                                amN=0, amP=0, apN=0, apP=0,  countPplus=0;
    
                                for(j=begin_row;j<=end_row;++j)
                                    {
                                        if(j==diagindex) continue;
    
                                        for(k=Ptr->IA[i];k<Ptr->IA[i+1];++k) {
                                            if(Ptr->JA[k]==A->JA[j]) break;
                                        }
    
                                        if(A->val[j]>0) {
                                            apN+=A->val[j];
                                            if(k<Ptr->IA[i+1]) {
                                                apP+=A->val[j];
                                                countPplus++;
                                            }
                                        }
                                        else
                                            {
                                                amN+=A->val[j];
                                                if(k<Ptr->IA[i+1]) {
                                                    amP+=A->val[j];
                                                }
                                            }
                                    } // j
    
                                alpha=amN/amP;
                                if(countPplus>0) {
                                    beta=apN/apP;
                                }
                                else {
                                    beta=0;
                                    aii+=apN;
                                }
    
                                for(j=P.IA[i];j<P.IA[i+1];++j)
                                    {
                                        k=P.JA[j];
                                        for(l=A->IA[i];l<A->IA[i+1];l++)
                                            {
                                                if(A->JA[l]==k) break;
                                            }
    
                                        if(A->val[l]>0)
                                            {
                                                P.val[j]=-beta*A->val[l]/aii;
                                            }
                                        else
                                            {
                                                P.val[j]=-alpha*A->val[l]/aii;
                                            }
                                    }
                            }
                        else if(vec[i]==2)  // if node i is a special fine node
                            {
                            }
                        else // if node i is on coarse grid 
                            {
                                P.val[P.IA[i]]=1;
                            }
                    }
            }
    }
    else {
        for(i=0;i<A->row;++i)
            {
                begin_row=A->IA[i]; end_row=A->IA[i+1]-1;    
    
                for(diagindex=begin_row;diagindex<=end_row;diagindex++) {
                    if (A->JA[diagindex]==i) {
                        aii=A->val[diagindex];
                        break;
                    }
                }
    
                if(vec[i]==0)  // if node i is on fine grid 
                    {
                        amN=0, amP=0, apN=0, apP=0,  countPplus=0;
    
                        for(j=begin_row;j<=end_row;++j)
                            {
                                if(j==diagindex) continue;
    
                                for(k=Ptr->IA[i];k<Ptr->IA[i+1];++k) {
                                    if(Ptr->JA[k]==A->JA[j]) break;
                                }
    
                                if(A->val[j]>0) {
                                    apN+=A->val[j];
                                    if(k<Ptr->IA[i+1]) {
                                        apP+=A->val[j];
                                        countPplus++;
                                    }
                                }
                                else
                                    {
                                        amN+=A->val[j];
                                        if(k<Ptr->IA[i+1]) {
                                            amP+=A->val[j];
                                        }
                                    }
                            } // j
    
                        alpha=amN/amP;
                        if(countPplus>0) {
                            beta=apN/apP;
                        }
                        else {
                            beta=0;
                            aii+=apN;
                        }
    
                        for(j=P.IA[i];j<P.IA[i+1];++j)
                            {
                                k=P.JA[j];
                                for(l=A->IA[i];l<A->IA[i+1];l++)
                                    {
                                        if(A->JA[l]==k) break;
                                    }
    
                                if(A->val[l]>0)
                                    {
                                        P.val[j]=-beta*A->val[l]/aii;
                                    }
                                else
                                    {
                                        P.val[j]=-alpha*A->val[l]/aii;
                                    }
                            }
                    }
                else if(vec[i]==2)  // if node i is a special fine node
                    {
                    }
                else // if node i is on coarse grid 
                    {
                        P.val[P.IA[i]]=1;
                    }
            }
    }
    fasp_mem_free(Ptr->IA);
    fasp_mem_free(Ptr->JA);
    fasp_mem_free(Ptr->val);
    
    INT *CoarseIndex;

    CoarseIndex=(INT*)fasp_mem_calloc(A->row, sizeof(INT));

#if CHMEM_MODE
    total_alloc_mem += (A->row)*sizeof(INT);
#endif
    
    // The following is one of OPTIMAL parts ...0802...
    // Generate CoarseIndex in parallel
    if (use_openmp) {
#pragma omp master
        {    
            indexs = (INT *)fasp_mem_calloc(nthreads, sizeof(INT));
        }
#pragma omp parallel for private(myid, mybegin, myend, index, i)
        for (myid = 0; myid < nthreads; myid ++)
            {
                FASP_GET_START_END(myid, nthreads, A->row, mybegin, myend);
                index = 0;
                for (i=mybegin;i<myend;++i) {
                    if(vec[i]==1)
                        {
                            CoarseIndex[i]=index;
                            index++;
                        }
                }
                indexs[myid] = index;
            }
        for (i = 1; i < nthreads; i ++) {
            indexs[i] += indexs[i-1];
        }
#pragma omp parallel for private(myid, mybegin, myend, shift, i)
        for (myid = 0; myid < nthreads; myid ++)
            {
                FASP_GET_START_END(myid, nthreads, A->row, mybegin, myend);
                shift = 0;
                if (myid > 0) {
                    shift = indexs[myid-1];
                }
                for (i=mybegin;i<myend;++i) {
                    if(vec[i]==1)
                        {
                            CoarseIndex[i] += shift;
                        }
                }
            }
        fasp_mem_free(indexs);
    }
    else {
        index=0;
        for(i=0;i<A->row;++i) {
            if(vec[i]==1)
                {
                    CoarseIndex[i]=index;
                    index++;
                }
        }
    }
    
    if (P.IA[P.row] > OPENMP_HOLDS) {
#pragma omp parallel for private(myid, mybegin,myend,i,j) 
        for (myid = 0; myid < nthreads; myid++ )
            {
                FASP_GET_START_END(myid, nthreads,P.IA[P.row], mybegin, myend);
                for (i=mybegin; i<myend; ++i)
                    {
                        j=P.JA[i];
                        P.JA[i]=CoarseIndex[j];
                    }
            }
    }
    else {
        for(i=0;i<P.IA[P.row];++i)
            {
                j=P.JA[i];
                P.JA[i]=CoarseIndex[j];
            }
    }
//    if (Ptr->col > OPENMP_HOLDS) {
        // The following is another OPTIMAL part ...0802...
        ////////////////////////////////////////////////////////////////////////////////
        fasp_get_nbl_nbr_ysk(A, &nbl_ysk, &nbr_ysk);
        fasp_mod_coarse_index(A->row, CoarseIndex);
        fasp_get_icor_ysk(A->row, Ptr->col, CoarseIndex, nbl_ysk, nbr_ysk, vec, icor_ysk);
        ////////////////////////////////////////////////////////////////////////////////
//    }
    fasp_mem_free(CoarseIndex);
    
    /** Truncation of interpolation */
    REAL mMin, pMax;
    REAL mSum, pSum;
    REAL mTruncedSum, pTruncedSum;
    INT mTruncCount, pTruncCount;
    INT num_lost=0;
    
    
    Ptr->val=(REAL*)fasp_mem_calloc(P.IA[Ptr->row],sizeof(REAL));

#if CHMEM_MODE
    total_alloc_mem += (P.IA[Ptr->row])*sizeof(REAL);
#endif
    Ptr->JA=(INT*)fasp_mem_calloc(P.IA[Ptr->row],sizeof(INT));    
#if CHMEM_MODE
    total_alloc_mem += (P.IA[Ptr->row])*sizeof(INT);
#endif
    Ptr->IA=(INT*)fasp_mem_calloc(Ptr->row+1, sizeof(INT));
#if CHMEM_MODE
    total_alloc_mem += (Ptr->row+1)*sizeof(INT);
#endif
    
    INT index1=0, index2=0;
    for(i=0;i<P.row;++i)
        {
            mMin=0;
            pMax=0;
            mSum=0;
            pSum=0;
            mTruncedSum=0;
            pTruncedSum=0;
            mTruncCount=0;
            pTruncCount=0;
    
            Ptr->IA[i]-=num_lost;
    
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            mSum+=P.val[j];
                            if(P.val[j]<mMin)
                                {
                                    mMin=P.val[j];
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            pSum+=P.val[j];
                            if(P.val[j]>pMax)
                                {
                                    pMax=P.val[j];
                                }
                        }
                }
    
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            if(P.val[j]>mMin*epsilon_tr)
                                {
                                    mTruncCount++;
                                }
                            else
                                {
                                    num_lost--;
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            if(P.val[j]<pMax*epsilon_tr)
                                {
                                    pTruncCount++;
                                }
                            else
                                {
                                    num_lost--;
                                }
                        }
                }
    
            // step 2: Find the structure JA and fill the data A of Ptr
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            if(!(P.val[j]>mMin*epsilon_tr))
                                {
                                    Ptr->JA[index1]=P.JA[j];
                                    mTruncedSum+=P.val[j];
                                    index1++;
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            if(!(P.val[j]<pMax*epsilon_tr))
                                {
                                    Ptr->JA[index1]=P.JA[j];
                                    pTruncedSum+=P.val[j];
                                    index1++;
                                }
                        }
                }
    
            // step 3: Fill the data A of Ptr
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            if(!(P.val[j]>mMin*epsilon_tr))
                                {
                                    Ptr->val[index2]=P.val[j]/mTruncedSum*mSum;
                                    index2++;
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            if(!(P.val[j]<pMax*epsilon_tr))
                                {
                                    Ptr->val[index2]=P.val[j]/pTruncedSum*pSum;
                                    index2++;
                                }
                        }
                }
        }
    Ptr->IA[P.row]-=num_lost;
    Ptr->nnz=Ptr->IA[Ptr->row];
    
    Ptr->JA=(INT*)fasp_mem_realloc(Ptr->JA, Ptr->IA[Ptr->row]*sizeof(INT));    
    Ptr->val=(REAL*)fasp_mem_realloc(Ptr->val, Ptr->IA[Ptr->row]*sizeof(REAL));
    
    fasp_mem_free(P.IA);
    fasp_mem_free(P.JA);
    fasp_mem_free(P.val);


}


/**
 * \fn static void interp_RS2(dCSRmat *A, ivector *vertices, dCSRmat *Ptr, AMG_param *param)
 * \brief Direct interpolation 
 *
 * \param A         pointer to the stiffness matrix
 * \param vertices  pointer to the indicator of CF split node is on fine(current level) or coarse grid (fine: 0; coarse: 1)
 * \param Ptr       pointer to the dCSRmat matrix of resulted interpolation
 * \param param     pointer to AMG parameters
 *
 * Refer to P479, U. Trottenberg, C. W. Oosterlee, and A. Sch¨uller. Multigrid. 
 *              Academic Press Inc., San Diego, CA, 2001. 
 *          With contributions by A. Brandt, P. Oswald and K. St¨uben.
 */

static void interp_RS2(dCSRmat *A, 
		               ivector *vertices, 
					   dCSRmat *Ptr, 
					   AMG_param *param) 
{
    REAL epsilon_tr = param->truncation_threshold;
    REAL amN, amP, apN, apP;
    REAL alpha, beta, aii=0;
    INT *vec = vertices->val;
    INT countPplus, diagindex;
    
    unsigned INT i,j,k,l,index=0;
    INT begin_row, end_row;
    INT myid;
    INT mybegin;
    INT myend;
    INT stride_i;
    INT *indexs = NULL;
    INT shift;
   
	INT nthreads, use_openmp;
	if(!FASP_USE_OPENMP || A->row <= OPENMP_HOLDS){
		use_openmp = FALSE;
	}
	else{
		use_openmp = TRUE;
        nthreads = FASP_GET_NUM_THREADS();
	}
    
    /** Generate interpolation P */
    dCSRmat P=fasp_dcsr_create(Ptr->row,Ptr->col,Ptr->nnz);
    
    /** step 1: Find first the structure IA of P */
    fasp_iarray_cp(P.row+1, Ptr->IA, P.IA);
    
    /** step 2: Find the structure JA of P */
    fasp_iarray_cp(P.nnz, Ptr->JA, P.JA);
    
    /** step 3: Fill the data of P */
    if (use_openmp) {
        stride_i = A->row/nthreads;
#pragma omp parallel private(myid,mybegin,myend,i,begin_row,end_row,diagindex,aii,amN,amP,apN,apP,countPplus,j,k,alpha,beta,l) ////num_threads(nthreads)
        {
            myid = omp_get_thread_num();
            mybegin = myid*stride_i;
            if(myid < nthreads-1) myend = mybegin+stride_i;
            else myend = A->row;
            for (i=mybegin; i<myend; ++i)
                {
                    begin_row=A->IA[i]; end_row=A->IA[i+1]-1;    
    
                    for(diagindex=begin_row;diagindex<=end_row;diagindex++) {
                        if (A->JA[diagindex]==i) {
                            aii=A->val[diagindex];
                            break;
                        }
                    }
    
                    if(vec[i]==0)  // if node i is on fine grid 
                        {
                            amN=0, amP=0, apN=0, apP=0,  countPplus=0;
    
                            for(j=begin_row;j<=end_row;++j)
                                {
                                    if(j==diagindex) continue;
    
                                    for(k=Ptr->IA[i];k<Ptr->IA[i+1];++k) {
                                        if(Ptr->JA[k]==A->JA[j]) break;
                                    }
    
                                    if(A->val[j]>0) {
                                        apN+=A->val[j];
                                        if(k<Ptr->IA[i+1]) {
                                            apP+=A->val[j];
                                            countPplus++;
                                        }
                                    }
                                    else
                                        {
                                            amN+=A->val[j];
                                            if(k<Ptr->IA[i+1]) {
                                                amP+=A->val[j];
                                            }
                                        }
                                } // j
    
                            alpha=amN/amP;
                            if(countPplus>0) {
                                beta=apN/apP;
                            }
                            else {
                                beta=0;
                                aii+=apN;
                            }
    
                            for(j=P.IA[i];j<P.IA[i+1];++j)
                                {
                                    k=P.JA[j];
                                    for(l=A->IA[i];l<A->IA[i+1];l++)
                                        {
                                            if(A->JA[l]==k) break;
                                        }
    
                                    if(A->val[l]>0)
                                        {
                                            P.val[j]=-beta*A->val[l]/aii;
                                        }
                                    else
                                        {
                                            P.val[j]=-alpha*A->val[l]/aii;
                                        }
                                }
                        }
                    else if(vec[i]==2)  // if node i is a special fine node
                        {
                        }
                    else // if node i is on coarse grid 
                        {
                            P.val[P.IA[i]]=1;
                        }
                }
        }
    }
    else {
        for(i=0;i<A->row;++i)
            {
                begin_row=A->IA[i]; end_row=A->IA[i+1]-1;    
    
                for(diagindex=begin_row;diagindex<=end_row;diagindex++) {
                    if (A->JA[diagindex]==i) {
                        aii=A->val[diagindex];
                        break;
                    }
                }
    
                if(vec[i]==0)  // if node i is on fine grid 
                    {
                        amN=0, amP=0, apN=0, apP=0,  countPplus=0;
    
                        for(j=begin_row;j<=end_row;++j)
                            {
                                if(j==diagindex) continue;
    
                                for(k=Ptr->IA[i];k<Ptr->IA[i+1];++k) {
                                    if(Ptr->JA[k]==A->JA[j]) break;
                                }
    
                                if(A->val[j]>0) {
                                    apN+=A->val[j];
                                    if(k<Ptr->IA[i+1]) {
                                        apP+=A->val[j];
                                        countPplus++;
                                    }
                                }
                                else
                                    {
                                        amN+=A->val[j];
                                        if(k<Ptr->IA[i+1]) {
                                            amP+=A->val[j];
                                        }
                                    }
                            } // j
    
                        alpha=amN/amP;
                        if(countPplus>0) {
                            beta=apN/apP;
                        }
                        else {
                            beta=0;
                            aii+=apN;
                        }
    
                        for(j=P.IA[i];j<P.IA[i+1];++j)
                            {
                                k=P.JA[j];
                                for(l=A->IA[i];l<A->IA[i+1];l++)
                                    {
                                        if(A->JA[l]==k) break;
                                    }
    
                                if(A->val[l]>0)
                                    {
                                        P.val[j]=-beta*A->val[l]/aii;
                                    }
                                else
                                    {
                                        P.val[j]=-alpha*A->val[l]/aii;
                                    }
                            }
                    }
                else if(vec[i]==2)  // if node i is a special fine node
                    {
                    }
                else // if node i is on coarse grid 
                    {
                        P.val[P.IA[i]]=1;
                    }
            }
    }
    
    fasp_mem_free(Ptr->IA);
    fasp_mem_free(Ptr->JA);
    fasp_mem_free(Ptr->val);    
#if 1
    INT *CoarseIndex=(INT*)fasp_mem_calloc(A->row, sizeof(INT));
#else
    INT *CoarseIndex=(INT*)fasp_mem_calloc(vertices->row, sizeof(INT));
#endif
    
#if CHMEM_MODE
#if 1
    total_alloc_mem += (A->row)*sizeof(INT);
#else
    total_alloc_mem += (vertices->row)*sizeof(INT);
#endif
#endif
    
    index=0;
#if 0
#if 1
    for(i=0;i<A->row;++i)
#else
        for(i=0;i<vertices->row;++i)
#endif
            {
                if(vec[i]==1)
                    {
                        CoarseIndex[i]=index;
                        index++;
                    }
            }
#else
    // The following is one of OPTIMAL parts ...0802...
    // Generate CoarseIndex in parallel
    if (use_openmp) {
        indexs = (INT *)fasp_mem_calloc(nthreads, sizeof(INT));
#pragma omp parallel for private(myid, mybegin, myend, index, i)
        for (myid = 0; myid < nthreads; myid ++)
            {
                FASP_GET_START_END(myid, nthreads, A->row, mybegin, myend);
                index = 0;
                for (i=mybegin;i<myend;++i) {
                    if(vec[i]==1)
                        {
                            CoarseIndex[i]=index;
                            index++;
                        }
                }
                indexs[myid] = index;
            }
        for (i = 1; i < nthreads; i ++) {
            indexs[i] += indexs[i-1];
        }
#pragma omp parallel for private(myid, mybegin, myend, shift, i)
        for (myid = 0; myid < nthreads; myid ++)
            {
                FASP_GET_START_END(myid, nthreads, A->row, mybegin, myend);
                shift = 0;
                if (myid > 0) {
                    shift = indexs[myid-1];
                }
                for (i=mybegin;i<myend;++i) {
                    if(vec[i]==1)
                        {
                            CoarseIndex[i] += shift;
                        }
                }
            }
        fasp_mem_free(indexs);
    }
    else {
        index=0;
        for(i=0;i<A->row;++i) {
            if(vec[i]==1)
                {
                    CoarseIndex[i]=index;
                    index++;
                }
        }
    }
#endif
    if (P.IA[P.row] > OPENMP_HOLDS) {
        stride_i = P.IA[P.row]/nthreads;
#pragma omp parallel private(myid,mybegin,myend,i,j) ////num_threads(nthreads)
        {
            myid = omp_get_thread_num();
            mybegin = myid*stride_i;
            if(myid < nthreads-1) myend = mybegin+stride_i;
            else myend = P.IA[P.row];
            for (i=mybegin; i<myend; ++i)
                {
                    j=P.JA[i];
                    P.JA[i]=CoarseIndex[j];
                }
        }
    }
    else {
        for(i=0;i<P.IA[P.row];++i)
            {
                j=P.JA[i];
                P.JA[i]=CoarseIndex[j];
            }
    }
    fasp_mem_free(CoarseIndex);
    
    /** Truncation of interpolation */
    REAL mMin, pMax;
    REAL mSum, pSum;
    REAL mTruncedSum, pTruncedSum;
    INT mTruncCount, pTruncCount;
    INT num_lost=0;
    
    Ptr->val=(REAL*)fasp_mem_calloc(P.IA[Ptr->row],sizeof(REAL));
#if CHMEM_MODE
    total_alloc_mem += (P.IA[Ptr->row])*sizeof(REAL);
#endif
    Ptr->JA=(INT*)fasp_mem_calloc(P.IA[Ptr->row],sizeof(INT));    
#if CHMEM_MODE
    total_alloc_mem += (P.IA[Ptr->row])*sizeof(INT);
#endif
    Ptr->IA=(INT*)fasp_mem_calloc(Ptr->row+1, sizeof(INT));
#if CHMEM_MODE
    total_alloc_mem += (Ptr->row+1)*sizeof(INT);
#endif
    
    INT index1=0, index2=0;
    for(i=0;i<P.row;++i)
        {
            mMin=0;
            pMax=0;
            mSum=0;
            pSum=0;
            mTruncedSum=0;
            pTruncedSum=0;
            mTruncCount=0;
            pTruncCount=0;
    
            Ptr->IA[i]-=num_lost;
    
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            mSum+=P.val[j];
                            if(P.val[j]<mMin)
                                {
                                    mMin=P.val[j];
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            pSum+=P.val[j];
                            if(P.val[j]>pMax)
                                {
                                    pMax=P.val[j];
                                }
                        }
                }
    
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            if(P.val[j]>mMin*epsilon_tr)
                                {
                                    mTruncCount++;
                                }
                            else
                                {
                                    num_lost--;
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            if(P.val[j]<pMax*epsilon_tr)
                                {
                                    pTruncCount++;
                                }
                            else
                                {
                                    num_lost--;
                                }
                        }
                }
    
            // step 2: Find the structure JA and fill the data A of Ptr
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            if(!(P.val[j]>mMin*epsilon_tr))
                                {
                                    Ptr->JA[index1]=P.JA[j];
                                    mTruncedSum+=P.val[j];
                                    index1++;
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            if(!(P.val[j]<pMax*epsilon_tr))
                                {
                                    Ptr->JA[index1]=P.JA[j];
                                    pTruncedSum+=P.val[j];
                                    index1++;
                                }
                        }
                }
    
            // step 3: Fill the data A of Ptr
            for(j=P.IA[i];j<P.IA[i+1];++j)
                {
                    if(P.val[j]<0)
                        {
                            if(!(P.val[j]>mMin*epsilon_tr))
                                {
                                    Ptr->val[index2]=P.val[j]/mTruncedSum*mSum;
                                    index2++;
                                }
                        }
    
                    if(P.val[j]>0)
                        {
                            if(!(P.val[j]<pMax*epsilon_tr))
                                {
                                    Ptr->val[index2]=P.val[j]/pTruncedSum*pSum;
                                    index2++;
                                }
                        }
                }
        }
    Ptr->IA[P.row]-=num_lost;
    Ptr->nnz=Ptr->IA[Ptr->row];
    
    Ptr->JA=(INT*)fasp_mem_realloc(Ptr->JA, Ptr->IA[Ptr->row]*sizeof(INT));    
    Ptr->val=(REAL*)fasp_mem_realloc(Ptr->val, Ptr->IA[Ptr->row]*sizeof(REAL));
    
    fasp_mem_free(P.IA);
    fasp_mem_free(P.JA);
    fasp_mem_free(P.val);

}


/**
 * \fn static void interp_STD (dCSRmat *A, ivector *vertices, dCSRmat *P, iCSRmat *S, AMG_param *param)
 *
 * \brief Standard interpolation 
 *
 * \param A         Pointer to the stiffness matrix
 * \param S         Pointer to the strongly connection matrix
 * \param vertices  Pointer to the indicator of CF split node is on fine or coarse grid
 * \param P         Pointer to the dCSRmat matrix of resulted interpolation
 * \param param     Pointer to AMG parameters
 *
 * \author Kai Yang, Xiaozhe Hu
 * \date   05/21/2012  
 *
 * \note Ref P479, U. Trottenberg, C. W. Oosterlee, and A. Sch¨uller. Multigrid. 
 *             Academic Press Inc., San Diego, CA, 2001. 
 *           With contributions by A. Brandt, P. Oswald and K. St¨uben.
 */
static void interp_STD (dCSRmat *A, 
                        ivector *vertices, 
                        dCSRmat *P, 
                        iCSRmat *S,
                        AMG_param *param)
{
    const REAL epsilon_tr = param->truncation_threshold;
    
    REAL alpha;
    REAL *cs,*n,*diag,*hatA;
    REAL alN,alP;
    INT *vec = vertices->val,*flag,*Arind1,*Arind2;
    INT row=P->row;
    REAL akk,akh,aik,aki,rowsum;
    INT h,i,j,k,l,m,p,q,index=0;    
    
    cs=(REAL*)fasp_mem_calloc(row,sizeof(REAL));//for sums of strongly connected coarse neighbors
    
    n=(REAL*)fasp_mem_calloc(row,sizeof(REAL));//for sums of all neighbors.
    
    diag=(REAL*)fasp_mem_calloc(row,sizeof(REAL));//for diagonal numbers
    
    flag=(INT*)fasp_mem_calloc(row,sizeof(INT));// index for coarse neighbor point for every node
    
    // for some row of A, the index from column number to the the number in A.val
    Arind1=(INT*)fasp_mem_calloc(2*row,sizeof(INT));
    
    // for some row of A, the index from column number to the the number in A.val
    Arind2=(INT*)fasp_mem_calloc(2*row,sizeof(INT));    
    
    for(i=0;i<row;i++)
        flag[i]=-1;
    
    for (i=0; i<row; i++) {
        for (j=S->IA[i]; j<S->IA[i+1]; j++) {
            k=S->JA[j];
            if (vec[k]==CGPT) {
                flag[k]=i;
            }
        }
        
        for (j=A->IA[i]; j<A->IA[i+1]; j++) {
            k=A->JA[j];
            if (flag[k]==i) {
                
                cs[i]+=A->val[j];
                if(A->val[j]>0)
                    printf("### WARNING: Positive off diagonal value! (i,k)=(%d,%d),j:%d,val:%f!\n",
                           i,k,j,A->val[j]);
            }
            if(k==i)
            {
                diag[i]=A->val[j];
            }
            else{
                n[i]+=A->val[j];
            }
        }
    }
    
    
    hatA=(REAL*)fasp_mem_calloc(row,sizeof(REAL));//to record coefficents hat a_ij for relevant CGPT of the i-th node
    
    for (i=0; i<row; i++) {
        if (vec[i]==FGPT) {
            
            //make the reverse index Arind1
            
            for (j=A->IA[i]; j<A->IA[i+1]; j++) {
                k=A->JA[j];
                Arind1[k]=j;
            }
            
            alN=0;
            alP=0;
            
            alN=n[i];
            alP=cs[i];
            
            //clean up hatA for relevent nodes
            for (j=P->IA[i]; j<P->IA[i+1]; j++) {
                k=P->JA[j];
                hatA[k]=0;
            }
            hatA[i]=diag[i];
            
            for (j=S->IA[i]; j<S->IA[i+1]; j++) {
                
                k=S->JA[j];
                l=Arind1[k];
                
                if (vec[k]==CGPT) {
                    hatA[k]+=A->val[l];
                    
                }
                else if(vec[k]==FGPT)
                {   
                    aik=A->val[l];                    
                    akk=diag[k];
                    
                    //find aki
                    aki=0;
                    for (p=A->IA[k]; p<A->IA[k+1]; p++) {
                        q=A->JA[p];
                        if(q==i)
                        {
                            aki=A->val[p];
                        }
                    }
                    alN-=(n[k]-aki+akk)*aik/akk;
                    alP-=cs[k]*aik/akk;
                    
                    //visit all the strongly connected coarse neighbors of k
                    // make Arind2 for k
                    
                    for (m=A->IA[k]; m<A->IA[k+1]; m++) {
                        h=A->JA[m];
                        Arind2[h]=m;
                    }
                    
                    for (m=S->IA[k]; m<S->IA[k+1]; m++) {
                        h=S->JA[m];
                        akh=A->val[Arind2[h]];
                        
                        if (vec[h]==CGPT) {
                            
                            
                            hatA[h]-=aik*akh/akk;
                            if(aik*akh/akk<0)
                            {
                                printf("### WARNING: Problem with sign of product,aik=%f, akh=%f, i:%d,k:%d,h:%d\n",
                                       aik,akh,i,k,h);
                            }
                        }
                        else if(h==i)
                        {
                            hatA[h]-=aik*akh/akk;
                        }
                    }
                    
                }
            }
            
            alpha=alN/alP;
            
            
            rowsum=0;
            for (j=P->IA[i]; j<P->IA[i+1];j++ ) {
                k=P->JA[j];
                P->val[j]=-alpha*hatA[k]/hatA[i];
                rowsum+=P->val[j];
            }
        }
        else if(vec[i]==CGPT)
        {
            j=P->IA[i];
            
            P->val[j]=1;
            
            
        }
    }
    
    //modify the column number to make it coarse point indexing
    
    index=0;
	INT *CoarseIndex=(INT*)fasp_mem_calloc(A->row, sizeof(INT));
    
    for(i=0;i< A->row;++i){
		if(vec[i]==1){
			CoarseIndex[i]=index;
			index++;
            if (index>P->col) {
                printf("### WARNING: index=%d>p->col=%d\n",index,P->col);
            }
		}
	}
    P->col=index;
	for(i=0;i<P->IA[P->row];++i){
		j=P->JA[i];
		P->JA[i]=CoarseIndex[j];
	}
    
    /** Truncation of interpolation */
    REAL mMin, pMax;
    REAL mSum, pSum;
    REAL mTruncedSum, pTruncedSum;
    INT mTruncCount, pTruncCount;
    INT num_lost=0;
    REAL*Pval;
    INT *PIA,*PJA;
    
    Pval=(REAL*)fasp_mem_calloc(P->IA[row],sizeof(REAL));
    PJA=(INT*)fasp_mem_calloc(P->IA[row],sizeof(INT));    
    PIA=(INT*)fasp_mem_calloc(row+1, sizeof(INT));
    
    INT index1=0, index2=0;
    for (i=0;i<P->row;++i) {
        mMin=0;
        pMax=0;
        mSum=0;
        pSum=0;
        mTruncedSum=0;
        pTruncedSum=0;
        mTruncCount=0;
        pTruncCount=0;
        
        PIA[i]-=num_lost;
        
        for (j=P->IA[i];j<P->IA[i+1];++j) {
            if (P->val[j]<0) {
                mSum+=P->val[j];
                if (P->val[j]<mMin) {
                    mMin=P->val[j];
                }
            }
            
            if (P->val[j]>0) {
                pSum+=P->val[j];
                if (P->val[j]>pMax) {
                    pMax=P->val[j];
                }
            }
        }
        
        for (j=P->IA[i];j<P->IA[i+1];++j) {
            if (P->val[j]<0) {
                if (P->val[j]>mMin*epsilon_tr) {
                    mTruncCount++;
                }
                else {
                    num_lost--;
                }
            }
            
            if (P->val[j]>0) {
                if (P->val[j]<pMax*epsilon_tr) {
                    pTruncCount++;
                }
                else {
                    num_lost--;
                }
            }
        }
        
        // step 2: Find the structure JA and fill the data A of Ptr
        for (j=P->IA[i];j<P->IA[i+1];++j) {
            if (P->val[j]<0) {
                if (!(P->val[j]>mMin*epsilon_tr)) {
                    PJA[index1]=P->JA[j];
                    mTruncedSum+=P->val[j];
                    index1++;
                }
            }
            
            if (P->val[j]>0) {
                if (!(P->val[j]<pMax*epsilon_tr)) {
                    PJA[index1]=P->JA[j];
                    pTruncedSum+=P->val[j];
                    index1++;
                }
            }
        }
        
        // step 3: Fill the data A of Ptr
        for (j=P->IA[i];j<P->IA[i+1];++j) {
            if (P->val[j]<0) {
                if (!(P->val[j]>mMin*epsilon_tr)) {
                    Pval[index2]=P->val[j]/mTruncedSum*mSum;
                    index2++;
                }
            }
            
            if (P->val[j]>0) {
                if (!(P->val[j]<pMax*epsilon_tr)) {
                    Pval[index2]=P->val[j]/pTruncedSum*pSum;
                    index2++;
                }
            }
        }
    }
    PIA[P->row]-=num_lost;
    P->nnz=PIA[P->row];
    
    PJA=(INT*)fasp_mem_realloc(PJA, PIA[P->row]*sizeof(INT));    
    Pval=(REAL*)fasp_mem_realloc(Pval, PIA[P->row]*sizeof(REAL));
    
    fasp_mem_free(P->IA);
    fasp_mem_free(P->JA);
    fasp_mem_free(P->val);
    
    P->IA=PIA;
    P->JA=PJA;
    P->val=Pval;
    
    
    fasp_mem_free(CoarseIndex);
    fasp_mem_free(cs);
    fasp_mem_free(n);
    fasp_mem_free(diag);
    fasp_mem_free(Arind1);
    fasp_mem_free(Arind2);
    fasp_mem_free(hatA);
    
}

/*---------------------------------*/
/*--        End of File          --*/
/*---------------------------------*/