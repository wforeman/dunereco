
set(      MODULE_LIBRARIES larcorealg::Geometry
                           larcore::Geometry_Geometry_service
                           lardata::Utilities
                           lardataobj::RecoBase
                           larreco::RecoAlg
                           lardata::RecoObjects
                           lardata::ArtDataHelper
                           art::Framework_Core
                           art::Framework_Principal
                           art::Framework_Services_Registry
                           art_root_io::tfile_support
                           art_root_io::TFileService_service
                           art::Persistency_Common
                           art::Persistency_Provenance
                           art::Utilities
                           messagefacility::MF_MessageLogger
)

cet_build_plugin(TrackRemoval     art::module SOURCE TrackRemoval_module.cc     LIBRARIES ${MODULE_LIBRARIES} )

install_headers()
install_fhicl()
install_source()
