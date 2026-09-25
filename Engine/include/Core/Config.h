#pragma once
#pragma warning(disable : 4996)


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "PlatformDetection.h"


#ifdef LF_DEBUG
	#if defined(LF_PLATFORM_WINDOWS)
		#define LF_DEBUGBREAK() __debugbreak()
	#elif defined(LF_PLATFORM_LINUX) || defined(LF_PLATFORM_MACOS)
		#include <signal.h>
		#define LF_DEBUGBREAK() raise(SIGTRAP)
	#else
		#error "Platform doesn't support debugbreak yet!"
	#endif
	#define LF_ENABLE_ASSERTS
#else
	#define LF_DEBUGBREAK()
#endif

#define LF_EXPAND_MACRO(x) x
#define LF_STRINGIFY_MACRO(x) #x

// Define LF_API for import/export depending on platform and usage
#if defined(LF_PLATFORM_WINDOWS)
    #ifdef LF_EXPORT
    #ifdef _MSC_VER
        // #define LF_API __declspec(dllexport)
    #else
        // #define LF_API __attribute__((visibility("default")))
    #endif
    #endif

    #ifdef LF_IMPORT
    #ifdef _MSC_VER
        // #define LF_API __declspec(dllimport)
    #else
        #define LF_API
    #endif
    #endif
#else
    // Linux/macOS: only need to set visibility when exporting
    #ifdef LF_EXPORT
        // #define LF_API __attribute__((visibility("default")))
    #else
        #define LF_API
    #endif
#endif

#ifdef LF_PLATFORM_WINDOWS
    #ifndef NOMINMAX
    # define NOMINMAX
    #endif
    #include <Windows.h>
#endif

#define JPH_DEBUG

#define BIT(x) (1 << x)

#define BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

#define SCREEN_WIDTH 720
#define SCREEN_HEIGHT 1280

namespace LevyeForge {

    //--------------------- Scope = unique pointer --------------------
    template<typename T>
    using Scope = std::unique_ptr<T>;
    template<typename T, typename ... Args>
    constexpr Scope<T> CreateScope(Args&& ... args){

        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    //--------------------- Ref = shared pointer -----------------------
    template<typename T>
    using Ref = std::shared_ptr<T>;
    template<typename T, typename ... Args>
    constexpr Ref<T> CreateRef(Args&& ... args){

        return std::make_shared<T>(std::forward<Args>(args)...);
    }
}
