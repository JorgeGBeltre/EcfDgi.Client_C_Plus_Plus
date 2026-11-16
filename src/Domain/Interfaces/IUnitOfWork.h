#pragma once

namespace ecf::domain {

class IUnitOfWork {
public:
    virtual ~IUnitOfWork() = default;

    virtual int saveChanges() = 0;
};

}
