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
 D3D12_DESCRIPTOR_RANGE ranges[2] {{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,5,0,0,2}};
 D3D12_ROOT_PARAMETER rp[2]{}; rp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rp[0].DescriptorTable={2,ranges};
 rp[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;rp[1].Constants={0,0,4};
 D3D12_STATIC_SAMPLER_DESC sampler{}; sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
 sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
 sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;sampler.MaxLOD=D3D12_FLOAT32_MAX;
 D3D12_ROOT_SIGNATURE_DESC rs{2,rp,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_NONE};
 ComPtr<ID3DBlob> blob,errors;ck(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
 ComPtr<ID3D12RootSignature> root;ck(dev->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)));
 std::ifstream file("analysis/rtgi-native/DiffuseDepth.cso",std::ios::binary);
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
 for(UINT test=0;test<6;++test) {
  UINT w=test%2?65:64,h=test%2?33:64;float depth=test<2?.5f:test<4?0:1;
  ComPtr<ID3D12CommandAllocator> alloc;ck(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)));
  ComPtr<ID3D12GraphicsCommandList> cmd;ck(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&cmd)));
  auto barrier=[&](ID3D12Resource* r,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to) {
   D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};cmd->ResourceBarrier(1,&b);
  };
  D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;rd.Width=w;rd.Height=h;
  rd.DepthOrArraySize=1;rd.MipLevels=1;rd.SampleDesc.Count=1;
  ComPtr<ID3D12Resource> inputs[2];std::vector<ComPtr<ID3D12Resource>> uploads;
  for(int k=0;k<2;++k) {
   rd.Format=k?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R32G32B32A32_FLOAT;
   inputs[k]=make(rd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes;dev->GetCopyableFootprints(&rd,0,1,0,&fp,nullptr,nullptr,&bytes);
   auto up=buffer(bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* p;ck(up->Map(0,nullptr,&p));
   std::memset(p,0,bytes);
   for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x) {
    float* pixel=reinterpret_cast<float*>(static_cast<char*>(p)+y*fp.Footprint.RowPitch)+(k?x:x*4);
    if(k)pixel[0]=depth;else {pixel[0]=.2f;pixel[1]=.3f;pixel[2]=.4f;pixel[3]=1;}
   }
   up->Unmap(0,nullptr);D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=inputs[k].Get();b.pResource=up.Get();
   b.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;b.PlacedFootprint=fp;cmd->CopyTextureRegion(&a,0,0,0,&b,nullptr);
   barrier(inputs[k].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);uploads.push_back(up);
  }
  rd.Format=DXGI_FORMAT_R32_FLOAT;rd.MipLevels=5;rd.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  auto output=make(rd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,7,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};
  ComPtr<ID3D12DescriptorHeap> heap;ck(dev->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));
  auto cpu=heap->GetCPUDescriptorHandleForHeapStart();auto stride=dev->GetDescriptorHandleIncrementSize(hd.Type);
  for(int k=0;k<2;++k) { D3D12_SHADER_RESOURCE_VIEW_DESC s{};s.Format=inputs[k]->GetDesc().Format;
   s.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;s.Texture2D.MipLevels=1;s.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   dev->CreateShaderResourceView(inputs[k].Get(),&s,cpu);cpu.ptr+=stride; }
  for(int m=0;m<5;++m){D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=rd.Format;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;u.Texture2D.MipSlice=m;
   dev->CreateUnorderedAccessView(output.Get(),nullptr,&u,cpu);cpu.ptr+=stride;}
  struct {UINT w,h;float farPlane;UINT reversed;} constants{w,h,1000,test%2};
  auto hp=heap.Get();cmd->SetDescriptorHeaps(1,&hp);cmd->SetComputeRootSignature(root.Get());cmd->SetPipelineState(pipeline.Get());
  cmd->SetComputeRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());cmd->SetComputeRoot32BitConstants(1,4,&constants,0);
  cmd->Dispatch((w+31)/32,(h+31)/32,1);
  barrier(output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT fps[5];UINT64 bytes;dev->GetCopyableFootprints(&rd,0,5,0,fps,nullptr,nullptr,&bytes);
  auto read=buffer(bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
  for(int m=0;m<5;++m){D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=read.Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=fps[m];
   b.pResource=output.Get();b.SubresourceIndex=m;cmd->CopyTextureRegion(&a,0,0,0,&b,nullptr);}
  ck(cmd->Close());ID3D12CommandList* lists[]{cmd.Get()};q->ExecuteCommandLists(1,lists);
  ComPtr<ID3D12Fence> fence;ck(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));ck(q->Signal(fence.Get(),1));
  HANDLE event=CreateEvent(nullptr,FALSE,FALSE,nullptr);ck(fence->SetEventOnCompletion(1,event));
  if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)throw std::runtime_error("GPU timeout");CloseHandle(event);
  void* p;ck(read->Map(0,nullptr,&p));int bad=0;float d=constants.reversed?1-depth:depth;
  float expected=d/(1000-d*999)*1000+1;
  for(auto& fp:fps)for(UINT y=0;y<fp.Footprint.Height;++y)for(UINT x=0;x<fp.Footprint.Width;++x){
   float value=reinterpret_cast<float*>(static_cast<char*>(p)+fp.Offset+y*fp.Footprint.RowPitch)[x];
   if(!std::isfinite(value)||std::abs(value-expected)>.005f*expected)++bad;
  }
  read->Unmap(0,nullptr);std::cout<<"case="<<test<<" size="<<w<<"x"<<h<<" depth="<<depth<<" reversed="<<constants.reversed<<" bad="<<bad<<std::endl;failures+=bad;
 }
 ComPtr<ID3D12InfoQueue> info;if(SUCCEEDED(dev.As(&info)))for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T len=0;info->GetMessage(i,nullptr,&len);std::vector<char>s(len);auto m=reinterpret_cast<D3D12_MESSAGE*>(s.data());info->GetMessage(i,m,&len);if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<m->pDescription<<std::endl;++failures;}}
 return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 2;}
