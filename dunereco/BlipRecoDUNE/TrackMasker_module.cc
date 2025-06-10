////////////////////////////////////////////////////////////////////////
// Class:       TrackMasker
// Plugin Type: producer (art v2_05_01)
// File:        TrackMasker_module.cc
//
// W. Foreman
// March 2022
//
// TO-DO: modify to work on multiple TPCs or cryostats
// (ie, find way to tell if two channels are still in
// proximity even if they're in adjacent TPCs)
//
////////////////////////////////////////////////////////////////////////

// Framework includes
#include "art_root_io/TFileService.h"
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

// C++ includes
#include <memory>
#include <cstring>
#include <vector>
#include <map>
#include <utility>
#include <iterator>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <typeinfo>
#include <cmath>

// Larsoft includes
#include "larcore/Geometry/Geometry.h"
#include "larcorealg/Geometry/GeometryCore.h"
#include "lardata/Utilities/GeometryUtilities.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "lardataobj/RecoBase/PFParticle.h"
#include "lardataobj/RecoBase/Track.h"
#include "lardataobj/RecoBase/Vertex.h"
#include "lardataobj/RecoBase/Cluster.h"
#include "lardataobj/RecoBase/Hit.h"
#include "lardata/Utilities/AssociationUtil.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "nusimdata/SimulationBase/MCTruth.h"
#include "lardataobj/Simulation/SimEnergyDeposit.h"
#include "lardataobj/AnalysisBase/BackTrackerMatchingData.h"


class TrackMasker;

//###################################################
// Class Definition
//###################################################
class TrackMasker : public art::EDProducer {
public:
  explicit TrackMasker(fhicl::ParameterSet const & p);
  // The compiler-generated destructor is fine for non-base
  // classes without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  TrackMasker(TrackMasker const &) = delete;
  TrackMasker(TrackMasker &&) = delete;
  TrackMasker & operator = (TrackMasker const &) = delete;
  TrackMasker & operator = (TrackMasker &&) = delete;

  // Required functions.
  void produce(art::Event & e) override;

  // Selected optional functions.
  void beginJob() override;
  void endJob() override;


private:
  
  // Producer labels
  std::string   fHitProducer;
  std::string   fTrkProducer;
  
  // Options
  double fMinTrkLength;
  double fMinTrkLengthVeto;
  double fVetoRadius;
  bool   fIgnoreDataOverlay;
  //bool   fIgnoreDataTrks;

  // Vector to hold hit indices to be removed
  std::vector<size_t>   _vetohits;
  std::vector<size_t>   _flaggedhits;
  
  // TODO: remove these hard-coded conversion values
  //float pitch        = 0.3;    // cm
  
};


//###################################################
// Constructor
//###################################################
TrackMasker::TrackMasker(fhicl::ParameterSet const & p) : art::EDProducer(p)
{
  produces< std::vector< recob::Hit > >();
  
  fHitProducer            = p.get<std::string>("HitProducer",       "gaushit");
  fTrkProducer            = p.get<std::string>("TrkProducer",       "pandora");
  fMinTrkLength           = p.get<double>     ("MinTrkLength",      5);
  fMinTrkLengthVeto       = p.get<double>     ("MinTrkLengthVeto",  15);
  fVetoRadius             = p.get<double>     ("VetoRadius",        15);
  fIgnoreDataOverlay      = p.get<bool>       ("IgnoreDataOverlay", false);
  //fIgnoreDataTrks         = p.get<bool>       ("IgnoreDataTrks", false);

  // min means min!
  fMinTrkLength = std::min(fMinTrkLength, fMinTrkLengthVeto);

  _flaggedhits  .reserve(50000);
  _vetohits     .reserve(50000);
  
  // dignostic histograms
  art::ServiceHandle<art::TFileService> tfs;
  
}



