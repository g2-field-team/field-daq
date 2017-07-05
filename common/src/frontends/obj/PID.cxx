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
      fSampleTime     = 0.; 
      fLastTime       = 0.;
      fMaxOutput      = 0.;
      fMaxITermOutput = 0.;
      fMaxError       = 0.;  
      Clear();
   }
   //______________________________________________________________________________
   void PID::Clear(){
      fSetpoint    = 0.;
      fPTerm       = 0.;
      fITerm       = 0.;
      fDTerm       = 0.;
      fIntError    = 0.;
      fLastError   = 0.; 
      fWindupGuard = 20.; 
      fITerm_alt   = 0.;
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
      if (dtime >= fSampleTime) { 
	 if (fabs(err)<fMaxError) {
            // if the field change is a reasonable value, then update the P term 
	    fPTerm = fP*err;
	 }
      }
   }
   //______________________________________________________________________________
   void PID::UpdateITerm(double err,double dtime,double derror){
      derror += 0.; 
      if (dtime >= fSampleTime) { 
	 fITerm += fI*err*dtime;
	 if (fITerm< (-1.)*fWindupGuard) {
	    fITerm = (-1.)*fWindupGuard;
	 } else if (fITerm>fWindupGuard) {
	    fITerm = fWindupGuard;
	 }
      }
   }
   //______________________________________________________________________________
   void PID::UpdateITerm_alt(double err,double dtime,double derror){
      derror += 0.; 
      dtime  += 0.; 
      double abs_err = fabs(err); 
      double sign    = err/abs_err; 
      if (dtime >= fSampleTime ) { 
         // determine correction based on size of error term 
	 if (abs_err<fMaxCorrSize) { 
	    fITerm_alt += fI_alt*err;   // multiplying by coeff reduces abrupt changes to future data due to changing the coeff
	 } else { 
	    fITerm_alt += fI_alt*sign*fMaxCorrSize; 
	 }
	 // check against limits (retain the sign too) 
         if( fabs(fITerm_alt)>fMaxITermOutput){
	    fITerm_alt = ( fITerm_alt/fabs(fITerm_alt) )*fMaxITermOutput;
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
