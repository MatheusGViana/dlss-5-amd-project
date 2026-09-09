#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstring>
#include <string>
using Microsoft::WRL::ComPtr;
void ck(HRESULT hr) { if(FAILED(hr)) throw std::runtime_error("D3D12 operation failed: "+std::to_string(unsigned(hr))); }
int main() try {
 ComPtr<ID3D12Debug> debug;
 if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
 ComPtr<IDXGIFactory4> factory; ck(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<ID3D12Device> dev;
 for(UINT i=0;!dev;++i) {
  ComPtr<IDXGIAdapter1> a; if(factory->EnumAdapters1(i,&a)!=S_OK) break;
  DXGI_ADAPTER_DESC1 d; a->GetDesc1(&d);
  if(d.VendorId==0x1002) D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&dev));
 }
 if(!dev) throw std::runtime_error("No AMD D3D12 device");
 ComPtr<ID3D12CommandQueue> q; D3D12_COMMAND_QUEUE_DESC qd{}; ck(dev->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)));
 D3D12_DESCRIPTOR_RANGE ranges[2] {{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,7,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,2,0,0,7}};
 D3D12_ROOT_PARAMETER rp[2]{}; rp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rp[0].DescriptorTable={2,ranges};
 rp[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;rp[1].Constants={0,0,12};
 D3D12_STATIC_SAMPLER_DESC sampler{}; sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
 sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
 sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;sampler.MaxLOD=D3D12_FLOAT32_MAX;
 D3D12_STATIC_SAMPLER_DESC samplers[2]{sampler,sampler};samplers[1].ShaderRegister=1;samplers[1].Filter=D3D12_FILTER_MIN_MAG_MIP_POINT;
 D3D12_ROOT_SIGNATURE_DESC rs{2,rp,2,samplers,D3D12_ROOT_SIGNATURE_FLAG_NONE};
 ComPtr<ID3DBlob> blob,errors;ck(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
 ComPtr<ID3D12RootSignature> root;ck(dev->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)));
 std::ifstream file("analysis/rtgi-native/TraceWrapCubicCS.cso",std::ios::binary);
 std::vector<char> code((std::istreambuf_iterator<char>(file)),{});
 D3D12_COMPUTE_PIPELINE_STATE_DESC ps{};ps.pRootSignature=root.Get();ps.CS={code.data(),code.size()};
 ComPtr<ID3D12PipelineState> pipeline;ck(dev->CreateComputePipelineState(&ps,IID_PPV_ARGS(&pipeline)));
 auto make=[&](D3D12_RESOURCE_DESC rd,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state) {
  D3D12_HEAP_PROPERTIES hp{};hp.Type=type;ComPtr<ID3D12Resource> r;
  ck(dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,state,nullptr,IID_PPV_ARGS(&r)));return r;
 };
 auto buffer=[&](UINT64 bytes,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state) {
  D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;
  d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return make(d,type,state);
 };

 int failures=0;
 for(UINT test=0;test<6;++test){
  UINT w=test%2?65:64,h=test%2?33:64;
  ComPtr<ID3D12CommandAllocator> alloc;ck(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)));
  ComPtr<ID3D12GraphicsCommandList> cmd;ck(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&cmd)));
  auto transition=[&](ID3D12Resource* r,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){
   D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};cmd->ResourceBarrier(1,&b);
  };
  D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,9,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};
  ComPtr<ID3D12DescriptorHeap> heap;ck(dev->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));
  auto cpu=heap->GetCPUDescriptorHandleForHeapStart();auto stride=dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  std::vector<ComPtr<ID3D12Resource>> inputs,uploads;
  D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;rd.Width=w;rd.Height=h;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.SampleDesc.Count=1;
  for(int k=0;k<7;++k){
   rd.Width=w;rd.Height=h;rd.DepthOrArraySize=1;rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   rd.Format=k==0?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R32G32B32A32_FLOAT;
   std::vector<unsigned char> asset;
   if(k>=3&&k<=5){
    const char* names[]{"iMMERSE_bluenoise_temporal128","iMMERSE_bluenoise_temporal128_s","iMMERSE_horizonlut"};
    std::ifstream input(std::string("analysis/rtgi-native/")+names[k-3]+".rgba",std::ios::binary);
    UINT size[2];input.read(reinterpret_cast<char*>(size),8);if(!input)throw std::runtime_error("Missing trace texture");
    rd.Width=size[0];rd.Height=size[1];rd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    asset.resize(size[0]*size[1]*4);input.read(reinterpret_cast<char*>(asset.data()),asset.size());if(!input)throw std::runtime_error("Truncated trace texture");
   }
   if(k==6){rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;rd.Width=w/4;rd.Height=h/4;rd.DepthOrArraySize=15;}
   auto r=make(rd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes;dev->GetCopyableFootprints(&rd,0,1,0,&fp,nullptr,nullptr,&bytes);
   auto up=buffer(bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* p;ck(up->Map(0,nullptr,&p));std::memset(p,0,bytes);
   for(UINT z=0;z<fp.Footprint.Depth;++z)for(UINT y=0;y<rd.Height;++y)for(UINT x=0;x<rd.Width;++x){
    char* at=static_cast<char*>(p)+(z*rd.Height+y)*fp.Footprint.RowPitch;
    if(!asset.empty())std::memcpy(at+x*4,asset.data()+(y*rd.Width+x)*4,4);
    else if(k==0)reinterpret_cast<float*>(at)[x]=test<2?1001.f:(test<4?2.f:(x<w/2?2.f:1.5f));
    else {auto v=reinterpret_cast<float*>(at)+4*x;v[0]=k==6?.1f:0;v[1]=0;v[2]=k==6?.15f:-1;v[3]=0;}
   }
   up->Unmap(0,nullptr);D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=r.Get();b.pResource=up.Get();b.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;b.PlacedFootprint=fp;
   cmd->CopyTextureRegion(&a,0,0,0,&b,nullptr);transition(r.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=rd.Format;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   view.ViewDimension=k==6?D3D12_SRV_DIMENSION_TEXTURE3D:D3D12_SRV_DIMENSION_TEXTURE2D;view.Texture2D.MipLevels=1;
   dev->CreateShaderResourceView(r.Get(),&view,cpu);cpu.ptr+=stride;inputs.push_back(r);uploads.push_back(up);
  }
  rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;rd.Width=w;rd.Height=h;rd.DepthOrArraySize=1;rd.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;rd.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  auto output=make(rd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=rd.Format;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;dev->CreateUnorderedAccessView(output.Get(),nullptr,&u,cpu);cpu.ptr+=stride;
  auto outputDesc=rd;rd.Width=(w+31)/32*32;rd.Height=(h+31)/32*32;rd.Format=DXGI_FORMAT_R32_FLOAT;
  auto cache=make(rd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);u.Format=rd.Format;dev->CreateUnorderedAccessView(cache.Get(),nullptr,&u,cpu);
  D3D12_DESCRIPTOR_HEAP_DESC clearDesc{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,D3D12_DESCRIPTOR_HEAP_FLAG_NONE,0};ComPtr<ID3D12DescriptorHeap> clearHeap;
  ck(dev->CreateDescriptorHeap(&clearDesc,IID_PPV_ARGS(&clearHeap)));dev->CreateUnorderedAccessView(cache.Get(),nullptr,&u,clearHeap->GetCPUDescriptorHandleForHeapStart());
  auto hp=heap.Get();cmd->SetDescriptorHeaps(1,&hp);auto gpu=heap->GetGPUDescriptorHandleForHeapStart();gpu.ptr+=8*stride;float zero[4]{};
  cmd->ClearUnorderedAccessViewFloat(gpu,clearHeap->GetCPUDescriptorHandleForHeapStart(),cache.Get(),zero,0,nullptr);
  D3D12_RESOURCE_BARRIER ub{};ub.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;ub.UAV.pResource=cache.Get();cmd->ResourceBarrier(1,&ub);
  struct {UINT w,h;float farPlane,tanHalfFov;UINT frame,quality;float thickness,fade,ao,ambient;UINT debug,pad;} constants{w,h,1000,.7f,127,2,.5f,1,1,0,0,0};
  cmd->SetComputeRootSignature(root.Get());cmd->SetPipelineState(pipeline.Get());cmd->SetComputeRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());cmd->SetComputeRoot32BitConstants(1,12,&constants,0);
  cmd->Dispatch((w+31)/32,(h+31)/32,1);
  transition(output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes;dev->GetCopyableFootprints(&outputDesc,0,1,0,&fp,nullptr,nullptr,&bytes);
  auto read=buffer(bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);D3D12_TEXTURE_COPY_LOCATION a{},b{};
  a.pResource=read.Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=fp;b.pResource=output.Get();cmd->CopyTextureRegion(&a,0,0,0,&b,nullptr);
  ck(cmd->Close());ID3D12CommandList* lists[]{cmd.Get()};q->ExecuteCommandLists(1,lists);
  ComPtr<ID3D12Fence> fence;ck(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));ck(q->Signal(fence.Get(),1));HANDLE event=CreateEvent(nullptr,FALSE,FALSE,nullptr);ck(fence->SetEventOnCompletion(1,event));
  if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)throw std::runtime_error("Trace GPU timeout");CloseHandle(event);
  void* p;ck(read->Map(0,nullptr,&p));int bad=0;float maxLight=0;
  for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){auto v=reinterpret_cast<float*>(static_cast<char*>(p)+y*fp.Footprint.RowPitch)+4*x;
   for(int c=0;c<4;++c)if(!std::isfinite(v[c]))++bad;
   maxLight=std::max(maxLight,v[0]);
   if(test<2){for(int c=0;c<4;++c)if(std::abs(v[c])>1e-5f)++bad;}
   else if(test<4){if(std::abs(v[0])>1e-4f||std::abs(v[3]-1)>0.005f)++bad;}
  }
  if(test>=4 && maxLight<=0)++bad;
  read->Unmap(0,nullptr);std::cout<<"case="<<test<<" size="<<w<<"x"<<h<<" maxLight="<<maxLight<<" bad="<<bad<<std::endl;failures+=bad;
 }
 ComPtr<ID3D12InfoQueue> info;if(SUCCEEDED(dev.As(&info)))for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T len=0;info->GetMessage(i,nullptr,&len);std::vector<char>s(len);auto m=reinterpret_cast<D3D12_MESSAGE*>(s.data());info->GetMessage(i,m,&len);if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<m->pDescription<<std::endl;++failures;}}
 return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 2;}
