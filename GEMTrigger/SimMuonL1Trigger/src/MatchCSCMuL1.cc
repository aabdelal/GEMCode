// -*- C++ -*-
//
// Package:    SimMuL1
// Class:      MatchCSCMuL1
// 
/**\class MatchCSCMuL1 MatchCSCMuL1.cc MyCode/SimMuL1/src/MatchCSCMuL1.cc

 Description: Trigger Matching info for SimTrack in CSC

 Implementation:
     <Notes on implementation>
*/
//
// Original Author:  "Vadim Khotilovich"
//         Created:  Mon May  5 20:50:43 CDT 2008
// $Id$
//
//


#include "MatchCSCMuL1.h"

// system include files
#include <memory>
#include <cmath>
#include <set>

#include "DataFormats/Math/interface/normalizedPhi.h"
#include "DataFormats/Math/interface/deltaPhi.h"
#include "DataFormats/Math/interface/deltaR.h"

#include <L1Trigger/CSCCommonTrigger/interface/CSCConstants.h>
#include <L1Trigger/CSCCommonTrigger/interface/CSCTriggerGeometry.h>


using namespace std;
using namespace reco;
using namespace edm;

namespace 
{
const double MUON_MASS = 0.105658369 ; // PDG06

double ptscale[33] = { 
  -1.,   0.0,   1.5,   2.0,   2.5,   3.0,   3.5,   4.0,
  4.5,   5.0,   6.0,   7.0,   8.0,  10.0,  12.0,  14.0,  
  16.0,  18.0,  20.0,  25.0,  30.0,  35.0,  40.0,  45.0, 
  50.0,  60.0,  70.0,  80.0,  90.0, 100.0, 120.0, 140.0, 1.E6
};

}




MatchCSCMuL1::MatchCSCMuL1(const SimTrack  *s, const SimVertex *v, const CSCGeometry* g):
    strk(s), svtx(v), cscGeometry(g)
{
  double endcap = (strk->momentum().eta() >= 0) ? 1. : -1.;
  math::XYZVectorD v0(0.000001,0.,endcap);
  pME11 = pME1 = pME2 = pME3 = v0;
}


void 
MatchCSCMuL1::addSimHit( PSimHit & h)
{
  simHits.push_back(h);
  hitsMapLayer[h.detUnitId()].push_back(h);
  CSCDetId layerId( h.detUnitId() );
  hitsMapChamber[ layerId.chamberId().rawId() ].push_back(h);
}


int 
MatchCSCMuL1::keyStation()
{
 // first try ME2
 if (pME2.Rho()>0.01) return 2;
 // next, try ME3
 if (pME3.Rho()>0.01) return 3;
 // last, try ME1
 if (pME1.Rho()>0.01) return 1;
 return 99;
}

math::XYZVectorD
MatchCSCMuL1::vAtStation(int st)
{
  switch (st){
  case 0: return pME11; break;
  case 1: return pME1; break;
  case 2: return pME2; break;
  case 3: return pME3; break;
  }
  double endcap = (strk->momentum().eta() >= 0) ? 1. : -1.;
  math::XYZVectorD v0(0.000001,0.,endcap);
  return v0;
}

math::XYZVectorD
MatchCSCMuL1::vSmart()
{
  int key_st = keyStation();
  return vAtStation(key_st);
}

double
MatchCSCMuL1::deltaRAtStation(int station, double to_eta, double to_phi)
{
  math::XYZVectorD v = vAtStation(station);
  if (v.Rho()<0.01) return 999.;
  return deltaR(v.eta(),normalizedPhi(v.phi()), to_eta,to_phi);
}


double
MatchCSCMuL1::deltaRSmart(double to_eta, double to_phi)
{
  int key_st = keyStation();
  return deltaRAtStation(key_st, to_eta,to_phi);
}



int 
MatchCSCMuL1::nSimHits()
{
  if (!muOnly) return simHits.size();
  int n=0;
  for (unsigned j=0; j<simHits.size(); j++) if (abs(simHits[j].particleType())==13 ) n++;
  return n;
}


vector<int> 
MatchCSCMuL1::detsWithHits()
{
  set<int> dets;
  map<int, vector<PSimHit> >::const_iterator mapItr = hitsMapLayer.begin();
  for( ; mapItr != hitsMapLayer.end(); ++mapItr) 
    if ( !muOnly || abs((mapItr->second)[0].particleType())==13 ) 
      dets.insert(mapItr->first);
  vector<int> result( dets.begin() , dets.end() );
  return result;
}


vector<int> 
MatchCSCMuL1::chambersWithHits(int station, int ring, unsigned minNHits)
{
  set<int> chambers;
  set<int> layersWithHits;
  
  map<int, vector<PSimHit> >::const_iterator mapItr = hitsMapChamber.begin();
  for( ; mapItr != hitsMapChamber.end(); ++mapItr){
    CSCDetId cid(mapItr->first);
    if (station && cid.station() != station) continue;
    if (ring && cid.ring() != ring) continue;
    layersWithHits.clear();
    for (unsigned i=0; i<(mapItr->second).size(); i++)
      if ( !muOnly || abs((mapItr->second)[i].particleType())==13 ) {
        CSCDetId hid((mapItr->second)[i].detUnitId());
        layersWithHits.insert(hid.layer());
      }
    if (layersWithHits.size()>=minNHits) chambers.insert( mapItr->first );
  }
  vector<int> result( chambers.begin() , chambers.end() );
  return result;
}


vector<PSimHit>
MatchCSCMuL1::layerHits(int detId)
{
  vector<PSimHit> result;
  std::map<int, vector<PSimHit> >::const_iterator mapItr = hitsMapLayer.find(detId);
  if (mapItr == hitsMapLayer.end()) return result;
  for (unsigned i=0; i<(mapItr->second).size(); i++)
    if ( !muOnly || abs((mapItr->second)[i].particleType())==13 ) 
      result.push_back((mapItr->second)[i]);
  return result;
}


vector<PSimHit>
MatchCSCMuL1::chamberHits(int detId)
{
  // foolproof chamber id
  CSCDetId dId(detId);
  CSCDetId chamberId = dId.chamberId();

  vector<PSimHit> result;
  std::map<int, vector<PSimHit> >::const_iterator mapItr = hitsMapChamber.find(chamberId);
  if (mapItr == hitsMapChamber.end()) return result;
  for (unsigned i=0; i<(mapItr->second).size(); i++)
    if ( !muOnly || abs((mapItr->second)[i].particleType())==13 ) 
      result.push_back((mapItr->second)[i]);

  return result;
}


std::vector<PSimHit> 
MatchCSCMuL1::allSimHits()
{
  if (!muOnly) return simHits;
  vector<PSimHit> result;
  for (unsigned j=0; j<simHits.size(); j++) if (abs(simHits[j].particleType())==13 ) result.push_back(simHits[j]);
  return result;
}


int
MatchCSCMuL1::numberOfLayersWithHitsInChamber(int detId)
{
  set<int> layersWithHits;

  vector<PSimHit> chHits = chamberHits(detId);
  for (unsigned i = 0; i < chHits.size(); i++) 
  {
    CSCDetId hid(chHits[i].detUnitId());
    if ( !muOnly || abs(chHits[i].particleType())==13 ) 
      layersWithHits.insert(hid.layer());
  }
  return layersWithHits.size();
}


