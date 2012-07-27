#include <stdlib.h>
#include <string.h>
#include "logh.h"
#include "tuneable.h"
#include "starlist_struct.h"
#include "estuff_struct.h"
#include "search_struct.h"
#include "crudestat_struct.h"
#include "fixpass_struct.h"
#include "galpass_struct.h"
#include "funnypass_struct.h"
#include "trans7_struct.h"
#include "subraster_struct.h"
#include "fitarrays_struct.h"
#include "byvirtue_struct.h"
#include "addobj_struct.h"
#include "model_struct.h"
#include "cast_arr.h"
#include "offpic.h"
#include "toofaint.h"
#include "transmask.h"
#include "empiricals.h" //contains oneemp and oldemp
#include "galaxy.h"
#include "parupd.h" //contains twoupd as well
#include "errupd.h"
#include "fillerup.h"
#include "addstar.h"
#include "guess.h"
#include "pgauss.h" //even if ONESTAR is unique from PGAUSS, will need in !converge case
#include "onefit.h"
#include "twofit.h"
#include "shape.h"

/* dophot subroutine converted to c void function 3-13-2012 */
//onestar is a pointer to the function name ONESTAR7P which returns double
void shape_( double (*ONESTAR_7P)(short int*, float*, float*, int*, int*), double (*TWOSTAR)(short int*, float*, float*, int*, int*), int** BIG, int** NOISE, int* NFAST_ptr, int* NSLOW_ptr )
{

     /* dereference pointers */
     int NFAST  = *NFAST_ptr;
     int NSLOW  = *NSLOW_ptr;

     /* rename used common block variables 
        which AREN'T changed frequently in subroutines */
     int*      IMTYPE = starlist_.IMTYPE;
     float**  STARPAR = starlist_.starpar;
     float**  SHADOW  = starlist_.shadow;
     float**  SHADERR = starlist_.shaderr;
     float**    EMPAR = estuff_.empar;
     short int* EMSUB = estuff_.emsub;
     short int** ADDAREA = addobj_.addarea;
//   not renamed because changed frequently and by subroutines
//   search_.nstot (int)
//   crudestat_.npt (int)
//   galpass_.bigfoot (int/ logical)
//   fixpass_.fixxy (int/ logical)
//   funnypass_.funny (int/ logical)
//   trans7_.test7 (int/ logical)
     short int** XX = subraster_.xx;
     float* Z  = subraster_.z;
     float* YE = subraster_.ye;
     float*  A = fitarrays_.a;
     float* FA = fitarrays_.fa;
     float* C_ptr = fitarrays_.c; //not recasting as only passed
     float*  B = fitarrays_.b;
//     float* FB = fitarrays_.fb;
     float* CHI = byvirtue_.chi;
     int* WHICH_MODEL = model_.which_model;
     int* TESTED      = model_.tested;

     short int* IRECT = tune2_.irect;
     int NIT      = tune4_.nit;
     int NFIT2_7P = tune4_.nfit2;
     float STOGRAT = tune8_.stograt;
     float XTRA   = tune9_.xtra;
     float ENUFF7 = tune10_.enuff7;
     int lverb    = tune14_.lverb;
     float* ACC   = tune15_.acc;
     float* ALIM  = tune15_.alim;
     float* AVA   = tune15_.ava;
     int EMPOK    = tune22_.empok;
     int EMENAB   = tune22_.emenab;

     /* substance of subroutine begins here */
     float** TWOFPAR = malloc_float_2darr(NSMAX, NPMAX);
     int VERYBIG, OFFP, VFAINT, CONVERGE, NOTNUFF, GOTFAINT;
     int PGAUSS_CONVERGE, PGAUSS_CHI;
     int transmask_ret;
     static int IADD =  1;
     static int ISUB = -1;
     static int JADD =  2;
     static int JSUB = -2;
     static int IZERO = 0;

     int NSPREV, I, K; //K is the array subscript index == I-1
     float SKY, GALCHI, STARCHI;
     int JMTYPE;
     int IX, IY;
     int MIX, MIY; //negative IX and IY to pass to fxns as ptrs
     float dum, DX, DY;
     int ITFIT;
     int LAST;
     int index;

     double (*ONESTAR)(short int*, float*, float*, int*, int*);
     int NFIT2;

/*   NEXT WE GO THROUGH PREVIOUSLY IDENTIFIED STARS AND FIT AGAIN */

/*   91-Oct-27: We've introduced jmtype which is the "reduced" imtype of
C:   objects for which the positions are fixed (fixxy = .true.).  At the end
C:   of loop 2757 we increment the jmtype for such objects and assign
C:   its value to imtype(i).  -PLS  */
     trans7_.test7 = 1; //true
     NSPREV = search_.nstot;
     for (I = 1; I <= NSPREV; I++){
          K = I-1;
          if (WHICH_MODEL[K] == 0){ //normal specified model
               ONESTAR = ONESTAR_7P;
               NFIT2   = NFIT2_7P;
          }
          if (WHICH_MODEL[K] == 1){ //pseudogaussian
               ONESTAR = &pgauss2d_;
               NFIT2   = 7;
          }

          funnypass_.funny = 0; //false
          GOTFAINT         = 0; //false
          if (lverb > 20){
               fprintf(logfile,"Determining SHAPE for object No. %d\n", I);
          }

          fixpass_.fixxy = ( (IMTYPE[K] - (IMTYPE[K] % 10)) == 10);
          if (fixpass_.fixxy){
               JMTYPE = IMTYPE[K] - 10;
          }
          else{
               JMTYPE = IMTYPE[K];
          }
          VFAINT = (JMTYPE == 7);
     
          if (SHADOW[K][0] == 0.0f){
               SKY = guess2_(A, STARPAR[K], &IX, &IY);
          }
          else{
               SKY = guess2_(A, SHADOW[K], &IX, &IY);
          } 

          if (VFAINT){
               if (lverb > 20){
                    /* Changed indices. */
                    fprintf(logfile,"TOO FAINT, SKIPPING Object # %d", I);
                    fprintf(logfile," AT %f %f\n",
                                      STARPAR[K][2], STARPAR[K][3]);
               }
          }
          else{
               if ((JMTYPE == 4) || (JMTYPE == 9)){
                    if (lverb > 20){
                         /* Changed loop indices. */
                         fprintf(logfile,"SKIPPING NONCONVERGER.. ");
                         fprintf(logfile,"Obj# %d AT %f %f\n",
                                    I, STARPAR[K][2], STARPAR[K][3]);
                    }
               }
          }

          if ( (!VFAINT)     &&
               (JMTYPE != 4) && (JMTYPE != 6) &&
               (JMTYPE != 8) && (JMTYPE != 9)   ){
               if ((EMSUB[K] >= 1) && (EMPOK) && (EMENAB)){
                    addstar_(&oneemp_, BIG, NOISE,
                             &NFAST, &NSLOW, 
                             EMPAR[K],
                             ADDAREA[K], &JADD,
                             0, " ", 0, " ");
               }
               else{
                    galpass_.bigfoot = (JMTYPE == 2);
                    addstar_(ONESTAR, BIG, NOISE,
                             &NFAST, &NSLOW, 
                             STARPAR[K],
                             ADDAREA[K], &IADD,
                             0, " ", 0, " ");
                    galpass_.bigfoot = 0; //false
               }

               fillerup_(BIG, NOISE, &IX, &IY,
                             &NFAST, &NSLOW);
               NOTNUFF = (crudestat_.npt < (int)(ENUFF7*IRECT[0]*IRECT[1]));
               if (NOTNUFF){  //not enough pts for 7 parameter fit, enough for 4
                    if (lverb > 20){
                         fprintf(logfile,
                         "Obj#, NPTS, IX & IY = %d %d %d %d \n",
                                 I, crudestat_.npt, IX, IY); 
                         fprintf(logfile,
                 ".... SKIPPING STAR: not enough pixels for 7-param fit\n"); 
                    }
                    fprintf(logfile,
                    "Obj#, NPTS, IX & IY = %d %d %d %d \n",
                            I, crudestat_.npt, IX, IY); 
                    fprintf(logfile,
                 ".... SKIPPING STAR: not enough pixels for 7-param fit\n"); 
                    if (JMTYPE != 2){
                         JMTYPE = 5;
                    }
               }

               if (fixpass_.fixxy){
                    transmask_ret = transmask_(BIG, NOISE,
                                          &NFAST, &NSLOW,
                                          &IX, &IY, &SKY, &dum);
                    GOTFAINT = !transmask_ret;
                    if ((GOTFAINT) && (JMTYPE != 3)){
                         JMTYPE = 7;
                    }
               }

               if ((NOTNUFF) || (GOTFAINT)){
                    if (lverb > 20){
                         fprintf(logfile,
                         "Obj#, NPTS, IX & IY = %d %d %d %d \n",
                                 I, crudestat_.npt, IX, IY); 
                         fprintf(logfile,
                         ".... SKIPPING STAR: ought to be type 7\n");
                    }
                    if ((EMSUB[K] >= 1) && (EMPOK) && (EMENAB)){
                         addstar_(&oneemp_, BIG, NOISE,
                                  &NFAST, &NSLOW, 
                                  EMPAR[K],
                                  ADDAREA[K], &JSUB,
                                  0, " ", 0, " ");
                    }
                    else{
                         galpass_.bigfoot = (JMTYPE == 2);
                         addstar_(ONESTAR, BIG, NOISE,
                                  &NFAST, &NSLOW, 
                                  STARPAR[K],
                                  ADDAREA[K], &ISUB,
                                  0, " ", 0, " ");
                         galpass_.bigfoot = 0; //false
                    }
               } //faint and missing pixel objects are now resubtracted from image
               else{
                    if (lverb > 20){
                         fprintf(logfile,
                         "Obj#, #-PTS FIT, X, Y = %d %d %d %d \n",
                                 I, crudestat_.npt, IX, IY); 
                    }
                    ITFIT = NIT;
                    NIT = 2*ITFIT; 
                    //on first pass, fit with standard pseudogaussian model
                    //if it doesn't converge, set as object of type 9
                    //if it does converge, and the model is something else
                    //try the other model using the pseudogaussian parameters
                    //for the base 7 and the average for the extras
                    //if it succeeds, keep it.
                    //if it fails, use the pseudogaussian
                    //only do this for stars not already flagged for the pgauss model
                    if ((WHICH_MODEL[K] == 0) && (TESTED[K] == 0) &&
                        (strncmp(tune16_.flags[0], "PGAUSS", 5) != 0) ){
                         if (lverb > 20){
                              fprintf(logfile, "     fitting PGAUSS model \n");
                         }
                         TESTED[K] += 1; //specifying that the star has now been fit once
                         NFIT2 = 7;
                         PGAUSS_CHI = (float)onefit_(&pgauss2d_, XX, Z, YE, 
                                    &crudestat_.npt, A, FA, C_ptr,
                                    &NFIT2, ACC, ALIM, &NIT);
                         PGAUSS_CONVERGE = (PGAUSS_CHI < 1.0e10f);
                         if (PGAUSS_CONVERGE){ 
                              if (lverb > 20){
                                   fprintf(logfile, "     PGAUSS model CONVERGED \n");
                                   fprintf(logfile, "     trying alternate model \n");
                              }
                              NIT = 3*ITFIT; 
                              NFIT2 = tune4_.nfit2;
                              for (index = 7; index < NFIT2; index++){
                                   A[index]  = AVA[index];
                                   FA[index] = 1.0f;
                              }
                              GALCHI = (float)onefit_(ONESTAR, XX, Z, YE, 
                                    &crudestat_.npt, A, FA, C_ptr,
                                    &NFIT2, ACC, ALIM, &NIT);
                              CONVERGE = (GALCHI < 1.0e10f);
                              if (!CONVERGE){ //go back to pgauss model and keep it
                                   if (lverb > 20){
                                        fprintf(logfile,"Obj# %d at %f %f\n",
                                           I, STARPAR[K][2], STARPAR[K][3]);
                                        fprintf(logfile,"     converged with PGAUSS model, \n");
                                        fprintf(logfile,"     but FAILED TO CONVERGE with alt model \n");
                                        fprintf(logfile,"     using PGAUSS \n");
                                   }
                                   NIT = 2*ITFIT; 
                                   WHICH_MODEL[K] = 1; //specifying pgauss model hence
                                   NFIT2 = 7;
                                   ONESTAR = &pgauss2d_;
                                   SKY = guess2_(A, STARPAR[K], &IX, &IY);
                                   GALCHI = (float)onefit_(ONESTAR, XX, Z, YE, 
                                              &crudestat_.npt, A, FA, C_ptr,
                                              &NFIT2, ACC, ALIM, &NIT);
                                   CONVERGE = (GALCHI < 1.0e10f);
                              }
                              else{ //if the 7+ parameter model did converge keep it
                                   if (lverb > 20){
                                        fprintf(logfile,"Obj# %d at %f %f\n",
                                           I, STARPAR[K][2], STARPAR[K][3]);
                                        fprintf(logfile,"     CONVERGED with PGAUSS model, \n");
                                        fprintf(logfile,"     AND with ALTERNATE model \n");
                                        fprintf(logfile,"     using ALTERNATE \n");
                                   }
                              }
                         }
                         else{ //if PGAUSS didn't converge
                              if (lverb > 20){
                                   fprintf(logfile,"Obj# %d at %f %f\n",
                                           I, STARPAR[K][2], STARPAR[K][3]);
                                   fprintf(logfile,"FAILED to CONVERGE with PGAUSS model \n");
                              }
                              WHICH_MODEL[K] = 1; //specifying pgauss model hence
                                                  //certainly no more complexity is necessary
                              NFIT2 = 7;
                              ONESTAR = &pgauss2d_;
                         }
                    } //end if which model = 0 or if !PGAUSS default model
                    else{ //if which model = 1 or 2 or PGAUSS is the default model
                         GALCHI = (float)onefit_(ONESTAR, XX, Z, YE, 
                               &crudestat_.npt, A, FA, C_ptr,
                               &NFIT2, ACC, ALIM, &NIT);
                         CONVERGE = (GALCHI < 1.0e10f);
                         if (!(CONVERGE)){
                              if (lverb > 20){
                                   fprintf(logfile,"Obj# %d at %f %f\n",
                                      I, STARPAR[K][2], STARPAR[K][3]);
                                   fprintf(logfile,"FAILED TO CONVERGE!\n");
                              }
                         }
                    }

                    // regardless if you tested for the PGAUSS model,
                    // the PGAUSS model converged or didn't or the alt model did
                    // subsequently, update the params and eval VERYBIG and offpic.
                    NIT = ITFIT;
                    parupd_(A, SHADOW[K], &IX, &IY);
                    errupd_(C_ptr, SHADERR[K], &NFIT2);

                    VERYBIG = galaxy_(A, SHADERR[K], STARPAR[K]);
                    if (JMTYPE == 3){
                         VERYBIG = ((VERYBIG) && (CHI[3] > XTRA));
                    }
                    OFFP = offpic_(A, &IX, &IY, &NFAST, &NSLOW, &DX, &DY);
                    VERYBIG = ((VERYBIG) && (!OFFP) && (CONVERGE));
                  
                    /* 91-Oct-27 If the object position is fixed then 
                       we don't test for duplicity.  If it failed to 
                       converge, it is so flagged, and its original 
                       status as type 3 is preserved. */
                    if ((!VERYBIG) || (fixpass_.fixxy)){
                         if (JMTYPE != 3){
                              JMTYPE = 1;
                         }
                         if (!CONVERGE){
                              if (JMTYPE != 3){
                                   JMTYPE = 9;
                              }
                              SHADOW[K][0] = 0.0f;
                         }
                         else{
                              if (OFFP){
                                   JMTYPE = 9;
                                   SHADOW[K][0] = 0.0f;
                                   if (lverb > 20){
                                        /* Changed indices. */
                                        fprintf(logfile,"ABSURD SHAPE VALUES for ");
                                        fprintf(logfile,"Obj# %d at %f %f\n",
                                           I, STARPAR[K][2], STARPAR[K][3]);
                                        fprintf(logfile,
                                           "Fit center outside fit subraster\n");
                                        fprintf(logfile,".... DISCARD SOLUTION!\n");
                                   }
                              }
                         }

                         /* if it was type 2 and is now otherwise, 
                            subtract the analytic PSF */
                         if ((EMSUB[K] >= 1) && (EMPOK) && (EMENAB)){
                              addstar_(&oneemp_, BIG, NOISE,
                                       &NFAST, &NSLOW, 
                                       EMPAR[K],
                                       ADDAREA[K], &JSUB,
                                       0, " ", 0, " ");
                         }
                         else{
                              addstar_(ONESTAR, BIG, NOISE,
                                       &NFAST, &NSLOW, 
                                       STARPAR[K], 
                                       ADDAREA[K], &ISUB,
                                       0, " ", 0, " ");
                         }
                    }
                    else{ //to if !VERYBIG || FIXXY, check for double star or galaxy
                         if (lverb > 20){
                              /* Changed indices. */
                              fprintf(logfile,"Obj# %d at %f %f\n",
                                 I, STARPAR[K][2], STARPAR[K][3]);
                              fprintf(logfile,"  is VERY BIG....\n");
                              fprintf(logfile,"  ..Testing GALAXY vs. DBLE-STAR\n");
                         }
                         MIX = -IX;
                         MIY = -IY;
                         twoupd_(TWOFPAR[K], B, &MIX, &MIY);
                         IX = -MIX;
                         IY = -MIY;
                         STARCHI = (float)twofit_(TWOSTAR, STARPAR[K]);
                         if (STARCHI/GALCHI < STOGRAT){
                              if (lverb > 20){
                                   fprintf(logfile," Result -> A SPLIT STAR: \n");
                                   fprintf(logfile," GAL-CHI: %f  STAR-CHI: %f\n",
                                                     GALCHI, STARCHI);
                              }
                              ONESTAR = ONESTAR_7P; //make sure no longer pgauss
                              NFIT2   = tune4_.nfit2; //make sure no longer pgauss
                              WHICH_MODEL[K] = 0; //make sure no longer pgauss, but retest
                                            //for which model on subsequent passes
                              JMTYPE   = 3;
                              EMSUB[K] = 0;
                              parupd_(B, SHADOW[K],  &IX, &IY); 
                              parupd_(B, STARPAR[K], &IX, &IY); 
                              addstar_(ONESTAR, BIG, NOISE,
                                       &NFAST, &NSLOW, 
                                       STARPAR[K],
                                       ADDAREA[K], &ISUB,
                                       0, " ", 0, " ");
                              search_.nstot += 1;
                              LAST = search_.nstot - 1;
                              IMTYPE[LAST] = 3;
                              EMSUB[LAST] = 0;
                              parupd_(TWOFPAR[LAST], 
                                      TWOFPAR[K], &IZERO, &IZERO); 
                              parupd_((B+NFIT2), STARPAR[LAST], &IX, &IY); 
                              addstar_(ONESTAR, BIG, NOISE,
                                       &NFAST, &NSLOW, 
                                       STARPAR[LAST],
                                       ADDAREA[LAST], &ISUB,
                                       0, " ", 0, " ");
                         }
                         else{
                              if (lverb > 20){
                                   fprintf(logfile," Result -> A GALAXY: \n");
                                   fprintf(logfile," GAL-CHI: %f  STAR-CHI: %f\n",
                                                     GALCHI, STARCHI);
                              }
                              JMTYPE   = 2;
                              EMSUB[K] = 0;
                              parupd_(A,  STARPAR[K], &IX, &IY); 
                              twoupd_(B,  TWOFPAR[K], &IX, &IY);
                              // keep whatever model (ONSTAR_7P or pgauss) had converged earlier)
                              galpass_.bigfoot = 1; //true
                              addstar_(ONESTAR, BIG, NOISE,
                                       &NFAST, &NSLOW, 
                                       STARPAR[K],
                                       ADDAREA[K], &ISUB,
                                       0, " ", 0, " ");
                              galpass_.bigfoot = 0; //false
                         } //end galaxy v double star if/else
                    } //end !VERYBIG || FIXXY if/else
               } //end NOTNUFF || GOTFAINT if/else
          } //end big conditoinal on VFAINT and JMTYPE if
          
          IMTYPE[K] = JMTYPE;
          if (fixpass_.fixxy){
               IMTYPE[K] = JMTYPE + 10;
          }
          fixpass_.fixxy = 0; //false
     } //end I loop
     trans7_.test7 = 0; //false

     /* freeing locally allocated memory and recasting changed pointers */         
     free_float_2darr(NSMAX, TWOFPAR);

}



