/*! \file coarsening_rs.c
 *  \brief Coarsening with a modified Ruge-Stuben strategy.
 *
 *  \note Ref Multigrid by U. Trottenberg, C. W. Oosterlee and A. Schuller
 *            Appendix P475 A.7 (by A. Brandt, P. Oswald and K. Stuben)
 *            Academic Press Inc., San Diego, CA, 2001.
 *
 */

#include <omp.h>

#include "fasp.h"
#include "fasp_functs.h"

#include "linklist.inl"

// Private routines for RS coarsening
static INT  form_coarse_level    (dCSRmat *, iCSRmat *, ivector *, INT, INT);
static INT  form_coarse_level_ag (dCSRmat *, iCSRmat *, ivector *, INT, INT, INT);
static void generate_S    (dCSRmat *, iCSRmat *, AMG_param *);
static void generate_sparsity_P     (dCSRmat *, iCSRmat *, ivector *, INT, INT);
static void generate_sparsity_P_std (dCSRmat *, iCSRmat *, ivector *, INT, INT);
static void generate_S_rs_ag_1 (dCSRmat *, iCSRmat *, iCSRmat *, ivector *, ivector *, ivector *);
static void generate_S_rs_ag_2 (dCSRmat *, iCSRmat *, iCSRmat *, ivector *, ivector *, ivector *);

/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/**
 * \fn INT fasp_amg_coarsening_rs (dCSRmat *A, ivector *vertices, dCSRmat *P, iCSRmat *S,
 *                                 AMG_param *param)
 *
 * \brief RS coarsening
 *
 * \param A          Pointer to the coefficient matrix, the index starts from zero
 * \param vertices   Pointer to the indicator ivector of the C/F splitting of the vertices
 * \param P          Pointer to the interpolation matrix (nonzero pattern only)
 * \param S          Pointer to strength matrix
 * \param param      Pointer to AMG parameters
 *
 * \return           SUCCESS or Error message
 *
 * \author Xuehai Huang, Chensong Zhang, Xiaozhe Hu, Ludmil Zikatanov
 * \date   09/06/2010
 *
 * \note vertices = 0: fine gird, 1: coarse grid, 2: isolated points
 *
 * Modified by Xiaozhe Hu 05/23/2011: add strength matrix as an input/output
 * Modified by Chensong Zhang 04/21/2013
 * Modified by Xiaozhe Hu 04/24/2013: modfiy aggressive coarsening
 * Mofified by Chensong Zhang 04/28/2013: remove linked list
 *
 */
INT fasp_amg_coarsening_rs (dCSRmat *A,
                            ivector *vertices,
                            dCSRmat *P,
                            iCSRmat *S,
                            AMG_param *param)
{
    const SHORT coarsening_type = param->coarsening_type;
    const INT   row = A->row;
    const REAL  epsilon_str = param->strong_threshold;
    SHORT       interp_type = param->interpolation_type;
    SHORT       status = SUCCESS;
    INT         col = 0;
    INT         aggressive_path = param->aggressive_path;
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_amg_coarsening_rs ...... [Start]\n");
#endif
    
#if DEBUG_MODE
    printf("### DEBUG: Step 1. Form dependent set S ......\n");
#endif
    
    // Step 1: generate S
    switch ( coarsening_type ) {
            
        case COARSE_RS: // modified Ruge-Stuben
            generate_S(A, S, param); break;
            
        case COARSE_AC: // aggressive coarsening: use S from modified Ruge-Stuben
            interp_type = INTERP_STD; // make sure standard interp is used
            generate_S(A, S, param); break;
            
        case COARSE_CR: // compatible relaxation: use S from modified Ruge-Stuben
            generate_S(A, S, param); break;
            
        default:
            printf("### ERROR: Coarsening type %d is not recognized!\n", coarsening_type);
            return ERROR_AMG_COARSE_TYPE;
            
    }
    
    if ( S->nnz == 0 ) return RUN_FAIL;
    
#if DEBUG_MODE
    printf("### DEBUG: Step 2. Choose C points ......\n");
#endif
    
    // Step 2: standard coarsening algorithm
    switch ( coarsening_type ) {
            
        case COARSE_RS: // modified Ruge-Stuben
            col = form_coarse_level(A, S, vertices, row, interp_type);
            break;
            
        case COARSE_AC: // aggressive coarsening
            col = form_coarse_level_ag(A, S, vertices, row, interp_type, aggressive_path);
            break;
            
        case COARSE_CR: // compatible relaxation (Need to be modified --Chensong)
            col = fasp_amg_coarsening_cr(0, A->row-1, A, vertices, param);
            break;
            
    }
    
#if DEBUG_MODE
    printf("### DEBUG: Step 3. Find support of C points ......\n");
#endif
    
    // Step 3: generate sparsity pattern of P
    switch ( interp_type ) {
            
        case INTERP_STD: // standard interpolaiton
            generate_sparsity_P_std (P, S, vertices, row, col);
            break;
            
        default: // direct or energy minimization interpolaiton
            generate_sparsity_P (P, S, vertices, row, col);
            break;
            
    }
    
#if DEBUG_MODE
    printf("### DEBUG: fasp_amg_coarsening_rs ...... [Finish]\n");
#endif
    
#if CHMEM_MODE
    fasp_mem_usage();
#endif
    
    return status;
}

/*---------------------------------*/
/*--      Private Functions      --*/
/*---------------------------------*/

/**
 * \fn static void generate_S (dCSRmat *A, iCSRmat *S, AMG_param *param)
 *
 * \brief Generate the set of all strong couplings S
 *
 * \param A      Pointer to the coefficient matrix
 * \param S      Pointer to the set of all strong couplings matrix
 * \param param  Pointer to AMG parameters
 *
 * \author Xuehai Huang, Chensong Zhang
 * \date   09/06/2010
 *
 * Modified by Chunsheng Feng, Xiaoqiang Yue on 05/25/2012
 */