std::pair<int,int>
MatchCSCMuL1::wireGroupAndStripInChamber( int detId )
{
  std::pair<int,int> err_pair(-1,-1);
  
  vector<PSimHit> hits = chamberHits( detId );
  unsigned n = hits.size();
  if ( n == 0 ) return err_pair;

  if (CSCConstants::KEY_CLCT_LAYER != CSCConstants::KEY_ALCT_LAYER)  cout<<"ALARM: KEY_CLCT_LAYER != KEY_ALCT_LAYER"<<endl;

  // find LocalPoint of the highest energy muon simhit in key layer
  // if no hit in key layer, take the highest energy muon simhit local position
  LocalPoint lpkey(0.,0.,0.), lphe(0.,0.,0.);
  double elosskey=-1., eloss=-1.;
  for (unsigned i = 0; i < n; i++)
  {
    CSCDetId lid(hits[i].detUnitId());
    double el = hits[i].energyLoss();
    if ( el > eloss ) {
      lphe = hits[i].localPosition();
      eloss = el;
    }
    if (lid.layer() != CSCConstants::KEY_ALCT_LAYER) continue;
    if ( el > elosskey ) {
      lpkey = hits[i].localPosition();
      elosskey = el;
    }
  }
  LocalPoint theLP = lpkey;
  if (elosskey<0.) theLP = lphe;
  const LocalPoint cLP = theLP;
  
  CSCDetId keyID(detId + CSCConstants::KEY_ALCT_LAYER);
  const CSCLayer* csclayer = cscGeometry->layer(keyID);
  int hitWireG = csclayer->geometry()->wireGroup(csclayer->geometry()->nearestWire(cLP));
  int hitStrip = csclayer->geometry()->nearestStrip(cLP);

  std::pair<int,int> ws(hitWireG,hitStrip);
  return ws;
}


bool
MatchCSCMuL1::hasHitsInStation(int st, unsigned minNHits) // st=0 - any,  st=1,2,3,4 - ME1-4
{
  vector<int> chIds = chambersWithHits(st,0,minNHits);
  if (chIds.size()==0) return false;
  return true;
}


unsigned
MatchCSCMuL1::nStationsWithHits(bool me1, bool me2, bool me3, bool me4, unsigned minNHits)
{
  return ( (me1 & hasHitsInStation(1,minNHits))
         + (me2 & hasHitsInStation(2,minNHits))
         + (me3 & hasHitsInStation(3,minNHits))
         + (me4 & hasHitsInStation(4,minNHits)) );
}



std::vector< MatchCSCMuL1::ALCT > 
MatchCSCMuL1::ALCTsInReadOut()
{
  vector<ALCT> result;
  for (unsigned i=0; i<ALCTs.size();i++) 
    if ( ALCTs[i].inReadOut() ) result.push_back(ALCTs[i]);
  return result;
}

std::vector< MatchCSCMuL1::ALCT >
MatchCSCMuL1::vALCTs(bool readout)
{
  if (readout) return ALCTsInReadOut();
  return ALCTs;
}

std::vector<int>
MatchCSCMuL1::chambersWithALCTs(bool readout)
{
  vector<ALCT> tALCTs = vALCTs(readout);
  set<int> chambers;
  for (unsigned i=0; i<tALCTs.size();i++) chambers.insert( tALCTs[i].id.rawId() );
  vector<int> result( chambers.begin() , chambers.end() );
  return result;
}

std::vector<MatchCSCMuL1::ALCT>
MatchCSCMuL1::chamberALCTs( int detId, bool readout )
{
  vector<ALCT> tALCTs = vALCTs(readout);
  // foolproof chamber id
  CSCDetId dId(detId);
  CSCDetId chamberId = dId.chamberId();
  vector<ALCT> result;
  for (unsigned i=0; i<tALCTs.size();i++) 
    if ( tALCTs[i].id == chamberId ) result.push_back(tALCTs[i]);
  return result;
}

std::vector<int> 
MatchCSCMuL1::bxsWithALCTs( int detId, bool readout )
{
  vector<MatchCSCMuL1::ALCT> v = chamberALCTs( detId, readout );
  set<int> bxs;
  for (unsigned i=0; i<v.size();i++) bxs.insert( v[i].getBX() );
  vector<int> result( bxs.begin() , bxs.end() );
  return result;
}

std::vector<MatchCSCMuL1::ALCT> 
MatchCSCMuL1::chamberALCTsInBx( int detId, int bx, bool readout )
{
  vector<MatchCSCMuL1::ALCT> v = chamberALCTs( detId, readout );
  vector<MatchCSCMuL1::ALCT> result;
  for (unsigned i=0; i<v.size();i++)
    if ( v[i].getBX() == bx ) result.push_back(v[i]);
  return result;
}


std::vector< MatchCSCMuL1::CLCT >
MatchCSCMuL1::CLCTsInReadOut()
{
  vector<CLCT> result;
  for (unsigned i=0; i<CLCTs.size();i++)
    if ( CLCTs[i].inReadOut() ) result.push_back(CLCTs[i]);
  return result;
}

std::vector< MatchCSCMuL1::CLCT >
MatchCSCMuL1::vCLCTs(bool readout)
{
  if (readout) return CLCTsInReadOut();
  return CLCTs;
}

std::vector<int>
MatchCSCMuL1::chambersWithCLCTs( bool readout)
{
  vector<CLCT> tCLCTs = vCLCTs(readout);
  set<int> chambers;
  for (unsigned i=0; i<tCLCTs.size();i++) chambers.insert( tCLCTs[i].id.rawId() );
  vector<int> result( chambers.begin() , chambers.end() );
  return result;
}

std::vector<MatchCSCMuL1::CLCT>
MatchCSCMuL1::chamberCLCTs( int detId, bool readout )
{
  vector<CLCT> tCLCTs = vCLCTs(readout);
  // foolproof chamber id
  CSCDetId dId(detId);
  CSCDetId chamberId = dId.chamberId();
  vector<CLCT> result;
  for (unsigned i=0; i<tCLCTs.size();i++) 
    if ( tCLCTs[i].id == chamberId ) result.push_back(tCLCTs[i]);
  return result;
}

std::vector<int>
MatchCSCMuL1::bxsWithCLCTs( int detId, bool readout )
{
  vector<MatchCSCMuL1::CLCT> v = chamberCLCTs( detId, readout );
  set<int> bxs;
  for (unsigned i=0; i<v.size();i++) bxs.insert( v[i].getBX() );
  vector<int> result( bxs.begin() , bxs.end() );
  return result;
}

std::vector<MatchCSCMuL1::CLCT>
MatchCSCMuL1::chamberCLCTsInBx( int detId, int bx, bool readout )
{
  vector<MatchCSCMuL1::CLCT> v = chamberCLCTs( detId, readout );
  vector<MatchCSCMuL1::CLCT> result;
  for (unsigned i=0; i<v.size();i++)
    if ( v[i].getBX() == bx ) result.push_back(v[i]);
  return result;
}


std::vector< MatchCSCMuL1::LCT >
MatchCSCMuL1::LCTsInReadOut()
{
  vector<LCT> result;
  for (unsigned i=0; i<LCTs.size();i++)
    if ( LCTs[i].inReadOut() ) result.push_back(LCTs[i]);
  return result;
}

std::vector< MatchCSCMuL1::LCT >
MatchCSCMuL1::vLCTs(bool readout)
{
  if (readout) return LCTsInReadOut();
  return LCTs;
}

