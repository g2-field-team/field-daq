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
         double fI_alt,fITerm_alt;      // aleternative I term with a max correction size on the scale of ppb  
         double fMaxCorrSize;           // a maximum correction size (for use with I_alt) 
         double fMaxError;              // maximum allowable error term 
         double fLastOutput;            // last output 

	 double fWindupGuard;           // integral accumulation 
	 double fIntError,fLastError;   // integral error, last error 

         double fMaxOutput;             // max output of PID algorithm
         double fMaxITermOutput;        // max output of the integral term 

	 void Init(); 
	 void UpdatePTerm(double err,double dt,double derr);  
	 void UpdateITerm(double err,double dt,double derr);  
	 void UpdateDTerm(double err,double dt,double derr);  
	 void UpdateITerm_alt(double err,double dt,double derr);  

      public: 
	 PID(); 
	 ~PID();  

	 void Clear(); 
	 void Print(); 
	 void SetPCoeff(double p)                { fP              = p;    } 
	 void SetICoeff(double i)                { fI              = i;    } 
	 void SetDCoeff(double d)                { fD              = d;    } 
	 void SetPID(double p,double i,double d) { fP = p; fI = i; fD = d; } 
	 void SetIAltCoeff(double i)             { fI_alt          = i;    }
         void SetMaxCorrSize(double m)           { fMaxCorrSize    = m;    }  
	 void SetSetpoint(double sp)             { fSetpoint       = sp;   } 
	 void SetSampleTime(double st)           { fSampleTime     = st;   } 
	 void SetScaleFactor(double sf)          { fScaleFactor    = sf;   } 
         void SetWindupGuard(double wg)          { fWindupGuard    = wg;   } 
         void SetMaxPIDOutput(double max)        { fMaxOutput      = max;  } 
         void SetMaxITermOutput(double max)      { fMaxITermOutput = max;  } 
         void SetMaxError(double max)            { fMaxError       = max;  } 

	 double GetPCoeff()                const { return fP;              } 
	 double GetICoeff()                const { return fI;              } 
	 double GetDCoeff()                const { return fD;              } 
	 double GetIAltCoeff()             const { return fI_alt;          } 
	 double GetSetpoint()              const { return fSetpoint;       } 
	 double GetSampleTime()            const { return fSampleTime;     } 
	 double GetScaleFactor()           const { return fScaleFactor;    } 
         double GetMaxCorrSize()           const { return fMaxCorrSize;    }
         double GetMaxPIDOutput()          const { return fMaxOutput;      } 
         double GetMaxITermOutput()        const { return fMaxITermOutput; } 
         double GetMaxError()              const { return fMaxError;       } 

	 double Update(double current_time,double meas_value); 
   };

} 

#endif 