static void generate_S (dCSRmat *A,
                        iCSRmat *S,
                        AMG_param *param )
{
    REAL max_row_sum=param->max_row_sum;
    REAL epsilon_str=param->strong_threshold;
    const INT row=A->row, col=A->col;
    const INT row_plus_one = row+1;
    const INT nnz=A->IA[row]-A->IA[0];
    
    INT index, i, j, begin_row, end_row;
    INT *ia=A->IA, *ja=A->JA;
    REAL *aj=A->val;
    
    INT nthreads = 1, use_openmp = FALSE;
    
#ifdef _OPENMP
    if ( row > OPENMP_HOLDS ) {
        use_openmp = TRUE;
        nthreads = FASP_GET_NUM_THREADS();
    }
#endif
    
    // get the diagnal entry of A
    dvector diag; fasp_dcsr_getdiag(0, A, &diag);
    
    /* Step 1: generate S */
    REAL row_scale, row_sum;
    
    // copy the structure of A to S
    S->row=row; S->col=col; S->nnz=nnz; S->val=NULL;
    
    S->IA=(INT*)fasp_mem_calloc(row_plus_one, sizeof(INT));
    
#if CHMEM_MODE
    total_alloc_mem += (row+1)*sizeof(REAL);
#endif
    
    S->JA=(INT*)fasp_mem_calloc(nnz, sizeof(INT));
    
#if CHMEM_MODE
    total_alloc_mem += (nnz)*sizeof(INT);
#endif
    
    fasp_iarray_cp(row_plus_one, ia, S->IA);
    fasp_iarray_cp(nnz, ja, S->JA);
    
    if (use_openmp) {
        INT mybegin,myend,myid;
#ifdef _OPENMP
#pragma omp parallel for private(myid, mybegin,myend,i,row_scale,row_sum,begin_row,end_row,j)
#endif
        for (myid = 0; myid < nthreads; myid++ )
        {
            FASP_GET_START_END(myid, nthreads, row, &mybegin, &myend);
            for (i=mybegin; i<myend; i++)
            {
                /* compute scaling factor and row sum */
                row_scale=0; row_sum=0;
                
                begin_row=ia[i]; end_row=ia[i+1];
                
                for (j=begin_row;j<end_row;j++) {
                    row_scale=MIN(row_scale, aj[j]);
                    row_sum+=aj[j];
                }
                row_sum=ABS(row_sum)/MAX(SMALLREAL,ABS(diag.val[i]));
                
                /* compute row entries of S */
                for (j=begin_row;j<end_row;j++) {
                    if (ja[j]==i) {S->JA[j]=-1; break;}
                }
                
                /* if $|\sum_{j=1}^n a_{ij}|> \theta_2 |a_{ii}|$ */
                if ((row_sum>max_row_sum)&&(max_row_sum<1)) {
                    /* make all dependencies weak */
                    for (j=begin_row;j<end_row;j++) S->JA[j]=-1;
                }
                else {
                    for (j=begin_row;j<end_row;j++) {
                        /* if $a_{ij}>=\epsilon_{str}*\min a_{ij}$, the connection $a_{ij}$
                         is set to be weak connection */
                        if (A->val[j]>=epsilon_str*row_scale) S->JA[j]=-1;
                    }
                }
            } // end for i
        }
    }
    else {
        for (i=0;i<row;++i) {
            /* compute scaling factor and row sum */
            row_scale=0; row_sum=0;
            
            begin_row=ia[i]; end_row=ia[i+1];
            
            for (j=begin_row;j<end_row;j++) {
                row_scale=MIN(row_scale, aj[j]);
                row_sum+=aj[j];
            }
            row_sum=ABS(row_sum)/MAX(SMALLREAL,ABS(diag.val[i]));
            
            /* compute row entries of S */
            for (j=begin_row;j<end_row;j++) {
                if (ja[j]==i) {S->JA[j]=-1; break;}
            }
            
            /* if $|\sum_{j=1}^n a_{ij}|> \theta_2 |a_{ii}|$ */
            if ((row_sum>max_row_sum)&&(max_row_sum<1)) {
                /* make all dependencies weak */
                for (j=begin_row;j<end_row;j++) S->JA[j]=-1;
            }
            else {
                for (j=begin_row;j<end_row;j++) {
                    /* if $a_{ij}>=\epsilon_{str}*\min a_{ij}$, the connection $a_{ij}$ is set to be weak connection */
                    if (A->val[j]>=epsilon_str*row_scale) S->JA[j]=-1;
                }
            }
        } // end for i
    }
    
    /* Compress the strength matrix */
    index=0;
    for (i=0;i<row;++i) {
        S->IA[i]=index;
        begin_row=ia[i]; end_row=ia[i+1]-1;
        for (j=begin_row;j<=end_row;j++) {
            if (S->JA[j]>-1) {
                S->JA[index]=S->JA[j];
                index++;
            }
        }
    }
    
    if (index > 0) {
        S->IA[row]=index;
        S->nnz=index;
        S->JA=(INT*)fasp_mem_realloc(S->JA,index*sizeof(INT));
    }
    else {
        S->nnz = 0;
        S->JA = NULL;
    }
    
    fasp_dvec_free(&diag);
}

#if 0 // Will be removed later --Chensong
/**
 * \fn static void generate_S_rs (dCSRmat *A, iCSRmat *S, REAL epsilon_str, INT coarsening_type)
 *
 * \brief Generate the set of all strong negative or strong absolute couplings
 *
 * \param A                Pointer to the coefficient matrix
 * \param S                Pointer to the set of all strong couplings matrix
 * \param epsilon_str      Strong coupled ratio
 * \param coarsening_type  Coarsening type (2: strong negative couplings, 3: strong absolute couplings)
 *
 * \author Xuehai Huang, Chensong Zhang
 * \date   09/06/2010
 *
 * Modified by Chunsheng Feng, Zheng Li on 10/12/2012
 */
static void generate_S_rs (dCSRmat *A,
                           iCSRmat *S,
                           REAL epsilon_str,
                           INT coarsening_type)
{
    INT i,j;
    REAL *amax=(REAL *)fasp_mem_calloc(A->row,sizeof(REAL));
    
#ifdef _OPENMP
    // variables for OpenMP
    INT myid, mybegin, myend;
    INT nthreads = FASP_GET_NUM_THREADS();
#endif
    
    // get the maximum absolute negative value data in each row of A
    if (coarsening_type==2) { // original RS coarsening, just consider negative strong coupled
        
#ifdef _OPENMP
#pragma omp parallel for if(A->row>OPENMP_HOLDS) private(myid,mybegin,myend,i,j)
        for (myid=0; myid<nthreads; myid++) {
            FASP_GET_START_END(myid, nthreads, A->row, &mybegin, &myend);
            for (i=mybegin; i<myend; i++) {
#else
                for (i=0;i<A->row;++i) {
#endif
                    amax[i]=0;
                    for (j=A->IA[i];j<A->IA[i+1];j++) {
                        if ((-A->val[j])>amax[i] && A->JA[j]!=i) {
                            amax[i]=-A->val[j];
                        }
                    }
#ifdef _OPENMP
                }
            }
#else
        }
#endif
    }
    
    if (coarsening_type==3) { // original RS coarsening, consider absolute strong coupled
#ifdef _OPENMP
#pragma omp parallel for if(A->row>OPENMP_HOLDS) private(myid,mybegin,myend,i,j)
        for (myid=0; myid<nthreads; myid++) {
            FASP_GET_START_END(myid, nthreads, A->row, &mybegin, &myend);
            for (i=mybegin; i<myend; i++) {
#else
                for (i=0;i<A->row;++i) {
#endif
                    amax[i]=0;
                    for (j=A->IA[i];j<A->IA[i+1];j++) {
                        if (ABS(A->val[j])>amax[i] && A->JA[j]!=i) {
                            amax[i]=ABS(A->val[j]);
                        }
                    }
#ifdef _OPENMP
                }
            }
#else
        }
#endif
    }
    
    // step 1: Find first the structure IA of S
    S->row=A->row;
    S->col=A->col;
    S->val=NULL;
    S->JA=NULL;
    S->IA=(INT*)fasp_mem_calloc(S->row+1, sizeof(INT));
    
    if (coarsening_type==2) {
#ifdef _OPENMP
#pragma omp parallel for if(S->row>OPENMP_HOLDS) private(myid,mybegin,myend,i,j)
        for (myid=0; myid<nthreads; ++myid) {
            FASP_GET_START_END(myid, nthreads, S->row, &mybegin, &myend);
            for (i=mybegin; i<myend; ++i) {
#else
                for (i=0;i<S->row;++i) {
#endif
                    for (j=A->IA[i];j<A->IA[i+1];j++) {
                        if ((-A->val[j])>=epsilon_str*amax[i] && A->JA[j]!=i) {
                            S->IA[i+1]++;
                        }
                    }
#ifdef _OPENMP
                }
            }
#else
        }
#endif
    }
    
    if (coarsening_type==3) {
#ifdef _OPENMP
#pragma omp parallel for if(S->row>OPENMP_HOLDS) private(myid,mybegin,myend,i,j)
        for (myid=0; myid<nthreads; myid++) {
            FASP_GET_START_END(myid, nthreads, S->row, &mybegin, &myend);
            for (i=mybegin; i<myend; i++) {
#else
                for (i=0;i<S->row;++i) {
#endif
                    for (j=A->IA[i];j<A->IA[i+1];j++) {
                        if (ABS(A->val[j])>=epsilon_str*amax[i] && A->JA[j]!=i) {
                            S->IA[i+1]++;
                        }
                    }
#ifdef _OPENMP
                }
            }
#else
        }
#endif
    }
    
    for (i=0;i<S->row;++i) S->IA[i+1]+=S->IA[i];
    
    // step 2: Find the structure JA of S
    INT index=0;
    S->JA=(INT*)fasp_mem_calloc(S->IA[S->row],sizeof(INT));
    
    if (coarsening_type==2) {
        for (i=0;i<S->row;++i) {
            for (j=A->IA[i];j<A->IA[i+1];j++) {
                if ((-A->val[j])>=epsilon_str*amax[i] && A->JA[j]!=i) {
                    S->JA[index]= A->JA[j];
                    index++;
                }
            }
        }
    }
    
    if (coarsening_type==3) {
        for (i=0;i<S->row;++i) {
            for (j=A->IA[i];j<A->IA[i+1];j++) {
                if (ABS(A->val[j])>=epsilon_str*amax[i] && A->JA[j]!=i) {
                    S->JA[index]= A->JA[j];
                    index++;
                }
            }
        }
    }
    
    fasp_mem_free(amax);
}
#endif

