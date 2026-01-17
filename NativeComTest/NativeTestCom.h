#pragma once
#include <Windows.h>
#include <unknwn.h>
#include <atomic>

// {94652869-D6DC-4D27-A3C6-2298F527AC4D}
static const CLSID CLSID_NativeTestCom =
{ 0x94652869, 0xd6dc, 0x4d27, { 0xa3, 0xc6, 0x22, 0x98, 0xf5, 0x27, 0xac, 0x4d } };

// {BB722A82-E4CE-4C01-8C73-751BCCE3A002}
static const IID IID_INativeTestCom =
{ 0xbb722a82, 0xe4ce, 0x4c01, { 0x8c, 0x73, 0x75, 0x1b, 0xcc, 0xe3, 0xa0, 0x02 } };

struct INativeTestCom : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Ping() = 0;
};

class NativeTestCom : public INativeTestCom {
public:
    NativeTestCom();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE Ping() override;

private:
    std::atomic<ULONG> m_refCount;
};

class NativeTestComFactory : public IClassFactory {
public:
    NativeTestComFactory();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override;
    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock) override;

private:
    std::atomic<ULONG> m_refCount;
};

HRESULT RegisterNativeTestCom();
HRESULT UnregisterNativeTestCom();