std::vector<int>
MatchCSCMuL1::chambersWithLCTs( bool readout )
{
  vector<LCT> tLCTs = vLCTs(readout);
  set<int> chambers;
  for (unsigned i=0; i<tLCTs.size();i++) chambers.insert( tLCTs[i].id.rawId() );
  vector<int> result( chambers.begin() , chambers.end() );
  return result;
}

std::vector<MatchCSCMuL1::LCT>
MatchCSCMuL1::chamberLCTs( int detId, bool readout )
{
  vector<LCT> tLCTs = vLCTs(readout);
  // foolproof chamber id
  CSCDetId dId(detId);
  CSCDetId chamberId = dId.chamberId();
  vector<LCT> result;
  for (unsigned i=0; i<tLCTs.size();i++) 
    if ( tLCTs[i].id == chamberId ) result.push_back(tLCTs[i]);
  return result;
}

std::vector<MatchCSCMuL1::LCT*>
MatchCSCMuL1::chamberLCTsp( int detId, bool readout )
{
  // foolproof chamber id
  CSCDetId dId(detId);
  CSCDetId chamberId = dId.chamberId();
  vector<LCT*> result;
  for (unsigned i=0; i<LCTs.size();i++) {
    if ( readout && !(LCTs[i].inReadOut()) ) continue;
    if ( LCTs[i].id == chamberId ) result.push_back( &(LCTs[i]) );
  }
  return result;
}

std::vector<int>
MatchCSCMuL1::bxsWithLCTs( int detId, bool readout )
{
  vector<MatchCSCMuL1::LCT> v = chamberLCTs( detId, readout );
  set<int> bxs;
  for (unsigned i=0; i<v.size();i++) bxs.insert( v[i].getBX() );
  vector<int> result( bxs.begin() , bxs.end() );
  return result;
}

std::vector<MatchCSCMuL1::LCT>
MatchCSCMuL1::chamberLCTsInBx( int detId, int bx, bool readout )
{
  vector<MatchCSCMuL1::LCT> v = chamberLCTs( detId, readout );
  vector<MatchCSCMuL1::LCT> result;
  for (unsigned i=0; i<v.size();i++)
    if ( v[i].getBX() == bx ) result.push_back(v[i]);
  return result;
}


std::vector< MatchCSCMuL1::MPLCT >
MatchCSCMuL1::MPLCTsInReadOut()
{
  vector<MPLCT> result;
  for (unsigned i=0; i<MPLCTs.size();i++)
    if ( MPLCTs[i].inReadOut() ) result.push_back(MPLCTs[i]);
  return result;
}

std::vector< MatchCSCMuL1::MPLCT >
MatchCSCMuL1::vMPLCTs(bool readout)
{
  if (readout) return MPLCTsInReadOut();
  return MPLCTs;
}

std::vector<int>
MatchCSCMuL1::chambersWithMPLCTs(bool readout)
{
  vector<MPLCT> tMPLCTs = vMPLCTs(readout);
  set<int> chambers;
  for (unsigned i=0; i<tMPLCTs.size();i++) chambers.insert( tMPLCTs[i].id.rawId() );
  vector<int> result( chambers.begin() , chambers.end() );
  return result;
}

std::vector<MatchCSCMuL1::MPLCT>
MatchCSCMuL1::chamberMPLCTs( int detId, bool readout )
{
  vector<MPLCT> tMPLCTs = vMPLCTs(readout);
  // foolproof chamber id
  CSCDetId dId(detId);
  CSCDetId chamberId = dId.chamberId();
  vector<MPLCT> result;
  for (unsigned i=0; i<tMPLCTs.size();i++) 
    if ( tMPLCTs[i].id == chamberId ) result.push_back(tMPLCTs[i]);
  return result;
}

std::vector<int>
MatchCSCMuL1::bxsWithMPLCTs( int detId, bool readout )
{
  vector<MatchCSCMuL1::MPLCT> v = chamberMPLCTs( detId, readout );
  set<int> bxs;
  for (unsigned i=0; i<v.size();i++) bxs.insert( v[i].trgdigi->getBX() );
  vector<int> result( bxs.begin() , bxs.end() );
  return result;
}

std::vector<MatchCSCMuL1::MPLCT>
MatchCSCMuL1::chamberMPLCTsInBx( int detId, int bx, bool readout )
{
  vector<MatchCSCMuL1::MPLCT> v = chamberMPLCTs( detId, readout );
  vector<MatchCSCMuL1::MPLCT> result;
  for (unsigned i=0; i<v.size();i++)
    if ( v[i].trgdigi->getBX() == bx ) result.push_back(v[i]);
  return result;
}