/**
 * \fn static void generate_S_rs_ag_1 (dCSRmat *A, iCSRmat *S, iCSRmat *Sh, ivector *vertices, ivector *CGPT_index, ivector *CGPT_rindex)
 *
 * \brief Generate the set of all strong negative or strong absolute couplings, aggressive coarsening A1 or A2
 *
 * \param A                Pointer to the coefficient matrix
 * \param S                Pointer to the set of all strong couplings matrix
 * \param Sh               Pointer to the set of all strong couplings matrix between coarse grid points
 * \param vertices         Pointer to the type of variables--C/F splitting
 * \param CGPT_index       Pointer to the index of CGPT from the list of CGPT to the list of all points
 * \param CGPT_rindex      Pointer to the index of CGPT from the list of all points to the list of CGPT
 *
 * \author Kai Yang, Xiaozhe Hu
 * \date   09/06/2010
 */
static void generate_S_rs_ag_1 (dCSRmat *A,
                                iCSRmat *S,
                                iCSRmat *Sh,
                                ivector *vertices,
                                ivector *CGPT_index,
                                ivector *CGPT_rindex)
{
    
    INT   i,j,k;
    INT   num_c,count,ci,cj,ck,fj,cck;
    INT  *cp_index, *cp_rindex, *times_visited, *vec=vertices->val;
    
    CGPT_rindex->row=A->row;
    CGPT_rindex->val=(INT*)fasp_mem_calloc(vertices->row,sizeof(INT));// for the reverse indexing of coarse grid points
    cp_rindex=CGPT_rindex->val;
    
    //count the number of coarse grid points
    num_c=0;
    for(i=0;i<vertices->row;i++)
    {
        if(vec[i]==CGPT) num_c++;
    }
    
    CGPT_index->row=num_c;
    
    //generate coarse grid point index
    CGPT_index->val=(INT *)fasp_mem_calloc(num_c,sizeof(INT));
    cp_index=CGPT_index->val;
    j=0;
    for (i=0;i<vertices->row;i++) {
        if(vec[i]==CGPT) {
            cp_index[j]=i;
            cp_rindex[i]=j;
            j++;
        }
    }
    
    Sh->row=num_c;
    Sh->col=num_c;
    Sh->val=NULL;
    Sh->JA=NULL;
    Sh->IA=(INT*)fasp_mem_calloc(Sh->row+1,sizeof(INT));
    
    times_visited=(INT*)fasp_mem_calloc(num_c,sizeof(INT)); // record the number of times some coarse point is visited
    
    //for (i=0; i<num_c; i++) times_visited[i]=0;
    memset(times_visited, 0, sizeof(INT)*num_c);
    
    // step 1: Find first the structure IA of Sh
    Sh->IA[0]=0;
    
    for(ci=0;ci<Sh->row;ci++)
    {
        count=0; // count the the number of coarse point that i is strongly connected to w.r.t. (p,2)
        i=cp_index[ci];//find the index of the ci-th coarse grid point
        
        //visit all the fine neighbors that ci is strongly connected to
        for(j=S->IA[i];j<S->IA[i+1];j++)
        {
            
            fj=S->JA[j];
            
            if(vec[fj]==CGPT&&fj!=i)
            {
                cj=cp_rindex[fj];
                
                if(times_visited[cj]!=ci+1)
                {
                    //newly visited
                    times_visited[cj]=ci+1;//marked as strongly connected from ci
                    count++;
                }
                
            }
            else if(vec[fj]==FGPT) // it is a fine grid point,
            {
                
                //find all the coarse neighbors that fj is strongly connected to
                for(k=S->IA[fj];k<S->IA[fj+1];k++)
                {
                    ck=S->JA[k];
                    
                    if(vec[ck]==CGPT&&ck!=i)// it is a coarse grid point
                    {
                        if(cp_rindex[ck]>=num_c) printf("generate_S_rs_ag: index exceed bound!\n");
                        
                        cck=cp_rindex[ck];
                        
                        if (times_visited[cck]!=ci+1) {
                            //newly visited
                            times_visited[cck]=ci+1;//marked as strongly connected from ci
                            count++;
                        }
                        
                        /*
                         if (times_visited[cck] == ci+1){
                         
                         }
                         else if (times_visited[cck] == -ci-1){
                         times_visited[cck]=ci+1;//marked as strongly connected from ci
                         count++;
                         }
                         else{
                         times_visited[cck]=-ci-1;//marked as visited
                         }
                         */
                        
                    }//end if
                }//end for k
                
            }//end if
        }//end for j
        
        Sh->IA[ci+1]=Sh->IA[ci]+count;
        
    }//end for i
    
    
    // step 2: Find JA of Sh
    
    for (i=0; i<num_c; i++) times_visited[i]=0; // clean up times_visited
    Sh->nnz=Sh->IA[Sh->row];
    Sh->JA=(INT*)fasp_mem_calloc(Sh->nnz,sizeof(INT));
    
    for(ci=0;ci<Sh->row;ci++)
    {
        i=cp_index[ci]; //find the index of the i-th coarse grid point
        count=Sh->IA[ci]; //count for coarse points
        
        //visit all the fine neighbors that ci is strongly connected to
        for(j=S->IA[i];j<S->IA[i+1];j++)
        {
            fj=S->JA[j];
            if(vec[fj]==CGPT&&fj!=i)
            {
                cj=cp_rindex[fj];
                if(times_visited[cj]!=ci+1)
                {
                    //newly visited
                    times_visited[cj]=ci+1;
                    Sh->JA[count]=cj;
                    count++;
                }
                
                
            }
            else if(vec[fj]==FGPT) // it is a fine grid point,
            {
                //find all the coarse neighbors that fj is strongly connected to
                for(k=S->IA[fj];k<S->IA[fj+1];k++)
                {
                    ck=S->JA[k];
                    if(vec[ck]==CGPT&&ck!=i)// it is a coarse grid point
                    {
                        cck=cp_rindex[ck];
                        
                        
                        if(times_visited[cck]!=ci+1)
                        {
                            //newly visited
                            times_visited[cck]=ci+1;
                            Sh->JA[count]=cck;
                            count++;
                        }
                        
                        /*
                         if (times_visited[cck] == ci+1){
                         
                         }
                         else if (times_visited[cck] == -ci-1){
                         times_visited[cck]=ci+1;
                         Sh->JA[count]=cck;
                         count++;
                         }
                         else {
                         times_visited[cck]=-ci-1;
                         }
                         */
                        
                    }//end if
                }//end for k
                
            }//end if
        }//end for j
        if(count!=Sh->IA[ci+1]) printf("generate_S_rs_ag: inconsistency in number of nonzeros values\n ");
    }//end for ci
    fasp_mem_free(times_visited);
}

