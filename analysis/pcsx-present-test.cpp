#include "AmdPreSr.h"
#include <cstring>
#include <cmath>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include "PresentExperimental.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
void ck(HRESULT hr) {
  if (FAILED(hr))
    throw std::runtime_error("HRESULT " +
                             std::to_string(static_cast<unsigned>(hr)));
}
void barrier(ID3D12GraphicsCommandList *c, ID3D12Resource *r,
             D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b) {
  D3D12_RESOURCE_BARRIER v{};
  v.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  v.Transition = {r, 0, a, b};
  c->ResourceBarrier(1, &v);
}
int wmain(int argc, wchar_t **argv) {
  try {
    if (argc < 2)
      return 2;
    UINT passes = argc > 2 ? _wtoi(argv[2]) : 1;
    UINT size = argc > 3 ? _wtoi(argv[3]) : 128;
    UINT active = argc > 4 ? _wtoi(argv[4]) : size;
    UINT depthBits = argc > 5 ? _wtoi(argv[5]) : 0;
    bool queueChanges = argc > 6 && _wtoi(argv[6]) != 0;
    bool resizeEveryFrame = argc > 7 && _wtoi(argv[7]) != 0;
    bool forceTimeout = argc > 8 && _wtoi(argv[8]) != 0;
    bool delayedNotification = argc > 9 && _wtoi(argv[9]) != 0;
    bool varySettings = argc > 10 && _wtoi(argv[10]) != 0;
    bool guideConversion = argc > 11 && _wtoi(argv[11]) != 0;
    int lookMode = argc > 12 ? _wtoi(argv[12]) : 0;
    int rtgiMode = argc > 13 ? _wtoi(argv[13]) : 0;
    if (size < 64 || size > 2048 || active < 64 || active > size)
      return 2;
    ComPtr<ID3D12Debug> debug;
    if (argc <= 19 && SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
      debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;
    ck(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; ++i) {
      DXGI_ADAPTER_DESC1 desc{};
      adapter->GetDesc1(&desc);
      if (desc.VendorId == 0x1002 &&
          SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                      IID_PPV_ARGS(&device))))
        break;
      adapter.Reset();
    }
    if (!device)
      throw std::runtime_error("No AMD D3D12 device");
    ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC qd{};
    ck(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)));

    auto context = new AmdPresentExperimental::Context(device.Get(),queue.Get());
    D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;rd.Width=128;rd.Height=128;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;rd.SampleDesc.Count=1;rd.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    ComPtr<ID3D12Resource> back;ck(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_PRESENT,nullptr,IID_PPV_ARGS(&back)));
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;ComPtr<ID3D12DescriptorHeap> rtv;ck(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)));device->CreateRenderTargetView(back.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
    ck(context->allocator->Reset());ck(context->cmd->Reset(context->allocator.Get(),nullptr));barrier(context->cmd.Get(),back.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);float clear[4]={.3f,.4f,.5f,1};context->cmd->ClearRenderTargetView(rtv->GetCPUDescriptorHandleForHeapStart(),clear,0,nullptr);barrier(context->cmd.Get(),back.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);ck(context->cmd->Close());ID3D12CommandList* lists[]={context->cmd.Get()};queue->ExecuteCommandLists(1,lists);ck(queue->Signal(context->fence.Get(),++context->serial));
    auto wait=[&]{auto start=GetTickCount64();while(context->fence->GetCompletedValue()<context->serial || (context->backend&&!context->backend->Ready())){if(GetTickCount64()-start>10000)throw std::runtime_error("test completion timeout");Sleep(1);}};wait();
    for(int i=0;i<8;++i){AmdPreSr::Settings cfg{};cfg.toneChannels=true;cfg.tone=.5f;context->Frame(back.Get(),argv[1],cfg);wait();std::cout<<context->backend->Status()<<std::endl;}
    ComPtr<ID3D12InfoQueue> info;unsigned errors=0;if(SUCCEEDED(device.As(&info))){for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T n=0;info->GetMessage(i,nullptr,&n);std::vector<char>b(n);auto m=(D3D12_MESSAGE*)b.data();info->GetMessage(i,m,&n);if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<m->pDescription<<std::endl;++errors;}}}
    if(!context->backend->Shutdown())throw std::runtime_error("shutdown failed");ExitProcess(errors?3:0);
  } catch(const std::exception& e){std::cerr<<e.what()<<std::endl;ExitProcess(1);}
}