void
MatchCSCMuL1::print (const char msg[300], bool psimtr, bool psimh,
              bool palct, bool pclct, bool plct, bool pmplct,
	      bool ptftrack, bool ptfcand)
{
  cout<<"####### MATCH PRINT: "<<msg<<" #######"<<endl;
  
  bool DETAILED_HIT_LAYERS = 0;
  
  if (psimtr) 
  {
    cout<<"****** SimTrack: id="<<strk->trackId()<<"  pt="<<sqrt(strk->momentum().perp2())
        <<"  eta="<<strk->momentum().eta()<<"  phi="<<normalizedPhi( strk->momentum().phi()) 
	<<"   nSimHits="<<simHits.size()<<endl;
    cout<<"                 nALCT="<<ALCTs.size()<<"    nCLCT="<<CLCTs.size()<<"    nLCT="<<LCTs.size()<<"    nMPLCT="<<MPLCTs.size()
        <<"    nTFTRACK="<<TFTRACKs.size()<<"    nTFCAND="<<TFCANDs.size()<<endl;
   
    int nALCTok=0,nCLCTok=0,nLCTok=0,nMPLCTok=0,nTFTRACKok=0;
    for(size_t i=0; i<ALCTs.size(); i++) if (ALCTs[i].deltaOk) nALCTok++;
    for(size_t i=0; i<CLCTs.size(); i++) if (CLCTs[i].deltaOk) nCLCTok++;
    for(size_t i=0; i<LCTs.size(); i++)  if (LCTs[i].deltaOk)  nLCTok++;
    for(size_t i=0; i<MPLCTs.size(); i++) if (MPLCTs[i].deltaOk) nMPLCTok++;
    int nok=0;
    for(size_t i=0; i<TFTRACKs.size(); i++) {
      for (size_t s=0; s<TFTRACKs[i].mplcts.size(); s++) if (TFTRACKs[i].mplcts[s]->deltaOk) nok++;
      if (nok>1) nTFTRACKok++;
    }
    cout<<"                 nALCTok="<<nALCTok<<"  nCLCTok="<<nCLCTok<<"  nLCTok="<<nLCTok<<"  nMPLCTok="<<nMPLCTok
        <<"  nTFTRACKok="<<nTFTRACKok<<endl;
    cout<<"                 eta, phi at ";
    for (size_t i=0; i<4; i++){
      math::XYZVectorD v = vAtStation(i);
      int st=i;
      if (i==0) st=11;
      cout<<"  ME"<<st<<": "<<v.eta()<<","<<v.phi();
    }
    cout<<endl;
    int key_st = keyStation();
    double dr_smart = deltaRSmart(strk->momentum().eta() , strk->momentum().phi());
    cout<<"                 DR to initial dir at station "<<key_st<<" is "<<dr_smart<<endl;
  }
  
  if (psimh) 
  {
    cout<<"****** SimTrack hits: total="<< nSimHits()<<" (is mu only ="<<muOnly<<"), in "<<hitsMapChamber.size()<<" chambers, in "<<hitsMapLayer.size()<<" detector IDs"<<endl;

    //self check 
    unsigned ntot=0;
    std::map<int, vector<PSimHit> >::const_iterator mapItr = hitsMapChamber.begin();
    for (; mapItr != hitsMapChamber.end(); mapItr++) 
    {
      unsigned nltot=0;
      std::map<int, vector<PSimHit> >::const_iterator lmapItr = hitsMapLayer.begin();
      for (; lmapItr != hitsMapLayer.end(); lmapItr++) 
      {
        CSCDetId lId(lmapItr->first);
        if (mapItr->first == (int)lId.chamberId().rawId()) nltot += hitsMapLayer[lmapItr->first].size();
      }
      if ( nltot != hitsMapChamber[mapItr->first].size() )
        cout<<" SELF CHACK ALARM!!! : chamber "<<mapItr->first<<" sum of hits in layers = "<<nltot<<" != # of hits in chamber "<<hitsMapChamber[mapItr->first].size()<<endl;
      ntot += nltot;
    }
    if (ntot != simHits.size()) 
      cout<<" SELF CHACK ALARM!!! : ntot hits in chambers = "<<ntot<<"!= simHits.size()"<<endl;
    

    vector<int> chIds = chambersWithHits(0,0,1);
    for (size_t ch = 0; ch < chIds.size(); ch++) {
      CSCDetId chid(chIds[ch]);
      std::pair<int,int> ws = wireGroupAndStripInChamber(chIds[ch]);
      cout<<"  chamber "<<chIds[ch]<<"   "<<chid<<"    #layers with hits = "<<numberOfLayersWithHitsInChamber(chIds[ch])<<"  w="<<ws.first<<"  s="<<ws.second<<endl;
      vector<PSimHit> chHits;
      if(DETAILED_HIT_LAYERS) chHits = chamberHits(chIds[ch]);
      for (unsigned i = 0; i < chHits.size(); i++) 
      {
        CSCDetId hid(chHits[i].detUnitId());
        cout<<"    L:"<<hid.layer()<<" "<<chHits[i]<<" "<<hid<<"  "<<chHits[i].momentumAtEntry()
	    <<" "<<chHits[i].energyLoss()<<" "<<chHits[i].particleType()<<" "<<chHits[i].trackId()<<endl;
      }
    }
//    for (unsigned j=0; j<simHits.size(); j++) {
//      CSCDetId hid(simHits[j].detUnitId());
//      cout<<"    "<<simHits[j]<<" "<<hid<<"  "<<simHits[j].momentumAtEntry()
//	  <<" "<<simHits[j].energyLoss()<<" "<<simHits[j].particleType()<<" "<<simHits[j].trackId()<<endl;
//    }
  }
  
  if (palct) 
  {
    std::vector<int> chs = chambersWithALCTs();
    cout<<"****** match ALCTs: total="<< ALCTs.size()<<" in "<<chs.size()<<" chambers"<<endl;
    for (size_t c=0; c<chs.size(); c++)
    {
      std::vector<int> bxs = bxsWithALCTs( chs[c] );
      CSCDetId id(chs[c]);
      cout<<" ***** chamber "<<chs[c]<<"  "<<id<<"  has "<<bxs.size()<<" ALCT bxs"<<endl;
      for (size_t b=0; b<bxs.size(); b++)
      {
	std::vector<ALCT> stubs = chamberALCTsInBx( chs[c], bxs[b] );
	cout<<"   *** bx "<<bxs[b]<<" has "<<stubs.size()<<" ALCTs"<<endl;
	for (size_t i=0; i<stubs.size(); i++)
	{
	  cout<<"     * ALCT: "<<*(stubs[i].trgdigi)<<endl;
	  cout<<"       inReadOut="<<stubs[i].inReadOut()<<"  eta="<<stubs[i].eta<<"  deltaWire="<<stubs[i].deltaWire<<" deltaOk="<<stubs[i].deltaOk<<endl;
	  cout<<"       matched simhits to ALCT n="<<stubs[i].simHits.size()<<" nHitsShared="<<stubs[i].nHitsShared<<endl;
	  if (psimh) for (unsigned h=0; h<stubs[i].simHits.size();h++) 
	    cout<<"     "<<(stubs[i].simHits)[h]<<" "<<(stubs[i].simHits)[h].exitPoint()
		<<"  "<<(stubs[i].simHits)[h].momentumAtEntry()<<" "<<(stubs[i].simHits)[h].energyLoss()
		<<" "<<(stubs[i].simHits)[h].particleType()<<" "<<(stubs[i].simHits)[h].trackId()<<endl;
	}
      }
    }
  }
  
  if (pclct) 
  {
    std::vector<int> chs = chambersWithCLCTs();
    cout<<"****** match CLCTs: total="<< CLCTs.size()<<" in "<<chs.size()<<" chambers"<<endl;
    for (size_t c=0; c<chs.size(); c++)
    {
      std::vector<int> bxs = bxsWithCLCTs( chs[c] );
      CSCDetId id(chs[c]);
      cout<<" ***** chamber "<<chs[c]<<"  "<<id<<"  has "<<bxs.size()<<" CLCT bxs"<<endl;
      for (size_t b=0; b<bxs.size(); b++)
      {
	std::vector<CLCT> stubs = chamberCLCTsInBx( chs[c], bxs[b] );
	cout<<"   *** bx "<<bxs[b]<<" has "<<stubs.size()<<" CLCTs"<<endl;
	for (size_t i=0; i<stubs.size(); i++)
	{
	  cout<<"     * CLCT: "<<*(stubs[i].trgdigi)<<endl;
          cout<<"       inReadOut="<<stubs[i].inReadOut()<<"  phi="<<stubs[i].phi<<"  deltaStrip="<<stubs[i].deltaStrip<<" deltaOk="<<stubs[i].deltaOk<<endl;
	  cout<<"       matched simhits to CLCT n="<<stubs[i].simHits.size()<<" nHitsShared="<<stubs[i].nHitsShared<<endl;
	  if (psimh) for (unsigned h=0; h<stubs[i].simHits.size();h++) 
	    cout<<"     "<<(stubs[i].simHits)[h]<<" "<<(stubs[i].simHits)[h].exitPoint()
		<<"  "<<(stubs[i].simHits)[h].momentumAtEntry()<<" "<<(stubs[i].simHits)[h].energyLoss()
		<<" "<<(stubs[i].simHits)[h].particleType()<<" "<<(stubs[i].simHits)[h].trackId()<<endl;
	}
      }
    }
  }

  if (plct)
  {
    std::vector<int> chs = chambersWithLCTs();
    cout<<"****** match LCTs: total="<< LCTs.size()<<" in "<<chs.size()<<" chambers"<<endl;
    for (size_t c=0; c<chs.size(); c++)
    {
      std::vector<int> bxs = bxsWithLCTs( chs[c] );
      CSCDetId id(chs[c]);
      cout<<" ***** chamber "<<chs[c]<<"  "<<id<<"  has "<<bxs.size()<<" LCT bxs"<<endl;
      for (size_t b=0; b<bxs.size(); b++)
      {
	std::vector<LCT> stubs = chamberLCTsInBx( chs[c], bxs[b] );
	cout<<"   *** bx "<<bxs[b]<<" has "<<stubs.size()<<" LCTs"<<endl;
	for (size_t i=0; i<stubs.size(); i++)
	{
	  bool matchALCT = (stubs[i].alct != 0), matchCLCT = (stubs[i].clct != 0);
	  cout<<"     * LCT: "<<*(stubs[i].trgdigi);
	  cout<<"         is ghost="<<stubs[i].ghost<<"  inReadOut="<<stubs[i].inReadOut()
	      <<"  found assiciated ALCT="<< matchALCT <<" CLCT="<< matchCLCT <<endl;
	  if (matchALCT && matchCLCT)
	    cout<<"         BX(A)-BX(C)="<<stubs[i].alct->getBX() - stubs[i].clct->getBX()
		<<"  deltaWire="<<stubs[i].alct->deltaWire<<"  deltaStrip="<<stubs[i].clct->deltaStrip
		<<"  deltaOk="<<stubs[i].deltaOk<<"="<<stubs[i].alct->deltaOk<<"&"<<stubs[i].clct->deltaOk<<endl;
	  
	}
      }
    }
  }

  if (pmplct)
  {
    std::vector<int> chs = chambersWithMPLCTs();
    cout<<"****** match MPLCTs: total="<< MPLCTs.size()<<" in "<<chs.size()<<" chambers"<<endl;
    for (size_t c=0; c<chs.size(); c++)
    {
      std::vector<int> bxs = bxsWithMPLCTs( chs[c] );
      CSCDetId id(chs[c]);
      cout<<" ***** chamber "<<chs[c]<<"  "<<id<<"  has "<<bxs.size()<<" MPLCT bxs"<<endl;
      for (size_t b=0; b<bxs.size(); b++)
      {
	std::vector<MPLCT> stubs = chamberMPLCTsInBx( chs[c], bxs[b] );
	cout<<"   *** bx "<<bxs[b]<<" has "<<stubs.size()<<" MPLCTs"<<endl;
	for (size_t i=0; i<stubs.size(); i++)
	{
	  bool matchLCT = (stubs[i].lct != 0);
	  cout<<"     * MPLCT: "<<*(stubs[i].trgdigi);
	  cout<<"         is ghost="<<stubs[i].ghost<<"  inReadOut="<<stubs[i].inReadOut()
	      <<"  found associated LCT="<<matchLCT<<endl;
	  if (matchLCT) {
	    if (stubs[i].lct->alct != 0 && stubs[i].lct->clct != 0) 
              cout<<"         BX(A)-BX(C)="<<stubs[i].lct->alct->getBX() - stubs[i].lct->clct->getBX()
		<<"  deltaWire="<<stubs[i].lct->alct->deltaWire<<"  deltaStrip="<<stubs[i].lct->clct->deltaStrip
		<<"  deltaOk="<<stubs[i].deltaOk<<"="<<stubs[i].lct->alct->deltaOk<<"&"<<stubs[i].lct->clct->deltaOk<<endl;
          }
	  else cout<<"       deltaOk="<<stubs[i].deltaOk<<endl;
	}
      }
    }
  }
  
  if (ptftrack){}
  
  if (ptfcand)
  {
    cout<<"--- match TFCANDs: total="<< TFCANDs.size()<<endl;
    for (size_t i=0; i<TFCANDs.size(); i++)
    {
      char tfi[4];
      sprintf(tfi," TFTrack %lu",i);
      if (TFCANDs[i].tftrack) TFCANDs[i].tftrack->print(tfi);
      else cout<<"Strange: tfcand "<<i<<" has no tftrack!!!"<<endl;
    }
  }
  
  cout<<"####### END MATCH PRINT #######"<<endl;
}

