#pragma once
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <stdexcept>
#include <cstring>
namespace AmdPreSr {
// SDR transfer choices; these do not change gamut or perform HDR tone mapping.
inline constexpr char EncodingShader[]=R"(
Texture2D<float4> src:register(t0);RWTexture2D<float4> dst:register(u0);
cbuffer Params:register(b0){uint w,h,mode,inverse;}
float transform(float v){float a=abs(v);float y=a;
 if(mode==2)y=inverse?(a<=0.0031308?12.92*a:1.055*pow(a,1.0/2.4)-0.055):(a<=0.04045?a/12.92:pow((a+0.055)/1.055,2.4));
 if(mode==3)y=pow(a,inverse?1.0/2.2:2.2);
 return sign(v)*y;}
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){if(p.x>=w||p.y>=h)return;float4 c=src.Load(int3(p.xy,0));dst[p.xy]=float4(transform(c.r),transform(c.g),transform(c.b),c.a);}
)";
class ColorEncoding {
 using MicrosoftPtr=Microsoft::WRL::ComPtr<ID3D12Resource>;
 Microsoft::WRL::ComPtr<ID3D12Device> device;
 Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
 Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
 Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
 MicrosoftPtr output;
 static void Check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("AMD encoding D3D12 error "+std::to_string((UINT)hr));}
 static void Barrier(ID3D12GraphicsCommandList*c,ID3D12Resource*r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){if(a==b)return;D3D12_RESOURCE_BARRIER v{};v.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;v.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,a,b};c->ResourceBarrier(1,&v);}
 public:
 ColorEncoding(ID3D12Device*d):device(d){
  D3D12_DESCRIPTOR_RANGE ranges[2]={{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1}};
  D3D12_ROOT_PARAMETER params[2]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[0].DescriptorTable={2,ranges};params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[1].Constants={0,0,4};
  D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=2;rd.pParameters=params;Microsoft::WRL::ComPtr<ID3DBlob>b,e;Check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&b,&e));Check(d->CreateRootSignature(0,b->GetBufferPointer(),b->GetBufferSize(),IID_PPV_ARGS(&root)));
  Check(D3DCompile(EncodingShader,sizeof(EncodingShader),"AMD encoding",nullptr,nullptr,"main","cs_5_0",0,0,&b,&e));D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={b->GetBufferPointer(),b->GetBufferSize()};Check(d->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline)));
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=2;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;Check(d->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));
 }
 ID3D12Resource* Run(ID3D12GraphicsCommandList*c,ID3D12Resource*src,D3D12_RESOURCE_STATES state,UINT w,UINT h,UINT mode,bool inverse){
  if(!output||output->GetDesc().Width!=w||output->GetDesc().Height!=h){D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;rd.Width=w;rd.Height=h;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;rd.SampleDesc.Count=1;rd.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;output.Reset();Check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&output)));}
  auto format=src->GetDesc().Format;
  // Request raw channel values, avoiding automatic sRGB decoding twice.
  if(format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||format==DXGI_FORMAT_R8G8B8A8_TYPELESS)format=DXGI_FORMAT_R8G8B8A8_UNORM;
  if(format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||format==DXGI_FORMAT_B8G8R8A8_TYPELESS)format=DXGI_FORMAT_B8G8R8A8_UNORM;
  D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;auto cpu=heap->GetCPUDescriptorHandleForHeapStart();device->CreateShaderResourceView(src,&srv,cpu);cpu.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;device->CreateUnorderedAccessView(output.Get(),nullptr,&u,cpu);
  Barrier(c,src,state,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);Barrier(c,output.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);auto hh=heap.Get();c->SetDescriptorHeaps(1,&hh);c->SetComputeRootSignature(root.Get());c->SetPipelineState(pipeline.Get());c->SetComputeRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());UINT v[]={w,h,mode,inverse?1u:0u};c->SetComputeRoot32BitConstants(1,4,v,0);c->Dispatch((w+7)/8,(h+7)/8,1);Barrier(c,output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);Barrier(c,src,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,state);return output.Get();
 }
};
}
