#pragma once

#include <iostream>
#include <mutex>

class LockedOStream {
public:
    using ostream_manipulator = std::ostream&(std::ostream&);
    using scoped_lock = std::lock_guard<std::mutex>;

    LockedOStream(std::ostream& ostream, std::mutex& ostream_mutex) 
        : ostream(ostream)
        , ostream_lock(ostream_mutex)
    {
    }

    template <class T>
    LockedOStream& operator<<(const T& data) {
        ostream << data;
        return *this;
    }

    LockedOStream& operator<<(ostream_manipulator pf) {
        pf(ostream);
        return *this;
    }

private:
    //friend LockedOStream& operator<<(LockedOStream& os, ostream_manipulator);

    std::ostream& ostream;
    scoped_lock ostream_lock;
};

#if 0
static inline LockedOStream& operator<<(LockedOStream& os, ostream_manipulator pf) {
    pf(os.ostream);
    return os;
}
#endif
