/**
 *  @file vcs_MultiPhaseEquil.cpp
 *    Driver routine for the VCSnonideal equilibrium solver package
 */
/*
 * $Id$
 */
/*
 * Copywrite (2006) Sandia Corporation. Under the terms of
 * Contract DE-AC04-94AL85000 with Sandia Corporation, the
 * U.S. Government retains certain rights in this software.
 */

#include "vcs_MultiPhaseEquil.h"
#include "vcs_prob.h"
#include "vcs_internal.h"
#include "vcs_VolPhase.h"
#include "vcs_species_thermo.h"
#include "vcs_SpeciesProperties.h"
#include "vcs_VolPhase.h"

#include "vcs_solve.h"

#include "ct_defs.h"
#include "mix_defs.h"
#include "clockWC.h"
#include "ThermoPhase.h"
#include "speciesThermoTypes.h"
#ifdef WITH_IDEAL_SOLUTIONS
#include "IdealSolidSolnPhase.h"
#endif
#ifdef WITH_ELECTROLYTES
#include "IdealMolalSoln.h"
#endif
#include "ChemEquil.h"

#include <string>
#include <vector>

using namespace Cantera;
using namespace std;
//using namespace VCSnonideal;

namespace VCSnonideal {


  vcs_MultiPhaseEquil::vcs_MultiPhaseEquil() :
    m_vprob(0),
    m_mix(0),
    m_printLvl(0),
    m_vsolvePtr(0)
  {
  }

  vcs_MultiPhaseEquil::vcs_MultiPhaseEquil(mix_t* mix, int printLvl) :
    m_vprob(0),
    m_mix(0),
    m_printLvl(printLvl),
    m_vsolvePtr(0)
  {
    // Debugging level
  
    int nsp = mix->nSpecies();
    int nel = mix->nElements();
    int nph = mix->nPhases();
    
    /*
     * Create a VCS_PROB object that describes the equilibrium problem.
     * The constructor just mallocs the necessary objects and sizes them.
     */
    m_vprob = new VCS_PROB(nsp, nel, nph);
    m_mix = mix;
    m_vprob->m_printLvl = m_printLvl;
    /*
     *  Work out the details of the VCS_VPROB construction and
     *  Transfer the current problem to VCS_PROB object
     */
    int res = vcs_Cantera_to_vprob(mix, m_vprob);
    if (res != 0) {
      plogf("problems\n");
   }
  }

  vcs_MultiPhaseEquil::~vcs_MultiPhaseEquil() {
    delete m_vprob;
    m_vprob = 0;
    if (m_vsolvePtr) {
      delete m_vsolvePtr;
      m_vsolvePtr = 0;
    }
  }

  int vcs_MultiPhaseEquil::equilibrate_TV(int XY, doublereal xtarget,
					  int estimateEquil,
					  int printLvl, doublereal err, 
					  int maxsteps, int loglevel) {

    addLogEntry("problem type","fixed T, V");
    //            doublereal dt = 1.0e3;
    doublereal Vtarget = m_mix->volume();
    doublereal dVdP;
    if ((XY != TV) && (XY != HV) && (XY != UV) && (XY != SV)) {
      throw CanteraError("vcs_MultiPhaseEquil::equilibrate_TV",
			 "Wrong XY flag:" + int2str(XY));
    }
    int maxiter = 100;
    int iSuccess = 0;
    int innerXY;
    double Pnow;
    if (XY == TV) {
      m_mix->setTemperature(xtarget);
    }
    double Pnew;
    int strt = estimateEquil;
    double P1 = 0.0;
    double V1 = 0.0;
    double V2 = 0.0;
    double P2 = 0.0;
    doublereal Tlow = 0.5 * m_mix->minTemp();;
    doublereal Thigh = 2.0 * m_mix->maxTemp();
    doublereal Vnow, Verr;
    int printLvlSub = MAX(0, printLvl - 1);
    for (int n = 0; n < maxiter; n++) {
      Pnow = m_mix->pressure();
  
      beginLogGroup("iteration "+int2str(n));
      switch(XY) {
      case TV:
	iSuccess = equilibrate_TP(strt, printLvlSub, err, maxsteps, loglevel); 
	break;
      case HV:
	innerXY = HP;
	iSuccess = equilibrate_HP(xtarget, innerXY, Tlow, Thigh, strt, 
				  printLvlSub, err, maxsteps, loglevel); 
	break;
      case UV:
	innerXY = UP;
	iSuccess = equilibrate_HP(xtarget, innerXY, Tlow, Thigh, strt, 
				  printLvlSub, err, maxsteps, loglevel); 
	break;
      case SV:
	innerXY = SP;
	iSuccess = equilibrate_SP(xtarget, Tlow, Thigh, strt, 
				  printLvlSub, err, maxsteps, loglevel); 
	break;
      default:
	break;
      }
      strt = false;
      Vnow = m_mix->volume();
      if (n == 0) {
	V2 = Vnow;
	P2 = Pnow;
      } else if (n == 1) {
	V1 = Vnow;
	P1 = Pnow;
      } else {
	P2 = P1;
	V2 = V1;
	P1 = Pnow;
	V1 = Vnow;
      }
      
      Verr = fabs((Vtarget - Vnow)/Vtarget);
      addLogEntry("P",fp2str(Pnow));
      addLogEntry("V rel error",fp2str(Verr));
      endLogGroup();
                
      if (Verr < err) {
	addLogEntry("P iterations",int2str(n));
	addLogEntry("Final P",fp2str(Pnow));
	addLogEntry("V rel error",fp2str(Verr));
	goto done;
      }
      // find dV/dP
      if (n > 1) {
	dVdP = (V2 - V1) / (P2 - P1);
	if (dVdP == 0.0) {
	  throw CanteraError("vcs_MultiPhase::equilibrate_TV",
			     "dVdP == 0.0");
	} else {
	  Pnew = Pnow + (Vtarget - Vnow) / dVdP; 
	  if (Pnew < 0.2 * Pnow) {
	    Pnew = 0.2 * Pnow;
	  }
	  if (Pnew > 3.0 * Pnow) {
	    Pnew = 3.0 * Pnow;
	  }
	}

      } else {
	m_mix->setPressure(Pnow*1.01);
	dVdP = (m_mix->volume() - Vnow)/(0.01*Pnow);
	Pnew = Pnow + 0.5*(Vtarget - Vnow)/dVdP;
	if (Pnew < 0.5* Pnow) {
	  Pnew = 0.5 * Pnow;
	}
	if (Pnew > 1.7 * Pnow) {
	  Pnew = 1.7 * Pnow;
	}

      }
      m_mix->setPressure(Pnew);
    }
    throw CanteraError("vcs_MultiPhase::equilibrate_TV",
		       "No convergence for V");

  done:;
    return iSuccess;
  }