MatchCSCMuL1::ALCT * MatchCSCMuL1::bestALCT(CSCDetId id, bool readout)
{
  if (ALCTs.size()==0) return NULL;
  //double minDY=9999.;
  int minDW=9999;
  unsigned minN=99;
  for (unsigned i=0; i<ALCTs.size();i++) 
    if (id.chamberId().rawId() == ALCTs[i].id.chamberId().rawId())
      if (!readout || ALCTs[i].inReadOut())
	//if (fabs(ALCTs[i].deltaY)<minDY) { minDY = fabs(ALCTs[i].deltaY); minN=i;}
	if (abs(ALCTs[i].deltaWire)<minDW) { minDW = abs(ALCTs[i].deltaWire); minN=i;}
  if (minN==99) return NULL;
  return &(ALCTs[minN]);
}

MatchCSCMuL1::CLCT * MatchCSCMuL1::bestCLCT(CSCDetId id, bool readout)
{
  if (CLCTs.size()==0) return NULL;
  //double minDY=9999.;
  int minDS=9999;
  unsigned minN=99;
  for (unsigned i=0; i<CLCTs.size();i++) 
    if (id.chamberId().rawId() == CLCTs[i].id.chamberId().rawId())
      if (!readout || CLCTs[i].inReadOut())
	//if (fabs(CLCTs[i].deltaY)<minDY) { minDY = fabs(CLCTs[i].deltaY); minN=i;}
	if (abs(CLCTs[i].deltaStrip)<minDS) { minDS = abs(CLCTs[i].deltaStrip); minN=i;}
  if (minN==99) return NULL;
  return &(CLCTs[minN]);
}


MatchCSCMuL1::TFTRACK * MatchCSCMuL1::bestTFTRACK(std::vector< TFTRACK > & tracks, bool sortPtFirst)
{
  if (tracks.size()==0) return NULL;
  
//  determine max # of matched stubs
  int maxNMatchedMPC = 0;
  for (unsigned i=0; i<tracks.size(); i++) 
  {
    int nst=0;
    for (size_t s=0; s<tracks[i].ids.size(); s++) 
      if (tracks[i].mplcts[s]->deltaOk) nst++;
    if (nst>maxNMatchedMPC) maxNMatchedMPC = nst;
  }
// collect tracks with max # of matched stubs
  std::vector<unsigned> bestMatchedTracks;
  for (unsigned i=0; i<tracks.size(); i++) 
  {
    int nst=0;
    for (size_t s=0; s<tracks[i].ids.size(); s++) 
      if (tracks[i].mplcts[s]->deltaOk) nst++;
    if (nst==maxNMatchedMPC) bestMatchedTracks.push_back(i);
  }
  if (bestMatchedTracks.size()==1) return &(tracks[bestMatchedTracks[0]]);
  
// first sort by quality
  double qBase  = 1000000.;
// then sort by Pt inside the cone (if sortPtFirst), then sort by DR
  double ptBase = 0.;
  if (sortPtFirst) ptBase = 1000.;
  unsigned maxI = 99;
  double maxRank = -999999.;
  for (unsigned i=0; i<tracks.size(); i++) 
  {
    if (bestMatchedTracks.size()) {
      bool gotit=0;
      for (unsigned m=0;m<bestMatchedTracks.size();m++) if (bestMatchedTracks[m]==i) gotit=1;
      if (!gotit) continue;
    }
    double rr = qBase*tracks[i].q_packed + ptBase*tracks[i].pt_packed + 1./(0.01 + tracks[i].dr);
    if (rr > maxRank) { maxRank = rr; maxI = i;}
  }
  if (maxI==99) return NULL;
  return &(tracks[maxI]);
}


