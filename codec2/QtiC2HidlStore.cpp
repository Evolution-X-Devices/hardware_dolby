/*
 * SPDX-FileCopyrightText: acerhizm
 * SPDX-License-Identifier: Apache-2.0
 */

// HIDL IComponentStore/default for the Dolby Vision codecs. c2.dolby.client.so
// (DolbyC2Client::CreateFromService) reaches the QTI video codecs through
// android.hardware.media.c2@1.0::IComponentStore::getService("default"), but the
// 6.6 vendor.qti.media.c2@1.0-service only registers the AIDL store. This service
// loads the same QTI store and serves it over HIDL. The framework never lists it:
// with media.c2.hal.selection=aidl Codec2Client only enumerates AIDL stores.

#define LOG_TAG "dolbyvision-qti-c2-hidl"

#include <dlfcn.h>
#include <signal.h>

#include <android-base/logging.h>
#include <binder/ProcessState.h>
#include <codec2/hidl/1.2/ComponentStore.h>
#include <hidl/HidlTransportSupport.h>
#include <minijail.h>

// Same policy files vendor.qti.media.c2@1.0-service applies to this store.
static constexpr char kBaseSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/codec2.vendor.base-arm64.policy";
static constexpr char kExtSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/codec2.vendor.ext-arm64.policy";

static constexpr int kThreadCount = 8;

// libqcodec2_core.so interface, as vendor.qti.media.c2@1.0-service uses it:
// QC2ComponentStoreFactoryGetter(1, 0), then vtable slot 2 (after the two
// destructor entries) returns the store by value.
class QC2ComponentStoreFactory {
  public:
    virtual ~QC2ComponentStoreFactory() = default;
    virtual std::shared_ptr<C2ComponentStore> getInstance() = 0;
};
using QC2ComponentStoreFactoryGetter_t = QC2ComponentStoreFactory* (*)(int majorVersion,
                                                                        int minorVersion);

int main() {
    using namespace ::android::hardware::media::c2::V1_2;

    signal(SIGPIPE, SIG_IGN);
    android::SetUpMinijail(kBaseSeccompPolicyPath, kExtSeccompPolicyPath);

    android::ProcessState::initWithDriver("/dev/vndbinder");
    android::ProcessState::self()->startThreadPool();
    android::hardware::configureRpcThreadpool(kThreadCount, true /* callerWillJoin */);

    void* lib = dlopen("libqcodec2_core.so", RTLD_NOW | RTLD_NODELETE);
    CHECK(lib) << "dlopen libqcodec2_core.so: " << dlerror();
    auto getter = reinterpret_cast<QC2ComponentStoreFactoryGetter_t>(
            dlsym(lib, "QC2ComponentStoreFactoryGetter"));
    CHECK(getter) << "dlsym QC2ComponentStoreFactoryGetter: " << dlerror();
    QC2ComponentStoreFactory* factory = getter(1, 0);
    CHECK(factory) << "No QTI Codec2 store factory.";
    std::shared_ptr<C2ComponentStore> c2Store = factory->getInstance();
    CHECK(c2Store) << "No QTI Codec2 store.";

    android::sp<IComponentStore> store = new utils::ComponentStore(c2Store);
    CHECK_EQ(store->registerAsService("default"), android::OK)
            << "Cannot register IComponentStore/default";
    LOG(INFO) << "QTI Codec2 store registered as HIDL IComponentStore/default";

    android::hardware::joinRpcThreadpool();
    return EXIT_FAILURE;
}