  int vcs_MultiPhaseEquil::equilibrate_HP(doublereal Htarget, 
					  int XY, double Tlow, double Thigh,
					  int estimateEquil,
					  int printLvl, doublereal err, 
					  int maxsteps, int loglevel) {
    int maxiter = 100;
    int iSuccess;
    if (XY != HP && XY != UP) {
      throw CanteraError("vcs_MultiPhaseEquil::equilibrate_HP",
			 "Wrong XP" + XY);
    }
    int strt = estimateEquil;
  
    // Lower bound on T. This will change as we progress in the calculation
    if (Tlow <= 0.0) {
      Tlow = 0.5 * m_mix->minTemp();
    }    
      // Upper bound on T. This will change as we progress in the calculation
    if (Thigh <= 0.0 || Thigh > 1.0E6) { 
      Thigh = 2.0 * m_mix->maxTemp();
    }
    addLogEntry("problem type","fixed H,P");
    addLogEntry("H target",fp2str(Htarget));

    doublereal cpb = 1.0, dT, dTa, dTmax, Tnew;
    doublereal Hnow;
    doublereal Hlow = Undef;
    doublereal Hhigh = Undef;
    doublereal Herr, HConvErr;
    doublereal Tnow = m_mix->temperature();
    int printLvlSub = MAX(printLvl - 1, 0);

    for (int n = 0; n < maxiter; n++) {


      // start with a loose error tolerance, but tighten it as we get 
      // close to the final temperature
      beginLogGroup("iteration "+int2str(n));

      try {
	Tnow = m_mix->temperature();
	iSuccess = equilibrate_TP(strt, printLvlSub, err, maxsteps, loglevel); 
	strt = 0;
	if (XY == UP) {
	  Hnow = m_mix->IntEnergy();
	} else {
	  Hnow = m_mix->enthalpy();
	}
	double pmoles[10];
	pmoles[0] = m_mix->phaseMoles(0);
	double Tmoles = pmoles[0];
	double HperMole = Hnow/Tmoles;
	if (printLvl > 0) {
	  plogf("T = %g, Hnow = %g ,Tmoles = %g,  HperMole = %g",
		Tnow, Hnow, Tmoles, HperMole);
	  plogendl();
	}

	// the equilibrium enthalpy monotonically increases with T; 
	// if the current value is below the target, then we know the
	// current temperature is too low. Set the lower bounds.


	if (Hnow < Htarget) {
	  if (Tnow > Tlow) {
	    Tlow = Tnow;
	    Hlow = Hnow;
	  }
	}
	// the current enthalpy is greater than the target; therefore the
	// current temperature is too high. Set the high bounds.
	else {
	  if (Tnow < Thigh) {
	    Thigh = Tnow;
	    Hhigh = Hnow;
	  }
	}
	if (Hlow != Undef && Hhigh != Undef) {
	  cpb = (Hhigh - Hlow)/(Thigh - Tlow);
	  dT = (Htarget - Hnow)/cpb;
	  dTa = fabs(dT);
	  dTmax = 0.5*fabs(Thigh - Tlow);
	  if (dTa > dTmax) dT *= dTmax/dTa;  
	}
	else {
	  Tnew = sqrt(Tlow*Thigh);
	  dT = Tnew - Tnow;
	  if (dT < -200.) dT = 200;
	  if (dT > 200.) dT = 200.;
	}
	double acpb = MAX(fabs(cpb), 1.0E-6);
	double denom = MAX(fabs(Htarget), acpb);
	Herr = Htarget - Hnow;
	HConvErr = fabs((Herr)/denom);
	addLogEntry("T",fp2str(m_mix->temperature()));
	addLogEntry("H",fp2str(Hnow));
	addLogEntry("Herr",fp2str(Herr));
	addLogEntry("H rel error",fp2str(HConvErr));
	addLogEntry("lower T bound",fp2str(Tlow));
	addLogEntry("upper T bound",fp2str(Thigh));
	endLogGroup(); // iteration
	if (printLvl > 0) {
	  plogf("   equilibrate_HP: It = %d, Tcurr  = %g Hcurr = %g, Htarget = %g\n",
		 n, Tnow, Hnow, Htarget);
	  plogf("                   H rel error = %g, cp = %g, HConvErr = %g\n", 
		 Herr, cpb, HConvErr);
	}

	if (HConvErr < err) { // || dTa < 1.0e-4) {
	  addLogEntry("T iterations",int2str(n));
	  addLogEntry("Final T",fp2str(m_mix->temperature()));
	  addLogEntry("H rel error",fp2str(Herr));
	  if (printLvl > 0) {
	    plogf("   equilibrate_HP: CONVERGENCE: Hfinal  = %g Tfinal = %g, Its = %d \n",
		   Hnow, Tnow, n);
	    plogf("                   H rel error = %g, cp = %g, HConvErr = %g\n", 
		   Herr, cpb, HConvErr);
	  }
	  goto done;
	}
	Tnew = Tnow + dT;
	if (Tnew < 0.0) Tnew = 0.5*Tnow;
	m_mix->setTemperature(Tnew);

      }
      catch (CanteraError err) {
	if (!estimateEquil) {
	  addLogEntry("no convergence",
		      "try estimating composition at the start");
	  strt = -1;
	}
	else {
	  Tnew = 0.5*(Tnow + Thigh);
	  if (fabs(Tnew - Tnow) < 1.0) Tnew = Tnow + 1.0;
	  m_mix->setTemperature(Tnew);
	  addLogEntry("no convergence",
		      "trying T = "+fp2str(Tnow));
	}
	endLogGroup();
      }

    }
    addLogEntry("reached max number of T iterations",int2str(maxiter));
    endLogGroup();
    throw CanteraError("MultiPhase::equilibrate_HP",
		       "No convergence for T");
  done:;
    return iSuccess;
  }