/**
 * \fn static void generate_S_rs_ag_2 (dCSRmat *A, iCSRmat *S, iCSRmat *Sh, ivector *vertices, ivector *CGPT_index, ivector *CGPT_rindex)
 *
 * \brief Generate the set of all strong negative or strong absolute couplings, aggressive coarsening A1 or A2
 *
 * \param A                Pointer to the coefficient matrix
 * \param S                Pointer to the set of all strong couplings matrix
 * \param Sh               Pointer to the set of all strong couplings matrix between coarse grid points
 * \param vertices         Pointer to the type of variables--C/F splitting
 * \param CGPT_index       Pointer to the index of CGPT from the list of CGPT to the list of all points
 * \param CGPT_rindex      Pointer to the index of CGPT from the list of all points to the list of CGPT
 *
 * \author Xiaozhe Hu
 * \date   04/24/2013
 *
 * Note: the difference between generate_S_rs_ag_1 and generate_S_rs_ag_2 is that generate_S_rs_ag_1 uses
 *       path 1 to detetermine strongly coupled C points while generate_S_rs_ag_2 uses path 2 to
 *       determinestrongly coupled C points.  Usually generate_S_rs_ag_1 gives more aggresive coarsening
 */
static void generate_S_rs_ag_2 (dCSRmat *A,
                                iCSRmat *S,
                                iCSRmat *Sh,
                                ivector *vertices,
                                ivector *CGPT_index,
                                ivector *CGPT_rindex)
{
    
    INT   i,j,k;
    INT   num_c,count,ci,cj,ck,fj,cck;
    INT  *cp_index, *cp_rindex, *times_visited, *vec=vertices->val;
    
    CGPT_rindex->row=A->row;
    CGPT_rindex->val=(INT*)fasp_mem_calloc(vertices->row,sizeof(INT));// for the reverse indexing of coarse grid points
    cp_rindex=CGPT_rindex->val;
    
    //count the number of coarse grid points
    num_c=0;
    for(i=0;i<vertices->row;i++)
    {
        if(vec[i]==CGPT) num_c++;
    }
    
    CGPT_index->row=num_c;
    
    //generate coarse grid point index
    CGPT_index->val=(INT *)fasp_mem_calloc(num_c,sizeof(INT));
    cp_index=CGPT_index->val;
    j=0;
    for (i=0;i<vertices->row;i++) {
        if(vec[i]==CGPT) {
            cp_index[j]=i;
            cp_rindex[i]=j;
            j++;
        }
    }
    
    Sh->row=num_c;
    Sh->col=num_c;
    Sh->val=NULL;
    Sh->JA=NULL;
    Sh->IA=(INT*)fasp_mem_calloc(Sh->row+1,sizeof(INT));
    
    times_visited=(INT*)fasp_mem_calloc(num_c,sizeof(INT)); // record the number of times some coarse point is visited
    
    //for (i=0; i<num_c; i++) times_visited[i]=0;
    memset(times_visited, 0, sizeof(INT)*num_c);
    
    // step 1: Find first the structure IA of Sh
    Sh->IA[0]=0;
    
    for(ci=0;ci<Sh->row;ci++)
    {
        count=0; // count the the number of coarse point that i is strongly connected to w.r.t. (p,2)
        i=cp_index[ci];//find the index of the ci-th coarse grid point
        
        //visit all the fine neighbors that ci is strongly connected to
        for(j=S->IA[i];j<S->IA[i+1];j++)
        {
            
            fj=S->JA[j];
            
            if(vec[fj]==CGPT&&fj!=i)
            {
                cj=cp_rindex[fj];
                
                if(times_visited[cj]!=ci+1)
                {
                    //newly visited
                    times_visited[cj]=ci+1;//marked as strongly connected from ci
                    count++;
                }
                
            }
            else if(vec[fj]==FGPT) // it is a fine grid point,
            {
                
                //find all the coarse neighbors that fj is strongly connected to
                for(k=S->IA[fj];k<S->IA[fj+1];k++)
                {
                    ck=S->JA[k];
                    
                    if(vec[ck]==CGPT&&ck!=i)// it is a coarse grid point
                    {
                        if(cp_rindex[ck]>=num_c) printf("generate_S_rs_ag: index exceed bound!\n");
                        
                        cck=cp_rindex[ck];
                        
                        if (times_visited[cck] == ci+1){
                            
                        }
                        else if (times_visited[cck] == -ci-1){
                            times_visited[cck]=ci+1;//marked as strongly connected from ci
                            count++;
                        }
                        else{
                            times_visited[cck]=-ci-1;//marked as visited
                        }
                        
                    }//end if
                }//end for k
                
            }//end if
        }//end for j
        
        Sh->IA[ci+1]=Sh->IA[ci]+count;
        
    }//end for i
    
    
    // step 2: Find JA of Sh
    
    for (i=0; i<num_c; i++) times_visited[i]=0; // clean up times_visited
    Sh->nnz=Sh->IA[Sh->row];
    Sh->JA=(INT*)fasp_mem_calloc(Sh->nnz,sizeof(INT));
    
    for(ci=0;ci<Sh->row;ci++)
    {
        i=cp_index[ci]; //find the index of the i-th coarse grid point
        count=Sh->IA[ci]; //count for coarse points
        
        //visit all the fine neighbors that ci is strongly connected to
        for(j=S->IA[i];j<S->IA[i+1];j++)
        {
            fj=S->JA[j];
            if(vec[fj]==CGPT&&fj!=i)
            {
                cj=cp_rindex[fj];
                if(times_visited[cj]!=ci+1)
                {
                    //newly visited
                    times_visited[cj]=ci+1;
                    Sh->JA[count]=cj;
                    count++;
                }
                
                
            }
            else if(vec[fj]==FGPT) // it is a fine grid point,
            {
                //find all the coarse neighbors that fj is strongly connected to
                for(k=S->IA[fj];k<S->IA[fj+1];k++)
                {
                    ck=S->JA[k];
                    if(vec[ck]==CGPT&&ck!=i)// it is a coarse grid point
                    {
                        cck=cp_rindex[ck];
                        
                        if (times_visited[cck] == ci+1){
                            
                        }
                        else if (times_visited[cck] == -ci-1){
                            times_visited[cck]=ci+1;
                            Sh->JA[count]=cck;
                            count++;
                        }
                        else {
                            times_visited[cck]=-ci-1;
                        }
                        
                    }//end if
                }//end for k
                
            }//end if
        }//end for j
        if(count!=Sh->IA[ci+1]) printf("generate_S_rs_ag: inconsistency in number of nonzeros values\n ");
    }//end for ci
    fasp_mem_free(times_visited);
}