//###################################################
// Main producer function
//###################################################
void TrackMasker::produce(art::Event & evt)
{
  //std::cout<<"\n"
  //<<"=========== TrackMasker =========================\n"
  //<<"Event "<<e.id().event()<<" / run "<<e.id().run()<<"\n";
  
  // *****************************
  // Grab data products from file
  // *****************************

  // grab tracks
  auto const& trk_h = evt.getValidHandle<std::vector<recob::Track>>(fTrkProducer);
  std::vector<art::Ptr<recob::Track> > trklist;
  art::fill_ptr_vector(trklist,trk_h);
  
  // grab hits
  auto const& hit_h = evt.getValidHandle<std::vector<recob::Hit>>(fHitProducer);
  std::vector<art::Ptr<recob::Hit> > hitlist;
  art::fill_ptr_vector(hitlist, hit_h);

  // grab hits associated to track
  art::FindManyP<recob::Track> hit_trk_assn_v(hit_h, evt, fTrkProducer);

  // produce hits
  std::unique_ptr< std::vector<recob::Hit> > Hit_v(new std::vector<recob::Hit>);
 
  // clear track-like hit vector
  _vetohits.clear();
  _flaggedhits.clear();
  

  std::cout<<"Found "<<hit_h->size()<<" hits from "<<fHitProducer<<"\n";
  std::cout<<"Found "<<trk_h->size()<<" tracks from "<<fTrkProducer<<"\n";
  
  // boolean designating each hit as tracked
  std::vector<bool> hitIsVetoed( hitlist.size(), false);

  //******************************************************* 
  // Assuming input collection is original 'gaushit', we can 
  // use gaushitTruthMatch metadata to ID MC hits
  //******************************************************* 
  if( fIgnoreDataOverlay ) { 
    art::FindMany<simb::MCParticle,anab::BackTrackerHitMatchingData> fmhh(hit_h,evt,"gaushitTruthMatch");
    for (size_t h=0; h < hitlist.size(); h++) {
      if( !fmhh.at(h).size() ) {
        hitIsVetoed[h] = true;
        _vetohits.push_back(h);
      }
    }
    std::cout <<"\n*** TRACKMASKER WARNING: ignoring all non-MC hits and tracks!\n"
              <<"*** "<<_vetohits.size()<<" out of "<<hitlist.size()<<" hits ignored.\n";
  }
  
  //***************************************************
  // First go through tracks and designate which ones
  // pass our track length cuts by saving a map based
  // on each track's "ID".
  //***************************************************
  
  // Map of track ID to track length
  std::map<size_t,double> trklmap;
  std::map<size_t,bool>   flagged_trks;
  for( size_t t=0; t < trklist.size(); t++ ) {
    auto const& trk = trklist[t];
    // must be some minimum length
    if (trk->Length() < fMinTrkLength) continue;
    // and track length must be < twice start-end distance
    if (trk->Length() > 2 * (trk->Vertex()-trk->End()).R() ) continue;
    trklmap[trk->ID()] = (double)trk->Length();
    if (trk->Length() < fMinTrkLengthVeto) continue;
    flagged_trks[trk->ID()] = true;
  }


  //***************************************************
  // Now categorize hits as 'tracked' or 'untracked'
  //***************************************************
  
  // map of (un-vetoed) hits to planes
  std::map<size_t,std::vector<size_t>> planehitmap;
  
  // 2D wire-time coordinates for each hit
  std::vector<TVector2> wtpoint( hitlist.size());

  auto const detProp  = art::ServiceHandle<detinfo::DetectorPropertiesService>()->DataForJob(); 
  auto const detClock = art::ServiceHandle<detinfo::DetectorClocksService>()->DataForJob();
  double samplePeriod = detClock.TPCClock().TickPeriod();
  double driftVel     = detProp.DriftVelocity(detProp.Efield(0),detProp.Temperature()); 
  art::ServiceHandle<geo::Geometry> geom;

  for (size_t h=0; h < hitlist.size(); h++) {

    if( hitIsVetoed[h] ) continue;
    
    int plane = hitlist[h]->WireID().Plane;

    // find associated track
    auto const& trk_v = hit_trk_assn_v.at(h);
    if( trk_v.size() ) {
      size_t trkID = trk_v.at(0)->ID();
      if( trklmap[trkID] > 0 ) { 
        hitIsVetoed[h] = true;
        _vetohits.push_back(h);
        if( flagged_trks[trkID] ) 
          _flaggedhits.push_back(h);
      }
    }//endif track association exists
    
    if( !hitIsVetoed[h] ) planehitmap[plane].push_back(h);
    
    // calculate wire-time coordinates of every hit so we aren't
    // repeating these calculations a bunch of times later on
    float pitch = geom->WirePitch(hitlist[h]->WireID())
    float w = hitlist[h]->WireID().Wire * pitch;
    float t = hitlist[h]->PeakTime() * samplePeriod * driftVel;
    wtpoint.at(h).Set(w,t);
  
  }//endloop over hits
  
  //std::cout<<" --> removed "<<_vetohits.size()<<" tracked hits\n";
  //std::cout<<" --> flagged "<<_flaggedhits.size()<<" hits in tracks long enough to apply veto radius\n";
  //size_t veto_hits_trk = _vetohits.size();
  
  //***************************************************
  // Loop through all tracked hits and for each one, 
  // check all un-tracked hits to determine if any
  // are within the veto radius.
  //**************************************************
  
  float vetoRadSq = pow(fVetoRadius,2); 
  size_t additional_vetoed_hits=0;
  for( auto const& h : _flaggedhits ) {
    for(auto const& hh : planehitmap[hitlist[h]->WireID().Plane] ) {
      // skip hits that are already vetoed
      if( hitIsVetoed[hh] ) continue;
      // skip hits on far-away wires
      float dw = fabs(wtpoint.at(hh).X()-wtpoint.at(h).X());
      if( dw > fVetoRadius ) continue;
      // skip hits that are sufficiently separated in time
      float dt = fabs(wtpoint.at(hh).Y()-wtpoint.at(h).Y());
      if( dt > fVetoRadius ) continue;
      // finally, check 2D proximity
      if( (pow(dw,2)+pow(dt,2)) > vetoRadSq ) continue;
      _vetohits.push_back(hh);
      hitIsVetoed[hh] = true;
      additional_vetoed_hits++;

      // TODO: group in nearby hits
      // TODO: if hit in question was in a short track,
      //       apply same veto to all other hits in that
      //       track even if they are outside this radius.
    }
  }
  
  //std::cout<<" --> vetoed additional "<<additional_vetoed_hits<<" hits within radius\n";


  // ********************************************
  // finally, save hits that weren't marked veto
  // ********************************************
  for (size_t h=0; h < hit_h->size(); h++) {
    if( !hitIsVetoed[h] ) Hit_v->emplace_back(hit_h->at(h));
  }

  
  //printf("input hits  : %lu\n",hit_h->size());
  //printf("output hits : %lu\n",Hit_v->size());

  evt.put(std::move(Hit_v));
        
}

  
//###################################################
void TrackMasker::beginJob()
{
}


//###################################################
void TrackMasker::endJob()
{
}


DEFINE_ART_MODULE(TrackMasker)
