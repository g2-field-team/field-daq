#include <iostream>
#include <stdio.h>
#include <string.h>

#include "TTree.h"
#include "TFile.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "/home/newg2/Applications/field-daq/common/src/frontends/core/include/field_structs.hh"
//#include "/home/newg2/Applications/field-daq/common/src/frontends/core/include/field_constants.hh"

using namespace std;
using namespace g2field;

UShort_t trace[ABS_NMR_LENGTH];

int main(int argc, char** argv)
{
  char filename[500];
  int filenumber = atoi(argv[1]);
  sprintf(filename,"AbsoluteProbe_%05d.root",filenumber);
  TFile *filein  = new TFile(filename,"read");
  TTree* t;
  t = (TTree *)filein->Get("t_AbsProbe");
  g2field::absolute_nmr_info_t AbsProbeData;
  t->SetBranchAddress("ABPB",&(AbsProbeData.time_stamp));
  t->SetBranchAddress("NMRTrace",&(trace[0]));
  //Define plots
  TCanvas *c1 = new TCanvas("c1","c1",0,0,800,600);
  TGraph *WaveForm = new TGraph();
  int returnVal = 0;
  int iEvent=0;
  while(1){
    returnVal = t->GetEntry(iEvent);
    if (returnVal==0) break; //eof
    cout << AbsProbeData.length<<endl;
    if (AbsProbeData.length!=0){
      for (unsigned int i=0;i<AbsProbeData.length;i++){
	WaveForm->SetPoint(i,i,trace[i]);
      }
    }
    WaveForm->Draw("APL");
    cin.get();
    delete WaveForm;
    WaveForm = new TGraph();
  iEvent++;
  }
  c1->SaveAs("Plot.pdf");
  return 0;
}
