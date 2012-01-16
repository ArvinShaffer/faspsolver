/*! \file ordering.c
 *  \brief A collection of ordering, merging, removing duplicated integers functions
 */

#include "fasp.h"

static void dSwapping(REAL *w, INT i, INT j);
static void iSwapping(INT *w, INT i, INT j);

/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/*!
 * \fn INT fasp_aux_unique (INT numbers[], INT size)
 *
 * \brief remove duplicates in an sorted (ascending order) array 
 *
 * \param numbers   pointer to the array needed to be sorted
 * \param size      length of the target array
 * \return          newsize after removing duplicates
 *
 * \note Does not use any extra or temprary storage. Use the original array for returning!
 *
 * \author Chensong Zhang
 * \date 11/21/2010
 */
INT fasp_aux_unique (INT numbers[], 
                     INT size)
{
	INT i, newsize; 
	
	if (size==0) return(0);
	
	for (newsize=0, i=1; i<size; ++i) {
		if (numbers[newsize] < numbers[i]) {
			newsize++;
			numbers[newsize] = numbers[i];
		}
	}
	
	return(newsize+1);
}

/*!
 * \fn void fasp_aux_merge(INT numbers[], INT work[], INT left, INT mid, INT right)
 *
 * \brief merge two sorted arraies
 *
 * \param numbers   pointer to the array needed to be sorted
 * \param work      pointer to the work array with same size as numbers
 * \param left      the starting index of array 1
 * \param mid       the starting index of array 2
 * \param right     the ending index of array 1 and 2
 * 
 * \note Both arraies are stored in numbers! Arraies should be pre-sorted!
 *
 * \author Chensong Zhang
 * \date 11/21/2010
 */
void fasp_aux_merge (INT numbers[], 
                     INT work[], 
                     INT left, 
                     INT mid, 
                     INT right)
{	
	INT i, left_end, num_elements, tmp_pos;
	
	left_end = mid - 1;	
	tmp_pos = left;	
	num_elements = right - left + 1;	
	
	while ((left <= left_end) && (mid <= right)) {		
		
		if (numbers[left] <= numbers[mid]) // first branch <=
		{			
			work[tmp_pos] = numbers[left];
			tmp_pos = tmp_pos + 1;
			left = left +1;
		}
		else // second branch >
		{			
			work[tmp_pos] = numbers[mid];			
			tmp_pos = tmp_pos + 1;			
			mid = mid + 1;			
		}		
	}		
	
	while (left <= left_end) {		
		work[tmp_pos] = numbers[left];		
		left = left + 1;		
		tmp_pos = tmp_pos + 1;		
	}
	
	while (mid <= right) {		
		work[tmp_pos] = numbers[mid];		
		mid = mid + 1;
		tmp_pos = tmp_pos + 1;		
	}
	
	for (i = 0; i < num_elements; ++i) {		
		numbers[right] = work[right];		
		right = right - 1;		
	}
	
}

/*!
 * \fn void fasp_aux_msort(INT numbers[], INT work[], INT left, INT right)
 *
 * \brief sort the INT array ascendingly with the merge sort algorithm
 *
 * \param numbers   pointer to the array needed to be sorted
 * \param work      pointer to the work array with same size as numbers
 * \param left      the starting index
 * \param right     the ending index
 *
 * \note 'left' and 'right' are usually set to be 0 and n-1, respectively
 *
 * \author Chensong Zhang
 * \date 11/21/2010
 */
void fasp_aux_msort (INT numbers[], 
                     INT work[], 
                     INT left, 
                     INT right)
{
	INT mid;
	
	if (right > left) {		
		mid = (right + left) / 2;		
		fasp_aux_msort(numbers, work, left, mid);
		fasp_aux_msort(numbers, work, mid+1, right);				
		fasp_aux_merge(numbers, work, left, mid+1, right);		
	}
	
}

/*!
 * \fn void fasp_aux_iQuickSort(INT *a, INT left, INT right)
 *
 * \brief sort the array 'a'(INT type) ascendingly with the quick sorting algorithm
 *
 * \param *a     pointer to the array needed to be sorted
 * \param left   the starting index
 * \param right  the ending index
 *
 * \note 'left' and 'right' are usually set to be 0 and n-1,respectively,where n is the 
 *        length of 'a'.  
 *
 * \author Zhiyang Zhou
 * \date 2009/11/28 
 */
void fasp_aux_iQuickSort(INT *a, 
                         INT left, 
                         INT right)
{
	INT i, last;
	
	if (left >= right) return;
	
	iSwapping(a, left, (left+right)/2);
	
	last = left;
	for (i = left+1; i <= right; ++i) {
		if (a[i] < a[left]) {
			iSwapping(a, ++last, i);
		}
	}
	
	iSwapping(a, left, last);
	
	fasp_aux_iQuickSort(a, left, last-1);
	fasp_aux_iQuickSort(a, last+1, right);
}

