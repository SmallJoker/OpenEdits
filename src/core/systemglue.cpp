#include "systemglue.h"

#include "macros.h" // ASSERT_FORCED
#include "version.h"
#include <filesystem>
#include <stdlib.h> // getenv
#ifdef _WIN32
	#include <windows.h> // GetModuleFileNameA
#endif

#define PRINT_DBG(...) if (0) printf(__VA_ARGS__)

static std::filesystem::path get_binary_path()
{
#if defined(__unix__)
	return std::filesystem::canonical("/proc/self/exe");
#elif defined(_WIN32)
	char buf[1024];
	DWORD ret = GetModuleFileNameA(nullptr, buf, sizeof(buf));
	ASSERT_FORCED(ret != 0 && ret != sizeof(buf), "Cannot get executable path");
	return std::filesystem::canonical(buf);
#else
	#error Unknown platform
#endif
}

void change_working_dir()
{
	auto path = get_binary_path();
	PRINT_DBG("Binary dir='%s', Executable name='%s'\n",
		path.parent_path().string().c_str(),
		path.filename().string().c_str()
	);

	std::filesystem::current_path(path.parent_path());
}

std::vector<std::string> get_user_paths(bool create_if_needed)
{
	const char *p_path = getenv(
// TODO: MacOS, Haiku ?
#if defined(__unix__)
		"XDG_CONFIG_HOME"
#elif defined(_WIN32)
		"APPDATA"
#else
		#error Unknown platform
#endif
	);

	std::vector<std::string> ret;
	if (p_path) {
		// Main directory for settings and the like
		ret.emplace_back(
			std::filesystem::absolute(p_path)
				.append(APPLICATION_NAME)
					.string()
		);
	} else {
		fprintf(stderr, "-!- User path is not defined! Falling back to working directory.\n");
	}

	if (create_if_needed && !ret.empty())
		std::filesystem::create_directories(ret[0]);

	ret.emplace_back(std::filesystem::current_path().string());
	for (const auto &str : ret)
		printf("User path: %s\n", str.c_str());

	return ret;
}
