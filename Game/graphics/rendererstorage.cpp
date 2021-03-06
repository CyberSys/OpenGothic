#include "rendererstorage.h"

#include <Tempest/Device>

#include "gothic.h"
#include "resources.h"

#include "shader.h"

using namespace Tempest;

void RendererStorage::ShaderPair::load(Device &device, const char *tag, const char *format) {
  char buf[256]={};

  std::snprintf(buf,sizeof(buf),format,tag,"vert");
  auto sh = GothicShader::get(buf);
  vs = device.shader(sh.data,sh.len);

  std::snprintf(buf,sizeof(buf),format,tag,"frag");
  sh = GothicShader::get(buf);
  fs = device.shader(sh.data,sh.len);
  }

void RendererStorage::ShaderPair::load(Device& device, const char* tag) {
  load(device,tag,"%s.%s.sprv");
  }

void RendererStorage::Material::load(Device &device, const char *tag) {
  char fobj[256]={};
  char fani[256]={};
  if(tag==nullptr || tag[0]=='\0') {
    std::snprintf(fobj,sizeof(fobj),"obj");
    std::snprintf(fani,sizeof(fani),"ani");
    } else {
    std::snprintf(fobj,sizeof(fobj),"obj_%s",tag);
    std::snprintf(fani,sizeof(fani),"ani_%s",tag);
    }
  obj.load(device,fobj,"%s.%s.sprv");
  ani.load(device,fani,"%s.%s.sprv");
  }

RendererStorage::RendererStorage(Device& device, Gothic& gothic)
  :device(device) {
  obj        .load(device,"");
  objG       .load(device,"gbuffer");
  objAt      .load(device,"at");
  objAtG     .load(device,"at_gbuffer");
  objEmi     .load(device,"emi");
  objShadow  .load(device,"shadow");
  objShadowAt.load(device,"shadow_at");

  initPipeline(gothic);
  initShadow();
  }

template<class Vertex>
RenderPipeline RendererStorage::pipeline(RenderState& st, const ShaderPair &sh) {
  return device.pipeline<Vertex>(Triangles,st,sh.vs,sh.fs);
  }