/*!
 * \fn void fasp_aux_dQuickSort(REAL *a, INT left, INT right)
 *
 * \brief sort the array 'a'(REAL type) ascendingly with the quick sorting algorithm
 *
 * \param *a     pointer to the array needed to be sorted
 * \param left   the starting index
 * \param right  the ending index
 *
 * \note 'left' and 'right' are usually set to be 0 and n-1,respectively,where n is the 
 *       length of 'a'.  
 *
 * \author Zhiyang Zhou
 * \date 2009/11/28 
 */
void fasp_aux_dQuickSort (REAL *a, 
                          INT left, 
                          INT right)
{
	INT i, last;
	
	if (left >= right) return;
	
	dSwapping(a, left, (left+right)/2);
	
	last = left;
	for (i = left+1; i <= right; ++i) {
		if (a[i] < a[left]) {
			dSwapping(a, ++last, i);
		}
	}
	
	dSwapping(a, left, last);
	
	fasp_aux_dQuickSort(a, left, last-1);
	fasp_aux_dQuickSort(a, last+1, right);
}

/*!
 * \fn void fasp_aux_iQuickSortIndex(INT *a, INT left, INT right, INT *index)
 *
 * \brief reorder the index of 'a'(INT type) so that 'a' is in ascending order  
 *
 * \param *a      pointer to the array 
 * \param left    the starting index
 * \param right   the ending index
 * \param *index  the index of 'a'
 *
 * \note 'left' and 'right' are usually set to be 0 and n-1,respectively,where n is the 
 *       length of 'a'. 'index' should be initialized in the nature order and it has the
 *       same length as 'a'.   
 *
 * \author Zhiyang Zhou
 * \date 2009/12/02 
 */
void fasp_aux_iQuickSortIndex (INT *a, 
                               INT left, 
                               INT right, 
                               INT *index)
{
	INT i, last;
	
	if (left >= right) return;
	
	iSwapping(index, left, (left+right)/2);
	
	last = left;
	for (i = left+1; i <= right; ++i) {
		if (a[index[i]] < a[index[left]]) {
			iSwapping(index, ++last, i);
		}
	}
	
	iSwapping(index, left, last);
	
	fasp_aux_iQuickSortIndex(a, left, last-1, index);
	fasp_aux_iQuickSortIndex(a, last+1, right, index);
}

/*!
 * \fn void fasp_aux_dQuickSortIndex(REAL *a, INT left, INT right, INT *index)
 *
 * \brief reorder the index of 'a'(REAL type) so that 'a' is ascending in such order  
 *
 * \param *a      pointer to the array 
 * \param left    the starting index
 * \param right   the ending index
 * \param *index  the index of 'a'
 *
 * \note 'left' and 'right' are usually set to be 0 and n-1,respectively,where n is the 
 *       length of 'a'. 'index' should be initialized in the nature order and it has the
 *       same length as 'a'. 
 *
 * \author Zhiyang Zhou
 * \date 2009/12/02 
 */
void fasp_aux_dQuickSortIndex (REAL *a, 
                               INT left, 
                               INT right, 
                               INT *index)
{
	INT i, last;
	
	if (left >= right) return;
	
	iSwapping(index, left, (left+right)/2);
	
	last = left;
	for (i = left+1; i <= right; ++i) {
		if (a[index[i]] < a[index[left]]) {
			iSwapping(index, ++last, i);
		}
	}
	
	iSwapping(index, left, last);
	
	fasp_aux_dQuickSortIndex(a, left, last-1, index);
	fasp_aux_dQuickSortIndex(a, last+1, right, index);
}

/*---------------------------------*/
/*--      Private Functions       --*/
/*---------------------------------*/

/**
 * \fn static void iSwapping (INT *w, INT i, INT j)
 *
 * \brief swap the i-th and j-th element in the array 'w' (INT type)
 *
 * \param *w   pointer to the array
 * \param i    one position in w
 * \param j    the other position in w  
 *
 * \author Zhiyang Zhou
 * \date 2009/11/28 
 */
static void iSwapping (INT *w, 
                       INT i, 
                       INT j)
{
	INT temp = w[i];
	w[i] = w[j];
	w[j] = temp;
}

/**
 * \fn static void dSwapping (REAL *w, INT i, INT j)
 *
 * \brief swap the i-th and j-th element in the array 'w' (REAL type)
 *
 * \param *w   pointer to the array
 * \param i    one position in w
 * \param j    the other position in w  
 *
 * \author Zhiyang Zhou 
 * \date 2009/11/28 
 */
static void dSwapping (REAL *w, 
                       INT i, 
                       INT j)
{
	REAL temp = w[i];
	w[i] = w[j];
	w[j] = temp;
}

/*---------------------------------*/
/*--        End of File          --*/
/*---------------------------------*/
