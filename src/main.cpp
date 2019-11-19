#include "fill_spill_merge.hpp"
#include "netcdf.hpp"
#include <iostream>
#include <richdem/common/Array2D.hpp>
#include <string>
#include <stdexcept>

namespace rd = richdem;
namespace dh = richdem::dephier;

int main(int argc, char **argv){
  if(argc!=5){
    std::cout<<"Syntax: "<<argv[0]<<" <Input> <Output> <outgraph> <Ocean Level>"<<std::endl;
    return -1;
  }

  const std::string in_name     = argv[1];
  const std::string out_name    = argv[2];
  const std::string out_graph   = argv[3];
  const float       ocean_level = std::stod(argv[4]);

  std::cout<<"m Processing    = "<<argv[1]<<std::endl;
  std::cout<<"m Output prefix = "<<argv[2]<<std::endl;
  std::cout<<"m Output graph  = "<<argv[3]<<std::endl;
  std::cout<<"m Ocean level   = "<<argv[4]<<std::endl;

  rd::Timer timer_io;
  timer_io.start();
  rd::Array2D<float> topo(in_name);   //Recharge (Percipitation minus Evapotranspiration)
  timer_io.stop();

  std::cout<<"m Data width  = "<<topo.width ()<<std::endl;
  std::cout<<"m Data height = "<<topo.height()<<std::endl;
  std::cout<<"m Data cells  = "<<topo.numDataCells()<<std::endl;

  rd::Array2D<float>          wtd     (topo.width(), topo.height(), 1000       ); //All cells have some water
  rd::Array2D<dh::dh_label_t> label   (topo.width(), topo.height(), dh::NO_DEP ); //No cells are part of a depression
  rd::Array2D<rd::flowdir_t>  flowdirs(topo.width(), topo.height(), rd::NO_FLOW); //No cells flow anywhere

  wtd.setNoData(topo.noData());

  //Label the ocean cells. This is a precondition for using
  //`GetDepressionHierarchy()`.
  #pragma omp parallel for
  for(unsigned int i=0;i<label.size();i++){
    if(topo.isNoData(i) || topo(i)==ocean_level){ //Ocean Level is assumed to be lower than any other cells (even Death Valley)
      label(i) = dh::OCEAN;
      wtd  (i) = 0;
    }
  }

  //Generate flow directions, label all the depressions, and get the hierarchy
  //connecting them
  auto deps = dh::GetDepressionHierarchy<float,rd::Topology::D8>(topo, label, flowdirs);

  dh::FillSpillMerge(topo, label, flowdirs, deps, wtd, ocean_level);

  for(unsigned int i=0;i<topo.size();i++)
    if(!topo.isNoData(i))
      wtd(i) += topo(i);

  SaveAsNetCDF(wtd,out_name+"-flooded.nc","value");

  //TODO: Remove. For viewing test cases.
    //GraphViz dot-style output for drawing depression hierarchy graphs.
    std::ofstream fgraph(out_graph);
    fgraph<<"{\n";
    for(unsigned int i=0;i<deps.size();i++){
      fgraph<<i<<":"<<deps[i].parent<<", \n";
    }
    fgraph<<"}\n";

  rd::FillDepressions<rd::Topology::D8>(topo);
  SaveAsNetCDF(topo,out_name+"-filled.nc","value");

  rd::Array2D<float> diff(wtd);
  for(unsigned int i=0;i<topo.size();i++)
    diff(i) = wtd(i)-topo(i);
  SaveAsNetCDF(diff,out_name+"-diff.nc","value");

  std::cout<<"Finished"<<std::endl;
  std::cout<<"IO time   = "<<timer_io.accumulated()<<" s"<<std::endl;

  return 0;
}
