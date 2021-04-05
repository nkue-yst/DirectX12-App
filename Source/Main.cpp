// Copyright 2021 Yoshito Nakaue

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

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

void DebugOutput(const char* format, ...)
{
#ifdef _DEBUG
    va_list valist;
    va_start(valist, format);
    vprintf(format, valist);
    va_end(valist);
#endif
}

void OutputFromResult(HRESULT result, int exit_num)
{
    LPVOID str;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        result,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&str,
        0,
        NULL
    );

    if (str != NULL)
        OutputDebugString((LPCWSTR)str);

    exit(exit_num);
}


int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
    IDXGIFactory6* dxgi_factory = nullptr;
    ID3D12Device* device = nullptr;
    ID3D12CommandAllocator* cmd_alloc = nullptr;
    ID3D12GraphicsCommandList* cmd_list = nullptr;
    ID3D12CommandQueue* cmd_queue = nullptr;
    IDXGISwapChain4* swapchain = nullptr;
    HRESULT hr;

    // Create and register window
    WNDCLASSEX win = {};
    win.cbSize = sizeof(WNDCLASSEX);
    win.lpfnWndProc = (WNDPROC)WindowProcedure;
    win.lpszClassName = TEXT("DirectX12_App");
    win.hInstance = GetModuleHandle(nullptr);
    RegisterClassEx(&win);

    constexpr unsigned int win_width = 1280;
    constexpr unsigned int win_height = 720;

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

    // Initialize DirectX system
    D3D_FEATURE_LEVEL levels[] =
    {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
    if (FAILED(hr))
        OutputFromResult(hr, 1);


    // Initialize adapters
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

    // Initialize device
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
    {
        DebugOutput("Failed to initialize device.");
        exit(2);
    }

    // Create commander
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_alloc));
    if (FAILED(hr))
        OutputFromResult(hr, 3);

    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_alloc, nullptr, IID_PPV_ARGS(&cmd_list));
    if (FAILED(hr))
        OutputFromResult(hr, 4);

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {};
    cmd_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    cmd_queue_desc.NodeMask = 0;
    cmd_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmd_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&cmd_queue));
    if (FAILED(hr))
        OutputFromResult(hr, 5);

    // Swapchain settings
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc1 = {};
    swapchain_desc1.Width = win_width;
    swapchain_desc1.Height = win_height;
    swapchain_desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_desc1.Stereo = false;
    swapchain_desc1.SampleDesc.Count = 1;
    swapchain_desc1.SampleDesc.Quality = 0;
    swapchain_desc1.BufferUsage = DXGI_USAGE_BACK_BUFFER;
    swapchain_desc1.BufferCount = 2;
    swapchain_desc1.Scaling = DXGI_SCALING_STRETCH;
    swapchain_desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchain_desc1.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    hr = dxgi_factory->CreateSwapChainForHwnd(cmd_queue, hwnd, &swapchain_desc1, nullptr, nullptr, (IDXGISwapChain1**)&swapchain);
    if (FAILED(hr))
        OutputFromResult(hr, 6);

    // Create descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NodeMask = 0;
    heap_desc.NumDescriptors = 2;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ID3D12DescriptorHeap* rtv_heaps = nullptr;
    hr = device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rtv_heaps));
    if (FAILED(hr))
        OutputFromResult(hr, 7);

    // Bind descriptor to buffer
    DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
    hr = swapchain->GetDesc(&swapchain_desc);
    if (FAILED(hr))
        OutputFromResult(hr, 8);

    std::vector<ID3D12Resource*> back_buffers(swapchain_desc.BufferCount);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtv_heaps->GetCPUDescriptorHandleForHeapStart();
    for (size_t i = 0; i < swapchain_desc.BufferCount; ++i)
    {
        hr = swapchain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&back_buffers[i]));
        if (FAILED(hr))
            OutputFromResult(hr, 9);

        device->CreateRenderTargetView(back_buffers[i], nullptr, handle);
        handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create fence
    ID3D12Fence* fence = nullptr;
    UINT64 fence_val = 0;
    hr = device->CreateFence(fence_val, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr))
        OutputFromResult(hr, 10);

    // Create vertices buffer
    DirectX::XMFLOAT3 vertices[] =
    {
        {-0.4f, -0.7f,  0.0f},
        {-0.4f,  0.7f,  0.0f},
        { 0.4f, -0.7f,  0.0f},
        { 0.4f,  0.7f,  0.0f},
    };

    // Heap settings
    D3D12_HEAP_PROPERTIES heap_property = {};
    heap_property.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_property.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_property.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    // Resource settings
    D3D12_RESOURCE_DESC resource_desc = {};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Width = sizeof(vertices);
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* vert_buff = nullptr;
    hr = device->CreateCommittedResource(
        &heap_property,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vert_buff)
    );
    if (FAILED(hr))
        OutputFromResult(hr, 11);

    // Copy vertices
    DirectX::XMFLOAT3* vertices_map = nullptr;
    hr = vert_buff->Map(0, nullptr, (void**)&vertices_map);
    if (FAILED(hr))
        OutputFromResult(hr, 12);

    std::copy(std::begin(vertices), std::end(vertices), vertices_map);
    vert_buff->Unmap(0, nullptr);

    // Create vertices buffer view
    D3D12_VERTEX_BUFFER_VIEW vert_buff_view = {};
    vert_buff_view.BufferLocation = vert_buff->GetGPUVirtualAddress();
    vert_buff_view.SizeInBytes = sizeof(vertices);
    vert_buff_view.StrideInBytes = sizeof(vertices[0]);

    unsigned short indices[] = { 0, 1, 2, 2, 1, 3 };
    
    ID3D12Resource* index_buff = nullptr;
    resource_desc.Width = sizeof(indices);
    hr = device->CreateCommittedResource(
        &heap_property,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&index_buff)
    );

    // Copy index buffer data
    unsigned short* mapped_index = nullptr;
    index_buff->Map(0, nullptr, (void**)&mapped_index);
    std::copy(std::begin(indices), std::end(indices), mapped_index);
    index_buff->Unmap(0, nullptr);

    // Create index buffer view
    D3D12_INDEX_BUFFER_VIEW index_buff_view = {};
    index_buff_view.BufferLocation = index_buff->GetGPUVirtualAddress();
    index_buff_view.Format = DXGI_FORMAT_R16_UINT;
    index_buff_view.SizeInBytes = sizeof(indices);

    // Shader compile
    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* err_blob = nullptr;

    // vertex shader from file
    hr = D3DCompileFromFile(
        L"Shader/SimpleVertexShader.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "SimpleVS", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &vs_blob, &err_blob
    );
    if (FAILED(hr))
        OutputFromResult(hr, 13);

    // pixel shader from file
    hr = D3DCompileFromFile(
        L"Shader/SimplePixelShader.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "SimplePS", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &ps_blob, &err_blob
    );
    if (FAILED(hr))
        OutputFromResult(hr, 14);
    
    // Input layout
    D3D12_INPUT_ELEMENT_DESC in_layout[] = {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },
    };

    // Graphics pipeline settings
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gp_pl = {};

    gp_pl.pRootSignature = nullptr;

    gp_pl.VS.pShaderBytecode = vs_blob->GetBufferPointer();
    gp_pl.VS.BytecodeLength = vs_blob->GetBufferSize();
    gp_pl.PS.pShaderBytecode = ps_blob->GetBufferPointer();
    gp_pl.PS.BytecodeLength = ps_blob->GetBufferSize();

    gp_pl.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    gp_pl.BlendState.AlphaToCoverageEnable = false;
    gp_pl.BlendState.IndependentBlendEnable = false;

    D3D12_RENDER_TARGET_BLEND_DESC render_target_blend_desc = {};
    render_target_blend_desc.BlendEnable = false;
    render_target_blend_desc.LogicOpEnable = false;
    render_target_blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    gp_pl.BlendState.RenderTarget[0] = render_target_blend_desc;

    gp_pl.RasterizerState.MultisampleEnable = false;
    gp_pl.RasterizerState.DepthClipEnable   = true;
    gp_pl.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    gp_pl.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

    gp_pl.RasterizerState.FrontCounterClockwise = false;
    gp_pl.RasterizerState.DepthBias      = D3D12_DEFAULT_DEPTH_BIAS;
    gp_pl.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    gp_pl.RasterizerState.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    gp_pl.RasterizerState.AntialiasedLineEnable = false;
    gp_pl.RasterizerState.ForcedSampleCount  = 0;
    gp_pl.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    gp_pl.DepthStencilState.DepthEnable   = false;
    gp_pl.DepthStencilState.StencilEnable = false;

    gp_pl.InputLayout.pInputElementDescs = in_layout;
    gp_pl.InputLayout.NumElements        = _countof(in_layout);

    gp_pl.IBStripCutValue       = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    gp_pl.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // Render target settings
    gp_pl.NumRenderTargets = 1;
    gp_pl.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    // Anti aliasing settings
    gp_pl.SampleDesc.Count = 1;
    gp_pl.SampleDesc.Quality = 0;

    // Root signature settings
    ID3D12RootSignature* root_signature = nullptr;

    D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob* root_signature_blob = nullptr;

    hr = D3D12SerializeRootSignature(
        &root_signature_desc,
        D3D_ROOT_SIGNATURE_VERSION_1_0,
        &root_signature_blob,
        &err_blob
    );
    if (FAILED(hr))
        OutputFromResult(hr, 15);

    // Create root signature object
    hr = device->CreateRootSignature(
        0,
        root_signature_blob->GetBufferPointer(),
        root_signature_blob->GetBufferSize(),
        IID_PPV_ARGS(&root_signature)
    );
    root_signature_blob->Release();
    if (FAILED(hr))
        OutputFromResult(hr, 16);

    gp_pl.pRootSignature = root_signature;

    // Create graphics pipeline state object
    ID3D12PipelineState* pipeline_state = nullptr;
    hr = device->CreateGraphicsPipelineState(&gp_pl, IID_PPV_ARGS(&pipeline_state));
    if (FAILED(hr))
        OutputFromResult(hr, 17);

    // Viewport settings
    D3D12_VIEWPORT viewport = {};
    viewport.Width = win_width;
    viewport.Height = win_height;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MaxDepth = 1.0f;
    viewport.MinDepth = 0.0f;

    // Create scissor rectangle
    D3D12_RECT scissor_rect = {};
    scissor_rect.top = 0;
    scissor_rect.left = 0;
    scissor_rect.right = scissor_rect.left + win_width;
    scissor_rect.bottom = scissor_rect.top + win_height;

    // Main loop
    ShowWindow(hwnd, SW_SHOW);
    MSG msg = {};
    unsigned int frame_num = 0;

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
        barrier_desc.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier_desc.Transition.pResource   = back_buffers[bb_idx];
        barrier_desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier_desc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier_desc.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

        // Command list settings
        cmd_list->ResourceBarrier(1, &barrier_desc);
        cmd_list->SetPipelineState(pipeline_state);

        auto rtv_handle = rtv_heaps->GetCPUDescriptorHandleForHeapStart();
        rtv_handle.ptr += static_cast<ULONG_PTR>(bb_idx) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        cmd_list->OMSetRenderTargets(1, &rtv_handle, false, nullptr);

        // Clear render target
        float clear_color[] = { 0.3f, 0.3f, 0.3f, 1.0f };
        cmd_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

        ++frame_num;

        cmd_list->RSSetViewports(1, &viewport);
        cmd_list->RSSetScissorRects(1, &scissor_rect);
        cmd_list->SetGraphicsRootSignature(root_signature);

        cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd_list->IASetVertexBuffers(0, 1, &vert_buff_view);
        cmd_list->IASetIndexBuffer(&index_buff_view);

        cmd_list->DrawIndexedInstanced(6, 1, 0, 0, 0);

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
        cmd_list->Reset(cmd_alloc, pipeline_state);

        // Swap screen
        swapchain->Present(1, 0);
    }

    UnregisterClass(win.lpszClassName, win.hInstance);

    return 0;
}