  int vcs_MultiPhaseEquil::equilibrate_SP(doublereal Starget, 
					  double Tlow, double Thigh,
					  int estimateEquil,
					  int printLvl, doublereal err, 
					  int maxsteps, int loglevel) {
    int maxiter = 100;
    int iSuccess;
    int strt = estimateEquil;
 
    // Lower bound on T. This will change as we progress in the calculation
    if (Tlow <= 0.0) {
      Tlow = 0.5 * m_mix->minTemp();
    }    
      // Upper bound on T. This will change as we progress in the calculation
    if (Thigh <= 0.0 || Thigh > 1.0E6) { 
      Thigh = 2.0 * m_mix->maxTemp();
    }
    addLogEntry("problem type","fixed S,P");
    addLogEntry("S target",fp2str(Starget));

    doublereal cpb = 1.0, dT, dTa, dTmax, Tnew;
    doublereal Snow;
    doublereal Slow = Undef;
    doublereal Shigh = Undef;
    doublereal Serr, SConvErr;
    doublereal Tnow = m_mix->temperature();
    if (Tnow < Tlow) {
      Tlow = Tnow;
    }
    if (Tnow > Thigh) {
      Thigh = Tnow;
    }
    int printLvlSub = MAX(printLvl - 1, 0);

    for (int n = 0; n < maxiter; n++) {

      // start with a loose error tolerance, but tighten it as we get 
      // close to the final temperature
      beginLogGroup("iteration "+int2str(n));

      try {
	Tnow = m_mix->temperature();
	iSuccess = equilibrate_TP(strt, printLvlSub, err, maxsteps, loglevel); 
	strt = 0;
	Snow = m_mix->entropy();
	double pmoles[10];
	pmoles[0] = m_mix->phaseMoles(0);
	double Tmoles = pmoles[0];
	double SperMole = Snow/Tmoles;
	plogf("T = %g, Snow = %g ,Tmoles = %g,  SperMole = %g\n",
	       Tnow, Snow, Tmoles, SperMole);


	// the equilibrium entropy monotonically increases with T; 
	// if the current value is below the target, then we know the
	// current temperature is too low. Set the lower bounds to the
	// current condition.
	if (Snow < Starget) {
	  if (Tnow > Tlow) {
	    Tlow = Tnow;
	    Slow = Snow;
	  } else {
	    if (Slow > Starget) {
	      if (Snow < Slow) {
		Thigh = Tlow;
		Shigh = Slow;
		Tlow = Tnow;
		Slow = Snow;
	      }
	    }
	  }
	}
	// the current enthalpy is greater than the target; therefore the
	// current temperature is too high. Set the high bounds.
	else {
	  if (Tnow < Thigh) {
	    Thigh = Tnow;
	    Shigh = Snow;
	  }
	}
	if (Slow != Undef && Shigh != Undef) {
	  cpb = (Shigh - Slow)/(Thigh - Tlow);
	  dT = (Starget - Snow)/cpb;
	  Tnew = Tnow + dT;
	  dTa = fabs(dT);
	  dTmax = 0.5*fabs(Thigh - Tlow);
	  if (Tnew > Thigh || Tnew < Tlow) {
	    dTmax = 1.5*fabs(Thigh - Tlow);
	  }
	  dTmax = MIN(dTmax, 300.);
	  if (dTa > dTmax) dT *= dTmax/dTa;  
	} else {
	  Tnew = sqrt(Tlow*Thigh);
	  dT = Tnew - Tnow;
	}

	double acpb = MAX(fabs(cpb), 1.0E-6);
	double denom = MAX(fabs(Starget), acpb);
	Serr = Starget - Snow;
	SConvErr = fabs((Serr)/denom);
	addLogEntry("T",fp2str(m_mix->temperature()));
	addLogEntry("S",fp2str(Snow));
	addLogEntry("Serr",fp2str(Serr));
	addLogEntry("S rel error",fp2str(SConvErr));
	addLogEntry("lower T bound",fp2str(Tlow));
	addLogEntry("upper T bound",fp2str(Thigh));
	endLogGroup(); // iteration
	if (printLvl > 0) {
	  plogf("   equilibrate_SP: It = %d, Tcurr  = %g Scurr = %g, Starget = %g\n",
		 n, Tnow, Snow, Starget);
	  plogf("                   S rel error = %g, cp = %g, SConvErr = %g\n", 
		 Serr, cpb, SConvErr);
	}

	if (SConvErr < err) { // || dTa < 1.0e-4) {
	  addLogEntry("T iterations",int2str(n));
	  addLogEntry("Final T",fp2str(m_mix->temperature()));
	  addLogEntry("S rel error",fp2str(Serr));
	  if (printLvl > 0) {
	    plogf("   equilibrate_SP: CONVERGENCE: Sfinal  = %g Tfinal = %g, Its = %d \n",
		   Snow, Tnow, n);
	    plogf("                   S rel error = %g, cp = %g, HConvErr = %g\n", 
		   Serr, cpb, SConvErr);
	  }
	  goto done;
	}
	Tnew = Tnow + dT;
	if (Tnew < 0.0) Tnew = 0.5*Tnow;
	m_mix->setTemperature(Tnew);

      }
      catch (CanteraError err) {
	if (!estimateEquil) {
	  addLogEntry("no convergence",
		      "try estimating composition at the start");
	  strt = -1;
	}
	else {
	  Tnew = 0.5*(Tnow + Thigh);
	  if (fabs(Tnew - Tnow) < 1.0) Tnew = Tnow + 1.0;
	  m_mix->setTemperature(Tnew);
	  addLogEntry("no convergence",
		      "trying T = "+fp2str(Tnow));
	}
	endLogGroup();
      }

    }
    addLogEntry("reached max number of T iterations",int2str(maxiter));
    endLogGroup();
    throw CanteraError("MultiPhase::equilibrate_SP",
		       "No convergence for T");
  done:;
    return iSuccess;
  }


  /*
   * Equilibrate the solution using the current element abundances
   */
  int vcs_MultiPhaseEquil::equilibrate(int XY, int estimateEquil, 
				       int printLvl, doublereal err, 
				       int maxsteps, int loglevel) {
    int iSuccess;
    doublereal xtarget;
    if (XY == TP) {
      iSuccess = equilibrate_TP(estimateEquil, printLvl, err, maxsteps, loglevel); 
    } else if (XY == HP || XY == UP) {
      if (XY == HP) {
	xtarget = m_mix->enthalpy();
      } else {
	xtarget = m_mix->IntEnergy();
      }
       double Tlow  = 0.5 * m_mix->minTemp();
       double Thigh = 2.0 * m_mix->maxTemp();
       iSuccess = equilibrate_HP(xtarget, XY, Tlow, Thigh, 
				 estimateEquil, printLvl, err, maxsteps, loglevel); 
    } else if (XY == SP) {
      xtarget = m_mix->entropy();
      double Tlow  = 0.5 * m_mix->minTemp();
      double Thigh = 2.0 * m_mix->maxTemp();
      iSuccess = equilibrate_SP(xtarget, Tlow, Thigh, 
				estimateEquil, printLvl, err, maxsteps, loglevel); 

    } else if (XY == TV) {
      xtarget = m_mix->temperature();
      iSuccess = equilibrate_TV(XY, xtarget,
				estimateEquil, printLvl, err, maxsteps, loglevel); 
    } else if (XY == HV) {
      xtarget = m_mix->enthalpy();
      iSuccess = equilibrate_TV(XY, xtarget,
				estimateEquil, printLvl, err, maxsteps, loglevel); 
    } else if (XY == UV) {
      xtarget = m_mix->IntEnergy();
      iSuccess = equilibrate_TV(XY, xtarget,
				estimateEquil, printLvl, err, maxsteps, loglevel); 
    } else if (XY == SV) {
      xtarget = m_mix->entropy();
      iSuccess = equilibrate_TV(XY, xtarget, estimateEquil,
				printLvl, err, maxsteps, loglevel); 
    } else {
      throw CanteraError(" vcs_MultiPhaseEquil::equilibrate",
			 "Unsupported Option");
    }
    return iSuccess;
  }
     