MatchCSCMuL1::TFCAND * MatchCSCMuL1::bestTFCAND(std::vector< TFCAND > & cands, bool sortPtFirst)
{
  if (cands.size()==0) return NULL;

//  determine max # of matched stubs
  int maxNMatchedMPC = 0;
  for (unsigned i=0; i<cands.size(); i++) 
  {
    int nst=0;
    if (cands[i].tftrack==0) continue;
    for (size_t s=0; s<cands[i].tftrack->ids.size(); s++) 
      if (cands[i].tftrack->mplcts[s]->deltaOk) nst++;
    if (nst>maxNMatchedMPC) maxNMatchedMPC = nst;
  }
// collect tracks with max # of matched stubs
  std::vector<unsigned> bestMatchedTracks;
  if (maxNMatchedMPC>0) {
    for (unsigned i=0; i<cands.size(); i++) 
    {
      int nst=0;
      if (cands[i].tftrack==0) continue;
      for (size_t s=0; s<cands[i].tftrack->ids.size(); s++) 
        if (cands[i].tftrack->mplcts[s]->deltaOk) nst++;
      if (nst==maxNMatchedMPC) bestMatchedTracks.push_back(i);
    }
    if (bestMatchedTracks.size()==1) return &(cands[bestMatchedTracks[0]]);
  }
  
// first sort by quality
  double qBase  = 1000000.;
// then sort by Pt inside the cone (if sortPtFirst), then sort by DR
  double ptBase = 0.;
  if (sortPtFirst) ptBase = 1000.;
  unsigned maxI = 99;
  double maxRank = -999999.;
  for (unsigned i=0; i<cands.size(); i++) 
  {
    if (bestMatchedTracks.size()) {
      bool gotit=0;
      for (unsigned m=0;m<bestMatchedTracks.size();m++) if (bestMatchedTracks[m]==i) gotit=1;
      if (!gotit) continue;
    }
    double rr = qBase*cands[i].l1cand->quality_packed() + ptBase*cands[i].l1cand->pt_packed() + 1./(0.01 + cands[i].dr);
    if (rr > maxRank) { maxRank = rr; maxI = i;}
  }
  if (maxI==99) return NULL;
  return &(cands[maxI]);
}

MatchCSCMuL1::GMTREGCAND * MatchCSCMuL1::bestGMTREGCAND(std::vector< GMTREGCAND > & cands, bool sortPtFirst)
{
// first sort by Pt inside the cone (if sortPtFirst), then sort by DR
  if (cands.size()==0) return NULL;
  double ptBase = 0.;
  if (sortPtFirst) ptBase = 1000.;
  unsigned maxI = 99;
  double maxRank = -999999.;
  for (unsigned i=0; i<cands.size(); i++) 
  {
    double rr = ptBase*cands[i].pt + 1./(0.01 + cands[i].dr);
    if (rr > maxRank) { maxRank = rr; maxI = i;}
  }
  if (maxI==99) return NULL;
  return &(cands[maxI]);
}

MatchCSCMuL1::GMTCAND * MatchCSCMuL1::bestGMTCAND(std::vector< GMTCAND > & cands, bool sortPtFirst)
{
// first sort by Pt inside the cone (if sortPtFirst), then sort by DR
  if (cands.size()==0) return NULL;
  double ptBase = 0.;
  if (sortPtFirst) ptBase = 1000.;
  unsigned maxI = 99;
  double maxRank = -999999.;
  for (unsigned i=0; i<cands.size(); i++) 
  {
    double rr = ptBase*cands[i].pt + 1./(0.01 + cands[i].dr);
    if (rr > maxRank) { maxRank = rr; maxI = i;}
  }
  if (maxI==99) return NULL;
  return &(cands[maxI]);
}



MatchCSCMuL1::ALCT::ALCT():match(0),trgdigi(0) {}
MatchCSCMuL1::ALCT::ALCT(MatchCSCMuL1 *m):match(m),trgdigi(0) {}

MatchCSCMuL1::CLCT::CLCT():match(0),trgdigi(0) {}
MatchCSCMuL1::CLCT::CLCT(MatchCSCMuL1 *m):match(m),trgdigi(0) {}

MatchCSCMuL1::LCT::LCT():match(0),trgdigi(0) {}
MatchCSCMuL1::LCT::LCT(MatchCSCMuL1 *m):match(m),trgdigi(0) {}

MatchCSCMuL1::MPLCT::MPLCT():match(0),trgdigi(0) {}
MatchCSCMuL1::MPLCT::MPLCT(MatchCSCMuL1 *m):match(m),trgdigi(0) {}

MatchCSCMuL1::TFTRACK::TFTRACK():match(0),l1trk(0), deltaOk1(0), deltaOk2(0), deltaOkME1(0), debug(0) {}
MatchCSCMuL1::TFTRACK::TFTRACK(MatchCSCMuL1 *m):match(m),l1trk(0), deltaOk1(0), deltaOk2(0), deltaOkME1(0), debug(0) {}

MatchCSCMuL1::TFCAND::TFCAND():match(0),l1cand(0) {}
MatchCSCMuL1::TFCAND::TFCAND(MatchCSCMuL1 *m):match(m),l1cand(0) {}



bool MatchCSCMuL1::ALCT::inReadOut()
{
  if (getBX()>=match->minBxALCT && getBX()<=match->maxBxALCT) 
    return true;
  return false;
}

bool MatchCSCMuL1::CLCT::inReadOut()
{
  if (getBX()>=match->minBxCLCT && getBX()<=match->maxBxCLCT)
    return true;
  return false;
}
bool MatchCSCMuL1::LCT::inReadOut()
{
  if (getBX()>=match->minBxLCT && getBX()<=match->maxBxLCT)
    return true;
  return false;
}
bool MatchCSCMuL1::MPLCT::inReadOut()
{
  if (getBX()>=match->minBxMPLCT && getBX()<=match->maxBxMPLCT)
    return true;
  return false;
}