/**
 * \fn static INT form_coarse_level (dCSRmat *A, iCSRmat *S, ivector *vertices,
 *                                   INT row, INT interp_type)
 *
 * \brief Find coarse level points
 *
 * \param A            Pointer to the coefficient matrix
 * \param S            Pointer to the set of all strong couplings matrix
 * \param vertices     Pointer to the type of variables
 * \param row          Number of rows of P
 * \param interp_type  Interpolation methods for P
 *
 * \return Number of cols of P
 *
 * \author Xuehai Huang, Chensong Zhang
 * \date   09/06/2010
 *
 * Modified by Chunsheng Feng, Xiaoqiang Yue on 05/24/2012
 * Modified by Chensong Zhang on 07/06/2012: fix a data type bug
 */
static INT form_coarse_level (dCSRmat *A,
                              iCSRmat *S,
                              ivector *vertices,
                              INT row,
                              INT interp_type)
{
    INT col = 0;
    unsigned INT maxlambda, maxnode, num_left=0;
    INT measure, new_meas;
    
    INT *ia=A->IA, *vec=vertices->val;
    INT ci_tilde = -1, ci_tilde_mark = -1;
    INT set_empty = 1, C_i_nonempty = 0;
    INT ji,jj,i,j,k,l,index;
    INT myid;
    INT mybegin;
    INT myend;
    
    INT *work = (INT*)fasp_mem_calloc(4*row,sizeof(INT));
    INT *lists = work, *where = lists+row, *lambda = where+row, *graph_array = lambda+row;
    
    LinkList LoL_head = NULL, LoL_tail = NULL, list_ptr = NULL;
    
    iCSRmat ST; fasp_icsr_trans(S, &ST);
    
    INT nthreads = 1, use_openmp = FALSE;
    
#ifdef _OPENMP
    if ( row > OPENMP_HOLDS ) {
        use_openmp = TRUE;
        nthreads = FASP_GET_NUM_THREADS();
    }
#endif
    
    /**************************************************/
    /* Coarsening Phase ONE: find coarse level points */
    /**************************************************/
    
    // 1. Initialize lambda
    if (use_openmp) {
#ifdef _OPENMP
#pragma omp parallel for private(myid, mybegin,myend,i)
#endif
        for (myid = 0; myid < nthreads; myid++ )
        {
            FASP_GET_START_END(myid, nthreads, row, &mybegin, &myend);
            for (i=mybegin; i<myend; i++) lambda[i]=ST.IA[i+1]-ST.IA[i];
        }
    }
    else {
        for (i=0;i<row;++i) lambda[i]=ST.IA[i+1]-ST.IA[i];
    }
    
    // 2. Before the following algorithm starts, filter out the variables which
    // have no connections at all and assign special F-variables.
    if (use_openmp) {
#ifdef _OPENMP
#pragma omp parallel for reduction(+:num_left) private(myid, mybegin, myend, i)
#endif
        for (myid = 0; myid < nthreads; myid++ )
        {
            FASP_GET_START_END(myid, nthreads, row, &mybegin, &myend);
            for (i=mybegin; i<myend; i++)
            {
                if ( (ia[i+1]-ia[i])<=1 ) {
                    vec[i]=ISPT; // set i as an ISOLATED fine node
                    lambda[i]=0;
                }
                else {
                    vec[i]=UNPT; // set i as a undecided node
                    num_left++;
                }
            }
        }
    }
    else {
        for (i=0;i<row;++i)
        {
            if ( (ia[i+1]-ia[i])<=1 ) {
                vec[i]=ISPT; // set i as an ISOLATED fine node
                lambda[i]=0;
            }
            else {
                vec[i]=UNPT; // set i as a undecided node
                num_left++;
            }
        }
    }
    
    // 3. Set the variables with nonpositvie measure as F-variables
    for (i=0;i<row;++i)
    {
        measure=lambda[i];
        if (vec[i]!=ISPT) {
            if (measure>0) {
                enter_list(&LoL_head, &LoL_tail, lambda[i], i, lists, where);
            }
            else {
                if (measure<0) printf("### WARNING: Negative lambda!\n");
                vec[i]=FGPT; // set i as fine node
                for (k=S->IA[i];k<S->IA[i+1];++k) {
                    j=S->JA[k];
                    if (vec[j]!=ISPT) {
                        if (j<i) {
                            new_meas=lambda[j];
                            if (new_meas>0) {
                                remove_node(&LoL_head, &LoL_tail, new_meas, j, lists, where);
                            }
                            new_meas= ++(lambda[j]);
                            enter_list(&LoL_head, &LoL_tail,  new_meas, j, lists, where);
                        }
                        else {
                            new_meas= ++(lambda[j]);
                        }
                    }
                }
                --num_left;
            }
        }
    }
    
    // 4. Main loop
    while (num_left>0) {
        
        // pick $i\in U$ with $\max\lambda_i: C:=C\cup\{i\}, U:=U\\{i\}$
        maxnode=LoL_head->head;
        maxlambda=lambda[maxnode];
        
        vec[maxnode]=CGPT; // set maxnode as coarse node
        lambda[maxnode]=0;
        --num_left;
        remove_node(&LoL_head, &LoL_tail, maxlambda, maxnode, lists, where);
        col++;
        
        // for all $j\in S_i^T\cap U: F:=F\cup\{j\}, U:=U\backslash\{j\}$
        for (i=ST.IA[maxnode];i<ST.IA[maxnode+1];++i) {
            j=ST.JA[i];
            
            /* if j is unkown */
            if (vec[j]==UNPT) {
                vec[j]=FGPT;  // set j as fine node
                remove_node(&LoL_head, &LoL_tail, lambda[j], j, lists, where);
                --num_left;
                
                for (l=S->IA[j];l<S->IA[j+1];l++) {
                    k=S->JA[l];
                    if (vec[k]==UNPT) // k is unkown
                    {
                        remove_node(&LoL_head, &LoL_tail, lambda[k], k, lists, where);
                        new_meas= ++(lambda[k]);
                        enter_list(&LoL_head, &LoL_tail,new_meas, k, lists, where);
                    }
                }
                
            } // if
        } // i
        
        for (i=S->IA[maxnode];i<S->IA[maxnode+1];++i) {
            j=S->JA[i];
            if (vec[j]==UNPT) // j is unkown
            {
                measure=lambda[j];
                remove_node(&LoL_head, &LoL_tail, measure, j, lists, where);
                lambda[j]=--measure;
                if (measure>0) {
                    enter_list(&LoL_head, &LoL_tail, measure, j, lists, where);
                }
                else {
                    vec[j]=FGPT; // set j as fine variable
                    --num_left;
                    
                    for (l=S->IA[j];l<S->IA[j+1];l++) {
                        k=S->JA[l];
                        if (vec[k]==UNPT) // k is unkown
                        {
                            remove_node(&LoL_head, &LoL_tail, lambda[k], k, lists, where);
                            new_meas= ++(lambda[k]);
                            enter_list(&LoL_head, &LoL_tail,new_meas, k, lists, where);
                        }
                    } // end for l
                } // end if
            } // end if
        } // end for
    } // while
    
    if (LoL_head) {
        list_ptr=LoL_head;
        LoL_head->prev_node=NULL;
        LoL_head->next_node=NULL;
        LoL_head = list_ptr->next_node;
        
        fasp_mem_free(list_ptr);
    }
    
    if (interp_type != INTERP_STD) {
        /****************************************************************/
        /* Coarsening Phase TWO: check fine points for coarse neighbors */
        /****************************************************************/
        if (use_openmp) {
#ifdef _OPENMP
#pragma omp parallel for private(myid, mybegin,myend,i)
#endif
            for (myid = 0; myid < nthreads; myid++ )
            {
                FASP_GET_START_END(myid, nthreads, row, &mybegin, &myend);
                for (i=mybegin; i<myend; ++i) graph_array[i] = -1;
            }
        }
        else {
            for (i=0; i < row; ++i) graph_array[i] = -1;
        }
        
        for (i=0; i < row; ++i)
        {
            if (ci_tilde_mark |= i) ci_tilde = -1;
            
            if (vec[i] == FGPT) // F-variable
            {
                for (ji = S->IA[i]; ji < S->IA[i+1]; ++ji) {
                    j = S->JA[ji];
                    if (vec[j] == CGPT) // C-variable
                        graph_array[j] = i;
                } // end for ji
                
                for (ji = S->IA[i]; ji < S->IA[i+1]; ++ji) {
                    
                    j = S->JA[ji];
                    
                    if (vec[j] == FGPT) {
                        set_empty = 1;
                        
                        for (jj = S->IA[j]; jj < S->IA[j+1]; ++jj) {
                            index = S->JA[jj];
                            if (graph_array[index] == i) {
                                set_empty = 0;
                                break;
                            }
                        } // end for jj
                        
                        if (set_empty) {
                            if (C_i_nonempty) {
                                vec[i] = CGPT;
                                col++;
                                if (ci_tilde > -1) {
                                    vec[ci_tilde] = FGPT;
                                    col--;
                                    ci_tilde = -1;
                                }
                                C_i_nonempty = 0;
                                break;
                            }
                            else {
                                ci_tilde = j;
                                ci_tilde_mark = i;
                                vec[j] = CGPT;
                                col++;
                                C_i_nonempty = 1;
                                i--;
                                break;
                            } // end if C_i_nonempty
                        } // end if set_empty
                        
                    }
                }
            }
        }
    }
    
    fasp_icsr_free(&ST);
    
    fasp_mem_free(work);
    
    return col;
}