  /*
   * Equilibrate the solution using the current element abundances
   */
  int vcs_MultiPhaseEquil::equilibrate_TP(int estimateEquil, 
					  int printLvl, doublereal err, 
					  int maxsteps, int loglevel) {
   // Debugging level
   
    int maxit = maxsteps;;
    clockWC tickTock;
    int nsp = m_mix->nSpecies();
    int nel = m_mix->nElements();
    int nph = m_mix->nPhases();
    if (m_vprob == 0) {
      m_vprob = new VCS_PROB(nsp, nel, nph);
    }
    m_printLvl = printLvl;
    m_vprob->m_printLvl = printLvl;
  

   /*    
    *     Extract the current state information
    *     from the MultiPhase object and
    *     Transfer it to VCS_PROB object.
    */
   int res = vcs_Cantera_update_vprob(m_mix, m_vprob);
   if (res != 0) {
     plogf("problems\n");
   }
 

   // Set the estimation technique
   if (estimateEquil) {
     m_vprob->iest = estimateEquil;
   } else {
     m_vprob->iest = 0;
   }

   // Check obvious bounds on the temperature and pressure
   // NOTE, we may want to do more here with the real bounds 
   // given by the ThermoPhase objects.
   double T = m_mix->temperature();
   if (T <= 0.0) {
     throw CanteraError("vcs_MultiPhaseEquil::equilibrate",
			"Temperature less than zero on input");
   }
   double pres = m_mix->pressure();
   if (pres <= 0.0) {
     throw CanteraError("vcs_MultiPhaseEquil::equilibrate",
			"Pressure less than zero on input");
   }
   
   beginLogGroup("vcs_MultiPhaseEquil::equilibrate_TP", loglevel);
   addLogEntry("problem type","fixed T,P");
   addLogEntry("Temperature", T);
   addLogEntry("Pressure", pres);
   

   /*
    * Print out the problem specification from the point of
    * view of the vprob object.
    */
   m_vprob->prob_report(m_printLvl);

   /*
    * Call the thermo Program
    */
   int ip1 = m_printLvl;
   int ipr = MAX(0, m_printLvl-1);
   if (m_printLvl >= 3) {  
     ip1 = m_printLvl - 2;
   } else {
     ip1 = 0;
   }
   if (!m_vsolvePtr) {
     m_vsolvePtr = new VCS_SOLVE();
   }
   int iSuccess = m_vsolvePtr->vcs(m_vprob, 0, ipr, ip1, maxit);

   /*
    * Transfer the information back to the MultiPhase object.
    * Note we don't just call setMoles, because some multispecies
    * solution phases may be zeroed out, and that would cause a problem
    * for that routine. Also, the mole fractions of such zereod out
    * phases actually contain information about likely reemergent
    * states.
    */
   m_mix->uploadMoleFractionsFromPhases();
   int kGlob = 0;
   for (int ip = 0; ip < m_vprob->NPhase; ip++) {
     double phaseMole = 0.0;
     Cantera::ThermoPhase &tref = m_mix->phase(ip);
     int nspPhase = tref.nSpecies();
     for (int k = 0; k < nspPhase; k++, kGlob++) {
       phaseMole += m_vprob->w[kGlob];
     }
     //phaseMole *= 1.0E-3;
     m_mix->setPhaseMoles(ip, phaseMole);
   }
  
   double te = tickTock.secondsWC();
   if (printLvl > 0) {
     plogf("\n Results from vcs:\n");
     if (iSuccess != 0) {
       plogf("\nVCS FAILED TO CONVERGE!\n");
     }
     plogf("\n");
     plogf("Temperature = %g Kelvin\n",  m_vprob->T);
     plogf("Pressure    = %g Pa\n", m_vprob->PresPA);
     plogf("\n");
     plogf("----------------------------------------"
	    "---------------------\n");
     plogf(" Name             Mole_Number");
     if (m_vprob->m_VCS_UnitsFormat == VCS_UNITS_MKS) {
       plogf("(kmol)");
     } else {
       plogf("(gmol)");
     }
     plogf("  Mole_Fraction     Chem_Potential");
     if (m_vprob->m_VCS_UnitsFormat == VCS_UNITS_KCALMOL) 
       plogf(" (kcal/mol)\n");
     else if (m_vprob->m_VCS_UnitsFormat == VCS_UNITS_UNITLESS) 
       plogf(" (Dimensionless)\n");
     else if (m_vprob->m_VCS_UnitsFormat == VCS_UNITS_KJMOL) 
       plogf(" (kJ/mol)\n");
     else if (m_vprob->m_VCS_UnitsFormat == VCS_UNITS_KELVIN) 
       plogf(" (Kelvin)\n");
     else if (m_vprob->m_VCS_UnitsFormat == VCS_UNITS_MKS) 
       plogf(" (J/kmol)\n");
     plogf("--------------------------------------------------"
	    "-----------\n");
     for (int i = 0; i < m_vprob->nspecies; i++) {
       plogf("%-12s", m_vprob->SpName[i].c_str());
       if (m_vprob->SpeciesUnknownType[i] == VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	 plogf("  %15.3e %15.3e  ", 0.0, m_vprob->mf[i]);
	 plogf("%15.3e\n", m_vprob->m_gibbsSpecies[i]);
       } else {
       plogf("  %15.3e   %15.3e  ", m_vprob->w[i], m_vprob->mf[i]);
       if (m_vprob->w[i] <= 0.0) {
	 int iph = m_vprob->PhaseID[i];
	 vcs_VolPhase *VPhase = m_vprob->VPhaseList[iph];
	 if (VPhase->nSpecies() > 1) {
	   plogf("     -1.000e+300\n");
	 } else {
	   plogf("%15.3e\n", m_vprob->m_gibbsSpecies[i]);
	 }
       } else {
	 plogf("%15.3e\n", m_vprob->m_gibbsSpecies[i]);
       }
       }
     }
     plogf("------------------------------------------"
	    "-------------------\n"); 
     if (printLvl > 2) {
       if (m_vsolvePtr->m_timing_print_lvl > 0) {
         plogf("Total time = %12.6e seconds\n", te);
       }
     }
   }
   if (loglevel > 0) {
     endLogGroup();
   }
   return iSuccess;
  }



