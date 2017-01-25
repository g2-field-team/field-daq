#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <math.h>

#include "TTree.h"
#include "TFile.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TH1.h"
#include "/home/newg2/Applications/field-daq/common/src/frontends/core/include/field_structs.hh"
#include "/home/newg2/Applications/field-daq/common/src/frontends/core/include/field_constants.hh"

using namespace std;
using namespace g2field;

int main(int argc, char** argv)
{
  char filename[500];
  int filenumber = atoi(argv[1]);
  sprintf(filename,"TrolleyInterface_%05d.root",filenumber);
  TH1* Frequency = new TH1D("hF","hF",100,50.3942,50.3950);
  TGraph *gFreq = new TGraph();
  gFreq->SetName("freq");
  TFile *filein  = new TFile(filename,"read");
  TTree* t;
  t = (TTree *)filein->Get("t_TrolleySim");
  trolley_nmr_t NMRData;
  trolley_barcode_t BarcodeData;
  trolley_monitor_t MonitorData;
  t->SetBranchAddress("TLNP",&(NMRData));
  t->SetBranchAddress("TLBC",&(BarcodeData));
  t->SetBranchAddress("TLMN",&(MonitorData));
  //Define plots
  TCanvas *c1 = new TCanvas("c1","c1",0,0,800,600);
  vector <TGraph*> graphArray;
  vector <int> zeros;
  vector <short> peakValues;
  vector <short> baselineValues;
  vector <short> peakIndices;
  int returnVal = 0;
  int iEvent;
  int igraph=0;
  while(1){
    returnVal = t->GetEntry(iEvent);
    if (returnVal==0) break; //eof
    if (NMRData.length!=0){
      TGraph *WaveForm = new TGraph();
      zeros.clear();
      WaveForm->SetName(Form("Waveform%d",igraph));
      for (int i=0;i<NMRData.length;i++){
	WaveForm->SetPoint(i,i*0.001,NMRData.trace[i]);
	if (NMRData.trace[i-1]*NMRData.trace[i]<=0 && i>4000 && i<14400){
	  zeros.push_back(i);
	}
      }
      int n = zeros.size();
      //double freq = (1.0/(2.0*double(zeros[n-1]*0.001 - zeros[0]*0.001)/double(zeros.size()-1)))/6.2e7*1e6;
      double t0 = zeros[0]-1+double(NMRData.trace[zeros[0]-1])/double(NMRData.trace[zeros[0]]+NMRData.trace[zeros[0]-1]);
      double tn = zeros[n-1]-1+double(NMRData.trace[zeros[n-1]-1])/double(NMRData.trace[zeros[n-1]]+NMRData.trace[zeros[n-1]-1]);
      long double freq = (1000000.0/(2.0*double(tn-t0)/double(zeros.size()-1)));
      cout << freq<<endl;
      Frequency->Fill(freq);
      gFreq->SetPoint(igraph,igraph,freq);
      graphArray.push_back(WaveForm);
      igraph++;
    }
    iEvent++;
  }
  filein->Close();
  TFile *fileout = new TFile("Waveforms.root","recreate");
  Frequency->Write();
  gFreq->Write();
  for (int i=0;i<igraph;i++){
    graphArray[i]->Write();
    delete graphArray[i];
  }
  fileout->Close();
  delete gFreq;
  delete Frequency;
  return 0;
}
