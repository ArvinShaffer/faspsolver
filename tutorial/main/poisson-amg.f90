!
!		First example for FASP: F90 version
!
!       Solving the Poisson equation (P1 FEM) with AMG
! 
!------------------------------------------------------
!
!		Created by Chensong Zhang on 12/21/2011.
!
!------------------------------------------------------
!

!> \file testf.f90
!> \brief Test function for F90 interfaces

program test

  implicit none

  double precision, dimension(:), allocatable :: u,b
  double precision, dimension(:), allocatable :: a
  integer,          dimension(:), allocatable :: ia,ja

  integer          :: iufile, n, nnz, i, prt_lvl, maxit, stype
  double precision :: tol

  ! Step 0: user defined variables
  stype = 2  
  prt_lvl = 2
  maxit = 100
  tol = 1.0d-8
  iufile = 1

  ! Step 1: read A and b 

  !===> Read data A from file

  open(unit=iufile,file='data/matFD.dat')

  read(iufile,*) n

  allocate(ia(1:n+1))   
  read(iufile,*) (ia(i),i=1,n+1)   
  nnz=ia(n+1)-ia(1)

  allocate(ja(1:nnz),a(1:nnz))
  read(iufile,*) (ja(i),i=1,nnz)
  read(iufile,*) (a(i),i=1,nnz)   

  close(iufile)

  !===> Read data b from file

  open(unit=iufile,file='data/rhsFD.dat')

  read(iufile,*) n
  allocate(b(1:n))
  read(iufile,*) (b(i),i=1,n)
  close(iufile)

  !===> Shift the index to start from 0 (for C routines)
  forall (i=1:n+1) ia(i)=ia(i)-1
  forall (i=1:nnz) ja(i)=ja(i)-1

  ! Step 2: Solve the system 

  !===> Initial guess
  allocate(u(1:n))
  u=0.0d0 

  if (stype == 1) then 
     call fasp_fwrapper_amg(n,nnz,ia,ja,a,b,u,tol,maxit,prt_lvl);
  else if (stype == 2) then 
     call fasp_fwrapper_krylov_amg(n,nnz,ia,ja,a,b,u,tol,maxit,prt_lvl);
  else
     write(*,*) 'Wrong solver type!'
  end if

  ! Step 3: Clean up memory
  deallocate(ia,ja,a)
  deallocate(b,u)

end program test

!/*---------------------------------*/
!/*--        End of File          --*/
!/*---------------------------------*/