void MatchCSCMuL1::TFTRACK::init(const csc::L1Track *t, CSCTFPtLUT* ptLUT,
    edm::ESHandle< L1MuTriggerScales > &muScales,
    edm::ESHandle< L1MuTriggerPtScale > &muPtScale)
{
  l1trk = t;

  unsigned gbl_phi = t->localPhi() + ((t->sector() - 1)*24) + 6; //for now, convert using this. LUT in the future
  if(gbl_phi > 143) gbl_phi -= 143;
  phi_packed = gbl_phi & 0xff;

  unsigned eta_sign = (t->endcap() == 1 ? 0 : 1);
  int gbl_eta = t->eta_packed() | eta_sign << (L1MuRegionalCand::ETA_LENGTH - 1);
  eta_packed  = gbl_eta & 0x3f;

  unsigned rank = t->rank(), gpt = 0, quality = 0;
  //if (rank != 0 ) {
  //  quality = rank >> L1MuRegionalCand::PT_LENGTH;
  //  gpt = rank & ( (1<<L1MuRegionalCand::PT_LENGTH) - 1);
  //}
  csc::L1Track::decodeRank(rank, gpt, quality);
  q_packed = quality & 0x3;
  pt_packed = gpt & 0x1f;

  //pt = muPtScale->getPtScale()->getLowEdge(pt_packed) + 1.e-6;
  eta = muScales->getRegionalEtaScale(2)->getCenter(t->eta_packed());
  phi = normalizedPhi( muScales->getPhiScale()->getLowEdge(phi_packed));
  
  //Pt needs some more workaround since it is not in the unpacked data
  //  PtAddress gives an handle on other parameters
  ptadd thePtAddress(t->ptLUTAddress());
  ptdat thePtData  = ptLUT->Pt(thePtAddress);
  // front or rear bit? 
  unsigned trPtBit = (thePtData.rear_rank&0x1f);
  if (thePtAddress.track_fr) trPtBit = (thePtData.front_rank&0x1f);
  // convert the Pt in human readable values (GeV/c)
  pt  = muPtScale->getPtScale()->getLowEdge(trPtBit); 

  if (trPtBit!=pt_packed) cout<<" trPtBit!=pt_packed: "<<trPtBit<<"!="<<pt_packed<<"  pt="<<pt<<" eta="<<eta<<endl;

  bool sc_debug = 0;
  if (sc_debug && deltaOk2){
    double stpt=-99., steta=-99., stphi=-99.;
    if (match){
      stpt = sqrt(match->strk->momentum().perp2());
      steta = match->strk->momentum().eta();
      stphi = normalizedPhi( match->strk->momentum().phi() );
    }
    //debug = 1;

    double my_phi = normalizedPhi( phi_packed*0.043633231299858237 + 0.0218 ); // M_PI*2.5/180 = 0.0436332312998582370
    double my_eta = 0.05 * eta_packed + 0.925; //  0.9+0.025 = 0.925
    //double my_pt = ptscale[pt_packed];
    //if (fabs(pt - my_pt)>0.005) cout<<"scales pt diff: my "<<my_pt<<"  sc: pt "<<pt<<"  eta "<<eta<<" phi "<<phi<<"  mc: pt "<<stpt<<"  eta "<<steta<<" phi "<<stphi<<endl;
    if (fabs(eta - my_eta)>0.005) cout<<"scales eta diff: my "<<my_eta<<" sc "<<eta<<"  mc: pt "<<stpt<<"  eta "<<steta<<" phi "<<stphi<<endl;
    if (fabs(deltaPhi(phi,my_phi))>0.03) cout<<"scales phi diff: my "<<my_phi<<" sc "<<phi<<"  mc: pt "<<stpt<<"  eta "<<steta<<" phi "<<stphi<<endl;

    double old_pt = muPtScale->getPtScale()->getLowEdge(pt_packed) + 1.e-6;
    if (fabs(pt - old_pt)>0.005) { debug = 1;cout<<"lut pt diff: old "<<old_pt<<" lut "<<pt<<"  eta "<<eta<<" phi "<<phi<<"   mc: pt "<<stpt<<"  eta "<<steta<<" phi "<<stphi<<endl;}
    double lcl_phi = normalizedPhi( fmod( muScales->getPhiScale()->getLowEdge(t->localPhi()) + 
                                   (t->sector()-1)*M_PI/3. + //sector 1 starts at 15 degrees 
                                   M_PI/12. , 2.*M_PI) );
    if (fabs(deltaPhi(phi,lcl_phi))>0.03) cout<<"lcl phi diff: lcl "<<lcl_phi<<" sc "<<phi<<"  mc: pt "<<stpt<<"  eta "<<steta<<" phi "<<stphi<<endl;
  }
}


bool MatchCSCMuL1::TFTRACK::hasStub(int st)
{
  if(st==0 && l1trk->mb1ID() > 0 ) return true;
  if(st==1 && l1trk->me1ID() > 0 ) return true;
  if(st==2 && l1trk->me2ID() > 0 ) return true;
  if(st==3 && l1trk->me3ID() > 0 ) return true;
  if(st==4 && l1trk->me4ID() > 0 ) return true;
  return false;
}


bool MatchCSCMuL1::TFTRACK::hasStubCSCOk(int st)
{
  if (!hasStub(st)) return false;
  bool cscok = 0;
  for (size_t s=0; s<ids.size(); s++)
    if (ids[s].station() == st && mplcts[s]->deltaOk) { cscok = 1; break; }
  if (cscok) return true;
  return false;
}


unsigned int MatchCSCMuL1::TFTRACK::nStubs(bool mb1, bool me1, bool me2, bool me3, bool me4)
{
  return (mb1 & hasStub(0)) + (me1 & hasStub(1)) + (me2 & hasStub(2)) + (me3 & hasStub(3)) + (me4 & hasStub(4));
}


unsigned int MatchCSCMuL1::TFTRACK::nStubsCSCOk(bool mb1, bool me1, bool me2, bool me3, bool me4)
{
  return (me1 & hasStubCSCOk(1)) + (me2 & hasStubCSCOk(2)) + (me3 & hasStubCSCOk(3)) + (me4 & hasStubCSCOk(4));
}


bool MatchCSCMuL1::TFTRACK::passStubsMatch(int minLowHStubs, int minMidHStubs, int minHighHStubs)
{
  double steta = match->strk->momentum().eta();
  int nstubs = nStubs(1,1,1,1,1);
  int nstubsok = nStubsCSCOk(1,1,1,1);
  if (fabs(steta) <= 1.2)
  {
    if (nstubsok >=1 && nstubs >= minLowHStubs) return true;
    else return false;
  }
  else if (fabs(steta) <= 2.1)
  {
    if (nstubsok >=2 && nstubs >= minMidHStubs) return true;
    else return false;
  }
  else
  {
    if (nstubsok >=2 && nstubs >= minHighHStubs) return true;
    else return false;
  }
}


void MatchCSCMuL1::TFTRACK::print(const char msg[300])
{
  cout<<"#### TFTRACK PRINT: "<<msg<<" #####"<<endl;
  //cout<<"## L1MuRegionalCand print: ";
  //l1trk->print();
  //cout<<"\n## L1Track Print: ";
  //l1trk->Print();
  //cout<<"## TFTRACK:  
  cout<<"\tpt_packed: "<<pt_packed<<"  eta_packed: " << eta_packed<<"  phi_packed: " << phi_packed<<"  q_packed: "<< q_packed<<"  bx: "<<l1trk->bx()<<endl;
  cout<<"\tpt: "<<pt<<"  eta: "<<eta<<"  phi: "<<phi<<"  sector: "<<l1trk->sector()<<"  dr: "<<dr<<"   ok1: "<<deltaOk1<<"  ok2: "<<deltaOk2<<"  okME1: "<<deltaOkME1<<endl;
  cout<<"\tMB1 ME1 ME2 ME3 ME4 = "<<l1trk->mb1ID()<<" "<<l1trk->me1ID()<<" "<<l1trk->me2ID()<<" "<<l1trk->me3ID()<<" "<<l1trk->me4ID()
      <<" ("<<hasStub(0)<<" "<<hasStub(1)<<" "<<hasStub(2)<<" "<<hasStub(3)<<" "<<hasStub(4)<<")  "
      <<" ("<<hasStubCSCOk(1)<<" "<<hasStubCSCOk(2)<<" "<<hasStubCSCOk(3)<<" "<<hasStubCSCOk(4)<<")"<<endl;
  cout<<"\tptAddress: 0x"<<hex<<l1trk->ptLUTAddress()<<dec<<"  mode: "<<mode()<<"  sign: "<<sign()<<"  dphi12: "<<dPhi12()<<"  dphi23: "<<dPhi23()<<endl;
  cout<<"\thas "<<trgdigis.size()<<" stubs in ";
  for (size_t s=0; s<trgids.size(); s++) 
    cout<<trgids[s]<<" w:"<<trgdigis[s]->getKeyWG()<<" s:"<<trgdigis[s]->getStrip()/2 + 1<<" p:"<<trgdigis[s]->getPattern()<<" bx:"<<trgdigis[s]->getBX()<<"; ";
  cout<<endl;
  cout<<"\tstub_etaphis:";
  for (size_t s=0; s<trgids.size(); s++)
    cout<<"  "<<trgetaphis[s].first<<" "<<trgetaphis[s].second;
  cout<<endl;
  cout<<"\tstub_petaphis:";
  for (size_t s=0; s<trgstubs.size(); s++)
    cout<<"  "<<trgstubs[s].etaPacked()<<" "<<trgstubs[s].phiPacked();
  cout<<endl;
  cout<<"\thas "<<mplcts.size()<<" associated MPCs in ";
  for (size_t s=0; s<ids.size(); s++) 
    cout<<ids[s]<<" w:"<<mplcts[s]->trgdigi->getKeyWG()<<" s:"<<mplcts[s]->trgdigi->getStrip()/2 + 1<<" Ok="<<mplcts[s]->deltaOk<<"; ";
  cout<<endl;
  cout<<"\tMPCs meEtap and mePhip: ";
  for (size_t s=0; s<ids.size(); s++) cout<<mplcts[s]->meEtap<<", "<<mplcts[s]->mePhip<<";  ";
  cout<<endl;
  cout<<"#### TFTRACK END PRINT #####"<<endl;
}


