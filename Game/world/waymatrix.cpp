#include "waymatrix.h"

#include "world.h"

#include <algorithm>
#include <limits>

WayMatrix::WayMatrix(World &world, const ZenLoad::zCWayNetData &dat)
  :world(world) {
  wayPoints.resize(dat.waypoints.size());
  for(size_t i=0;i<wayPoints.size();++i){
    wayPoints[i] = WayPoint(dat.waypoints[i]);
    }

  for(auto& i:dat.edges){
    //at(i.first, i.second)=1.f;
    //at(i.second,i.first) =1.f;
    }

  for(auto& i:wayPoints)
    if(i.name.find("START")==0)
      startPoints.push_back(i);

  for(auto& i:wayPoints)
    if(i.name.find("START")!=std::string::npos)
      startPoints.push_back(i);
  }

void WayMatrix::buildIndex() {
  indexPoints.clear();
  adjustWaypoints(wayPoints);
  adjustWaypoints(freePoints);
  std::sort(indexPoints.begin(),indexPoints.end(),[](const WayPoint* a,const WayPoint* b){
    return a->name<b->name;
    });

  for(auto& i:freePoints)
    fpInd.push_back(&i);
  std::sort(fpInd.begin(),fpInd.end(),[](const WayPoint* a,const WayPoint* b){
    return a->x<b->x;
    });
  }

const WayPoint *WayMatrix::findWayPoint(float x, float y, float z) const {
  const WayPoint* ret =nullptr;
  float           dist=std::numeric_limits<float>::max();
  for(auto& w:wayPoints){
    float dx = w.x-x;
    float dy = w.y-y;
    float dz = w.z-z;
    float l=dx*dx+dy*dy+dz*dz;
    if(l<dist){
      ret  = &w;
      dist = l;
      }
    }
  return ret;
  }

const WayPoint *WayMatrix::findFreePoint(float x, float y, float z, const char *name) const {
  auto&  index = findFpIndex(name);
  return findFreePoint(x,y,z,index,nullptr);
  }

const WayPoint *WayMatrix::findNextFreePoint(float x, float y, float z, const char *name) const {
  auto&           index = findFpIndex(name);
  const WayPoint* cur   = findFreePoint(x,y,z,index,nullptr);
  return findFreePoint(x,y,z,index,cur);
  }

const WayPoint *WayMatrix::findNextPoint(float x, float y, float z) const {
  const WayPoint* ret   = nullptr;
  float           dist  = 20.f*100.f; // see scripting doc

  dist*=dist;
  for(auto pw:indexPoints){
    auto& w  = *pw;
    float dx = w.x-x;
    float dy = w.y-y;
    float dz = w.z-z;
    float l=dx*dx+dy*dy+dz*dz;
    if(l<dist && dz*dz<300*300 && !w.isLocked()){
      ret  = &w;
      dist = l;
      }
    }
  return ret;
  }

void WayMatrix::addFreePoint(float x, float y, float z, const char *name) {
  freePoints.emplace_back(x,y,z,name);
  }

void WayMatrix::addStartPoint(float x, float y, float z, const char *name) {
  startPoints.emplace_back(x,y,z,name);
  }

const WayPoint &WayMatrix::startPoint() const {
  for(auto& i:startPoints)
    if(i.name=="START_GOTHIC2")
      return i;

  if(startPoints.size()>0)
    return startPoints[0];
  static WayPoint p(0,0,0,"START");
  return p;
  }

const WayPoint *WayMatrix::findPoint(const char *name) const {
  if(name==nullptr)
    return nullptr;
  for(auto& i:startPoints)
    if(i.name==name)
      return &i;
  auto it = std::lower_bound(indexPoints.begin(),indexPoints.end(),name,[](const WayPoint* a,const char* b){
      return a->name<b;
    });
  if(it!=indexPoints.end() && (*it)->name==name)
    return *it;
  return nullptr;
  }

void WayMatrix::marchPoints(Tempest::Painter &p, const Tempest::Matrix4x4 &mvp, int w, int h) const {
  static bool ddraw=false;
  if(!ddraw)
    return;
  //for(auto& i:wayPoints) {
  for(auto& i:freePoints) {
    float x = i.x, y = i.y, z = i.z;
    mvp.project(x,y,z);
    if(z<0.f || z>1.f)
      continue;

    x = (0.5f*x+0.5f)*w;
    y = (0.5f*y+0.5f)*h;

    p.setBrush(Tempest::Color(1,0,0,1));
    p.drawRect(int(x),int(y),4,4);
    }
  }

void WayMatrix::adjustWaypoints(std::vector<WayPoint> &wp) {
  for(auto& w:wp) {
    w.y = world.physic()->dropRay(w.x,w.y,w.z);
    indexPoints.push_back(&w);
    }
  }

const WayMatrix::FpIndex &WayMatrix::findFpIndex(const char *name) const {
  auto it = std::lower_bound(fpIndex.begin(),fpIndex.end(),name,[](FpIndex& l,const char* r){
    return l.key<r;
    });
  if(it!=fpIndex.end() && it->key==name){
    return *it;
    }

  FpIndex id;
  id.key = name;
  for(auto& w:freePoints){
    if(w.name.find(name)==std::string::npos)
      continue;
    id.index.push_back(&w);
    }
  // TODO: good index, not sort by 'x' :)
  std::sort(id.index.begin(),id.index.end(),[](const WayPoint* a,const WayPoint* b){
    return a->x<b->x;
    });

  it = fpIndex.insert(it,std::move(id));
  return *it;
  }

const WayPoint *WayMatrix::findFreePoint(float x, float y, float z, const FpIndex& ind, const WayPoint *ex) const {
  float R = 20.f*100.f; // see scripting doc
  auto b = std::lower_bound(ind.index.begin(),ind.index.end(), x-R ,[](const WayPoint *a, float b){
    return a->x<b;
    });
  auto e = std::upper_bound(ind.index.begin(),ind.index.end(), x+R ,[](float a,const WayPoint *b){
    return a<b->x;
    });

  const WayPoint *ret=nullptr;
  int32_t count = std::distance(b,e);(void) count;
  float dist = R*R;
  for(auto i=b;i!=e;++i){
    auto& w  = **i;
    if(w.isLocked() || &w==ex)
      continue;
    float dx = w.x-x;
    float dy = w.y-y;
    float dz = w.z-z;
    float l=dx*dx+dy*dy+dz*dz;
    if(l<dist && dz*dz<300*300){
      ret  = &w;
      dist = l;
      }
    }
  return ret;
  }