//! @file Oxygen.h
#ifndef TPX_OXYGEN_H
#define TPX_OXYGEN_H

#include "cantera/tpx/Sub.h"

namespace tpx
{
//! Pure species representation of oxygen. Values and functions are
//! from "Thermodynamic Properties in SI" by W.C. Reynolds
class oxygen : public Substance
{
public:
    oxygen() {
        m_name="oxygen";
        m_formula="O2";
    }

    double MolWt();
    double Tcrit();
    double Pcrit();
    double Vcrit();
    double Tmin();
    double Tmax();
    char* name();
    char* formula();

    double Pp();
    double up();
    double sp();

    //! Saturation pressure. Equation S4 from Reynolds TPSI.
    double Psat();

private:
    //! Liquid density. Equation D2 from Reynolds TPSI.
    double ldens();

    //! Equation P4 from Reynolds TPSI.
    double C(int i, double rt, double rt2);
    double Cprime(int i, double rt, double rt2, double rt3);
    double I(int i, double egrho);
    double H(int i, double egrho);
    double W(int i, double egrho);
};

}
#endif // ! OXYGEN_H