  /**************************************************************************
   *
   *
   */
  void vcs_MultiPhaseEquil::reportCSV(const std::string &reportFile) {
    int k;
    int istart;
    int nSpecies;
  
    double vol = 0.0;
    string sName;
    int nphase = m_vprob->NPhase;

    FILE * FP = fopen(reportFile.c_str(), "w");
    if (!FP) {
      plogf("Failure to open file\n");
      exit(EXIT_FAILURE);
    }
    double Temp = m_mix->temperature();
    double pres = m_mix->pressure();
    double *mf = VCS_DATA_PTR(m_vprob->mf);
#ifdef DEBUG_MODE
    double *fe = VCS_DATA_PTR(m_vprob->m_gibbsSpecies);
#endif
    std::vector<double> VolPM;
    std::vector<double> activity;
    std::vector<double> ac;
    std::vector<double> mu;
    std::vector<double> mu0;
    std::vector<double> molalities;


    vol = 0.0;
    for (int iphase = 0; iphase < nphase; iphase++) {
      istart =    m_mix->speciesIndex(0, iphase);
      Cantera::ThermoPhase &tref = m_mix->phase(iphase);
      nSpecies = tref.nSpecies();
      VolPM.resize(nSpecies, 0.0);
      tref.getPartialMolarVolumes(VCS_DATA_PTR(VolPM));
      vcs_VolPhase *volP = m_vprob->VPhaseList[iphase];
  
      double TMolesPhase = volP->totalMoles();
      double VolPhaseVolumes = 0.0;
      for (k = 0; k < nSpecies; k++) {
	VolPhaseVolumes += VolPM[k] * mf[istart + k];
      }
      VolPhaseVolumes *= TMolesPhase;
      vol += VolPhaseVolumes;
    }

    fprintf(FP,"--------------------- VCS_MULTIPHASE_EQUIL FINAL REPORT"
	    " -----------------------------\n");
    fprintf(FP,"Temperature  = %11.5g kelvin\n", Temp);
    fprintf(FP,"Pressure     = %11.5g Pascal\n", pres);
    fprintf(FP,"Total Volume = %11.5g m**3\n", vol);
    fprintf(FP,"Number Basis optimizations = %d\n", m_vprob->m_NumBasisOptimizations);
    fprintf(FP,"Number VCS iterations = %d\n", m_vprob->m_Iterations);

    for (int iphase = 0; iphase < nphase; iphase++) {
      istart =    m_mix->speciesIndex(0, iphase);
      Cantera::ThermoPhase &tref = m_mix->phase(iphase);
      Cantera::ThermoPhase *tp = &tref;
      string phaseName = tref.name();
      vcs_VolPhase *volP = m_vprob->VPhaseList[iphase];
      double TMolesPhase = volP->totalMoles();
      //AssertTrace(TMolesPhase == m_mix->phaseMoles(iphase));
      nSpecies = tref.nSpecies();
      activity.resize(nSpecies, 0.0);
      ac.resize(nSpecies, 0.0);
   
      mu0.resize(nSpecies, 0.0);
      mu.resize(nSpecies, 0.0);
      VolPM.resize(nSpecies, 0.0);
      molalities.resize(nSpecies, 0.0);
 
      int actConvention = tp->activityConvention();
      tp->getActivities(VCS_DATA_PTR(activity));
      tp->getActivityCoefficients(VCS_DATA_PTR(ac));
      tp->getStandardChemPotentials(VCS_DATA_PTR(mu0));
  
      tp->getPartialMolarVolumes(VCS_DATA_PTR(VolPM));
      tp->getChemPotentials(VCS_DATA_PTR(mu));
      double VolPhaseVolumes = 0.0;
      for (k = 0; k < nSpecies; k++) {
	VolPhaseVolumes += VolPM[k] * mf[istart + k];
      }
      VolPhaseVolumes *= TMolesPhase;
      vol += VolPhaseVolumes;

 
      if (actConvention == 1) {
#ifdef WITH_ELECTROLYTES
	MolalityVPSSTP *mTP = static_cast<MolalityVPSSTP *>(tp);
	mTP->getMolalities(VCS_DATA_PTR(molalities));
#endif
	tp->getChemPotentials(VCS_DATA_PTR(mu));
  
	if (iphase == 0) {
	  fprintf(FP,"        Name,      Phase,  PhaseMoles,  Mole_Fract, "
		  "Molalities,  ActCoeff,   Activity,"
		  "ChemPot_SS0,   ChemPot,   mole_num,       PMVol, Phase_Volume\n");
     
	  fprintf(FP,"            ,           ,      (kmol),            , "
		  "          ,          ,           ,"
		  "   (J/kmol),  (J/kmol),     (kmol), (m**3/kmol),     (m**3)\n"); 
	}
	for (k = 0; k < nSpecies; k++) {
	  sName = tp->speciesName(k);
	  fprintf(FP,"%12s, %11s, %11.3e, %11.3e, %11.3e, %11.3e, %11.3e,"
		  "%11.3e, %11.3e, %11.3e, %11.3e, %11.3e\n", 
		  sName.c_str(), 
		  phaseName.c_str(), TMolesPhase,
		  mf[istart + k], molalities[k], ac[k], activity[k],
		  mu0[k]*1.0E-6, mu[k]*1.0E-6,
		  mf[istart + k] * TMolesPhase,
		  VolPM[k],  VolPhaseVolumes );
	}
 
      } else {
	if (iphase == 0) {
	  fprintf(FP,"        Name,       Phase,  PhaseMoles,  Mole_Fract,  "
		  "Molalities,   ActCoeff,    Activity,"
		  "  ChemPotSS0,     ChemPot,   mole_num,       PMVol, Phase_Volume\n");
    
	  fprintf(FP,"            ,            ,      (kmol),            ,  "
		  "          ,           ,            ,"
		  "    (J/kmol),    (J/kmol),     (kmol), (m**3/kmol),       (m**3)\n");
	}
	for (k = 0; k < nSpecies; k++) {
	  molalities[k] = 0.0;
	}
	for (k = 0; k < nSpecies; k++) {
	  sName = tp->speciesName(k);
	  fprintf(FP,"%12s, %11s, %11.3e, %11.3e, %11.3e, %11.3e, %11.3e, "
		  "%11.3e, %11.3e,% 11.3e, %11.3e, %11.3e\n",
		  sName.c_str(),
		  phaseName.c_str(), TMolesPhase,
		  mf[istart + k],  molalities[k], ac[k], 
		  activity[k], mu0[k]*1.0E-6, mu[k]*1.0E-6, 
		  mf[istart + k] * TMolesPhase,
		  VolPM[k],  VolPhaseVolumes );       
	}
      }

#ifdef DEBUG_MODE
      /*
       * Check consistency: These should be equal
       */
      tp->getChemPotentials(fe+istart);
      for (k = 0; k < nSpecies; k++) {
	if (!vcs_doubleEqual(fe[istart+k], mu[k])) {
	  fprintf(FP,"ERROR: incompatibility!\n");
	  fclose(FP);
	  plogf("ERROR: incompatibility!\n");
	  exit(EXIT_FAILURE);
	}
      }
#endif

    }
    fclose(FP);
  }

 
  static void print_char(const char letter, const int num) {
    for (int i = 0; i < num; i++) plogf("%c", letter);
  }
  