/**
 * \fn static void generate_sparsity_P (dCSRmat *P, iCSRmat *S, ivector *vertices,
 *                                      INT row, INT col)
 *
 * \brief Find coarse level points
 *
 * \param P         Pointer to the prolongation matrix
 * \param S         Pointer to the set of all strong couplings matrix
 * \param vertices  Pointer to the type of variables
 * \param row       Number of rows of P
 * \param col       Number of cols of P
 *
 * \author Xuehai Huang, Chensong Zhang
 * \date   09/06/2010
 *
 * Modified by Chunsheng Feng, Xiaoqiang Yue on 05/23/2012
 */

static void generate_sparsity_P (dCSRmat *P,
                                 iCSRmat *S,
                                 ivector *vertices,
                                 INT row,
                                 INT col )
{
    INT i,j,k,index=0;
    INT *vec=vertices->val;
    
    P->row=row; P->col=col;
    P->IA=(INT*)fasp_mem_calloc(row+1, sizeof(INT));
    
    INT nthreads = 1, use_openmp = FALSE;
    
#ifdef _OPENMP
    if ( row > OPENMP_HOLDS ) {
        use_openmp = TRUE;
        nthreads = FASP_GET_NUM_THREADS();
    }
#endif
    
#if CHMEM_MODE
    total_alloc_mem += (row+1)*sizeof(INT);
#endif
    
    // step 1: Find the structure IA of P first
    if (use_openmp) {
        INT mybegin,myend,myid;
#ifdef _OPENMP
#pragma omp parallel for private(myid, mybegin,myend,i,j,k)
#endif
        for (myid = 0; myid < nthreads; myid++ )
        {
            FASP_GET_START_END(myid, nthreads, row, &mybegin, &myend);
            for (i=mybegin; i<myend; ++i)
            {
                if (vec[i]==FGPT) // if node i is on fine grid
                {
                    for (j=S->IA[i];j<S->IA[i+1];j++)
                    {
                        k=S->JA[j];
                        if (vec[k]==CGPT)
                        {
                            P->IA[i+1]++;
                        }
                    }
                }
                else if (vec[i]==ISPT) { // if node i is a special fine node
                    P->IA[i+1]=0;
                }
                else { // if node i is on coarse grid
                    P->IA[i+1]=1;
                }
            }
        }
    }
    else {
        for (i=0;i<row;++i)
        {
            if (vec[i]==FGPT) // if node i is on fine grid
            {
                for (j=S->IA[i];j<S->IA[i+1];j++)
                {
                    k=S->JA[j];
                    if (vec[k]==CGPT)
                    {
                        P->IA[i+1]++;
                    }
                }
            }
            else if (vec[i]==ISPT) { // if node i is a special fine node
                P->IA[i+1]=0;
            }
            else { // if node i is on coarse grid
                P->IA[i+1]=1;
            }
        }
    }
    
    for (i=0;i<P->row;++i) P->IA[i+1]+=P->IA[i];
    
    P->nnz=P->IA[P->row]-P->IA[0];
    
    // step 2: Find the structure JA of P
    P->JA=(INT*)fasp_mem_calloc(P->nnz,sizeof(INT));
    P->val=(REAL*)fasp_mem_calloc(P->nnz,sizeof(REAL));
    
#if CHMEM_MODE
    total_alloc_mem += (P->nnz)*sizeof(INT);
    total_alloc_mem += (P->nnz)*sizeof(REAL);
#endif
    
    for (i=0;i<row;++i)
    {
        if (vec[i]==FGPT) // fine node
        {
            for (j=S->IA[i];j<S->IA[i+1];j++) {
                k=S->JA[j];
                if (vec[k]==CGPT) {
                    P->JA[index]=k;
                    index++;
                } // end if
            } // end for j
        } // end if
        else if (vec[i]==ISPT)
        {
            // if node i is a special fine node, do nothing
        }
        else // if node i is on coarse grid
        {
            P->JA[index]=i;
            index++;
        }
    }
}

/**
 * \fn static void generate_sparsity_P_std (dCSRmat *P, iCSRmat *S, ivector *vertices, INT row, INT col)
 *
 * \brief Find sparsity pattern of P for standard interpolation
 *
 * \param P         Pointer to the prolongation matrix
 * \param S         Pointer to the set of all strong couplings matrix
 * \param vertices  Pointer to the type of variables
 * \param row       Number of rows of P
 * \param col       Number of cols of P
 *
 * \author Kai Yang, Xiaozhe Hu
 * \date   05/21/2012
 *
 * Modified by Chunsheng Feng, Zheng Li on 10/13/2012
 */
