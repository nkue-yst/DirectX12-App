#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <string>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void EnableDebugLayer()
{
    ID3D12Debug* debug_layer = nullptr;
    D3D12GetDebugInterface(IID_PPV_ARGS(&debug_layer));

    debug_layer->EnableDebugLayer();
    debug_layer->Release();
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
    // Create window
    WNDCLASSEX win = {};
    win.cbSize = sizeof(WNDCLASSEX);
    win.lpfnWndProc   = (WNDPROC)WindowProcedure;
    win.lpszClassName = TEXT("DirectX12_App");
    win.hInstance = GetModuleHandle(nullptr);

    RegisterClassEx(&win);

    constexpr int win_width = 1280;
    constexpr int win_height = 720;
    RECT wrc = { 0, 0, win_width, win_height };

    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    HWND hwnd = CreateWindow(win.lpszClassName,
        TEXT("DirectX12_App"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        nullptr,
        nullptr,
        win.hInstance,
        nullptr
    );

#ifdef _DEBUG
    EnableDebugLayer();
#endif

    ID3D12Device* device = nullptr;
    D3D_FEATURE_LEVEL levels[] =
    {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D_FEATURE_LEVEL feature_level;
    for (auto level : levels)
    {
        if (D3D12CreateDevice(nullptr, level, IID_PPV_ARGS(&device)) == S_OK)
        {
            feature_level = level;
            break;
        }
    }
    if (device == nullptr)
        exit(1);

    IDXGIFactory6* dxgi_factory = nullptr;
    if (CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgi_factory)) != S_OK)
        exit(2);

    std::vector <IDXGIAdapter*> adapters;
    IDXGIAdapter* tmp_adapter = nullptr;

    for (int i = 0; dxgi_factory->EnumAdapters(i, &tmp_adapter) != DXGI_ERROR_NOT_FOUND; ++i)
        adapters.push_back(tmp_adapter);

    for (auto adapter : adapters)
    {
        DXGI_ADAPTER_DESC adapter_desc = {};
        adapter->GetDesc(&adapter_desc);

        std::wstring str_desc = adapter_desc.Description;

        if (str_desc.find(L"NVIDIA") != std::wstring::npos)
        {
            tmp_adapter = adapter;
            break;
        }
    }

    ID3D12CommandAllocator* cmd_alloc   = nullptr;
    if (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_alloc)) != S_OK)
        exit(3);

    ID3D12GraphicsCommandList* cmd_list = nullptr;
    if (device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_alloc, nullptr, IID_PPV_ARGS(&cmd_list)) != S_OK)
        exit(4);

    ID3D12CommandQueue* cmd_queue = nullptr;
    D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {};
    cmd_queue_desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    cmd_queue_desc.NodeMask = 0;
    cmd_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmd_queue_desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;

    // Create queue
    if (device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&cmd_queue)) != S_OK)
        exit(5);

    // Swapchain settings
    IDXGISwapChain4* swapchain = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
    swapchain_desc.Width  = win_width;
    swapchain_desc.Height = win_height;
    swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_desc.Stereo = false;
    swapchain_desc.SampleDesc.Count   = 1;
    swapchain_desc.SampleDesc.Quality = 0;
    swapchain_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
    swapchain_desc.BufferCount = 2;
    swapchain_desc.Scaling     = DXGI_SCALING_STRETCH;
    swapchain_desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchain_desc.Flags       = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    if (dxgi_factory->CreateSwapChainForHwnd(cmd_queue, hwnd, &swapchain_desc, nullptr, nullptr, (IDXGISwapChain1**)&swapchain) != S_OK)
        exit(6);

    // Create descriptor head
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NodeMask = 0;
    heap_desc.NumDescriptors = 2;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ID3D12DescriptorHeap* rtv_heaps = nullptr;
    if (device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rtv_heaps)) != S_OK)
        exit(7);


    // Bind descriptor to buffer
    DXGI_SWAP_CHAIN_DESC swc_desc = {};
    if (swapchain->GetDesc(&swc_desc) != S_OK)
        exit(8);

    std::vector<ID3D12Resource*> back_buffers(swc_desc.BufferCount);
    for (size_t i = 0; i < swc_desc.BufferCount; ++i)
    {
        if (swapchain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&back_buffers[i])) != S_OK)
            exit(9);

        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtv_heaps->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += i * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        device->CreateRenderTargetView(back_buffers[i], nullptr, handle);
    }

    // Create fence
    ID3D12Fence* fence = nullptr;
    UINT64 fence_val = 0;
    
    if (device->CreateFence(fence_val, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)) != S_OK)
        exit(10);

    // Main loop
    ShowWindow(hwnd, SW_SHOW);
    MSG msg = {};
    while (true)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (msg.message == WM_QUIT)
            break;

        // Setting render target
        auto bb_idx = swapchain->GetCurrentBackBufferIndex();

        D3D12_RESOURCE_BARRIER barrier_desc = {};
        barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier_desc.Transition.pResource = back_buffers[bb_idx];
        barrier_desc.Transition.Subresource = 0;
        barrier_desc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier_desc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        cmd_list->ResourceBarrier(1, &barrier_desc);

        auto rtv_handle = rtv_heaps->GetCPUDescriptorHandleForHeapStart();
        rtv_handle.ptr += static_cast<ULONG_PTR>(bb_idx) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        cmd_list->OMSetRenderTargets(1, &rtv_handle, true, nullptr);

        // Clear render target
        float clear_color[] = { 0.f, 1.f, 0.f, 1.f };
        cmd_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

        barrier_desc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier_desc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        cmd_list->ResourceBarrier(1, &barrier_desc);

        // Execute commands
        cmd_list->Close();

        ID3D12CommandList* cmd_lists[] = { cmd_list };
        cmd_queue->ExecuteCommandLists(1, cmd_lists);

        cmd_queue->Signal(fence, ++fence_val);

        if (fence->GetCompletedValue() != fence_val)
        {
            auto ev = CreateEvent(nullptr, false, false, nullptr);
            fence->SetEventOnCompletion(fence_val, ev);

            WaitForSingleObject(ev, INFINITE);
            CloseHandle(ev);
        }

        cmd_alloc->Reset();
        cmd_list->Reset(cmd_alloc, nullptr);

        // Swap screen
        swapchain->Present(1, 0);
    }

    UnregisterClass(win.lpszClassName, win.hInstance);

    return 0;
}
