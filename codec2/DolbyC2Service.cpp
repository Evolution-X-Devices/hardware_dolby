/*
 * SPDX-FileCopyrightText: acerhizm
 * SPDX-License-Identifier: Apache-2.0
 */

// AIDL Codec2 service around a Dolby C2ComponentStore blob, after
// frameworks/av/media/codec2/hal/services/vendor.cpp. Codec2Client lists either
// the AIDL or the HIDL stores, never both, and the 6.6 QTI store is AIDL only
// (media.c2.hal.selection=aidl), so the HIDL Dolby services would stay unused.

#define LOG_TAG "dolby-c2-service"

#include <signal.h>
#include <string.h>

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <binder/ProcessState.h>
#include <C2BufferPriv.h>
#include <C2Component.h>
#include <C2PlatformSupport.h>
#include <codec2/aidl/ComponentStore.h>
#include <hidl/HidlTransportSupport.h>

namespace android {
// Exported by libcodec2_store_dolby.so (audio) and c2.dolby.store.so (Dolby Vision).
std::shared_ptr<C2ComponentStore> GetCodec2DolbyComponentStore();
}  // namespace android


namespace {

// c2.dolby.client.so hands each C2Work to the QTI store through the HIDL objcpy(),
// which only accepts blocks backed by a HIDL BufferPoolData. With the AIDL HAL
// selected, the framework's input blocks arrive here backed by AIDL (bufferpool2)
// data, so objcpy() fails with "BufferPoolData unavailable in a block" and the
// Dolby component rejects every input ("queue -- bad input").
//
// Copy each linear input buffer into a block of a HIDL-versioned pool before the
// Dolby component sees it. Inputs are compressed bitstream, so the copy is cheap.
//
// The copies must not travel back to the framework: when the work is done, the AIDL
// listener converts the returned input buffers again, and a HIDL pool block has no
// AIDL BufferPoolData ("BufferPoolData unavailable in a block", "Invalid
// C2Work::input", "received corrupted work items"). So the original buffers are
// kept by frame index and put back into the work item before the AIDL listener
// gets it, which also keeps the framework's input slots held until the work is done.

// The original input buffers of the works queued with a copy, by frame index.
class InputRegistry {
  public:
    void stash(uint64_t frameIndex, std::vector<std::shared_ptr<C2Buffer>> originals) {
        std::lock_guard<std::mutex> lock(mMutex);
        mInputs[frameIndex] = std::move(originals);
    }

    void drop(uint64_t frameIndex) {
        std::vector<std::shared_ptr<C2Buffer>> released;
        std::lock_guard<std::mutex> lock(mMutex);
        auto it = mInputs.find(frameIndex);
        if (it != mInputs.end()) {
            released = std::move(it->second);
            mInputs.erase(it);
        }
    }

    void clear() {
        std::map<uint64_t, std::vector<std::shared_ptr<C2Buffer>>> released;
        std::lock_guard<std::mutex> lock(mMutex);
        released.swap(mInputs);
    }

    // Replaces the copied inputs of returned (or flushed) works with the originals.
    void restore(std::list<std::unique_ptr<C2Work>>* items) {
        // The copies are released after the lock is dropped.
        std::vector<std::vector<std::shared_ptr<C2Buffer>>> copies;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (const std::unique_ptr<C2Work>& work : *items) {
                if (!work) {
                    continue;
                }
                auto it = mInputs.find(work->input.ordinal.frameIndex.peekull());
                if (it == mInputs.end()) {
                    continue;
                }
                copies.push_back(std::move(work->input.buffers));
                work->input.buffers = std::move(it->second);
                mInputs.erase(it);
            }
        }
    }

  private:
    std::mutex mMutex;
    std::map<uint64_t, std::vector<std::shared_ptr<C2Buffer>>> mInputs;
};

// Gives the original inputs back before the framework's listener converts the work.
class RestoreInputsListener : public C2Component::Listener {
  public:
    RestoreInputsListener(std::shared_ptr<C2Component::Listener> base,
                          std::shared_ptr<InputRegistry> registry)
        : mBase(std::move(base)), mRegistry(std::move(registry)) {}

    void onWorkDone_nb(std::weak_ptr<C2Component> component,
                       std::list<std::unique_ptr<C2Work>> workItems) override {
        mRegistry->restore(&workItems);
        mBase->onWorkDone_nb(std::move(component), std::move(workItems));
    }

    void onTripped_nb(std::weak_ptr<C2Component> component,
                      std::vector<std::shared_ptr<C2SettingResult>> settingResult) override {
        mBase->onTripped_nb(std::move(component), std::move(settingResult));
    }

    void onError_nb(std::weak_ptr<C2Component> component, uint32_t errorCode) override {
        mBase->onError_nb(std::move(component), errorCode);
    }

  private:
    const std::shared_ptr<C2Component::Listener> mBase;
    const std::shared_ptr<InputRegistry> mRegistry;
};

