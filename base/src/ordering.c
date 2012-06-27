/*! \file ordering.c
 *  \brief A collection of ordering, merging, removing duplicated integers functions
 */

#include "fasp.h"

static void dSwapping(REAL *w, INT i, INT j);
static void iSwapping(INT *w, INT i, INT j);

/*---------------------------------*/
/*--      Public Functions       --*/
/*---------------------------------*/

/**
 * \fn INT fasp_BinarySearch (INT *list, INT value, INT list_length)
 * 
 * \brief Binary Search
 *
 * \param list   Pointer to a set of values
 * \param value  The target 
 * \param list_length Length of the array list
 *
 * \return  The location of value in array list if successed, otherwise, return -1.
 *
 * \author Chunsheng Feng
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

/*!
 * \fn INT fasp_aux_unique (INT numbers[], INT size)
 *
 * \brief Remove duplicates in an sorted (ascending order) array 
 *
 * \param numbers   Pointer to the array needed to be sorted (in/out)
 * \param size      Length of the target array
 *
 * \return          New size after removing duplicates
 *
 * \author Chensong Zhang
 * \date   11/21/2010
 *
 * \note Operation is in place. Does not use any extra or temprary storage.
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
 * \fn void fasp_aux_merge (INT numbers[], INT work[], INT left, INT mid, INT right)
 *
 * \brief Merge two sorted arraies
 *
 * \param numbers   Pointer to the array needed to be sorted
 * \param work      Pointer to the work array with same size as numbers
 * \param left      Starting index of array 1
 * \param mid       Starting index of array 2
 * \param right     Ending index of array 1 and 2
 *
 * \author Chensong Zhang
 * \date   11/21/2010
 * 
 * \note Both arraies are stored in numbers! Arraies should be pre-sorted!
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
 * \fn void fasp_aux_msort (INT numbers[], INT work[], INT left, INT right)
 *
 * \brief Sort the INT array ascendingly with the merge sort algorithm
 *
 * \param numbers   Pointer to the array needed to be sorted
 * \param work      Pointer to the work array with same size as numbers
 * \param left      Starting index
 * \param right     Ending index
 *
 * \author Chensong Zhang
 * \date   11/21/2010
 *
 * \note 'left' and 'right' are usually set to be 0 and n-1, respectively
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
 * \fn void fasp_aux_iQuickSort (INT *a, INT left, INT right)
 *
 * \brief Sort the array (INT type) ascendingly with the quick sorting algorithm
 *
 * \param a      Pointer to the array needed to be sorted
 * \param left   Starting index
 * \param right  Ending index
 *
 * \author Zhiyang Zhou
 * \date   11/28/2009 
 *
 * \note 'left' and 'right' are usually set to be 0 and n-1, respectively
 *        where n is the length of 'a'.  
 */
void fasp_aux_iQuickSort (INT *a, 
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
 * \fn void fasp_aux_dQuickSort (REAL *a, INT left, INT right)
 *
 * \brief Sort the array (REAL type) ascendingly with the quick sorting algorithm
 *
 * \param a      Pointer to the array needed to be sorted
 * \param left   Starting index
 * \param right  Ending index
 *
 * \author Zhiyang Zhou
 * \date   2009/11/28 
 *
 * \note 'left' and 'right' are usually set to be 0 and n-1, respectively
 *        where n is the length of 'a'.  
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
 * \fn void fasp_aux_iQuickSortIndex (INT *a, INT left, INT right, INT *index)
 *
 * \brief Reorder the index of (INT type) so that 'a' is in ascending order  
 *
 * \param a       Pointer to the array 
 * \param left    Starting index
 * \param right   Ending index
 * \param index   Index of 'a' (out)
 *
 * \author Zhiyang Zhou
 * \date   2009/12/02 
 *
 * \note 'left' and 'right' are usually set to be 0 and n-1,respectively,where n is the 
 *       length of 'a'. 'index' should be initialized in the nature order and it has the
 *       same length as 'a'.   
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
 * \brief Reorder the index of (REAL type) so that 'a' is ascending in such order  
 *
 * \param a       Pointer to the array 
 * \param left    Starting index
 * \param right   Ending index
 * \param index   Index of 'a' (out)
 *
 * \author Zhiyang Zhou
 * \date   2009/12/02 
 *
 * \note 'left' and 'right' are usually set to be 0 and n-1,respectively,where n is the 
 *       length of 'a'. 'index' should be initialized in the nature order and it has the
 *       same length as 'a'. 
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
 * \param w    Pointer to the array
 * \param i    One entry in w
 * \param j    Another entry in w  
 *
 * \author Zhiyang Zhou
 * \date   2009/11/28 
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
 * \param w    Pointer to the array
 * \param i    One entry in w
 * \param j    Another entry in w  
 *
 * \author Zhiyang Zhou 
 * \date   2009/11/28 
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
