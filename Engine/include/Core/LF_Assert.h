#pragma once

#include "Config.h"
#include "Log.h"
#include <filesystem>

#ifdef LF_ENABLE_ASSERTS

	// Alteratively we could use the same "default" message for both "WITH_MSG" and "NO_MSG" and
	// provide support for custom formatting by concatenating the formatting string instead of having the format inside the default message
	#define LF_INTERNAL_ASSERT_IMPL(type, check, msg, ...) { if(!(check)) { LF##type##ERROR(msg, __VA_ARGS__); LF_DEBUGBREAK(); } }
	#define LF_INTERNAL_ASSERT_WITH_MSG(type, check, ...) LF_INTERNAL_ASSERT_IMPL(type, check, "Assertion failed: {0}", __VA_ARGS__)
	#define LF_INTERNAL_ASSERT_NO_MSG(type, check) LF_INTERNAL_ASSERT_IMPL(type, check, "Assertion '{0}' failed at {1}:{2}", LF_STRINGIFY_MACRO(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)

	#define LF_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
	#define LF_INTERNAL_ASSERT_GET_MACRO(...) LF_EXPAND_MACRO( LF_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, LF_INTERNAL_ASSERT_WITH_MSG, LF_INTERNAL_ASSERT_NO_MSG) )

	// Currently accepts at least the condition and one additional parameter (the message) being optional
	#define LF_ASSERT(...) LF_EXPAND_MACRO( LF_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__) )
	#define LF_CORE_ASSERT(...) LF_EXPAND_MACRO( LF_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__) )
#else
	#define LF_ASSERT(...)
	#define LF_CORE_ASSERT(...)
#endif
