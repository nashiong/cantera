/*
 * $Id: testPureWater.cpp 255 2009-11-09 23:36:49Z hkmoffa $
 */

#include "cantera/Cantera.h"
#include "cantera/PureFluid.h"

#include <new>
#include <cstdio>
#include <cmath>

using namespace std;
using namespace Cantera;

double tvalue(double val, double atol = 1.0E-9) {
    double rval = val;
    if (fabs(val) < atol) {
      rval = 0.0;
    }
    return rval;
}



int main () {

    double pres;
    try {

      XML_Node *xc = get_XML_File("liquidvapor.xml"); 
      XML_Node * const xs = xc->findNameID("phase", "carbondioxide");
#ifdef CANTERA_HAS_TEMPLATES
      ThermoPhase<doublereal> *water_tp = newPhase<doublereal>(*xs);
#else
      ThermoPhase *water_tp = newPhase(*xs);
#endif
      PureFluidPhase *w = dynamic_cast <PureFluidPhase *>(water_tp);

      /* 
       * Print out the triple point conditions
       */
      double temp = 216.54;
      pres = w->satPressure(temp);
      printf("psat(%g) = %g\n", temp, pres);
      double presLow = 1.0E-2;
      temp = 298.15;
      double oneBar = 1.0E5;

      printf("Comparisons to NIST: (see http://webbook.nist.gov):\n\n");

      w->setDensity(1.0E-8);
      w->setState_TP(temp, presLow);
      double h = w->enthalpy_mole();
      printf("H0(298.15) = %g J/kmol\n", h);
      double h298 = h;

      double s = w->entropy_mole();
      s -= GasConstant * log(oneBar/presLow);
      printf("S0(298.15) = %g J/kmolK\n", s);


      double T[20];
      T[0] = 298.15;
      T[1] = 300.;
      T[2] = 400.;
      T[3] = 500.;
      T[4] = 600.;
      T[5] = 1000.;

      double Cp0, delh0, delg0, g;

      printf("\nIdeal Gas Standard State:\n");
      printf ("        T      Cp0           S0     "
	      " -(G0-H298)/T       H0-H298\n");
      printf ("       (K)   (J/molK)     (J/molK)  "
	      "   (J/molK)        (kJ/mol)\n");
      for (int i = 0; i < 6; i++) {
	temp = T[i];
	w->setState_TP(temp, presLow);
	h = w->enthalpy_mole();
	delh0 = tvalue(h - h298, 1.0E-6);
	g = w->gibbs_mole();
	delg0 = (g - h298)/temp + GasConstant * log(oneBar/presLow);
	Cp0 = w->cp_mole();
	s = w->entropy_mole();
	s -= GasConstant * log(oneBar/presLow);
	printf("%10g %10g %13g %13g %13g\n", temp, Cp0*1.0E-3, s*1.0E-3,
	       -delg0*1.0E-3, delh0*1.0E-6);
      }
      printf("\n\n");

      temp = 298.15;
      w->setDensity(1000.);
      w->setState_TP(temp, oneBar);
      h = w->enthalpy_mole();
      printf("H_liq(298.15, onebar) = %g J/kmol\n", h);
      double h298l = h;
      s = w->entropy_mole();
      printf("S_liq(298.15, onebar) = %g J/kmolK\n", s);

      int n = 0;

      T[n++] = 216.59;
      T[n++] = 220.;
      T[n++] = 230.;
      T[n++] = 240.;
      T[n++] = 250.;
      T[n++] = 260.;
      T[n++] = 273.15;
      T[n++] = 298.15;
      T[n++] = 300.;
      T[n++] = 304.;
      // T[c] = 304.21;
    
      printf("\nLiquid 1bar or psat Standard State\n");
      printf ("       T     press         psat            Cp0            S0   "
	      "  -(G0-H298)/T       H0-H298\n");
      printf ("      (K)     (bar)        (bar)        (J/molK)       (J/molK)"
	      "     (J/molK)        (kJ/mol)\n");
 
      for (int i = 0; i < n; i++) {
	temp = T[i];
	double psat = w->satPressure(temp);
	double press = oneBar;
	if (psat > press) {
	  press = psat*1.002;
	}
	w->setState_TP(temp, press);
	h = w->enthalpy_mole();
	delh0 = tvalue(h - h298l, 1.0E-6);
	g = w->gibbs_mole();
	delg0 = (g - h298l)/temp;
	Cp0 = w->cp_mole();
	s = w->entropy_mole();
	printf("%10g %10g %12g %13g %13g %13g %13g\n", temp, press*1.0E-5,
	       psat*1.0E-5,
	       Cp0*1.0E-3, s*1.0E-3,
	       -delg0*1.0E-3, delh0*1.0E-6);
      }

      printf("\nLiquid Densities:\n");
      printf ("       T     press         psat        Density          molarVol   "
	      "\n");
      printf ("      (K)     (bar)        (bar)      (kg/m3)          (m3/kmol)"
	      "\n");
      for (int i = 0; i < 6; i++) {
	temp = T[i];
	double psat = w->satPressure(temp);
	double press = oneBar;
	if (psat > press) {
	  press = psat*1.002;
	}
	w->setState_TP(temp, press);
	double d = w->density();
	double mw = w->molecularWeight(0);
	double vbar = mw/d;
	// not implemented
	//w.getPartialMolarVolumes(&vbar);

	printf("%10g %10g %12g %13g %13g\n", temp, press*1.0E-5,
	       psat*1.0E-5, d, vbar);

      }

      printf("\n\nTable of increasing Enthalpy at 1 atm\n\n");
      double dens;
      printf("  Enthalpy,   Temperature,     x_Vapor,    Density, Entropy_mass, Gibbs_mass\n");
      w->setState_TP(298., OneAtm);
      double Hset = w->enthalpy_mass();
      double vapFrac = w->vaporFraction();
      double Tcalc = w->temperature();
      double Scalc = w->entropy_mass();
      double Gcalc = w->gibbs_mass();
      dens = w->density();
      printf(" %10g, %10g, %10g, %11.5g, %11.5g, %11.5g\n", Hset , Tcalc, vapFrac, dens, Scalc, Gcalc);
      w->setState_HP(Hset, OneAtm);
      vapFrac = w->vaporFraction();
      Tcalc = w->temperature();
      dens = w->density();
      Scalc = w->entropy_mass();
      Gcalc = w->gibbs_mass();
      printf(" %10g, %10g, %10g, %11.5g, %11.5g, %11.5g\n", Hset , Tcalc, vapFrac, dens, Scalc, Gcalc);

      double deltaH = 100000.;
      for (int i = 0; i < 10; i++) {
	Hset += deltaH;
	w->setState_HP(Hset, OneAtm);
	vapFrac = w->vaporFraction();
	Tcalc = w->temperature();
	dens = w->density();
	Scalc = w->entropy_mass();
	Gcalc = w->gibbs_mass();
	printf(" %10g, %10g, %10g, %11.5g, %11.5g, %11.5g\n", Hset , Tcalc, vapFrac, dens, Scalc, Gcalc);
      }
 


      printf("\n\nTable of increasing Enthalpy at 45 atm\n\n");
      printf("  Enthalpy,   Temperature,     x_Vapor,    Density, Entropy_mass, Gibbs_mass\n");
      w->setState_TP(268., 45.0 * OneAtm);
      Hset = w->enthalpy_mass();
      vapFrac = w->vaporFraction();
      Tcalc = w->temperature();
      Scalc = w->entropy_mass();
       Gcalc = w->gibbs_mass();
      dens = w->density();
      printf(" %10g, %10g, %10g, %11.5g, %11.5g, %11.5g\n", Hset , Tcalc, vapFrac, dens, Scalc, Gcalc);
      w->setState_HP(Hset, 45 * OneAtm);
      vapFrac = w->vaporFraction();
      Tcalc = w->temperature();
      dens = w->density();
      Scalc = w->entropy_mass();
      Gcalc = w->gibbs_mass();
      printf(" %10g, %10g, %10g, %11.5g, %11.5g, %11.5g\n", Hset , Tcalc, vapFrac, dens, Scalc, Gcalc);

      deltaH = 100000. * 0.2;
      for (int i = 0; i < 15; i++) {
        Hset += deltaH;
        w->setState_HP(Hset, 45 * OneAtm);
        vapFrac = w->vaporFraction();
        Tcalc = w->temperature();
        dens = w->density();
        Scalc = w->entropy_mass();
        Gcalc = w->gibbs_mass();
        printf(" %10g, %10g, %10g, %11.5g, %11.5g, %11.5g\n", Hset , Tcalc, vapFrac, dens, Scalc, Gcalc);
      }



      printf("Critical Temp     = %10.3g K\n", w->critTemperature());
      printf("Critical Pressure = %10.3g atm\n", w->critPressure()/OneAtm);
      printf("Critical Dens     = %10.3g kg/m3\n", w->critDensity());

      delete w;  
    } catch (CanteraError) {

      showErrors();
      Cantera::appdelete();
      return -1;
    }


    return 0;
}
