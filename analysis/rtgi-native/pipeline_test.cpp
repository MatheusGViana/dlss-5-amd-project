#include "RtgiNative.h"
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
int main(int argc,char** argv) try {
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
 auto make=[&](D3D12_RESOURCE_DESC rd,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state) {
  D3D12_HEAP_PROPERTIES hp{};hp.Type=type;ComPtr<ID3D12Resource> r;
  ck(dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,state,nullptr,IID_PPV_ARGS(&r)));return r;
 };
 auto buffer=[&](UINT64 bytes,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state) {
  D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;
  d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return make(d,type,state);
 };

 AmdPreSr::RtgiNative rtgi(dev.Get(),L"package-amd-presr/experimental_lighting");
 int failures=0;
 for(UINT test=0;test<12;++test){
  UINT w=argc>1?UINT(std::atoi(argv[1]))+(test%2):test%2?65:64,h=argc>1?w/2:test%2?33:64;
  ComPtr<ID3D12CommandAllocator> alloc;ck(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)));
  ComPtr<ID3D12GraphicsCommandList> cmd;ck(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&cmd)));
  auto transition=[&](ID3D12Resource* r,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};cmd->ResourceBarrier(1,&b);};
  ComPtr<ID3D12Resource> inputs[3];std::vector<ComPtr<ID3D12Resource>> uploads;
  D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;rd.Width=w;rd.Height=h;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.SampleDesc.Count=1;
  for(int k=0;k<3;++k){
   rd.Format=k==0?DXGI_FORMAT_R32G32B32A32_FLOAT:k==1?(argc>2?DXGI_FORMAT_R32G8X24_TYPELESS:DXGI_FORMAT_R32_FLOAT):DXGI_FORMAT_R32G32_FLOAT;
   rd.Flags=k==1&&argc>2?D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL:D3D12_RESOURCE_FLAG_NONE;
   if(k==1&&argc>2){
    inputs[k]=make(rd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_DEPTH_WRITE);
    D3D12_DESCRIPTOR_HEAP_DESC dh{D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,D3D12_DESCRIPTOR_HEAP_FLAG_NONE,0};ComPtr<ID3D12DescriptorHeap> dsv;ck(dev->CreateDescriptorHeap(&dh,IID_PPV_ARGS(&dsv)));
    D3D12_DEPTH_STENCIL_VIEW_DESC dv{};dv.Format=DXGI_FORMAT_D32_FLOAT_S8X24_UINT;dv.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;dev->CreateDepthStencilView(inputs[k].Get(),&dv,dsv->GetCPUDescriptorHandleForHeapStart());
    cmd->ClearDepthStencilView(dsv->GetCPUDescriptorHandleForHeapStart(),D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,.5f,0,0,nullptr);
    transition(inputs[k].Get(),D3D12_RESOURCE_STATE_DEPTH_WRITE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);continue;
   }
   inputs[k]=make(rd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes;dev->GetCopyableFootprints(&rd,0,1,0,&fp,nullptr,nullptr,&bytes);
   auto up=buffer(bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* p;ck(up->Map(0,nullptr,&p));std::memset(p,0,bytes);
   UINT channels=k==0?4:k==1?(argc>2?2:1):2;
   for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){auto v=reinterpret_cast<float*>(static_cast<char*>(p)+y*fp.Footprint.RowPitch)+x*channels;
    if(k==0){v[0]=.25f;v[1]=.5f;v[2]=.75f;v[3]=1;}
    if(k==1)v[0]=test>=2?.5f-.2f*std::cos(float(x)/float(w-1)*6.2831853f):.5f;
   }
   up->Unmap(0,nullptr);D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=inputs[k].Get();b.pResource=up.Get();b.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;b.PlacedFootprint=fp;
   cmd->CopyTextureRegion(&a,0,0,0,&b,nullptr);transition(inputs[k].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);uploads.push_back(up);
  }
  AmdPreSr::Frame frame;frame.colour=inputs[0].Get();frame.depth=inputs[1].Get();frame.motion=inputs[2].Get();frame.width=w;frame.height=h;frame.reset=true;
  AmdPreSr::RtgiSettings settings;settings.enabled=true;settings.occlusion=test>=4?5:0;settings.lighting=test>=2?10:0;settings.ambient=1;
  if(test==6||test==7){settings.lighting=0;settings.occlusion=0;settings.contact=2;}
  if(test==8||test==9){settings.saturation=0;settings.radius=3;}
  if(test==10||test==11){settings.saturation=2;settings.radius=.25f;}
  auto output=rtgi.Record(cmd.Get(),frame,settings);rd=output->GetDesc();transition(output,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes;dev->GetCopyableFootprints(&rd,0,1,0,&fp,nullptr,nullptr,&bytes);auto read=buffer(bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
  D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=read.Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=fp;b.pResource=output;cmd->CopyTextureRegion(&a,0,0,0,&b,nullptr);transition(output,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  ck(cmd->Close());ID3D12CommandList* lists[]{cmd.Get()};q->ExecuteCommandLists(1,lists);
  ComPtr<ID3D12Fence> fence;ck(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));ck(q->Signal(fence.Get(),1));HANDLE event=CreateEvent(nullptr,FALSE,FALSE,nullptr);ck(fence->SetEventOnCompletion(1,event));if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)throw std::runtime_error("Pipeline GPU timeout");CloseHandle(event);
  void* p;ck(read->Map(0,nullptr,&p));int bad=0,changed=0;const uint16_t expected[]{0x3400,0x3800,0x3a00,0x3c00};
  for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){auto v=reinterpret_cast<uint16_t*>(static_cast<char*>(p)+y*fp.Footprint.RowPitch)+4*x;bool different=false;
   for(int c=0;c<4;++c){if((v[c]&0x7c00)==0x7c00)++bad;if(v[c]!=expected[c])different=true;}changed+=different;
  }
  if((test<2||argc>2)&&changed)++bad;if(test>=2&&!changed&&!(argc>2))++bad;read->Unmap(0,nullptr);
  std::cout<<"case="<<test<<" size="<<w<<"x"<<h<<" changed="<<changed<<" bad="<<bad<<std::endl;failures+=bad;
 }
 ComPtr<ID3D12InfoQueue> info;if(SUCCEEDED(dev.As(&info)))for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T len=0;info->GetMessage(i,nullptr,&len);std::vector<char>s(len);auto m=reinterpret_cast<D3D12_MESSAGE*>(s.data());info->GetMessage(i,m,&len);if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<m->pDescription<<std::endl;++failures;}}
 return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 2;}