  /*
   * 
   *
   * HKM -> Work on transfering the current value of the voltages into the 
   *        equilibrium problem.
   */
  int  vcs_Cantera_to_vprob(Cantera::MultiPhase *mphase,
			    VCSnonideal::VCS_PROB *vprob) {
    int k;
    VCS_SPECIES_THERMO *ts_ptr = 0;

    /*
     * Calculate the total number of species and phases in the problem
     */
    int totNumPhases = mphase->nPhases();
    int totNumSpecies = mphase->nSpecies();

    // Problem type has yet to be worked out.
    vprob->prob_type = 0;
    vprob->nspecies  = totNumSpecies;
    vprob->ne        = 0;
    vprob->NPhase    = totNumPhases;
    vprob->m_VCS_UnitsFormat      = VCS_UNITS_MKS;
    // Set the initial estimate to a machine generated estimate for now
    // We will work out the details later.
    vprob->iest      = -1;
    vprob->T         = mphase->temperature();
    vprob->PresPA    = mphase->pressure();
    vprob->Vol       = mphase->volume();
    vprob->Title     = "MultiPhase Object";

    Cantera::ThermoPhase *tPhase = 0;

    int iSurPhase = -1;
    bool gasPhase;
    int printLvl = vprob->m_printLvl;

    /*
     * Loop over the phases, transfering pertinent information
     */
    int kT = 0;
    for (int iphase = 0; iphase < totNumPhases; iphase++) {

      /*
       * Get the thermophase object - assume volume phase
       */
      iSurPhase = -1;
      tPhase = &(mphase->phase(iphase));
      int nelem = tPhase->nElements();
    
      /*
       * Query Cantera for the equation of state type of the
       * current phase.
       */
      int eos = tPhase->eosType();
      if (eos == cIdealGas) gasPhase = true;
      else                  gasPhase = false;
    
      /*
       *    Find out the number of species in the phase
       */
      int nSpPhase = tPhase->nSpecies();
      /*
       *    Find out the name of the phase
       */
      string phaseName = tPhase->name();
	 
      /*
       *    Call the basic vcs_VolPhase creation routine.
       *    Properties set here: 
       *       ->PhaseNum = phase number in the thermo problem
       *       ->GasPhase = Boolean indicating whether it is a gas phase
       *       ->NumSpecies = number of species in the phase
       *       ->TMolesInert = Inerts in the phase = 0.0 for cantera
       *       ->PhaseName  = Name of the phase
       */

      vcs_VolPhase *VolPhase = vprob->VPhaseList[iphase];
      VolPhase->resize(iphase, nSpPhase, nelem, phaseName.c_str(), 0.0);
      VolPhase->m_gasPhase = gasPhase;
      /*
       * Tell the vcs_VolPhase pointer about cantera
       */
      VolPhase->p_VCS_UnitsFormat = vprob->m_VCS_UnitsFormat;
      VolPhase->setPtrThermoPhase(tPhase);
      VolPhase->setTotalMoles(0.0);
      /*
       * Set the electric potential of the volume phase from the
       * ThermoPhase object's value.
       */
      VolPhase->setElectricPotential(tPhase->electricPotential());
      /*
       * Query the ThermoPhase object to find out what convention
       * it uses for the specification of activity and Standard State.
       */
      VolPhase->p_activityConvention = tPhase->activityConvention();
      /*
       * Assign the value of eqn of state 
       *  -> Handle conflicts here.
       */
      switch (eos) {
      case cIdealGas:
	VolPhase->m_eqnState = VCS_EOS_IDEAL_GAS;
	break;
      case cIncompressible:
	VolPhase->m_eqnState = VCS_EOS_CONSTANT;
	break;
      case cSurf:
	plogf("cSurf not handled yet\n");
	exit(EXIT_FAILURE);
      case cStoichSubstance:
	VolPhase->m_eqnState = VCS_EOS_STOICH_SUB;
	break;
      case  cPureFluid:
	if (printLvl > 1) {
	  plogf("cPureFluid not recognized yet by VCSnonideal\n");
	}
	break;
      case cEdge:
	plogf("cEdge not handled yet\n");
	exit(EXIT_FAILURE);
      case cIdealSolidSolnPhase0:
      case cIdealSolidSolnPhase1:
      case cIdealSolidSolnPhase2:
	VolPhase->m_eqnState = VCS_EOS_IDEAL_SOLN;
	break;
      default:
	if (printLvl > 1) {
	  plogf("Unknown Cantera EOS to VCSnonideal: %d\n", eos);
	}
	VolPhase->m_eqnState = VCS_EOS_UNK_CANTERA;
	if (!VolPhase->usingCanteraCalls()) {
	  plogf("vcs functions asked for, but unimplemented\n");
	  exit(EXIT_FAILURE);
	}
	break;
      }
  
      /*
       * Transfer all of the element information from the
       * ThermoPhase object to the vcs_VolPhase object.
       * Also decide whether we need a new charge neutrality
       * element in the phase to enforce a charge neutrality
       * constraint.
       *  We also decide whether this is a single species phase
       *  with the voltage being the independent variable setting
       *  the chemical potential of the electrons.
       */
      VolPhase->transferElementsFM(tPhase);

      /*
       * Combine the element information in the vcs_VolPhase
       * object into the vprob object.
       */
      vprob->addPhaseElements(VolPhase);

      VolPhase->setState_TP(vprob->T, vprob->PresPA);
      vector<double> muPhase(tPhase->nSpecies(),0.0);
      tPhase->getChemPotentials(&muPhase[0]);
      double tMoles = 0.0;
      /*
       *    Loop through each species in the current phase
       */ 
      for (k = 0; k < nSpPhase; k++) {
	/*
	 * Obtain the molecular weight of the species from the
	 * ThermoPhase object
	 */
	vprob->WtSpecies[kT] = tPhase->molecularWeight(k);

	/*
	 * Obtain the charges of the species from the
	 * ThermoPhase object
	 */
	vprob->Charge[kT] = tPhase->charge(k);

	/*
	 *   Set the phaseid of the species
	 */
	vprob->PhaseID[kT] = iphase;

	/*
	 *  Transfer the Species name
	 */
	string stmp = mphase->speciesName(kT);
	vprob->SpName[kT] = stmp;

	/*
	 *   Set the initial estimate of the number of kmoles of the species
	 *   and the mole fraction vector. translate from
	 *   kmol to gmol.
	 */
	vprob->w[kT] = mphase->speciesMoles(kT);
	tMoles += vprob->w[kT];
	vprob->mf[kT] = mphase->moleFraction(kT);

	/*
	 * transfer chemical potential vector
	 */
	vprob->m_gibbsSpecies[kT] = muPhase[k];
	/*
	 * Transfer the type of unknown
	 */
	vprob->SpeciesUnknownType[kT] = VolPhase->speciesUnknownType(k);
	/*
	 * Transfer the species information from the 
	 * volPhase structure to the VPROB structure
	 * This includes:
	 *      FormulaMatrix[][]
	 *      VolPhase->IndSpecies[]
	 */
	vprob->addOnePhaseSpecies(VolPhase, k, kT);

	/*
	 * Get a pointer to the thermo object
	 */
	ts_ptr = vprob->SpeciesThermo[kT]; 
	/*
	 * Fill in the vcs_SpeciesProperty structure
	 */
	vcs_SpeciesProperties *sProp = VolPhase->speciesProperty(k);
	sProp->NumElements = vprob->ne;
	sProp->SpName = vprob->SpName[kT];
	sProp->SpeciesThermo = ts_ptr;
	sProp->WtSpecies = tPhase->molecularWeight(k);
	sProp->FormulaMatrixCol.resize(vprob->ne, 0.0);
	for (int e = 0; e < vprob->ne; e++) {
	  sProp->FormulaMatrixCol[e] = vprob->FormulaMatrix[e][kT];
	}
	sProp->Charge = tPhase->charge(k);
	sProp->SurfaceSpecies = false;
	sProp->VolPM = 0.0;
	  
	/*
	 *  Transfer the thermo specification of the species
	 *              vprob->SpeciesThermo[]
	 */
	ts_ptr->UseCanteraCalls = VolPhase->usingCanteraCalls();
	ts_ptr->m_VCS_UnitsFormat = VolPhase->p_VCS_UnitsFormat;
	/*
	 * Add lookback connectivity into the thermo object first
	 */
	ts_ptr->IndexPhase = iphase;
	ts_ptr->IndexSpeciesPhase = k;
	ts_ptr->OwningPhase = VolPhase;
	/*
	 *   get a reference to the Cantera species thermo.
	 */
	SpeciesThermo &sp = tPhase->speciesThermo();

	int spType;
	double c[150];
	double minTemp, maxTemp, refPressure;
	sp.reportParams(k, spType, c, minTemp, maxTemp, refPressure);

	if (spType == SIMPLE) {
	  ts_ptr->SS0_Model  = VCS_SS0_CONSTANT;
	  ts_ptr->SS0_T0  = c[0];
	  ts_ptr->SS0_H0  = c[1];
	  ts_ptr->SS0_S0  = c[2];
	  ts_ptr->SS0_Cp0 = c[3];
	  if (gasPhase) {
	    ts_ptr->SSStar_Model = VCS_SSSTAR_IDEAL_GAS;
	    ts_ptr->SSStar_Vol_Model  = VCS_SSVOL_IDEALGAS;	       
	  } else {
	    ts_ptr->SSStar_Model = VCS_SSSTAR_CONSTANT;
	    ts_ptr->SSStar_Vol_Model  = VCS_SSVOL_CONSTANT;
	  }
	  ts_ptr->Activity_Coeff_Model  = VCS_AC_CONSTANT;
	  ts_ptr->Activity_Coeff_Params = NULL;
	} else {
	  if (vprob->m_printLvl > 2) {
	    plogf("vcs_Cantera_convert: Species Type %d not known \n",
		  spType);
	  }
	  ts_ptr->SS0_Model = VCS_SS0_NOTHANDLED;
	  ts_ptr->SSStar_Model = VCS_SSSTAR_NOTHANDLED;
	  if (!(ts_ptr->UseCanteraCalls )) {
	    plogf("Cantera calls not being used -> exiting\n");
	    exit(EXIT_FAILURE);
	  }
	}
	    
	/*
	 *  Transfer the Volume Information -> NEEDS WORK
	 */
	if (gasPhase) {
	  ts_ptr->SSStar_Vol_Model = VCS_SSVOL_IDEALGAS;
	  ts_ptr->SSStar_Vol_Params = NULL;
	  ts_ptr->SSStar_Vol0 = 82.05 * 273.15 / 1.0;

	} else {
	  std::vector<double> phaseTermCoeff(nSpPhase, 0.0);
	  int nCoeff;
	  tPhase->getParameters(nCoeff, VCS_DATA_PTR(phaseTermCoeff));
	  ts_ptr->SSStar_Vol_Model = VCS_SSVOL_CONSTANT;
	  ts_ptr->SSStar_Vol0 = phaseTermCoeff[k];
	}
	kT++;
      }

      /*
       * Now go back through the species in the phase and assign
       * a valid mole fraction to all phases, even if the initial
       * estimate of the total number of moles is zero.
       */
      if (tMoles > 0.0) {
	for (k = 0; k < nSpPhase; k++) {
	  int kTa = VolPhase->spGlobalIndexVCS(k);
	  vprob->mf[kTa] = vprob->w[kTa] / tMoles;
	}
      } else {
	/*
	 * Perhaps, we could do a more sophisticated treatment below.
	 * But, will start with this.
	 */
	for (k = 0; k < nSpPhase; k++) {
	  int kTa = VolPhase->spGlobalIndexVCS(k);
	  vprob->mf[kTa]= 1.0 / (double) nSpPhase;
	}
      }

      VolPhase->setMolesFromVCS(VCS_STATECALC_OLD, VCS_DATA_PTR(vprob->w));
      /*
       * Now, calculate a sample naught gibbs free energy calculation
       * at the specified temperature.
       */
      double R = vcsUtil_gasConstant(vprob->m_VCS_UnitsFormat);
      for (k = 0; k < nSpPhase; k++) {
	vcs_SpeciesProperties *sProp = VolPhase->speciesProperty(k);
	ts_ptr = sProp->SpeciesThermo;
	ts_ptr->SS0_feSave = VolPhase->G0_calc_one(k)/ R;
	ts_ptr->SS0_TSave = vprob->T;
      }
     
    }

    /*
     *  Transfer initial element abundances to the vprob object.
     *  We have to find the mapping index from one to the other
     *
     */
    vprob->gai.resize(vprob->ne, 0.0);
    vprob->set_gai();

    /*
     *          Printout the species information: PhaseID's and mole nums
     */
    if (vprob->m_printLvl > 1) {
      plogf("\n"); print_char('=', 80); plogf("\n");
      print_char('=', 16);
      plogf(" Cantera_to_vprob: START OF PROBLEM STATEMENT ");
      print_char('=', 20); plogf("\n");
      print_char('=', 80); plogf("\n");
      plogf("             Phase IDs of species\n");
      plogf("            species     phaseID        phaseName   ");
      plogf(" Initial_Estimated_kMols\n");
      for (int i = 0; i < vprob->nspecies; i++) {
	int iphase = vprob->PhaseID[i];

	vcs_VolPhase *VolPhase = vprob->VPhaseList[iphase];
	plogf("%16s      %5d   %16s", vprob->SpName[i].c_str(), iphase,
	       VolPhase->PhaseName.c_str());
	plogf("             %-10.5g\n",  vprob->w[i]);
      }
      
      /*
       *   Printout of the Phase structure information
       */
      plogf("\n"); print_char('-', 80); plogf("\n");
      plogf("             Information about phases\n");
      plogf("  PhaseName    PhaseNum SingSpec GasPhase EqnState NumSpec");
      plogf("  TMolesInert       Tmoles(kmol)\n");
   
      for (int iphase = 0; iphase < vprob->NPhase; iphase++) {
	vcs_VolPhase *VolPhase = vprob->VPhaseList[iphase];
	std::string sEOS = string16_EOSType(VolPhase->m_eqnState);
	plogf("%16s %5d %5d %8d %16s %8d %16e ", VolPhase->PhaseName.c_str(),
	       VolPhase->VP_ID,       VolPhase->m_singleSpecies,
	       VolPhase->m_gasPhase,    sEOS.c_str(),
	       VolPhase->nSpecies(), VolPhase->totalMolesInert() );
	plogf("%16e\n",  VolPhase->totalMoles());
      }
   
      plogf("\n"); print_char('=', 80); plogf("\n");
      print_char('=', 16); 
      plogf(" Cantera_to_vprob: END OF PROBLEM STATEMENT ");
      print_char('=', 20); plogf("\n");
      print_char('=', 80); plogf("\n\n");
    }
 
    return VCS_SUCCESS;
  }

