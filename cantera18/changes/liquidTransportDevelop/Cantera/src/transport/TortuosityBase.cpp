/** 
 * @file TortuosityBase.cpp
 *   Base class to compute the increase in diffusive path length associated with 
 * tortuous path diffusion through, for example, porous media.
 */

/*
 * Copywrite (2005) Sandia Corporation. Under the terms of
 * Contract DE-AC04-94AL85000 with Sandia Corporation, the
 * U.S. Government retains certain rights in this software.
 */

/* 
 * $Revision: 572 $
 * $Date: 2010-08-13 20:21:57 -0600 (Fri, 13 Aug 2010) $
 */

#include "TortuosityBase.h"
#include "ctexceptions.h"

#include <string>

namespace Cantera {
  //====================================================================================================================
  static void err(const std::string r) {
    throw Cantera::CanteraError("TortuosityBase", "Error calling base class " + r);
  }
  //====================================================================================================================
  // Default constructor
  TortuosityBase::TortuosityBase() 
  {
  }
  //====================================================================================================================
  // Copy Constructor
  /*
   * @param right  Object to be copied
   */
  TortuosityBase::TortuosityBase(const TortuosityBase &right)
  {
    *this = right;
  }
  //====================================================================================================================
  // Default destructor for TortuosityBase
  TortuosityBase::~TortuosityBase() {

  }
  //====================================================================================================================
  // Assignment operator
  /*
   * @param right Object to be copied
   */
  TortuosityBase &  TortuosityBase::operator=(const TortuosityBase &right) {
    if (&right == this) {
      return *this; 
    }
    return *this;
  }
  //====================================================================================================================
  // Duplication operator
  /*
   *  @return  Returns a pointer to a duplicate of the current object given a 
   *           base class pointer
   */
  TortuosityBase * TortuosityBase::duplMyselfAsTortuosityBase() const {
    TortuosityBase * tb = new TortuosityBase(*this);
    return tb;
  }
  //====================================================================================================================
  // The tortuosity factor models the effective increase in the diffusive transport length.
  /*
   * This method returns \f$ 1/\tau^2 \f$ in the description of the  flux
   *
   *    \f$  C_T D_i \nabla X_i / \tau^2 \f$.  
   *
   *  
   */ 
  doublereal TortuosityBase::tortuosityFactor(doublereal porosity) {
    err("tortuosityFactor");
    return 0.0;
  }
  //====================================================================================================================
  // The McMillan number is the ratio of the flux-like variable to the value it would have without porous flow.
  /* 
   * The McMillan number combines the effect of toruosity 
   * and volume fraction of the transported phase.  The net flux
   * observed is then the product of the McMillan number and the 
   * non-porous transport rate.  For a conductivity in a non-porous 
   * media, \f$ \kappa_0 \f$, the conductivity in the porous media
   * would be \f$ \kappa = (\rm McMillan) \kappa_0 \f$.
   */
  doublereal TortuosityBase::McMillanFactor(doublereal porosity) {
    err("McMillanFactor");
    return 0.0;
  }
  //====================================================================================================================
}
