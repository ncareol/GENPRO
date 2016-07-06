C** COMMON BLOCKS TO BE USED WITH THE HEADER AND DATA FILES FOR THE DAP **
C*      Copyright University Corporation for Atmospheric Research, 1995   *
C  <861105.1604>
C    Revised by RLR to add integer declarations   <880225.1133> **
C    Revised by RLR to add SAVE declarations      <880328.1300> **
C 
      INTEGER NVARSZ
      PARAMETER (NVARSZ=1000)
      COMMON / HEAD1 / NMUSER(1),IDATEF(3),ITIMEF(3),NOTES    
      COMMON / HEAD1 / IDATED(3),IPROJ,IARCFT,IFLGHT
      COMMON / HEAD1 / NMRECS,NWORDS,IDELTT,NTMSEG
      COMMON / HEAD1 / NVAR,NVOLT,NAMES(NVARSZ),UNITS(NVARSZ) 
      COMMON / HEAD1 / QCX,PSX,TTX,AVANE,BVANE,DPX
      CHARACTER*6 QCX,PSX,TTX,AVANE,BVANE,DPX 
      CHARACTER*16 NMUSER 
c....... this changed March 94 from 6 to 11, new Xanadu format.
c..      Change back to use with old DAP routines...
c..      This now permits full 8-character GENPRO variables
      CHARACTER*11 NAMES 
      CHARACTER*4 UNITS 
      CHARACTER*2 IDATEF,ITIMEF 
      CHARACTER*4 IARCFT
      CHARACTER*80 NOTES
      INTEGER NMRECS
      INTEGER IDATED,IPROJ,IFLGHT,NWORDS,IDELTT,NTMSEG,NVAR,NVOLT 
C 
C     NMUSER   => NAME OF THE USER (ASCII)
C     IDATEF   => DATE OF LAST FILE ACCESS (ASCII)
C     ITIMEF   => TIME OF LAST FILE ACCESS (ASCII)
C     NOTES    => USER NOTE SPACE 
C     IDATED   => DATE DATA WAS TAKEN (INTEGER) 
C     IPROJ    => PROJECT NUMBER
C     IARCFT   => AIRCRAFT NUMBER (ASCII) 
C     IFLGHT   => FLIGHT NUMBER (INTEGER) 
C     NMRECS   => NUMBER OF RECORDS IN DATA FILE  (2 WORD INTEGER)
C     NWORDS   => NUMBER OF WORDS IN A DATA FILE RECORD 
C     IDELTT   => TIME BETWEEN SAMPLES IN THE DATA FILE (MILLISECONDS)
C     NTMSEG   => TOTAL NUMBER OF TIME SEGMENTS IN THE DATA FILE
C     NVAR     => TOTAL NUMBER OF VARIABLES 
C     NVOLT    => NUMBER OF MODE 1 (VOLTAGES) VARIABLES IN THE FILE 
C     NAMES    => 6 CHARACTER ARRAY OF VARIABLE NAMES 
C     UNITS    => 4 CHAR ARRAY CONTAINING ASCII UNITS OF EACH VARIABLE
C     QCX      => 6 CHARACTER NAME OF QC FOR COMPUTING TAS,WINDS
C     PSX      => 6 CHARACTER NAME OF STATIC PRESS. FOR TAS,WINDS 
C     TTX      => 6 CHARACTER NAME OF TEMPERATURE FOR TAS,WINDS 
C     AVANE    => 6 CHARACTER NAME OF ATTACK VANE FOR WINDS 
C     BVANE    => 6 CHARACTER NAME OF SIDESLIP VANE FOR WINDS 
C     DPX      => 6 CHARACTER NAME OF DEW POINT USED FOR HUMIDITY 
C 
      COMMON / HEAD2 / ITMSEG(6,50) 
      INTEGER ITMSEG
C 
C     ITMSEG   => START TIME OF SEGMENT #N IN ISEGTM(1,N),(2,N),(3,N) 
C                 END TIME OF SEGMENT #N IN ISEGTM(4,N),(5,N),(6,N) 
C 
      COMMON / DATA / IHR,IMIN,ISEC,IMSEC,VALUES(NVARSZ) 
      INTEGER IHR,IMIN,ISEC,IMSEC 
      REAL VALUES 
C 
C     IHR      => TIME OF RECORD HOURS
C     IMIN     => TIME OF RECORD MINUTES
C     ISEC     => TIME OF RECORD SECONDS
C     IMSEC    => TIME OF RECORD MILLI-SECONDS
C     VALUES   => VALUES OF EACH VARIABLE IN HP FLOATING POINT
C 
      COMMON / IFILE / NMHEAD,NMDATA,IOPNFI
      LOGICAL IOPNFI
      CHARACTER*6 NMHEAD,NMDATA
C 
C     IDCB     => DATA CONTROL BUFFER FOR FMP CALLS 
C     NMHEAD   => NAME OF THE HEADER FILE Hxxxxx
C     NMDATA   => NAME OF THE DATA FILE Dxxxxx
C     IOPNFI   => DCB IS OPEN, IF .TRUE.
C 
      SAVE /HEAD1/,/HEAD2/,/DATA/,/IFILE/ 
C
C