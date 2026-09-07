#include "AmdPreSr.h"
#include <cstring>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
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
    if (size < 64 || size > 2048 || active < 64 || active > size)
      return 2;
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
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
    ComPtr<ID3D12CommandQueue> secondQueue, presentQueue;
    ck(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&secondQueue)));
    ck(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&presentQueue)));
    ComPtr<ID3D12CommandAllocator> alloc;
    ck(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      IID_PPV_ARGS(&alloc)));
    ComPtr<ID3D12GraphicsCommandList> cmd;
    ck(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(),
                                 nullptr, IID_PPV_ARGS(&cmd)));
    auto buffer = [&](UINT64 size, D3D12_HEAP_TYPE type,
                      D3D12_RESOURCE_STATES state) {
      ComPtr<ID3D12Resource> r;
      D3D12_HEAP_PROPERTIES hp{};
      hp.Type = type;
      D3D12_RESOURCE_DESC rd{};
      rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      rd.Width = size;
      rd.Height = 1;
      rd.DepthOrArraySize = 1;
      rd.MipLevels = 1;
      rd.SampleDesc.Count = 1;
      rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      ck(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, state,
                                         nullptr, IID_PPV_ARGS(&r)));
      return r;
    };
    std::vector<ComPtr<ID3D12Resource>> uploads;
    ComPtr<ID3D12DescriptorHeap> depthHeap;
    auto tex = [&](DXGI_FORMAT format, int kind) {
      ComPtr<ID3D12Resource> r;
      D3D12_HEAP_PROPERTIES hp{};
      hp.Type = D3D12_HEAP_TYPE_DEFAULT;
      D3D12_RESOURCE_DESC rd{};
      rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      rd.Width = size;
      rd.Height = size;
      rd.DepthOrArraySize = 1;
      rd.MipLevels = 1;
      rd.SampleDesc.Count = 1;
      rd.Format = format;
      if (kind == 2 && depthBits) {
        DXGI_FORMAT view = depthBits == 16 ? DXGI_FORMAT_D16_UNORM :
                           depthBits == 24 ? DXGI_FORMAT_D24_UNORM_S8_UINT :
                           depthBits == 64 ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_D32_FLOAT;
        rd.Format = depthBits == 16 ? DXGI_FORMAT_R16_TYPELESS :
                    depthBits == 24 ? DXGI_FORMAT_R24G8_TYPELESS :
                    depthBits == 64 ? DXGI_FORMAT_R32G8X24_TYPELESS : DXGI_FORMAT_R32_TYPELESS;
        rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        ck(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, nullptr, IID_PPV_ARGS(&r)));
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        hd.NumDescriptors = 1;
        ck(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&depthHeap)));
        D3D12_DEPTH_STENCIL_VIEW_DESC vd{};
        vd.Format = view;
        vd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        auto handle = depthHeap->GetCPUDescriptorHandleForHeapStart();
        device->CreateDepthStencilView(r.Get(), &vd, handle);
        cmd->ClearDepthStencilView(handle, D3D12_CLEAR_FLAG_DEPTH, 0.5f, 0, 0, nullptr);
        D3D12_RESOURCE_BARRIER transition{};
        transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        transition.Transition = {r.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
        cmd->ResourceBarrier(1, &transition);
        return r;
      }
      ck(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                         D3D12_RESOURCE_STATE_COPY_DEST,
                                         nullptr, IID_PPV_ARGS(&r)));
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
      UINT64 totalBytes;
      device->GetCopyableFootprints(&rd, 0, 1, 0, &fp, nullptr, nullptr,
                                    &totalBytes);
      auto up = buffer(totalBytes, D3D12_HEAP_TYPE_UPLOAD,
                       D3D12_RESOURCE_STATE_GENERIC_READ);
      unsigned char *data;
      ck(up->Map(0, nullptr, reinterpret_cast<void **>(&data)));
      std::memset(data, 0, totalBytes);
      for (UINT y = 0; y < size; ++y)
        for (UINT x = 0; x < size; ++x) {
          auto p = data + y * fp.Footprint.RowPitch;
          if (kind == 0) {
            uint16_t pixel[4] = {static_cast<uint16_t>(0x3000 + (x * 12)),
                                 static_cast<uint16_t>(0x3400 + y * 8), 0x3800,
                                 0x3c00};
            std::memcpy(p + x * 8, pixel, 8);
          } else if (kind == 2) {
            float z = 0.5f;
            std::memcpy(p + x * 4, &z, 4);
          }
        }
      up->Unmap(0, nullptr);
      D3D12_TEXTURE_COPY_LOCATION dst{};
      dst.pResource = r.Get();
      dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      D3D12_TEXTURE_COPY_LOCATION src{};
      src.pResource = up.Get();
      src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      src.PlacedFootprint = fp;
      cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
      barrier(cmd.Get(), r.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
              D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      uploads.push_back(up);
      return r;
    };
    auto colour = tex(DXGI_FORMAT_R16G16B16A16_FLOAT, 0);
    auto motion = tex(DXGI_FORMAT_R16G16_FLOAT, 1);
    auto depth = tex(DXGI_FORMAT_R32_FLOAT, 2);
    auto backend = new AmdPreSr::Backend(device.Get(), queueChanges ? presentQueue.Get() : queue.Get(), argv[1]);
    AmdPreSr::Frame frame{};
    frame.colour = colour.Get();
    frame.motion = motion.Get();
    frame.depth = depth.Get();
    frame.width = active;
    frame.height = active;
    frame.motionScaleX = (float)active;
    frame.motionScaleY = (float)active;
    AmdPreSr::Settings settings{};
    settings.passes = passes;
    size_t badFrames = 0;
    for (int iteration = 0; iteration < 8; ++iteration) {
      if (varySettings) {
        settings.passes = 1 + iteration % passes;
        settings.tone = iteration % 2 ? 0.5f : 0.0f;
        settings.structure = iteration % 2 ? 0.75f : 1.0f;
      }
      if (resizeEveryFrame) {
        active = iteration % 2 == 0 ? size : size / 2;
        frame.width = active;
        frame.height = active;
      } else if (iteration == 4 && active >= 128) {
        active -= 32;
        frame.width = active;
        frame.height = active;
      }
      frame.reset = (iteration == 5);
      if (iteration == 3) backend->InvalidateHistory();
      auto out = backend->Record(cmd.Get(), frame, settings);
      if (!out && forceTimeout && backend->Status().find("retry in 1s") != std::string::npos) {
        std::cout << "Cooldown observed; waiting to test recovery" << std::endl;
        Sleep(1100);
        out = backend->Record(cmd.Get(), frame, settings);
      }
      if (!out)
        throw std::runtime_error(backend->Status());
      if (varySettings || iteration == 3) {
        for (UINT pass = 1; pass <= settings.passes; ++pass) {
          auto name = L"dlssnr_amd_pass" + std::to_wstring(pass) + L".dll";
          auto base = reinterpret_cast<unsigned char*>(GetModuleHandleW(name.c_str()));
          if (!base || *(base + 0x765f8) != 0)
            throw std::runtime_error("Temporal history was not invalidated before the next job");
        }
      }
      auto desc = out->GetDesc();
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
      UINT64 bytes;
      device->GetCopyableFootprints(&desc, 0, 1, 0, &fp, nullptr, nullptr,
                                    &bytes);
      auto read = buffer(bytes, D3D12_HEAP_TYPE_READBACK,
                         D3D12_RESOURCE_STATE_COPY_DEST);
      barrier(cmd.Get(), out, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
              D3D12_RESOURCE_STATE_COPY_SOURCE);
      D3D12_TEXTURE_COPY_LOCATION dst{};
      dst.pResource = read.Get();
      dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      dst.PlacedFootprint = fp;
      D3D12_TEXTURE_COPY_LOCATION src{};
      src.pResource = out;
      src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
      barrier(cmd.Get(), out, D3D12_RESOURCE_STATE_COPY_SOURCE,
              D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      ck(cmd->Close());
      ID3D12CommandList *lists[]{cmd.Get()};
      auto start = GetTickCount64();
      auto submitQueue = queueChanges && (iteration % 4 >= 2) ? secondQueue.Get() : queue.Get();
      const bool forced = forceTimeout && iteration == 2;
      if (forced && !delayedNotification) {
        // Inject the recovered watchdog abort word, without changing its code.
        for (UINT pass = 1; pass <= settings.passes; ++pass) {
          auto name = L"dlssnr_amd_pass" + std::to_wstring(pass) + L".dll";
          auto base = reinterpret_cast<unsigned char*>(GetModuleHandleW(name.c_str()));
          if (!base) throw std::runtime_error("Test runtime not loaded");
          auto abortWord = *reinterpret_cast<volatile LONG**>(base + 0x76c68);
          auto job = *reinterpret_cast<UINT*>(base + 0x76d74);
          if (!abortWord) throw std::runtime_error("Watchdog test word unavailable");
          InterlockedExchange(abortWord, static_cast<LONG>(job));
        }
      }
      // An unrelated presentation submission must not publish our HIP job.
      backend->Submitting(presentQueue.Get(), 0, nullptr);
      backend->Submitted(presentQueue.Get(), 0, nullptr);
      if (!(forced && delayedNotification)) backend->Submitting(submitQueue, 1, lists);
      submitQueue->ExecuteCommandLists(1, lists);
      if (forced && delayedNotification) {
        ComPtr<ID3D12Fence> boundedFence;
        ck(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&boundedFence)));
        ck(submitQueue->Signal(boundedFence.Get(), 1));
        HANDLE done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        ck(boundedFence->SetEventOnCompletion(1, done));
        auto result = WaitForSingleObject(done, 1500);
        CloseHandle(done);
        if (result != WAIT_OBJECT_0) throw std::runtime_error("GPU did not exit bounded wait without notification");
        std::cout << "GPU completed before worker notification in " << GetTickCount64()-start << "ms" << std::endl;
      } else if (forced) Sleep(100);
      backend->Submitted(submitQueue, 1, lists);
      ComPtr<ID3D12Fence> fence;
      ck(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
      ck(submitQueue->Signal(fence.Get(), 1));
      HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
      ck(fence->SetEventOnCompletion(1, event));
      if (WaitForSingleObject(event, 15000) != WAIT_OBJECT_0)
        throw std::runtime_error("GPU fence timeout");
      CloseHandle(event);
      unsigned char *data;
      ck(read->Map(0, nullptr, reinterpret_cast<void **>(&data)));
      size_t changed = 0, invalid = 0;
      for (UINT y = 0; y < active; ++y)
        for (UINT x = 0; x < active; ++x) {
          auto p = reinterpret_cast<uint16_t *>(
              data + y * fp.Footprint.RowPitch + x * 8);
          if (p[0] != (0x3000 + x * 12) || p[1] != (0x3400 + y * 8) ||
              p[2] != 0x3800)
            ++changed;
          for (int c = 0; c < 3; ++c)
            if ((p[c] & 0x7c00) == 0x7c00)
              ++invalid;
        }
      std::ofstream raw(
          std::filesystem::path(argv[1]) /
              (L"smoke-pass" + std::to_wstring(passes) + L".rgba16f"),
          std::ios::binary);
      for (UINT y = 0; y < active; ++y)
        raw.write(reinterpret_cast<char *>(data + y * fp.Footprint.RowPitch),
                  active * 8);
      raw.close();
      read->Unmap(0, nullptr);
      std::cout << "frame=" << iteration << " active=" << active
                << " passes=" << settings.passes << " changed_pixels=" << changed
                << " nonfinite=" << invalid
                << " elapsed_ms=" << GetTickCount64() - start
                << " status=" << backend->Status() << std::endl;
      badFrames += ((forced ? changed != 0 : changed == 0) || invalid > 0);
      if (iteration < 7) {
        ck(alloc->Reset());
        ck(cmd->Reset(alloc.Get(), nullptr));
      }
    }
    // Stop and join the recovered workers before process teardown.
    ComPtr<ID3D12InfoQueue> info;
    if (SUCCEEDED(device.As(&info))) {
      for (UINT64 i = 0;
           i < info->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i) {
        SIZE_T len = 0;
        info->GetMessage(i, nullptr, &len);
        std::vector<unsigned char> storage(len);
        auto m = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
        info->GetMessage(i, m, &len);
        if (m->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
          std::cerr << m->pDescription << std::endl;
          ++badFrames;
        }
      }
    }
    if (!backend->Shutdown())
      throw std::runtime_error("Worker shutdown was not safe");
    ExitProcess(badFrames == 0 ? 0 : 3);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    ExitProcess(1);
  }
}
