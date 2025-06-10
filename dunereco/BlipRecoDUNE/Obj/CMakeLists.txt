
art_make(
  LIB_LIBRARIES
  DICT_LIBRARIES
  larcorealg::Geometry
  larcore::Geometry_Geometry_service
  lardataobj::RecoBase
  nusimdata::SimulationBase
  messagefacility::MF_MessageLogger
  cetlib::cetlib 
  cetlib_except::cetlib_except
  lardataobj::RecoBase
  ROOT::Core
  ROOT::Geom
  ROOT::GenVector
)

install_headers()
install_source()
