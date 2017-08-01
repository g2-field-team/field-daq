#include "PID.hh"

namespace g2field { 
   //______________________________________________________________________________
   PID::PID(){
      Init();
   }
   //______________________________________________________________________________
   PID::~PID(){

   }
   //______________________________________________________________________________
   void PID::Init(){
      fP              = 0.;
      fI              = 0.;
      fD              = 0.;
      fI_alt          = 0.;
      fMaxCorrSize    = 0.; 
      fScaleFactor    = 1.0; 
      fSetpoint       = 0.;
      fSampleTime     = 0.; 
      fLastTime       = 0.;
      fWindupGuard    = 20.; 
      fMaxOutput      = 0.;
      fMaxITermOutput = 0.;
      fMaxError       = 0.;  
      Clear();
   }
   //______________________________________________________________________________
   void PID::Clear(){
      fPTerm       = 0.;
      fITerm       = 0.;
      fDTerm       = 0.;
      fITerm_alt   = 0.;
      fIntError    = 0.;
      fLastError   = 0.; 
   }
   //______________________________________________________________________________
   double PID::Update(double current_time,double meas_value){
      double err  = fSetpoint - meas_value;
      double dt   = current_time - fLastTime; 
      double derr = err - fLastError;
      UpdatePTerm(err,dt,derr);  
      UpdateITerm(err,dt,derr);  
      UpdateITerm_alt(err,dt,derr);  
      UpdateDTerm(err,dt,derr); 
      double output = fScaleFactor*( fPTerm + fITerm + fITerm_alt + fD*fDTerm );
      // check against limits (retain the sign too) 
      if(fabs(output)>fMaxOutput) output = ( output/fabs(output) )*fMaxOutput;  
      // remember values for next calculation 
      fLastTime     = current_time; 
      fLastError    = err; 
      fLastOutput   = output; 
      return output;  
   }
   //______________________________________________________________________________
   void PID::UpdatePTerm(double err,double dtime,double derror){
      derror += 0.; 
      double abs_err = fabs(err); 
      double sign    = err/abs_err; 
      if (dtime >= fSampleTime) { 
	 // determine correction based on size of error term 
	 if (abs_err<fMaxCorrSize) {
	    fPTerm = fP*err;
	 } else {
	    fPTerm = fP*sign*fMaxCorrSize;
         }
      }
   }
   //______________________________________________________________________________
   void PID::UpdateITerm(double err,double dtime,double derror){
      derror += 0.; 
      dtime  += 0.; 
      double abs_err = fabs(err); 
      double sign    = err/abs_err; 
      if (dtime >= fSampleTime ) { 
         // determine correction based on size of error term 
	 if (abs_err<fMaxCorrSize) { 
	    fITerm += fI*err;   // multiplying by coeff reduces abrupt changes to future data due to changing the coeff
	 } else { 
	    fITerm += fI*sign*fMaxCorrSize; 
	 }
	 // check against limits (retain the sign too) 
         if(fabs(fITerm)>fMaxITermOutput){
	    fITerm = ( fITerm/fabs(fITerm) )*fMaxITermOutput;
         } 
      }  
   }
   //______________________________________________________________________________
   void PID::UpdateITerm_alt(double err,double dtime,double derror){
      derror += 0.; 
      if (dtime >= fSampleTime) { 
	 fITerm_alt += fI_alt*err*dtime;
	 if (fITerm_alt< (-1.)*fWindupGuard) {
	    fITerm_alt = (-1.)*fWindupGuard;
	 } else if (fITerm_alt>fWindupGuard) {
	    fITerm_alt = fWindupGuard;
	 }
      }
   }
   //______________________________________________________________________________
   void PID::UpdateDTerm(double err,double dtime,double derror){
      fDTerm = 0.;
      err   += 0.;
      if (dtime >= fSampleTime) { 
	 if (dtime>0) fDTerm = derror/dtime;
      } 
   }

}