  // Transfer the current state of mphase into the VCS_PROB object
  /*
   * The basic problem has already been set up.
   */
  int vcs_Cantera_update_vprob(Cantera::MultiPhase *mphase, 
			       VCSnonideal::VCS_PROB *vprob) {
    int totNumPhases = mphase->nPhases();
    int kT = 0;
    std::vector<double> tmpMoles;
    // Problem type has yet to be worked out.
    vprob->prob_type = 0;
    // Whether we have an estimate or not gets overwritten on
    // the call to the equilibrium solver.
    vprob->iest      = -1;
    vprob->T         = mphase->temperature();
    vprob->PresPA    = mphase->pressure();
    vprob->Vol       = mphase->volume();
    Cantera::ThermoPhase *tPhase = 0;

    for (int iphase = 0; iphase < totNumPhases; iphase++) {
      tPhase = &(mphase->phase(iphase));
      vcs_VolPhase *volPhase = vprob->VPhaseList[iphase];
      /*
       * Set the electric potential of the volume phase from the
       * ThermoPhase object's value.
       */
      volPhase->setElectricPotential(tPhase->electricPotential());

      volPhase->setState_TP(vprob->T, vprob->PresPA);
      vector<double> muPhase(tPhase->nSpecies(),0.0);
      tPhase->getChemPotentials(&muPhase[0]);
      /*
       *    Loop through each species in the current phase
       */
      int nSpPhase = tPhase->nSpecies();
      // volPhase->TMoles = 0.0;
      tmpMoles.resize(nSpPhase);
      for (int k = 0; k < nSpPhase; k++) {
	tmpMoles[k] = mphase->speciesMoles(kT);
	vprob->w[kT] = mphase->speciesMoles(kT);
	vprob->mf[kT] = mphase->moleFraction(kT);

	/*
	 * transfer chemical potential vector
	 */
	vprob->m_gibbsSpecies[kT] = muPhase[k];

	kT++;
      }
      if (volPhase->phiVarIndex() >= 0) {
	int kphi = volPhase->phiVarIndex();
	int kglob = volPhase->spGlobalIndexVCS(kphi);
	vprob->w[kglob] = tPhase->electricPotential();
      }
      volPhase->setMolesFromVCS(VCS_STATECALC_OLD, VCS_DATA_PTR(vprob->w));
      if ((nSpPhase == 1) && (volPhase->phiVarIndex() == 0)) {
	volPhase->setExistence(VCS_PHASE_EXIST_ALWAYS);
      }	else if (volPhase->totalMoles() > 0.0) {
	volPhase->setExistence(VCS_PHASE_EXIST_YES);
      } else {
	volPhase->setExistence(VCS_PHASE_EXIST_NO);
      }
   
    }
    /*
     *  Transfer initial element abundances to the vprob object.
     *  Put them in the front of the object. There may be
     *  more constraints than there are elements. But, we 
     *  know the element abundances are in the front of the
     *  vector.
     */
    vprob->set_gai();

    /*
     *          Printout the species information: PhaseID's and mole nums
     */
    if (vprob->m_printLvl > 1) {
      plogf("\n"); print_char('=', 80); plogf("\n");
      print_char('=', 20);
      plogf(" Cantera_to_vprob: START OF PROBLEM STATEMENT ");
      print_char('=', 20); plogf("\n");
      print_char('=', 80); plogf("\n\n");
      plogf("             Phase IDs of species\n");
      plogf("            species     phaseID        phaseName   ");
      plogf(" Initial_Estimated_kMols\n");
      for (int i = 0; i < vprob->nspecies; i++) {
	int iphase = vprob->PhaseID[i];

	vcs_VolPhase *VolPhase = vprob->VPhaseList[iphase];
	plogf("%16s      %5d   %16s", vprob->SpName[i].c_str(), iphase,
	       VolPhase->PhaseName.c_str());
	plogf("             %-10.5g\n",  vprob->w[i]);
      }
      
      /*
       *   Printout of the Phase structure information
       */
      plogf("\n"); print_char('-', 80); plogf("\n");
      plogf("             Information about phases\n");
      plogf("  PhaseName    PhaseNum SingSpec GasPhase EqnState NumSpec");
      plogf("  TMolesInert       Tmoles(kmol)\n");
   
      for (int iphase = 0; iphase < vprob->NPhase; iphase++) {
	vcs_VolPhase *VolPhase = vprob->VPhaseList[iphase];
	std::string sEOS = string16_EOSType(VolPhase->m_eqnState);
	plogf("%16s %5d %5d %8d %16s %8d %16e ", VolPhase->PhaseName.c_str(),
	       VolPhase->VP_ID,       VolPhase->m_singleSpecies,
	       VolPhase->m_gasPhase,    sEOS.c_str(),
	       VolPhase->nSpecies(), VolPhase->totalMolesInert() );
	plogf("%16e\n",  VolPhase->totalMoles() ); 
      }
   
      plogf("\n"); print_char('=', 80); plogf("\n");
      print_char('=', 20); 
      plogf(" Cantera_to_vprob: END OF PROBLEM STATEMENT ");
      print_char('=', 20); plogf("\n");
      print_char('=', 80); plogf("\n\n");
    }
 
    return VCS_SUCCESS;
  }

