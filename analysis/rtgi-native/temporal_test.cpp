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
 D3D12_DESCRIPTOR_RANGE ranges[2] {{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,10,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,5,0,0,10}};
 D3D12_ROOT_PARAMETER rp[2]{}; rp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rp[0].DescriptorTable={2,ranges};
 rp[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;rp[1].Constants={0,0,16};
 D3D12_STATIC_SAMPLER_DESC sampler{}; sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
 sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
 sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;sampler.MaxLOD=D3D12_FLOAT32_MAX;
 D3D12_STATIC_SAMPLER_DESC samplers[2]{sampler,sampler};samplers[1].ShaderRegister=1;samplers[1].Filter=D3D12_FILTER_MIN_MAG_MIP_POINT;
 D3D12_ROOT_SIGNATURE_DESC rs{2,rp,2,samplers,D3D12_ROOT_SIGNATURE_FLAG_NONE};
 ComPtr<ID3DBlob> blob,errors;ck(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
 ComPtr<ID3D12RootSignature> root;ck(dev->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)));
 std::ifstream file("analysis/rtgi-native/TemporalReprojectionCS.cso",std::ios::binary);
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
 for(UINT test=0;test<8;++test){
  UINT w=test%2?65:64,h=test%2?33:64;
  ComPtr<ID3D12CommandAllocator> alloc;ck(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)));
  ComPtr<ID3D12GraphicsCommandList> cmd;ck(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&cmd)));
  auto transition=[&](ID3D12Resource* r,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};cmd->ResourceBarrier(1,&b);};
  D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,15,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};
  ComPtr<ID3D12DescriptorHeap> heap;ck(dev->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));auto cpu=heap->GetCPUDescriptorHandleForHeapStart();auto stride=dev->GetDescriptorHandleIncrementSize(hd.Type);
  std::vector<ComPtr<ID3D12Resource>> inputs,uploads;
  D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;rd.Width=w;rd.Height=h;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.SampleDesc.Count=1;
  for(int k=0;k<10;++k){
   rd.Format=k==0?DXGI_FORMAT_R32_FLOAT:k==3?DXGI_FORMAT_R32G32_FLOAT:DXGI_FORMAT_R32G32B32A32_FLOAT;
   auto r=make(rd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes;dev->GetCopyableFootprints(&rd,0,1,0,&fp,nullptr,nullptr,&bytes);
   auto up=buffer(bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* p;ck(up->Map(0,nullptr,&p));std::memset(p,0,bytes);
   UINT channels=k==0?1:k==3?2:4;
   for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){auto v=reinterpret_cast<float*>(static_cast<char*>(p)+y*fp.Footprint.RowPitch)+x*channels;
    if(k==0)v[0]=2;
    else if(k==1||k==2)v[2]=-1;
    else if(k==3){v[0]=test>=4&&test<6?2.f:0;v[1]=0;}
    else if(k==4||k==8){v[0]=.2f;v[1]=.1f;v[2]=.05f;v[3]=.8f;}
    else if(k==5){v[0]=.8f;v[1]=.1f;v[2]=.05f;v[3]=.8f;}
    else if(k==6||k==9){v[0]=test>=6?100.f:2.f;v[1]=1;v[2]=1;v[3]=0;}
    else if(k==7){v[0]=.2f;v[1]=0;v[2]=.8f;v[3]=0;}
   }
   up->Unmap(0,nullptr);D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=r.Get();b.pResource=up.Get();b.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;b.PlacedFootprint=fp;
   cmd->CopyTextureRegion(&a,0,0,0,&b,nullptr);transition(r.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=rd.Format;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Texture2D.MipLevels=1;
   dev->CreateShaderResourceView(r.Get(),&view,cpu);cpu.ptr+=stride;inputs.push_back(r);uploads.push_back(up);
  }
  rd.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;rd.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  ComPtr<ID3D12Resource> outputs[5];
  for(int k=0;k<5;++k){outputs[k]=make(rd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=rd.Format;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;dev->CreateUnorderedAccessView(outputs[k].Get(),nullptr,&u,cpu);cpu.ptr+=stride;}
  struct {UINT w,h;float farPlane,tanHalfFov;UINT history,quality,iteration;float smoothness,fade,ao,ambient,il;UINT debug,p0,p1,p2;} constants{w,h,1000,.7f,test>=2,1,0,.5f,1,1,1,1,0,0,0,0};
  auto hp=heap.Get();cmd->SetDescriptorHeaps(1,&hp);cmd->SetComputeRootSignature(root.Get());cmd->SetPipelineState(pipeline.Get());cmd->SetComputeRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());cmd->SetComputeRoot32BitConstants(1,16,&constants,0);
  cmd->Dispatch((w+7)/8,(h+7)/8,1);
  ComPtr<ID3D12Resource> reads[2];D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes;dev->GetCopyableFootprints(&rd,0,1,0,&fp,nullptr,nullptr,&bytes);
  for(int k=0;k<2;++k){transition(outputs[k+3].Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);reads[k]=buffer(bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=reads[k].Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=fp;b.pResource=outputs[k+3].Get();cmd->CopyTextureRegion(&a,0,0,0,&b,nullptr);}
  ck(cmd->Close());ID3D12CommandList* lists[]{cmd.Get()};q->ExecuteCommandLists(1,lists);
  ComPtr<ID3D12Fence> fence;ck(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));ck(q->Signal(fence.Get(),1));HANDLE event=CreateEvent(nullptr,FALSE,FALSE,nullptr);ck(fence->SetEventOnCompletion(1,event));if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)throw std::runtime_error("Temporal GPU timeout");CloseHandle(event);
  int bad=0;float expected=test>=2&&test<4?.71f:.2f;
  for(int k=0;k<2;++k){void* p;ck(reads[k]->Map(0,nullptr,&p));for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){auto v=reinterpret_cast<float*>(static_cast<char*>(p)+y*fp.Footprint.RowPitch)+4*x;
   for(int c=0;c<4;++c)if(!std::isfinite(v[c]))++bad;
   if(k==0&&std::abs(v[0]-expected)>.001f)++bad;
   if(k==1&&(std::abs(v[0]-2)>.001f||v[3]<0))++bad;
  }reads[k]->Unmap(0,nullptr);}
  std::cout<<"case="<<test<<" size="<<w<<"x"<<h<<" expected_luminance="<<expected<<" bad="<<bad<<std::endl;failures+=bad;
 }
 ComPtr<ID3D12InfoQueue> info;if(SUCCEEDED(dev.As(&info)))for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T len=0;info->GetMessage(i,nullptr,&len);std::vector<char>s(len);auto m=reinterpret_cast<D3D12_MESSAGE*>(s.data());info->GetMessage(i,m,&len);if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<m->pDescription<<std::endl;++failures;}}
 return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 2;}
