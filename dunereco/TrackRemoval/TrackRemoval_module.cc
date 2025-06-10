////////////////////////////////////////////////////////////////////////
// Class:       TrackRemoval
// Plugin Type: producer (Unknown Unknown)
// File:        TrackRemoval_module.cc
//
// Generated at Wed Oct  4 14:56:12 2023 by Walker Johnson using cetskelgen
// from  version .
////////////////////////////////////////////////////////////////////////

// Framework includes

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
#include "lardataobj/RecoBase/Wire.h"
#include "lardataobj/RecoBase/Vertex.h"
#include "lardataobj/RecoBase/Cluster.h"
#include "lardataobj/RecoBase/Hit.h"
#include "lardata/ArtDataHelper/HitCreator.h"
#include "lardata/Utilities/AssociationUtil.h"

#include "tbb/concurrent_vector.h"


namespace TrackRemoval {
  class TrackRemoval;
}


class TrackRemoval::TrackRemoval : public art::EDProducer {
public:
  explicit TrackRemoval(fhicl::ParameterSet const& p);
  // The compiler-generated destructor is fine for non-base
  // classes without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  TrackRemoval(TrackRemoval const&) = delete;
  TrackRemoval(TrackRemoval&&) = delete;
  TrackRemoval& operator=(TrackRemoval const&) = delete;
  TrackRemoval& operator=(TrackRemoval&&) = delete;

  // Required functions.
  void produce(art::Event& e) override;

  //selected optional functions
  void beginJob() override;
  void endJob() override;

private:
  // Declare member data here.
  std::string fHitProducer;
  std::string fTrkProducer;
  std::string fWireProducer;


  double fMinTrkLength;
  double fMinTrkLengthVeto;
  double fVetoRadius;
  
  std::vector<size_t> _vetohits;
  std::vector<size_t> _flaggedhits;

  float pitch; 

  geo::GeometryCore const* fGeom;

  double samplePeriod;
  double driftVel;
  

};


TrackRemoval::TrackRemoval::TrackRemoval(fhicl::ParameterSet const& p)
  : EDProducer{p},
  fHitProducer                (p.get<std::string>("HitProducer", "")),
  fTrkProducer                (p.get<std::string>("TrkProducer", "")),
  fWireProducer               (p.get<std::string>("WireProducer", "")),
  fMinTrkLength               (p.get<double>("MinTrkLength", 5)),
  fMinTrkLengthVeto           (p.get<double>("MinTrkLengthVeto", 15)),
  fVetoRadius                 (p.get<double>("VetoRadius", 15))
  
{
  const std::string instanceName = "";
  recob::HitCollectionCreator::declare_products(producesCollector(), instanceName, true, false);

  fGeom = art::ServiceHandle<geo::Geometry const>().get();

  fMinTrkLength = std::min(fMinTrkLength, fMinTrkLengthVeto);
  
  _flaggedhits.reserve(5000);
  _vetohits.reserve(5000);
  
  auto const clock_data = art::ServiceHandle<detinfo::DetectorClocksService const>()->DataForJob();
  auto const det_prop = art::ServiceHandle<detinfo::DetectorPropertiesService const>()->DataForJob(clock_data);

  samplePeriod = clock_data.TPCClock().TickPeriod();
  driftVel = det_prop.DriftVelocity(det_prop.Efield(), det_prop.Temperature());


}


