      PARAMETER (NUMVARS = 1000)
      COMMON /GENBLK/ ILGBIT,IDATLG,IRATE(NUMVARS),IPOS(NUMVARS),
     &                KEY(NUMVARS), KEYSCL(NUMVARS),
     &                TERM(NUMVARS),FACTR(NUMVARS),NVARS,NLOGRC,
     &                MTLU,MDFY,QUIT,IDATSIZ,strttm,DEVNAME,FLNAME
      COMMON /TAPCTL/ IFD,BSEC,ESEC,ifmtd,BRFLG,HDENS
      COMMON /DAPBLK/ VNAME(NUMVARS),UNITSG(NUMVARS),IND(NUMVARS)
      CHARACTER*11 VNAME 
      CHARACTER*4 UNITSG
      CHARACTER*3 DEVNAME
      INTEGER*4 ILGBIT,IDATSIZ
      LOGICAL MDFY,QUIT,BRFLG,HDENS
c....... wac:
      CHARACTER*120 FLNAME