void MatchCSCMuL1::TFCAND::init(const L1MuRegionalCand *t, CSCTFPtLUT* ptLUT,
    edm::ESHandle< L1MuTriggerScales > &muScales,
    edm::ESHandle< L1MuTriggerPtScale > &muPtScale)
{
  l1cand = t;

  pt = muPtScale->getPtScale()->getLowEdge(t->pt_packed()) + 1.e-6;
  eta = muScales->getRegionalEtaScale(2)->getCenter(t->eta_packed());
  phi = normalizedPhi( muScales->getPhiScale()->getLowEdge(t->phi_packed()));
  nTFStubs = -1;
/*
  //Pt needs some more workaround since it is not in the unpacked data
  //  PtAddress gives an handle on other parameters
  ptadd thePtAddress(t->ptLUTAddress());
  ptdat thePtData  = ptLUT->Pt(thePtAddress);
  // front or rear bit? 
  unsigned trPtBit = (thePtData.rear_rank&0x1f);
  if (thePtAddress.track_fr) trPtBit = (thePtData.front_rank&0x1f);
  // convert the Pt in human readable values (GeV/c)
  pt  = muPtScale->getPtScale()->getLowEdge(trPtBit); 
*/
  bool sc_debug = 1;
  if (sc_debug){
    double my_phi = normalizedPhi( t->phi_packed()*0.043633231299858237 + 0.0218 ); // M_PI*2.5/180 = 0.0436332312998582370
    double sign_eta = ( (t->eta_packed() & 0x20) == 0) ? 1.:-1;
    double my_eta = sign_eta*(0.05 * (t->eta_packed() & 0x1F) + 0.925); //  0.9+0.025 = 0.925
    double my_pt = ptscale[t->pt_packed()];
    if (fabs(pt - my_pt)>0.005) cout<<"tfcand scales pt diff: my "<<my_pt<<" sc "<<pt<<endl;
    if (fabs(eta - my_eta)>0.005) cout<<"tfcand scales eta diff: my "<<my_eta<<" sc "<<eta<<endl;
    if (fabs(deltaPhi(phi,my_phi))>0.03) cout<<"tfcand scales phi diff: my "<<my_phi<<" sc "<<phi<<endl;
  }  
}


void MatchCSCMuL1::GMTREGCAND::print(const char msg[300])
{
  string sys="Mu";
  if (l1reg->type_idx()==2) sys = "CSC";
  if (l1reg->type_idx()==3) sys = "RPCf";
  cout<<"#### GMTREGCAND ("<<sys<<") PRINT: "<<msg<<" #####"<<endl;
  //l1reg->print();
  cout<<" bx="<<l1reg->bx()<<" values: pt="<<pt<<" eta="<<eta<<" phi="<<phi<<" packed: pt="<<l1reg->pt_packed()<<" eta="<<eta_packed<<" phi="<<phi_packed<<"  q="<<l1reg->quality()<<"  ch="<<l1reg->chargeValue()<<" chOk="<<l1reg->chargeValid()<<endl;
  if (tfcand!=NULL) cout<<"has tfcand with "<<ids.size()<<" stubs"<<endl;
  cout<<"#### GMTREGCAND END PRINT #####"<<endl;
}


void MatchCSCMuL1::GMTREGCAND::init(const L1MuRegionalCand *t,
    edm::ESHandle< L1MuTriggerScales > &muScales,
    edm::ESHandle< L1MuTriggerPtScale > &muPtScale)
{
  l1reg = t;

  pt = muPtScale->getPtScale()->getLowEdge(t->pt_packed()) + 1.e-6;
  eta = muScales->getRegionalEtaScale(t->type_idx())->getCenter(t->eta_packed());
  //cout<<"regetac"<<t->type_idx()<<"="<<eta<<endl;
  //cout<<"regetalo"<<t->type_idx()<<"="<<muScales->getRegionalEtaScale(t->type_idx())->getLowEdge(t->eta_packed() )<<endl;
  phi = normalizedPhi( muScales->getPhiScale()->getLowEdge(t->phi_packed()));
  nTFStubs = -1;

  bool sc_debug = 0;
  if (sc_debug){
    double my_phi = normalizedPhi( t->phi_packed()*0.043633231299858237 + 0.0218 ); // M_PI*2.5/180 = 0.0436332312998582370
    double sign_eta = ( (t->eta_packed() & 0x20) == 0) ? 1.:-1;
    double my_eta = sign_eta*(0.05 * (t->eta_packed() & 0x1F) + 0.925); //  0.9+0.025 = 0.925
    double my_pt = ptscale[t->pt_packed()];
    if (fabs(pt - my_pt)>0.005) cout<<"gmtreg scales pt diff: my "<<my_pt<<" sc "<<pt<<endl;
    if (fabs(eta - my_eta)>0.005) cout<<"gmtreg scales eta diff: my "<<my_eta<<" sc "<<eta<<endl;
    if (fabs(deltaPhi(phi,my_phi))>0.03) cout<<"gmtreg scales phi diff: my "<<my_phi<<" sc "<<phi<<endl;
  }  
}


void MatchCSCMuL1::GMTCAND::init(const L1MuGMTExtendedCand *t,
    edm::ESHandle< L1MuTriggerScales > &muScales,
    edm::ESHandle< L1MuTriggerPtScale > &muPtScale)
{
  l1gmt = t;

  // keep x and y components non-zero and protect against roundoff.
  pt = muPtScale->getPtScale()->getLowEdge( t->ptIndex() ) + 1.e-6 ;
  eta = muScales->getGMTEtaScale()->getCenter( t->etaIndex() ) ;
  //cout<<"gmtetalo="<<muScales->getGMTEtaScale()->getLowEdge(t->etaIndex() )<<endl;
  //cout<<"gmtetac="<<eta<<endl;
  phi = normalizedPhi( muScales->getPhiScale()->getLowEdge( t->phiIndex() ) ) ;
  math::PtEtaPhiMLorentzVector p4( pt, eta, phi, MUON_MASS );
  pt = p4.pt();
  q = t->quality();
  rank = t->rank();
}