  // This routine hasn't been checked yet
  void vcs_MultiPhaseEquil::getStoichVector(index_t rxn, Cantera::vector_fp& nu) {
    int nsp = m_vsolvePtr->m_numSpeciesTot;
    nu.resize(nsp, 0.0);
    for (int i = 0; i < nsp; i++) {
      nu[i] = 0.0;
    }
    int nc = numComponents();
    // scMatrix [nrxn][ncomp]
    const DoubleStarStar &scMatrix = m_vsolvePtr->m_stoichCoeffRxnMatrix;
    const  std::vector<int> indSpecies = m_vsolvePtr->m_speciesMapIndex;
    if ((int) rxn > nsp - nc) return;
    int j = indSpecies[rxn + nc];
    nu[j] = 1.0;
    for (int kc = 0; kc < nc; kc++) {
      j = indSpecies[kc];
      nu[j] = scMatrix[rxn][kc];
    }
    
  }
  
  int vcs_MultiPhaseEquil::numComponents() const {
    int nc = -1;
    if (m_vsolvePtr) {
      nc =  m_vsolvePtr->m_numComponents;
    }
    return nc;
  }

  int vcs_MultiPhaseEquil::numElemConstraints() const {
    int nec = -1;
    if (m_vsolvePtr) {
      nec =  m_vsolvePtr->m_numElemConstraints;
    }
    return nec;
  }
 

  int vcs_MultiPhaseEquil::component(int m) const {
    int nc = numComponents();
    if (m < nc) return m_vsolvePtr->m_speciesMapIndex[m]; 
    else return -1;
  }

}
