# generate prototypes for Samba C code
# originally writen by A. Tridge, June 1996
# this modified version was taken from rsync 2.4.6

# removed some things that I do not need in SiPSMG ( ltz, 2009 )
# modified by Chensong Zhang for FASP ( 04/04/2010 )
# modified by Chensong Zhang for FASP+Doxygen ( 05/22/2010 )
# modified by Xiaozhe Hu for BSR format ( 10/26/2010 )
# modified by Chensong Zhang for the new format ( 03/03/2011 )
# modified by Xiaozhe Hu for CUDA ( 09/20/2011 )
# modified by Chensong Zhang to fix the CPP bug in VC++ ( 12/13/2011 )

BEGIN {
  inheader=0;
  print "/*******************************************************************/  "
  print "/* WARNING: DO NOT EDIT THIS FILE!!!                               */  "
  print "/* This header file is automatically generated with \"make headers\" */" 
  print "/*******************************************************************/\n"
  print "/*! \\file " name
  print " *  \\brief Function decroration for the FASP package"
  print " */ \n"
  print "#include \"fasp.h\" "
  print "#include \"fasp_block.h\" "
  #print "#ifdef __cplusplus"
  #print "extern \"C\" { "
  #print "#endif" 
}

{
  if (inheader) {
    if (match($0,"[)][ \t]*$")) {
      inheader = 0;
      printf "%s;\n\n",$0;
    } else {
      printf "%s\n",$0;
    }
    next;
  }
}

/\/*! \\file/ {
    printf "\n/*-------- In file: %s --------*/\n\n",$3;  
}

/^static|^extern/ || !/^[a-zA-Z]/ || /[;]/ {
  next;
}

!/^INT|^SHORT|^LONG|^REAL|^FILE|^OFF_T|^size_t|^off_t|^pid_t|^unsigned|^mode_t|^DIR|^user|^int|^short|^long|^char|^uint|^struct|^BOOL|^void|^double|^time|^dCSRmat|^dCOOmat|^dvector|^iCSRmat|^ivector|^AMG_data|^ILU_data|^dSTRmat|^dBSRmat|^dCSRLmat|^precond|^cudvector|^cuivector|^cudCSRmat/ {
  next;
}

/[(].*[)][ \t]*$/ {
    printf "%s;\n\n",$0;
    next;
}

/[(]/ {
  inheader=1;
  printf "%s\n",$0;
  next;
}

END {
  #print "#ifdef __cplusplus"
  #print "} "
  #print "#endif"
  print "/* End of " name " */"
}
