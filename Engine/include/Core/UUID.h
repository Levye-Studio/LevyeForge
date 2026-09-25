#pragma once
#include "Config.h"
#ifdef LF_PLATFORM_WINDOWS
#include <xhash>
#elif defined(LF_PLATFORM_LINUX)
#include <hashtable.h>
#endif
#include <cstdint>

namespace LevyeForge {

    class UUID{
    public:
        UUID();
        UUID(uint64_t uuid);
        UUID(const UUID&) = default;

        operator uint64_t() const { return m_UUID;}
    private:
        uint64_t m_UUID;
    };
}

namespace std {

    template<>
    struct hash<LevyeForge::UUID>{

        std::size_t operator()(const LevyeForge::UUID& uuid) const{

            return hash<uint64_t>()((uint64_t)uuid);
        }
    };
}
