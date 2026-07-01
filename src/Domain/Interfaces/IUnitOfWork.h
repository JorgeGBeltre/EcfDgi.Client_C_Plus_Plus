#pragma once

namespace ecf::domain {

class IUnitOfWork {
public:
    virtual ~IUnitOfWork() = default;

    // Persists pending changes; returns the number of affected records.
    virtual int saveChanges() = 0;
};

}  // namespace ecf::domain
