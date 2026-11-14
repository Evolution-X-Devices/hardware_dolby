/*
 * SPDX-FileCopyrightText: 2026 The Evolution X Project
 * SPDX-License-Identifier: Apache-2.0
 */

// Serves the Qualcomm Codec2 component store over HIDL as
// android.hardware.media.c2@1.2::IComponentStore/default.
//
// The 6.6 vendor blobs only run vendor.qti.media.c2@1.0-service, which
// registers the store over AIDL. The Dolby Vision decoders (c2.dolby.*.dec)
// still reach the hardware decoder through DolbyC2Client, which calls
// android.hardware.media.c2@1.0::IComponentStore::getService("default").
// This service gives them that HIDL entry point without touching the AIDL
// service the framework uses.
//
// The store is created the same way vendor.qti.media.c2@1.0-service does it:
// libqcodec2_core.so exports QC2ComponentStoreFactoryGetter(1, 0), which returns
// a factory object, and the factory's first virtual method (vtable slot 2
// after the two destructors) returns std::shared_ptr<C2ComponentStore> through
// the return slot. The layout was recovered from the stock service's call
// site and from the getter itself, which returns nullptr unless it is called
// with (1, 0). It is not a published interface, so it has to be checked again
// whenever libqcodec2_core.so is updated.

#define LOG_TAG "android.hardware.media.c2-qc2hidl-service"

#include <android-base/logging.h>
#include <codec2/hidl/1.2/ComponentStore.h>
#include <hidl/HidlTransportSupport.h>

#include <C2Component.h>

#include <dlfcn.h>
#include <signal.h>

#include <memory>

namespace {

constexpr char kCoreLibrary[] = "libqcodec2_core.so";
constexpr char kFactoryGetterSymbol[] = "QC2ComponentStoreFactoryGetter";
constexpr char kInstanceName[] = "default";

// Only the vtable layout matters here: [0] and [1] are the destructors, [2] is
// the method the stock service calls to get the store. The object is never
// deleted, like in the stock service.
class QC2ComponentStoreFactory {
  public:
    virtual ~QC2ComponentStoreFactory() = default;
    virtual std::shared_ptr<C2ComponentStore> getStore() = 0;
};

using FactoryGetterFn = QC2ComponentStoreFactory* (*)(int version, int flags);

constexpr int kFactoryVersion = 1;
constexpr int kFactoryFlags = 0;
constexpr int kRpcThreadCount = 8;

}  // anonymous namespace

int main() {
    signal(SIGPIPE, SIG_IGN);

    void* handle = dlopen(kCoreLibrary, RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        LOG(ERROR) << "Cannot load " << kCoreLibrary << ": " << dlerror();
        return 1;
    }

    auto getFactory = reinterpret_cast<FactoryGetterFn>(dlsym(handle, kFactoryGetterSymbol));
    if (getFactory == nullptr) {
        LOG(ERROR) << kCoreLibrary << " has no " << kFactoryGetterSymbol << ": " << dlerror();
        return 1;
    }

    QC2ComponentStoreFactory* factory = getFactory(kFactoryVersion, kFactoryFlags);
    if (factory == nullptr) {
        LOG(ERROR) << kFactoryGetterSymbol << " rejected version " << kFactoryVersion
                   << ", the factory interface changed";
        return 1;
    }

    std::shared_ptr<C2ComponentStore> c2Store = factory->getStore();
    if (c2Store == nullptr) {
        LOG(ERROR) << "Cannot create the Qualcomm Codec2 store";
        return 1;
    }

    // The decoders' listener callbacks arrive over hwbinder.
    android::hardware::configureRpcThreadpool(kRpcThreadCount, true /* callerWillJoin */);

    using namespace ::android::hardware::media::c2::V1_2;
    ::android::sp<IComponentStore> store = new utils::ComponentStore(c2Store);
    if (store->registerAsService(kInstanceName) != ::android::OK) {
        LOG(ERROR) << "Cannot register HIDL IComponentStore@1.2/" << kInstanceName;
        return 1;
    }
    LOG(INFO) << "Registered HIDL IComponentStore@1.2/" << kInstanceName << " from "
              << kCoreLibrary;

    android::hardware::joinRpcThreadpool();
    return 1;
}
