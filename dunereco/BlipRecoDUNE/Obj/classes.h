//
// Build a dictionary.
//
// $Id: classes.h,v 1.8 2010/04/12 18:12:28  Exp $
// $Author:  $
// $Date: 2010/04/12 18:12:28 $
// 
// Original author Rob Kutschke, modified by wes
//

#include "canvas/Persistency/Common/PtrVector.h"
#include "canvas/Persistency/Common/Wrapper.h"
#include "canvas/Persistency/Common/Assns.h"

// data-products
// lardataobj
#include "lardataobj/RecoBase/PFParticle.h"
#include "lardataobj/RecoBase/Hit.h"
#include "dunereco/BlipRecoDUNE/Utils/DataTypes.h"

//
// Only include objects that we would like to be able to put into the event.
// Do not include the objects they contain internally.
//

template class std::vector<blipobj::Blip>;
template class art::Wrapper<std::vector<blipobj::Blip> >;

template class art::Assns<recob::Hit,blipobj::Blip,void>;
template class art::Wrapper<art::Assns<recob::Hit,blipobj::Blip,void> >;

template class art::Assns<blipobj::Blip,recob::Hit,void>;
template class art::Wrapper<art::Assns<blipobj::Blip,recob::Hit,void> >;

template class std::vector<blipobj::TrueBlip>;
template class art::Wrapper<std::vector<blipobj::TrueBlip> >;

template class std::vector<blipobj::HitClust>;
template class art::Wrapper<std::vector<blipobj::HitClust> >;