void TrackRemoval::TrackRemoval::produce(art::Event& evt)
{
  std::cout<<"\n" << "----------------TrackRemoval-------------" << std::endl;


  const std::string instanceName = "";

  auto const& trk_h = evt.getValidHandle<std::vector<recob::Track>>(fTrkProducer);
  std::vector<art::Ptr<recob::Track>> trklist;
  art::fill_ptr_vector(trklist, trk_h);

  auto const wire_h = evt.getValidHandle<std::vector<recob::Wire>>(fWireProducer);
  
  auto const& hit_h = evt.getValidHandle<std::vector<recob::Hit>>(fHitProducer);
  std::vector<art::Ptr<recob::Hit>> hitlist;
  art::fill_ptr_vector(hitlist, hit_h);

  art::FindManyP<recob::Track> hit_trk_assn_v(hit_h, evt, fTrkProducer);
  art::FindOneP<recob::Wire> hit_wire_assn_v(hit_h, evt, fHitProducer);

  std::unique_ptr<std::vector<recob::Hit>> Hit_v(new std::vector<recob::Hit>);
  //ptr for wire assn
  std::unique_ptr<std::vector<recob::Wire>> Wire_v(new std::vector<recob::Wire>);
  std::unique_ptr<art::Assns<recob::Hit, recob::Wire, void> > Assn_v(new art::Assns<recob::Hit, recob::Wire, void>);

  _vetohits.clear();
  _flaggedhits.clear();

  std::vector<bool> hitIsVetoed(hitlist.size(), false);

  
  struct hitstruct {
    recob::Hit hit_tbb;
    art::Ptr<recob::Wire> wire_tbb;
  };

  tbb::concurrent_vector< hitstruct > hitstruct_vec;
  
  std::cout<< "\n" << "Creating hit collector with instance name: " << instanceName << std::endl;

  recob::HitCollectionCreator allHitCol(evt, instanceName, true, false);


  std::map<size_t,double> trklmap;
  std::map<size_t,bool> flagged_trks;
  for(size_t t=0; t<trklist.size(); t++){
    auto const& trk = trklist[t];
    if(trk->Length() < fMinTrkLength) continue;
    if(trk->Length() > 2*(trk->Vertex() - trk->End()).R()) continue;
    trklmap[trk->ID()] = (double)trk->Length();
    if(trk->Length() < fMinTrkLengthVeto) continue;
    flagged_trks[trk->ID()] = true;
  }

  std::map<size_t, std::vector<size_t>> planehitmap;
  std::vector<TVector2> wtpoint(hitlist.size());

  


  for(size_t h=0; h<hitlist.size(); h++){

    auto const& hit = hitlist[h];
    pitch = fGeom->WirePitch(hit->WireID());

    if(hitIsVetoed[h]) continue;
    
    auto const& trk_v = hit_trk_assn_v.at(h);
    if( trk_v.size() ){
      size_t trkID = trk_v.at(0)->ID();
      if( trklmap[trkID] > 0) {
	hitIsVetoed[h] = true;
	_vetohits.push_back(h);
	if(flagged_trks[trkID]){
	  _flaggedhits.push_back(h);
	}
      }
    }

    if( !hitIsVetoed[h])planehitmap[hitlist[h]->WireID().Plane].push_back(h);

    float w = hitlist[h]->WireID().Wire * pitch;
    float t = hitlist[h]->PeakTime() * samplePeriod * driftVel;
    wtpoint.at(h).Set(w,t);
  } //endloop over hits


  float vetoRadSq = pow(fVetoRadius,2);
  size_t additional_vetoed_hits=0;
  for(auto const& h : _flaggedhits){
    for(auto const& hh : planehitmap[hitlist[h]->WireID().Plane]){
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
    }
  }

  art::PtrMaker<recob::Hit> makeHitPtr(evt);

  for(size_t h=0; h<hit_h->size(); h++){
    if(!hitIsVetoed[h]){
      //art::Ptr<recob::Wire> wire = hit_wire_assn_v.at(h);
      hitstruct tmp{std::move(hit_h->at(h)), hit_wire_assn_v.at(h)};
      hitstruct_vec.push_back(std::move(tmp));
    }
  }

  for(size_t i = 0; i < hitstruct_vec.size();  i++){
    allHitCol.emplace_back(hitstruct_vec[i].hit_tbb, hitstruct_vec[i].wire_tbb);
  }

  allHitCol.put_into(evt);
  
}

void TrackRemoval::TrackRemoval::beginJob()
{
}

void TrackRemoval::TrackRemoval::endJob()
{
}

DEFINE_ART_MODULE(TrackRemoval::TrackRemoval)
