from pathlib import Path
p=Path('analysis/smoke.cpp');s=p.read_text().replace('#include <dxgi1_6.h>','#include <dxgi1_6.h>\n#include <d3d12sdklayers.h>')
s=s.replace(' ComPtr<IDXGIFactory6> factory;', ' ComPtr<ID3D12Debug> debug; if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();\n ComPtr<IDXGIFactory6> factory;',1)
s=s.replace(' frame.reset=(iteration==3);',' if(iteration==2 && active>=128){active-=32;frame.width=active;frame.height=active;}\n frame.reset=(iteration==3);')
s=s.replace(' if(!backend->Shutdown())',' ComPtr<ID3D12InfoQueue> info;if(SUCCEEDED(device.As(&info))){for(UINT64 i=0;i<info->GetNumStoredMessagesAllowedByRetrievalFilter();++i){SIZE_T len=0;info->GetMessage(i,nullptr,&len);std::vector<unsigned char> storage(len);auto m=reinterpret_cast<D3D12_MESSAGE*>(storage.data());info->GetMessage(i,m,&len);if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<m->pDescription<<std::endl;++badFrames;}}}\n if(!backend->Shutdown())')
p.write_text(s)
