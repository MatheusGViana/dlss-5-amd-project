#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <iostream>
#include "imgui.h"
#include "imgui_impl_dx12.h"
using Microsoft::WRL::ComPtr;
static void Check(HRESULT hr) { if (FAILED(hr)) throw hr; }
int main()
{
    try {
        ComPtr<IDXGIFactory4> factory;
        Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
        ComPtr<IDXGIAdapter> warp;
        Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        ComPtr<ID3D12Device5> device;
        Check(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
        ComPtr<ID3D12CommandQueue> queue;
        D3D12_COMMAND_QUEUE_DESC qd{};
        Check(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)));
        ComPtr<ID3D12DescriptorHeap> heap;
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.NumDescriptors = 8;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        Check(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)));
        ImGui::CreateContext();
        DescriptorHeapAllocator allocator;
        allocator.Create(device.Get(), heap.Get());
        ImGui_ImplDX12_InitInfo info;
        info.Device = device.Get(); info.CommandQueue = queue.Get();
        info.NumFramesInFlight = 2; info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        info.SrvDescriptorHeap = heap.Get(); info.UserData = &allocator;
        info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* i, D3D12_CPU_DESCRIPTOR_HANDLE* c, D3D12_GPU_DESCRIPTOR_HANDLE* g) {
            static_cast<DescriptorHeapAllocator*>(i->UserData)->Alloc(c, g);
        };
        info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* i, D3D12_CPU_DESCRIPTOR_HANDLE c, D3D12_GPU_DESCRIPTOR_HANDLE g) {
            static_cast<DescriptorHeapAllocator*>(i->UserData)->Free(c, g);
        };
        if (!ImGui_ImplDX12_Init(&info)) return 2;
        ImTextureData normal;
        normal.Create(ImTextureFormat_RGBA32, 4, 4);
        normal.SetStatus(ImTextureStatus_WantCreate);
        ImGui_ImplDX12_UpdateTexture(&normal);
        if (normal.Status != ImTextureStatus_OK) return 3;
        normal.UnusedFrames = 2;
        normal.SetStatus(ImTextureStatus_WantDestroy);
        ImGui_ImplDX12_UpdateTexture(&normal);
        std::cout << "Normal texture upload and destruction passed\n";
        device->RemoveDevice(); // Only this isolated WARP test device.
        std::cout << "RemoveDevice returned, reason=" << std::hex << device->GetDeviceRemovedReason() << std::endl;
        ImTextureData lost;
        lost.Create(ImTextureFormat_RGBA32, 4, 4);
        lost.SetStatus(ImTextureStatus_WantCreate);
        ImGui_ImplDX12_UpdateTexture(&lost);
        std::cout << "Lost-device update returned, status=" << lost.Status << std::endl;
        if (lost.BackendUserData != nullptr || lost.Status != ImTextureStatus_WantCreate) return 4;
        std::cout << "Removed-device texture update skipped without dereferencing a failed allocation\n";
        ImGui_ImplDX12_Shutdown();
        ImGui::DestroyContext();
        return 0;
    } catch (HRESULT hr) { std::cerr << std::hex << hr << std::endl; return 1; }
}