static void generate_sparsity_P_std (dCSRmat *P,
                                     iCSRmat *S,
                                     ivector *vertices,
                                     INT row,
                                     INT col)
{
    INT i,j,k,l,h,index=0;
    INT *vec=vertices->val;
    INT *times_visited;
    
    P->row=row; P->col=col;
    P->IA=(INT*)fasp_mem_calloc(row+1, sizeof(INT));
    
    times_visited=(INT*)fasp_mem_calloc(row,sizeof(INT)); // to record the number of times a coarse point is visited.
    
#ifdef _OPENMP
#pragma omp parallel for if(row>OPENMP_HOLDS)
#endif
    for (i=0; i<row; i++) times_visited[i] = -1;  // initilize
    
#if CHMEM_MODE
    total_alloc_mem += (row+1)*sizeof(INT);
#endif
    
    // step 1: Find the structure IA of P first
    for (i=0; i<row; ++i) {
        if (vec[i]==FGPT) { // if node i is a F point
            for (j=S->IA[i];j<S->IA[i+1];j++) {
                k=S->JA[j];
                if (vec[k]==CGPT) { // if neighbor k of i is a C point
                    if (times_visited[k] == i) {
                        // already visited, do nothing
                    }
                    else {
                        // have not visited, do something
                        times_visited[k] = i;
                        P->IA[i+1]++;
                    }
                }
                else if( (vec[k]==FGPT)&&(k!=i) ) {// if neighbor k of i is a F point and k is not i
                    for(l=S->IA[k];l<S->IA[k+1];l++) { // look at the neighbors of k
                        h=S->JA[l];
                        if(vec[h]==CGPT) {// only care about the C point neighbors of k
                            if (times_visited[h] == i) {
                                // already visited, do nothing
                            }
                            else {
                                // have not visited, do something
                                times_visited[h] = i;
                                P->IA[i+1]++;
                            }
                        }
                    }  // end for(l=S->IA[k];l<S->IA[k+1];l++)
                }  // end if (vec[k]==CGPT)
            } // end for (j=S->IA[i];j<S->IA[i+1];j++)
        }
        else if (vec[i]==ISPT) { // if node i is a special F point
            P->IA[i+1]=0;
        }
        else { // if node i is on coarse grid
            P->IA[i+1]=1;
        }  // end if (vec[i]==FGPT)
    } // end for (i=0;i<row;++i)
    
    for (i=0;i<P->row;++i) P->IA[i+1]+=P->IA[i];
    
    P->nnz=P->IA[P->row]-P->IA[0];
    
    // step 2: Find the structure JA of P
    P->JA=(INT*)fasp_mem_calloc(P->nnz,sizeof(INT));
    P->val=(REAL*)fasp_mem_calloc(P->nnz,sizeof(REAL));
    
#if CHMEM_MODE
    total_alloc_mem += (P->nnz)*sizeof(INT);
    total_alloc_mem += (P->nnz)*sizeof(REAL);
#endif
    
#ifdef _OPENMP
#pragma omp parallel for if(row>OPENMP_HOLDS)
#endif
    for (i=0; i<row; i++) times_visited[i] = -1;  // reinitilize
    
    for (i=0;i<row;++i)
    {
        index = 0;
        if (vec[i]==FGPT)  // if node i is a F point
        {
            for (j=S->IA[i];j<S->IA[i+1];j++) {
                k=S->JA[j];
                if (vec[k]==CGPT) // if neighbor k of i is a C point
                {
                    if (times_visited[k] == i) {
                        // already visited, do nothing
                    }
                    else {
                        // have not visited, do something
                        times_visited[k] = i;
                        P->JA[P->IA[i]+index] = k;
                        index++;
                    }
                    
                }
                else if( (vec[k]==FGPT)&&(k!=i) ) // if neighbor k of i is a F point and k is not i
                {
                    for(l=S->IA[k];l<S->IA[k+1];l++) // look at the neighbors of k
                    {
                        h=S->JA[l];
                        if(vec[h]==CGPT) // only care about the C point neighbors of k
                        {
                            if (times_visited[h] == i){
                                // already visited, do nothing
                            }
                            else {
                                // have not visited, do something
                                times_visited[h] = i;
                                P->JA[P->IA[i]+index] = h;
                                index++;
                            }
                        }
                        
                    }  // end for(l=S->IA[k];l<S->IA[k+1];l++)
                    
                }  // end if (vec[k]==CGPT)
                
            } // end for (j=S->IA[i];j<S->IA[i+1];j++)
        }
        else if (vec[i]==ISPT)
        {
            // if node i is a special F point, do nothing
        }
        else // if node i is a C point
        {
            P->JA[P->IA[i]]=i;
        }
    }
    // clea up
    fasp_mem_free(times_visited);
    
}

/**
 * \fn static INT form_coarse_level_ag (dCSRmat *A, iCSRmat *S, ivector *vertices, INT row,
 *                                      INT interp_type INT aggressive_path)
 *
 * \brief Find coarse level points by aggressive coarsening
 *
 * \param A             Pointer to the coefficient matrix
 * \param S             Pointer to the set of all strong couplings matrix
 * \param vertices      Pointer to the type of variables
 * \param row           Number of rows of P
 * \param interp_type   Type of interpolation
 *
 * \return Number of cols of P
 *
 * \author Kai Yang, Xiaozhe Hu
 * \date   09/06/2010
 *
 * Modified by Chensong Zhang on 07/05/2012: Fix a data type bug
 * Modified by Chunsheng Feng, Zheng Li on 10/13/2012
 * Modified by Xiaozhe Hu on 04/24/2013: modify aggresive coarsening
 */
