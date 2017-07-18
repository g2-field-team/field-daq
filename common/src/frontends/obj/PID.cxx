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
      fP           = 0.;
      fI           = 0.;
      fD           = 0.;
      fScaleFactor = 1.0; 
      fSampleTime  = 0.; 
      fLastTime    = 0.; 
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
   }
   //______________________________________________________________________________
   void PID::Print(){
      std::cout << "Setpoint     = " << fSetpoint    << std::endl;
      std::cout << "P            = " << fP           << std::endl;
      std::cout << "I            = " << fI           << std::endl;
      std::cout << "D            = " << fD           << std::endl;
      std::cout << "PTerm        = " << fPTerm       << std::endl;
      std::cout << "ITerm        = " << fITerm       << std::endl;
      std::cout << "DTerm        = " << fDTerm       << std::endl;
      std::cout << "Scale Factor = " << fScaleFactor << std::endl;
      std::cout << "Last Time    = " << fLastTime    << std::endl;
      std::cout << "Sample Time  = " << fSampleTime  << std::endl;
      std::cout << "IntErr       = " << fIntError    << std::endl;
      std::cout << "LastErr      = " << fLastError   << std::endl; 
      std::cout << "Windup Guard = " << fWindupGuard << std::endl; 
   }
   //______________________________________________________________________________
   double PID::Update(double current_time,double meas_value){
      double err  = fSetpoint - meas_value; 
      double dt   = current_time - fLastTime; 
      double derr = err - fLastError;
      UpdatePTerm(err,dt,derr);  
      UpdateITerm(err,dt,derr);  
      UpdateDTerm(err,dt,derr); 
      double output = fScaleFactor*( fPTerm + fI*fITerm + fD*fDTerm );
      // remember values for next calculation 
      fLastTime     = current_time; 
      fLastError    = err; 
      return output;  
   }
   //______________________________________________________________________________
   void PID::UpdatePTerm(double err,double dtime,double derror){
      derror += 0.; 
      if (dtime >= fSampleTime) { 
	 fPTerm = fP*err;
      }
   }
   //______________________________________________________________________________
   void PID::UpdateITerm(double err,double dtime,double derror){
      derror += 0.; 
      if (dtime >= fSampleTime) { 
	 fITerm += err*dtime;
	 if (fITerm< (-1.)*fWindupGuard) {
	    fITerm = (-1.)*fWindupGuard;
	 } else if (fITerm>fWindupGuard) {
	    fITerm = fWindupGuard;
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