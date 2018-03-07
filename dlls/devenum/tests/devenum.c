/*
 * Some unit tests for devenum
 *
 * Copyright (C) 2012 Christian Costa
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include <stdio.h>

#include "wine/test.h"
#include "initguid.h"
#include "ole2.h"
#include "strmif.h"
#include "uuids.h"

static const WCHAR friendly_name[] = {'F','r','i','e','n','d','l','y','N','a','m','e',0};
static const WCHAR fcc_handlerW[] = {'F','c','c','H','a','n','d','l','e','r',0};
static const WCHAR clsidW[] = {'C','L','S','I','D',0};
static const WCHAR mrleW[] = {'m','r','l','e',0};

static void test_devenum(IBindCtx *bind_ctx)
{
    IEnumMoniker *enum_cat, *enum_moniker;
    ICreateDevEnum* create_devenum;
    IPropertyBag *prop_bag;
    IMoniker *moniker;
    BOOL have_mrle = FALSE;
    GUID cat_guid, clsid;
    VARIANT var;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
                           &IID_ICreateDevEnum, (LPVOID*)&create_devenum);
    ok(hr == S_OK, "Failed to create devenum: %#x\n", hr);

    hr = ICreateDevEnum_CreateClassEnumerator(create_devenum, &CLSID_ActiveMovieCategories, &enum_cat, 0);
    ok(hr == S_OK, "Failed to enum categories: %#x\n", hr);

    while (IEnumMoniker_Next(enum_cat, 1, &moniker, NULL) == S_OK)
    {
        hr = IMoniker_BindToStorage(moniker, bind_ctx, NULL, &IID_IPropertyBag, (void **)&prop_bag);
        ok(hr == S_OK, "IMoniker_BindToStorage failed: %#x\n", hr);

        VariantInit(&var);
        hr = IPropertyBag_Read(prop_bag, friendly_name, &var, NULL);
        ok(hr == S_OK, "Failed to read FriendlyName: %#x\n", hr);

        if (winetest_debug > 1)
            trace("%s:\n", wine_dbgstr_w(V_BSTR(&var)));

        VariantClear(&var);
        hr = IPropertyBag_Read(prop_bag, clsidW, &var, NULL);
        ok(hr == S_OK, "Failed to read CLSID: %#x\n", hr);

        hr = CLSIDFromString(V_BSTR(&var), &cat_guid);
        ok(hr == S_OK, "got %#x\n", hr);

        IPropertyBag_Release(prop_bag);
        IMoniker_Release(moniker);

        hr = ICreateDevEnum_CreateClassEnumerator(create_devenum, &cat_guid, &enum_moniker, 0);
        ok(SUCCEEDED(hr), "Failed to enum devices: %#x\n", hr);

        if (hr == S_OK)
        {
            while (IEnumMoniker_Next(enum_moniker, 1, &moniker, NULL) == S_OK)
            {
                hr = IMoniker_GetClassID(moniker, NULL);
                ok(hr == E_INVALIDARG, "IMoniker_GetClassID should failed %x\n", hr);

                hr = IMoniker_GetClassID(moniker, &clsid);
                ok(hr == S_OK, "IMoniker_GetClassID failed with error %x\n", hr);
                ok(IsEqualGUID(&clsid, &CLSID_CDeviceMoniker),
                   "Expected CLSID_CDeviceMoniker got %s\n", wine_dbgstr_guid(&clsid));

                VariantInit(&var);
                hr = IMoniker_BindToStorage(moniker, bind_ctx, NULL, &IID_IPropertyBag, (LPVOID*)&prop_bag);
                ok(hr == S_OK, "IMoniker_BindToStorage failed with error %x\n", hr);

                hr = IPropertyBag_Read(prop_bag, friendly_name, &var, NULL);
                ok(hr == S_OK, "IPropertyBag_Read failed: %#x\n", hr);

                if (winetest_debug > 1)
                    trace("  %s\n", wine_dbgstr_w(V_BSTR(&var)));

                if (IsEqualGUID(&CLSID_VideoCompressorCategory, &cat_guid)) {
                    /* Test well known compressor to ensure that we really enumerate codecs */
                    hr = IPropertyBag_Read(prop_bag, fcc_handlerW, &var, NULL);
                    if (SUCCEEDED(hr)) {
                        ok(V_VT(&var) == VT_BSTR, "V_VT(var) = %d\n", V_VT(&var));
                        if(!lstrcmpW(V_BSTR(&var), mrleW))
                            have_mrle = TRUE;
                        VariantClear(&var);
                    }
                }

                IPropertyBag_Release(prop_bag);
                IMoniker_Release(moniker);
            }
            IEnumMoniker_Release(enum_moniker);
        }
    }

    ICreateDevEnum_Release(create_devenum);

    /* 64-bit windows are missing mrle codec */
    if(sizeof(void*) == 4)
        ok(have_mrle, "mrle codec not found\n");
}
static void test_moniker_isequal(void)
{
    HRESULT res;
    ICreateDevEnum *create_devenum = NULL;
    IEnumMoniker *enum_moniker0 = NULL, *enum_moniker1 = NULL;
    IMoniker *moniker0 = NULL, *moniker1 = NULL;

    res = CoCreateInstance(&CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
                           &IID_ICreateDevEnum, (LPVOID*)&create_devenum);
    if (FAILED(res))
    {
         skip("Cannot create SystemDeviceEnum object (%x)\n", res);
         return;
    }

    res = ICreateDevEnum_CreateClassEnumerator(create_devenum, &CLSID_LegacyAmFilterCategory, &enum_moniker0, 0);
    ok(SUCCEEDED(res), "Cannot create enum moniker (res = %x)\n", res);
    if (SUCCEEDED(res))
    {
        if (IEnumMoniker_Next(enum_moniker0, 1, &moniker0, NULL) == S_OK &&
            IEnumMoniker_Next(enum_moniker0, 1, &moniker1, NULL) == S_OK)
        {
            res = IMoniker_IsEqual(moniker0, moniker1);
            ok(res == S_FALSE, "IMoniker_IsEqual should fail (res = %x)\n", res);

            res = IMoniker_IsEqual(moniker1, moniker0);
            ok(res == S_FALSE, "IMoniker_IsEqual should fail (res = %x)\n", res);

            IMoniker_Release(moniker0);
            IMoniker_Release(moniker1);
        }
        else
            skip("Cannot get moniker for testing.\n");
    }
    IEnumMoniker_Release(enum_moniker0);

    res = ICreateDevEnum_CreateClassEnumerator(create_devenum, &CLSID_LegacyAmFilterCategory, &enum_moniker0, 0);
    ok(SUCCEEDED(res), "Cannot create enum moniker (res = %x)\n", res);
    res = ICreateDevEnum_CreateClassEnumerator(create_devenum, &CLSID_AudioRendererCategory, &enum_moniker1, 0);
    ok(SUCCEEDED(res), "Cannot create enum moniker (res = %x)\n", res);
    if (SUCCEEDED(res))
    {
        if (IEnumMoniker_Next(enum_moniker0, 1, &moniker0, NULL) == S_OK &&
            IEnumMoniker_Next(enum_moniker1, 1, &moniker1, NULL) == S_OK)
        {
            res = IMoniker_IsEqual(moniker0, moniker1);
            ok(res == S_FALSE, "IMoniker_IsEqual should failed (res = %x)\n", res);

            res = IMoniker_IsEqual(moniker1, moniker0);
            ok(res == S_FALSE, "IMoniker_IsEqual should failed (res = %x)\n", res);

            IMoniker_Release(moniker0);
            IMoniker_Release(moniker1);
        }
        else
            skip("Cannot get moniker for testing.\n");
    }
    IEnumMoniker_Release(enum_moniker0);
    IEnumMoniker_Release(enum_moniker1);

    res = ICreateDevEnum_CreateClassEnumerator(create_devenum, &CLSID_LegacyAmFilterCategory, &enum_moniker0, 0);
    ok(SUCCEEDED(res), "Cannot create enum moniker (res = %x)\n", res);
    res = ICreateDevEnum_CreateClassEnumerator(create_devenum, &CLSID_LegacyAmFilterCategory, &enum_moniker1, 0);
    ok(SUCCEEDED(res), "Cannot create enum moniker (res = %x)\n", res);
    if (SUCCEEDED(res))
    {
        if (IEnumMoniker_Next(enum_moniker0, 1, &moniker0, NULL) == S_OK &&
            IEnumMoniker_Next(enum_moniker1, 1, &moniker1, NULL) == S_OK)
        {
            res = IMoniker_IsEqual(moniker0, moniker1);
            ok(res == S_OK, "IMoniker_IsEqual failed (res = %x)\n", res);

            res = IMoniker_IsEqual(moniker1, moniker0);
            ok(res == S_OK, "IMoniker_IsEqual failed (res = %x)\n", res);

            IMoniker_Release(moniker0);
            IMoniker_Release(moniker1);
        }
        else
            skip("Cannot get moniker for testing.\n");
    }
    IEnumMoniker_Release(enum_moniker0);
    IEnumMoniker_Release(enum_moniker1);

    ICreateDevEnum_Release(create_devenum);

    return;
}