static INT form_coarse_level_ag (dCSRmat *A,
                                 iCSRmat *S,
                                 ivector *vertices,
                                 INT row,
                                 INT interp_type,
                                 INT aggressive_path)
{
    INT col = 0; // initialize col(P): returning output
    unsigned INT maxlambda, maxnode, num_left=0;
    INT measure, new_meas;
    INT *vec=vertices->val;
    INT i,j,k,l,m,flag,ci,cj,ck,cl,num_c;
    
    INT *work = (INT*)fasp_mem_calloc(4*row,sizeof(INT));
    INT *lists = work, *where = lists+row, *lambda = where+row;
    INT *cp_index; //*cp_rindex;
    
    LinkList LoL_head = NULL, LoL_tail = NULL, list_ptr = NULL;
    iCSRmat ST,Sh,ShT;
    // Sh is for the strong coupling matrix between temporary CGPTs
    // ShT is the transpose of Sh
    // Snew is for combining the information from S and Sh
    ivector CGPT_index, CGPT_rindex;
    fasp_icsr_trans(S, &ST);
    
#ifdef _OPENMP
    // variables for OpenMP
    INT myid, mybegin, myend;
    INT sub_col = 0;
    INT nthreads = FASP_GET_NUM_THREADS();
#endif
    
    vertices->row=A->row;
    
    /**************************************************/
    /* Coarsening Phase ONE: find temporary coarse level points */
    /**************************************************/
    num_c = form_coarse_level(A, S, vertices, row, interp_type);//return the number of temporary CGPT
    
    /**************************************************/
    /* Coarsening Phase TWO: find real coarse level points  */
    /**************************************************/
    //find Sh, the strong coupling between coarse grid points w.r.t. (path,2)
    if ( aggressive_path < 2 )
        generate_S_rs_ag_1(A, S, &Sh, vertices, &CGPT_index, &CGPT_rindex);
    else
        generate_S_rs_ag_2(A, S, &Sh, vertices, &CGPT_index, &CGPT_rindex);
    
    fasp_icsr_trans(&Sh, &ShT);
    
    CGPT_index.row=num_c;
    CGPT_rindex.row=row;
    cp_index=CGPT_index.val;
    // cp_rindex=CGPT_rindex.val;
    
    // 1. Initialize lambda
#ifdef _OPENMP
#pragma omp parallel for if(num_c>OPENMP_HOLDS)
#endif
    for (ci=0;ci<num_c;++ci) lambda[ci]=ShT.IA[ci+1]-ShT.IA[ci];
    
    // 2. Set the variables with nonpositvie measure as F-variables
    num_left=0; // number of CGPT in the list LoL
    for (ci=0;ci<num_c;++ci) {
        i=cp_index[ci];
        measure=lambda[ci];
        if (vec[i]!=ISPT) {
            if (measure>0) {
                enter_list(&LoL_head, &LoL_tail, lambda[ci], ci, lists, where);
                num_left++;
            }
            else {
                if (measure<0) printf("### WARNING (form_coarse_level_ag): negative lambda!\n");
                vec[i]=FGPT; // set i as fine node
                //update the lambda value in the CGPT neighbor of i
                for (ck=Sh.IA[ci];ck<Sh.IA[ci+1];++ck) {
                    cj=Sh.JA[ck];
                    j=cp_index[cj];
                    if (vec[j]!=ISPT) {
                        if (cj<ci) {
                            new_meas=lambda[cj];
                            if (new_meas>0) {
                                remove_node(&LoL_head, &LoL_tail, new_meas, cj, lists, where);
                                num_left--;
                            }
                            new_meas= ++(lambda[cj]);
                            enter_list(&LoL_head, &LoL_tail,  new_meas, cj, lists, where);
                            num_left++;
                        }
                        else{
                            new_meas= ++(lambda[cj]);
                        }//end if cj<ci
                    }//end if vec[j]!=ISPT
                }//end for ck
                
            }//end if
        }
    }
    
    
    // 4. Main loop
    while (num_left>0) {
        // pick $i\in U$ with $\max\lambda_i: C:=C\cup\{i\}, U:=U\\{i\}$
        maxnode=LoL_head->head;
        maxlambda=lambda[maxnode];
        if (maxlambda==0) { printf("head of the list has measure of 0\n");}
        vec[cp_index[maxnode]]=3; // set maxnode as real coarse node, labeled as num 3
        --num_left;
        remove_node(&LoL_head, &LoL_tail, maxlambda, maxnode, lists, where);
        lambda[maxnode]=0;
        col++;//count for the real coarse node after aggressive coarsening
        
        // for all $j\in S_i^T\cap U: F:=F\cup\{j\}, U:=U\backslash\{j\}$
        for (ci=ShT.IA[maxnode];ci<ShT.IA[maxnode+1];++ci) {
            cj=ShT.JA[ci];
            j=cp_index[cj];
            
            /* if j is temporary CGPT */
            if (vec[j]==CGPT) {
                vec[j]=4; // set j as 4--fake CGPT
                remove_node(&LoL_head, &LoL_tail, lambda[cj], cj, lists, where);
                --num_left;
                //update the measure for neighboring points
                for (cl=Sh.IA[cj];cl<Sh.IA[cj+1];cl++) {
                    ck=Sh.JA[cl];
                    k=cp_index[ck];
                    if (vec[k]==CGPT) {// k is temporary CGPT
                        remove_node(&LoL_head, &LoL_tail, lambda[ck], ck, lists, where);
                        new_meas= ++(lambda[ck]);
                        enter_list(&LoL_head, &LoL_tail,new_meas, ck, lists, where);
                    }
                }
            } // if
        } // ci
        for (ci=Sh.IA[maxnode]; ci<Sh.IA[maxnode+1];++ci) {
            cj=Sh.JA[ci];
            j=cp_index[cj];
            if (vec[j]==CGPT) {// j is temporary CGPT
                measure=lambda[cj];
                remove_node(&LoL_head, &LoL_tail, measure, cj, lists, where);
                measure--;
                lambda[cj]=measure;
                if (measure>0) {
                    enter_list(&LoL_head, &LoL_tail,measure, cj, lists, where);
                }
                else {
                    vec[j]=4; // set j as fake CGPT variable
                    --num_left;
                    for (cl=Sh.IA[cj];cl<Sh.IA[cj+1];cl++) {
                        ck=Sh.JA[cl];
                        k=cp_index[ck];
                        if (vec[k]==CGPT) {// k is temporary CGPT
                            remove_node(&LoL_head, &LoL_tail, lambda[ck], ck, lists, where);
                            new_meas= ++(lambda[ck]);
                            enter_list(&LoL_head, &LoL_tail,new_meas, ck, lists, where);
                        }
                    } // end for l
                } // end if
            } // end if
        } // end for
    } // while
    
    //organize the variable type
    // make temporary CGPT--1 and fake CGPT--4 become FGPT
    // make real CGPT--3 to be CGPT
#ifdef _OPENMP
#pragma omp parallel for if(row>OPENMP_HOLDS)
#endif
    for(i=0;i<row;i++) {
        if(vec[i]==CGPT||vec[i]==4)//change all the temporary or fake CGPT into FGPT
            vec[i]=FGPT;
    }
    
#ifdef _OPENMP
#pragma omp parallel for if(row>OPENMP_HOLDS)
#endif
    for(i=0;i<row;i++) {
        if(vec[i]==3)// if i is real CGPT
            vec[i]=CGPT;
    }
    
    /* Coarsening Phase THREE: Find all the FGPTs which
     have not CGPT neighbors within distance of 2.
     Change them into CGPT to make standard interpolation
     work  */
#ifdef _OPENMP
#pragma omp parallel for if(row>OPENMP_HOLDS) private(myid,mybegin,myend,i,flag,j,k,l,m,sub_col)
    for (myid=0; myid<nthreads; myid++) {
        FASP_GET_START_END(myid, nthreads, row, &mybegin, &myend);
        for (i=mybegin; i<myend; i++) {
#else
            for (i=0; i<row; i++) {
#endif
                if (vec[i]==FGPT) {
                    flag=0; //flag for whether there is any CGPT neighbor within distance of 2
                    for (j=S->IA[i]; j<S->IA[i+1]; j++) {
                        k=S->JA[j];
                        if (flag==1) {
                            break;
                        }
                        else if(vec[k]==CGPT)
                        {
                            flag=1;
                            break;
                        }
                        else if(vec[k]==FGPT)
                        {
                            for (l=S->IA[k]; l<S->IA[k+1]; l++) {
                                m=S->JA[l];
                                if (vec[m]==CGPT) {
                                    flag=1;
                                    break;
                                }
                            } //end for l
                        }
                    } //end for j
                    if (flag==0) {
                        vec[i]=CGPT;
#ifdef _OPENMP
                        sub_col++;
#else
                        col++;
#endif
                    }
                } //end if
            } //end for i
#ifdef _OPENMP
#pragma omp critical(col)
            col += sub_col;
        }
#endif
        
        if (LoL_head) {
            list_ptr=LoL_head;
            LoL_head->prev_node=NULL;
            LoL_head->next_node=NULL;
            LoL_head = list_ptr->next_node;
            fasp_mem_free(list_ptr);
        }
        
        fasp_icsr_free(&Sh);
        fasp_icsr_free(&ST);
        fasp_icsr_free(&ShT);
        fasp_mem_free(work);
        
        return col;
    }
    
    
    /*---------------------------------*/
    /*--        End of File          --*/
    /*---------------------------------*/
