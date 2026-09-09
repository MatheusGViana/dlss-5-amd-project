#pragma once
#include "AmdPreSr.h"
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <cstring>
#include <mutex>
#include <stdexcept>
namespace AmdPresentExperimental {
using Microsoft::WRL::ComPtr;
inline bool IsTarget() { return false; /* Paused after PCSX2 corruption report. */ static bool target=[] { wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr,path,MAX_PATH); return _wcsicmp(std::filesystem::path(path).filename().c_str(),L"pcsx2-qt.exe")==0; }(); return target; }
inline std::string status="PCSX2 experimental: final image, synthetic guides, no history";
inline std::mutex mutex;
inline void Check(HRESULT h) { if(FAILED(h)) throw std::runtime_error("PCSX2 final-image D3D12 failure: "+std::to_string((UINT)h)); }
inline void Transition(ID3D12GraphicsCommandList* c, ID3D12Resource* r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b) { D3D12_RESOURCE_BARRIER v{};v.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;v.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,a,b};c->ResourceBarrier(1,&v); }
struct Context {
 ComPtr<ID3D12Device> device; ComPtr<ID3D12CommandQueue> queue;
 ComPtr<ID3D12CommandAllocator> allocator; ComPtr<ID3D12GraphicsCommandList> cmd;
 ComPtr<ID3D12Fence> fence; UINT64 serial=0;
 ComPtr<ID3D12Resource> input,motion,depth,output,heldBack;
 ComPtr<ID3D12DescriptorHeap> heap,clearCpu;
 ComPtr<ID3D12RootSignature> root; ComPtr<ID3D12PipelineState> pipeline;
 AmdPreSr::Backend* backend=nullptr; UINT width=0,height=0; DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
 bool stopped=false;
 Context(ID3D12Device* d,ID3D12CommandQueue* q):device(d),queue(q) {
  Check(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
  Check(d->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&cmd)));Check(cmd->Close());
  Check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=2;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;Check(d->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));
  hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;Check(d->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&clearCpu)));
  D3D12_DESCRIPTOR_RANGE ranges[2]{};ranges[0]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};ranges[1]={D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1};
  D3D12_ROOT_PARAMETER params[2]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[0].DescriptorTable={2,ranges};params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[1].Constants={0,0,2};
  D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=2;rd.pParameters=params;ComPtr<ID3DBlob>b,e;
  Check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&b,&e));Check(d->CreateRootSignature(0,b->GetBufferPointer(),b->GetBufferSize(),IID_PPV_ARGS(&root)));
  const char* shader="Texture2D<float4> src:register(t0); RWTexture2D<float4> dst:register(u0); cbuffer C:register(b0){uint w,h;} [numthreads(8,8,1)] void main(uint3 p:SV_DispatchThreadID){if(p.x<w&&p.y<h)dst[p.xy]=src.Load(int3(p.xy,0));}";
  Check(D3DCompile(shader,strlen(shader),"PCSX2 final compose",nullptr,nullptr,"main","cs_5_0",0,0,&b,&e));D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={b->GetBufferPointer(),b->GetBufferSize()};Check(d->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline)));
 }
 ComPtr<ID3D12Resource> Texture(DXGI_FORMAT f,D3D12_RESOURCE_FLAGS flags) {
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC r{};r.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;r.Width=width;r.Height=height;r.DepthOrArraySize=1;r.MipLevels=1;r.Format=f;r.SampleDesc.Count=1;r.Flags=flags;ComPtr<ID3D12Resource> out;Check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&r,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&out)));return out;
 }
 void Frame(ID3D12Resource* back, const std::filesystem::path& directory,AmdPreSr::Settings settings) {
  if(stopped)return;
  if(fence->GetCompletedValue()<serial){status="PCSX2 experimental: GPU work pending";return;}
  if(backend && !backend->Ready()){status=backend->Status();return;}
  auto desc=back->GetDesc();
  if(!backend){
   if(desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM){status="PCSX2 experimental: unsupported backbuffer format";return;}
   // BGRA typed UAV stores are not universally available.
   if(desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM){status="PCSX2 experimental: BGRA output not supported yet";return;}
   width=(UINT)desc.Width;height=desc.Height;format=desc.Format;
   input=Texture(format,D3D12_RESOURCE_FLAG_NONE);output=Texture(format,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   motion=Texture(DXGI_FORMAT_R16G16_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);depth=Texture(DXGI_FORMAT_R32_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   backend=new AmdPreSr::Backend(device.Get(),queue.Get(),directory);
  }
  if(desc.Width!=width||desc.Height!=height||desc.Format!=format){status="PCSX2 experimental: restart emulator after output size change";return;}
  heldBack=back;
  Check(allocator->Reset());Check(cmd->Reset(allocator.Get(),nullptr));
  Transition(cmd.Get(),back,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE);
  Transition(cmd.Get(),input.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);cmd->CopyResource(input.Get(),back);
  Transition(cmd.Get(),back,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PRESENT);
  Transition(cmd.Get(),input.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  ID3D12DescriptorHeap* heaps[]={heap.Get()};cmd->SetDescriptorHeaps(1,heaps);
  auto cpu=heap->GetCPUDescriptorHandleForHeapStart();auto gpu=heap->GetGPUDescriptorHandleForHeapStart();
  auto clear=clearCpu->GetCPUDescriptorHandleForHeapStart();
  for(auto r:{motion.Get(),depth.Get()}) {
   D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=r->GetDesc().Format;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
   // Distinct descriptors for the two clears, never overwritten while executing.
   device->CreateUnorderedAccessView(r,nullptr,&u,cpu);device->CreateUnorderedAccessView(r,nullptr,&u,clear);Transition(cmd.Get(),r,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   float values[4]={r==depth.Get()?0.5f:0.f,0,0,0};cmd->ClearUnorderedAccessViewFloat(gpu,clear,r,values,0,nullptr);
   Transition(cmd.Get(),r,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   clear.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
   cpu.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);gpu.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  }
  AmdPreSr::Frame f{};f.colour=input.Get();f.motion=motion.Get();f.depth=depth.Get();f.width=width;f.height=height;f.reset=true;
  settings.passes=1;settings.modelScale=1;auto result=backend->Record(cmd.Get(),f,settings);
  // Composition uses a separate heap: guide descriptors must remain unchanged.
  // A skipped record is allowed; never reuse an old neural frame.
  Transition(cmd.Get(),input.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
  Transition(cmd.Get(),motion.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
  Transition(cmd.Get(),depth.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
  if(result) Compose(back,result);
  Check(cmd->Close());ID3D12CommandList* lists[]={cmd.Get()};backend->Submitting(queue.Get(),1,lists);queue->ExecuteCommandLists(1,lists);backend->Submitted(queue.Get(),1,lists);Check(queue->Signal(fence.Get(),++serial));
  status="PCSX2 experimental (synthetic guides, no history): "+backend->Status();
 }
 ComPtr<ID3D12DescriptorHeap> composeHeap;
 void Compose(ID3D12Resource* back,ID3D12Resource* result) {
  if(!composeHeap){D3D12_DESCRIPTOR_HEAP_DESC d{};d.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;d.NumDescriptors=2;d.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;Check(device->CreateDescriptorHeap(&d,IID_PPV_ARGS(&composeHeap)));}
  auto cpu=composeHeap->GetCPUDescriptorHandleForHeapStart();D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;device->CreateShaderResourceView(result,&srv,cpu);cpu.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=format;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;device->CreateUnorderedAccessView(output.Get(),nullptr,&u,cpu);
  Transition(cmd.Get(),output.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);auto h=composeHeap.Get();cmd->SetDescriptorHeaps(1,&h);cmd->SetComputeRootSignature(root.Get());cmd->SetPipelineState(pipeline.Get());cmd->SetComputeRootDescriptorTable(0,composeHeap->GetGPUDescriptorHandleForHeapStart());UINT dims[]={width,height};cmd->SetComputeRoot32BitConstants(1,2,dims,0);cmd->Dispatch((width+7)/8,(height+7)/8,1);
  Transition(cmd.Get(),output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);Transition(cmd.Get(),back,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST);cmd->CopyResource(back,output.Get());Transition(cmd.Get(),back,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PRESENT);Transition(cmd.Get(),output.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON);
 }
};
inline Context* context=nullptr;
inline std::string Status(){std::lock_guard g(mutex);return status;}
inline void Render(IDXGISwapChain3* sc,ID3D12CommandQueue* queue,const std::filesystem::path& directory,AmdPreSr::Settings s){
 std::lock_guard g(mutex);if(!queue)return;
 try{ComPtr<ID3D12Device>d;Check(sc->GetDevice(IID_PPV_ARGS(&d)));if(!context)context=new Context(d.Get(),queue);if(context->device.Get()!=d.Get()||context->queue.Get()!=queue){status="PCSX2 experimental: device/queue changed; restart required";return;}ComPtr<ID3D12Resource>b;Check(sc->GetBuffer(sc->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&b)));context->Frame(b.Get(),directory,s);}catch(const std::exception&e){status=e.what();if(context)context->stopped=true;}
}
}
