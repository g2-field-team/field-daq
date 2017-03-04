#include<iostream>
#include<fstream>

using namespace std;

int main()
{
  ofstream output;
  output.open("Test.scr",ios::out);
  for (int j=0;j<=800;j+=50){
    output<<"NMR_Command "<<std::hex<< 0x8000<<endl;
    output<<"NMR_Preamp_Delay "<<  373<<endl;
    output<<"NMR_Preamp_Period "<<  14000<<endl;
    output<<"NMR_ADC_Gate_Delay "<<  0<<endl;
    output<<"NMR_ADC_Gate_Offset "<<  0<<endl;
    output<<"NMR_ADC_Gate_Period "<<  15000<<endl;
    output<<"NMR_TX_Delay "<<  300<<endl;
    output<<"NMR_TX_Period "<<  310<<endl;
    output<<"User_Defined_Data "<<  8606<<endl;
    for (int i=1;i<=16;i++){
      unsigned int x = 0x0 | i;
      output<<"NMR_Command "<<std::hex<< x<<endl;
      output<<"NMR_Preamp_Delay "<<  373<<endl;
      output<<"NMR_Preamp_Period "<<  14000<<endl;
      output<<"NMR_ADC_Gate_Delay "<<  0<<endl;
      output<<"NMR_ADC_Gate_Offset "<<  0<<endl;
      output<<"NMR_ADC_Gate_Period "<<  15000<<endl;
      output<<"NMR_TX_Delay "<<  300<<endl;
      output<<"NMR_TX_Period "<<  310<<endl;
      output<<"User_Defined_Data "<<  8606<<endl;
    }
  }
  output.close();

  return 0;
}

