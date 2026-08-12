#pragma once

#include <memory>
#include <new>

namespace pd {

template <typename Service>
class AppScopedService {
public:
    AppScopedService() = default;
    AppScopedService(const AppScopedService&) = delete;
    AppScopedService& operator=(const AppScopedService&) = delete;

    bool setActive(bool active) {
        if (!active) {
            service_.reset();
            return true;
        }
        if (service_ == nullptr) service_.reset(new (std::nothrow) Service());
        return service_ != nullptr;
    }

    Service* get() { return service_.get(); }
    const Service* get() const { return service_.get(); }

private:
    std::unique_ptr<Service> service_;
};

}  // namespace pd
