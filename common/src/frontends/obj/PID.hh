#ifndef PID_HH
#define PID_HH

// a PID class for use in the PS feedback operation  

#include <iostream> 
#include <math.h> 

namespace g2field { 

   class PID {
      private: 
	 double fP,fI,fD;
	 double fPTerm,fITerm,fDTerm;
	 double fSampleTime,fLastTime;
	 double fSetpoint;  
	 double fScaleFactor;           // in Amps/Hz 

	 double fWindupGuard;           // integral accumulation 
	 double fIntError,fLastError;   // integral error, last error  

	 void Init(); 
	 void UpdatePTerm(double err,double dt,double derr);  
	 void UpdateITerm(double err,double dt,double derr);  
	 void UpdateDTerm(double err,double dt,double derr);  

      public: 
	 PID(); 
	 ~PID();  

	 void Clear(); 
	 void Print(); 
	 void SetPCoeff(double p)                { fP           = p;    } 
	 void SetICoeff(double i)                { fI           = i;    } 
	 void SetDCoeff(double d)                { fD           = d;    } 
	 void SetPID(double p,double i,double d) { fP = p; fI = i; fD = d;  } 
	 void SetSetpoint(double sp)             { fSetpoint    = sp;   } 
	 void SetSampleTime(double st)           { fSampleTime  = st;   } 
	 void SetScaleFactor(double sf)          { fScaleFactor = sf;   } 
         void SetWindupGuard(double wg)          { fWindupGuard = wg;   } 

	 double GetPCoeff()                const { return fP;           } 
	 double GetICoeff()                const { return fI;           } 
	 double GetDCoeff()                const { return fD;           } 
	 double GetSetpoint()              const { return fSetpoint;    } 
	 double GetSampleTime()            const { return fSampleTime;  } 
	 double GetScaleFactor()           const { return fScaleFactor; } 

	 double Update(double current_time,double meas_value); 
   };

} 

#endif 