void RendererStorage::initPipeline(Gothic& gothic) {
  RenderState stateAlpha;
  stateAlpha.setCullFaceMode(RenderState::CullMode::Front);
  stateAlpha.setBlendSource (RenderState::BlendMode::src_alpha);
  stateAlpha.setBlendDest   (RenderState::BlendMode::one_minus_src_alpha);
  stateAlpha.setZTestMode   (RenderState::ZTestMode::Less);
  stateAlpha.setZWriteEnabled(false);

  RenderState stateAdd;
  stateAdd.setCullFaceMode(RenderState::CullMode::Front);
  stateAdd.setBlendSource (RenderState::BlendMode::one);
  stateAdd.setBlendDest   (RenderState::BlendMode::one);
  stateAdd.setZTestMode   (RenderState::ZTestMode::Equal);
  stateAdd.setZWriteEnabled(false);

  RenderState stateObj;
  stateObj.setCullFaceMode(RenderState::CullMode::Front);
  stateObj.setZTestMode   (RenderState::ZTestMode::Less);

  RenderState stateFsq;
  stateFsq.setCullFaceMode(RenderState::CullMode::Front);
  stateFsq.setZTestMode   (RenderState::ZTestMode::LEqual);
  stateFsq.setZWriteEnabled(false);

  RenderState stateMAdd;
  stateMAdd.setCullFaceMode (RenderState::CullMode::Front);
  stateMAdd.setBlendSource  (RenderState::BlendMode::src_alpha);
  stateMAdd.setBlendDest    (RenderState::BlendMode::one);
  stateMAdd.setZTestMode    (RenderState::ZTestMode::Less);
  stateMAdd.setZWriteEnabled(false);

  auto sh     = GothicShader::get("shadow_compose.vert.sprv");
  auto vsComp = device.shader(sh.data,sh.len);
  sh          = GothicShader::get("shadow_compose.frag.sprv");
  auto fsComp = device.shader(sh.data,sh.len);

  pComposeShadow = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,vsComp, fsComp);

  pAnim          = pipeline<Resources::VertexA>(stateObj,   obj.ani);
  pAnimG         = pipeline<Resources::VertexA>(stateObj,   objG.ani);
  pAnimAt        = pipeline<Resources::VertexA>(stateObj,   objAt.ani);
  pAnimAtG       = pipeline<Resources::VertexA>(stateObj,   objAtG.ani);
  pAnimLt        = pipeline<Resources::VertexA>(stateAdd,   obj.ani);
  pAnimAtLt      = pipeline<Resources::VertexA>(stateAdd,   objAt.ani);

  pObject        = pipeline<Resources::Vertex> (stateObj,   obj.obj);
  pObjectG       = pipeline<Resources::Vertex> (stateObj,   objG.obj);
  pObjectAt      = pipeline<Resources::Vertex> (stateObj,   objAt.obj);
  pObjectAtG     = pipeline<Resources::Vertex> (stateObj,   objAtG.obj);
  pObjectLt      = pipeline<Resources::Vertex> (stateAdd,   obj.obj);
  pObjectAtLt    = pipeline<Resources::Vertex> (stateAdd,   objAt.obj);

  pObjectAlpha   = pipeline<Resources::Vertex> (stateAlpha, obj.obj);
  pAnimAlpha     = pipeline<Resources::VertexA>(stateAlpha, obj.ani);

  pObjectMAdd    = pipeline<Resources::Vertex> (stateMAdd,  objEmi.obj);
  pAnimMAdd      = pipeline<Resources::VertexA>(stateMAdd,  objEmi.ani);

  {
  RenderState state;
  state.setCullFaceMode (RenderState::CullMode::Front);
  state.setBlendSource  (RenderState::BlendMode::one);
  state.setBlendDest    (RenderState::BlendMode::one);
  state.setZTestMode    (RenderState::ZTestMode::Less);
  state.setZWriteEnabled(false);

  auto sh      = GothicShader::get("light.vert.sprv");
  auto vsLight = device.shader(sh.data,sh.len);
  sh           = GothicShader::get("light.frag.sprv");
  auto fsLight = device.shader(sh.data,sh.len);
  pLights      = device.pipeline<Resources::VertexL>(Triangles, state, vsLight, fsLight);
  }

  if(gothic.version().game==1) {
    auto sh    = GothicShader::get("sky_g1.vert.sprv");
    auto vsSky = device.shader(sh.data,sh.len);
    sh         = GothicShader::get("sky_g1.frag.sprv");
    auto fsSky = device.shader(sh.data,sh.len);
    pSky       = device.pipeline<Resources::VertexFsq>(Triangles, stateFsq, vsSky,  fsSky);
    } else {
    auto sh    = GothicShader::get("sky.vert.sprv");
    auto vsSky = device.shader(sh.data,sh.len);
    sh         = GothicShader::get("sky.frag.sprv");
    auto fsSky = device.shader(sh.data,sh.len);
    pSky       = device.pipeline<Resources::VertexFsq>(Triangles, stateFsq, vsSky,  fsSky);
    }
  }

void RendererStorage::initShadow() {
  RenderState state;
  state.setZTestMode   (RenderState::ZTestMode::Less);
  state.setCullFaceMode(RenderState::CullMode::Back);
  //state.setCullFaceMode(RenderState::CullMode::Front);

  pObjectSh   = pipeline<Resources::Vertex> (state,objShadow  .obj);
  pObjectAtSh = pipeline<Resources::Vertex> (state,objShadowAt.obj);
  pAnimSh     = pipeline<Resources::VertexA>(state,objShadow  .ani);
  pAnimAtSh   = pipeline<Resources::VertexA>(state,objShadowAt.ani);
  }