class HidlPoolComponent : public C2Component {
  public:
    HidlPoolComponent(std::shared_ptr<C2Component> base, std::shared_ptr<C2BlockPool> pool)
        : mBase(std::move(base)), mPool(std::move(pool)),
          mRegistry(std::make_shared<InputRegistry>()) {}

    c2_status_t setListener_vb(const std::shared_ptr<Listener>& listener,
                               c2_blocking_t mayBlock) override {
        std::shared_ptr<Listener> wrapped;
        if (listener) {
            wrapped = std::make_shared<RestoreInputsListener>(listener, mRegistry);
        }
        return mBase->setListener_vb(wrapped, mayBlock);
    }

    c2_status_t queue_nb(std::list<std::unique_ptr<C2Work>>* const items) override {
        std::vector<uint64_t> stashed;
        if (items != nullptr) {
            for (const std::unique_ptr<C2Work>& work : *items) {
                if (work && copyInputs(&work->input)) {
                    stashed.push_back(work->input.ordinal.frameIndex.peekull());
                }
            }
        }
        c2_status_t status = mBase->queue_nb(items);
        if (status != C2_OK) {
            for (uint64_t frameIndex : stashed) {
                mRegistry->drop(frameIndex);
            }
        }
        return status;
    }

    c2_status_t announce_nb(const std::vector<C2WorkOutline>& items) override {
        return mBase->announce_nb(items);
    }

    c2_status_t flush_sm(flush_mode_t mode,
                         std::list<std::unique_ptr<C2Work>>* const flushedWork) override {
        c2_status_t status = mBase->flush_sm(mode, flushedWork);
        if (flushedWork != nullptr) {
            mRegistry->restore(flushedWork);
        }
        return status;
    }

    c2_status_t drain_nb(drain_mode_t mode) override { return mBase->drain_nb(mode); }
    c2_status_t start() override { return mBase->start(); }

    c2_status_t stop() override {
        c2_status_t status = mBase->stop();
        mRegistry->clear();
        return status;
    }

    c2_status_t reset() override {
        c2_status_t status = mBase->reset();
        mRegistry->clear();
        return status;
    }

    c2_status_t release() override {
        c2_status_t status = mBase->release();
        mRegistry->clear();
        return status;
    }

    std::shared_ptr<C2ComponentInterface> intf() override { return mBase->intf(); }

  private:
    // Returns true if an input buffer was replaced by a copy, after stashing the originals.
    bool copyInputs(C2FrameData* input) {
        const std::vector<std::shared_ptr<C2Buffer>> originals = input->buffers;
        bool replaced = false;
        for (std::shared_ptr<C2Buffer>& buffer : input->buffers) {
            if (!buffer || buffer->data().type() != C2BufferData::LINEAR) {
                continue;
            }
            const std::vector<C2ConstLinearBlock> blocks = buffer->data().linearBlocks();
            if (blocks.size() != 1 || blocks[0].size() == 0) {
                continue;
            }
            const C2ConstLinearBlock& src = blocks[0];
            const uint32_t size = src.size();

            C2Acquirable<C2ReadView> readable = src.map();
            C2ReadView readView = readable.get();
            if (readView.error() != C2_OK) {
                LOG(ERROR) << "Cannot map the input block: " << readView.error();
                continue;
            }

            std::shared_ptr<C2LinearBlock> dst;
            c2_status_t status = mPool->fetchLinearBlock(
                    size, C2MemoryUsage(C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE),
                    &dst);
            if (status != C2_OK || !dst) {
                LOG(ERROR) << "Cannot fetch a " << size << " byte input block: " << status;
                continue;
            }
            C2Acquirable<C2WriteView> writable = dst->map();
            C2WriteView writeView = writable.get();
            if (writeView.error() != C2_OK || writeView.capacity() < size) {
                LOG(ERROR) << "Cannot map the copied input block: " << writeView.error();
                continue;
            }
            memcpy(writeView.base(), readView.data(), size);

            std::shared_ptr<C2Buffer> copy =
                    C2Buffer::CreateLinearBuffer(dst->share(0, size, C2Fence()));
            if (!copy) {
                continue;
            }
            for (const std::shared_ptr<const C2Info>& info : buffer->info()) {
                copy->setInfo(std::const_pointer_cast<C2Info>(info));
            }
            buffer = std::move(copy);
            replaced = true;
        }
        if (replaced) {
            mRegistry->stash(input->ordinal.frameIndex.peekull(), originals);
        }
        return replaced;
    }

    const std::shared_ptr<C2Component> mBase;
    const std::shared_ptr<C2BlockPool> mPool;
    const std::shared_ptr<InputRegistry> mRegistry;
};