static BOOL find_moniker(const GUID *class, IMoniker *needle)
{
    ICreateDevEnum *devenum;
    IEnumMoniker *enum_mon;
    IMoniker *mon;
    BOOL found = FALSE;

    CoCreateInstance(&CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, &IID_ICreateDevEnum, (void **)&devenum);
    ICreateDevEnum_CreateClassEnumerator(devenum, class, &enum_mon, 0);
    while (!found && IEnumMoniker_Next(enum_mon, 1, &mon, NULL) == S_OK)
    {
        if (IMoniker_IsEqual(mon, needle) == S_OK)
            found = TRUE;

        IMoniker_Release(mon);
    }

    IEnumMoniker_Release(enum_mon);
    ICreateDevEnum_Release(devenum);
    return found;
}

DEFINE_GUID(CLSID_TestFilter,  0xdeadbeef,0xcf51,0x43e6,0xb6,0xc5,0x29,0x9e,0xa8,0xb6,0xb5,0x91);

static void test_register_filter(void)
{
    static const WCHAR name[] = {'d','e','v','e','n','u','m',' ','t','e','s','t',0};
    IFilterMapper2 *mapper2;
    IMoniker *mon = NULL;
    REGFILTER2 rgf2 = {0};
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC, &IID_IFilterMapper2, (void **)&mapper2);
    ok(hr == S_OK, "Failed to create FilterMapper2: %#x\n", hr);

    rgf2.dwVersion = 2;
    rgf2.dwMerit = MERIT_UNLIKELY;
    S2(U(rgf2)).cPins2 = 0;

    hr = IFilterMapper2_RegisterFilter(mapper2, &CLSID_TestFilter, name, &mon, NULL, NULL, &rgf2);
    if (hr == E_ACCESSDENIED)
    {
        skip("Not enough permissions to register filters\n");
        IFilterMapper2_Release(mapper2);
        return;
    }
    ok(hr == S_OK, "RegisterFilter failed: %#x\n", hr);

    ok(find_moniker(&CLSID_LegacyAmFilterCategory, mon), "filter should be registered\n");

    hr = IFilterMapper2_UnregisterFilter(mapper2, NULL, NULL, &CLSID_TestFilter);
    ok(hr == S_OK, "UnregisterFilter failed: %#x\n", hr);

    ok(!find_moniker(&CLSID_LegacyAmFilterCategory, mon), "filter should not be registered\n");
    IMoniker_Release(mon);

    mon = NULL;
    hr = IFilterMapper2_RegisterFilter(mapper2, &CLSID_TestFilter, name, &mon, &CLSID_AudioRendererCategory, NULL, &rgf2);
    ok(hr == S_OK, "RegisterFilter failed: %#x\n", hr);

    ok(find_moniker(&CLSID_AudioRendererCategory, mon), "filter should be registered\n");

    hr = IFilterMapper2_UnregisterFilter(mapper2, &CLSID_AudioRendererCategory, NULL, &CLSID_TestFilter);
todo_wine
    ok(hr == S_OK, "UnregisterFilter failed: %#x\n", hr);

todo_wine
    ok(!find_moniker(&CLSID_AudioRendererCategory, mon), "filter should not be registered\n");
    IMoniker_Release(mon);

    IFilterMapper2_Release(mapper2);
}

START_TEST(devenum)
{
    IBindCtx *bind_ctx = NULL;
    HRESULT hr;

    CoInitialize(NULL);

    test_devenum(NULL);

    /* IBindCtx is allowed in IMoniker_BindToStorage (IMediaCatMoniker_BindToStorage) */
    hr = CreateBindCtx(0, &bind_ctx);
    ok(hr == S_OK, "Cannot create BindCtx: (res = 0x%x)\n", hr);
    if (bind_ctx) {
        test_devenum(bind_ctx);
        IBindCtx_Release(bind_ctx);
    }

    test_moniker_isequal();
    test_register_filter();

    CoUninitialize();
}