// Wraps every component the Dolby store creates in a HidlPoolComponent.
class HidlPoolComponentStore : public C2ComponentStore {
  public:
    static std::shared_ptr<C2ComponentStore> wrap(std::shared_ptr<C2ComponentStore> base) {
        if (!base) {
            return nullptr;
        }
        std::shared_ptr<C2Allocator> allocator;
        c2_status_t status = android::GetCodec2PlatformAllocatorStore()->fetchAllocator(
                android::C2PlatformAllocatorStore::ION, &allocator);
        if (status != C2_OK || !allocator) {
            LOG(ERROR) << "No linear allocator, not copying inputs: " << status;
            return base;
        }
        // Local id of a private pool: nothing else registers it.
        constexpr C2BlockPool::local_id_t kPoolId = C2BlockPool::PLATFORM_START + 0x100;
        auto pool = std::make_shared<C2PooledBlockPool>(allocator, kPoolId,
                                                        C2PooledBlockPool::VER_HIDL);
        return std::make_shared<HidlPoolComponentStore>(std::move(base), std::move(pool));
    }

    HidlPoolComponentStore(std::shared_ptr<C2ComponentStore> base,
                           std::shared_ptr<C2BlockPool> pool)
        : mBase(std::move(base)), mPool(std::move(pool)) {}

    C2String getName() const override { return mBase->getName(); }

    c2_status_t createComponent(C2String name,
                                std::shared_ptr<C2Component>* const component) override {
        std::shared_ptr<C2Component> base;
        c2_status_t status = mBase->createComponent(std::move(name), &base);
        if (status != C2_OK) {
            return status;
        }
        *component = std::make_shared<HidlPoolComponent>(std::move(base), mPool);
        return C2_OK;
    }

    c2_status_t createInterface(C2String name,
                                std::shared_ptr<C2ComponentInterface>* const intf) override {
        return mBase->createInterface(std::move(name), intf);
    }

    std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() override {
        return mBase->listComponents();
    }

    c2_status_t copyBuffer(std::shared_ptr<C2GraphicBuffer> src,
                           std::shared_ptr<C2GraphicBuffer> dst) override {
        return mBase->copyBuffer(std::move(src), std::move(dst));
    }

    c2_status_t query_sm(const std::vector<C2Param*>& stackParams,
                         const std::vector<C2Param::Index>& heapParamIndices,
                         std::vector<std::unique_ptr<C2Param>>* const heapParams) const override {
        return mBase->query_sm(stackParams, heapParamIndices, heapParams);
    }

    c2_status_t config_sm(const std::vector<C2Param*>& params,
                          std::vector<std::unique_ptr<C2SettingResult>>* const failures) override {
        return mBase->config_sm(params, failures);
    }

    std::shared_ptr<C2ParamReflector> getParamReflector() const override {
        return mBase->getParamReflector();
    }

    c2_status_t querySupportedParams_nb(
            std::vector<std::shared_ptr<C2ParamDescriptor>>* const params) const override {
        return mBase->querySupportedParams_nb(params);
    }

    c2_status_t querySupportedValues_sm(
            std::vector<C2FieldSupportedValuesQuery>& fields) const override {
        return mBase->querySupportedValues_sm(fields);
    }

  private:
    const std::shared_ptr<C2ComponentStore> mBase;
    const std::shared_ptr<C2BlockPool> mPool;
};

}  // namespace

static constexpr int kThreadCount = 8;

int main() {
    using namespace ::aidl::android::hardware::media::c2;

    signal(SIGPIPE, SIG_IGN);
    LOG(INFO) << "Dolby Codec2 AIDL service (" << DOLBY_C2_INSTANCE << ") starting...";

    // As the HIDL services did: vndbinder for vendor services, and a hwbinder pool,
    // since the components are HIDL clients (DMS, the QTI store of the Dolby Vision
    // client) and get their callbacks on hwbinder threads.
    android::ProcessState::initWithDriver("/dev/vndbinder");
    android::ProcessState::self()->startThreadPool();
    std::thread([] {
        android::hardware::configureRpcThreadpool(kThreadCount, true /* callerWillJoin */);
        android::hardware::joinRpcThreadpool();
    }).detach();

    ABinderProcess_setThreadPoolMaxThreadCount(kThreadCount);
    ABinderProcess_startThreadPool();

    std::shared_ptr<C2ComponentStore> c2Store =
            HidlPoolComponentStore::wrap(android::GetCodec2DolbyComponentStore());
    CHECK(c2Store) << "Cannot create the Dolby C2ComponentStore.";

    std::shared_ptr<IComponentStore> store =
            ::ndk::SharedRefBase::make<utils::ComponentStore>(c2Store);
    const std::string instance =
            std::string(IComponentStore::descriptor) + "/" + DOLBY_C2_INSTANCE;
    binder_exception_t ex = AServiceManager_addService(store->asBinder().get(), instance.c_str());
    CHECK_EQ(ex, EX_NONE) << "Cannot register " << instance;
    LOG(INFO) << "Registered " << instance;

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;
}
