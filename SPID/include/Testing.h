#pragma once

namespace Testing
{
	class TestResult
	{
	public:
		static TestResult Success(std::string testName = __builtin_FUNCTION()) { return { true, "passed", testName }; }
		static TestResult Fail(std::string message = "", std::string testName = __builtin_FUNCTION()) { return { false, message, testName }; }

		bool        success;
		std::string message;
		std::string testName;
	};
}

template <>
struct fmt::formatter<Testing::TestResult>
{
	template <class ParseContext>
	constexpr auto parse(ParseContext& ctx)
	{
		return ctx.begin();
	}
	template <class FormatContext>
	constexpr auto format(const Testing::TestResult& result, FormatContext& a_ctx) const
	{
		if (result.success) {
			return fmt::format_to(a_ctx.out(), "✅ {}", result.testName);
		} else {
			if (result.message.empty()) {
				return fmt::format_to(a_ctx.out(), "❌ {}", result.testName);
			}
			return fmt::format_to(a_ctx.out(), "❌ {}: {}", result.testName, result.message);
		}
	}
};

namespace Testing
{
	class Runner : public ISingleton<Runner>
	{
	private:
		using Cleanup = std::function<void()>;
		using Test = std::function<TestResult()>;
		using TestSuite = std::map<std::string, Test>;
		using TestModule = std::map<std::string, TestSuite>;
		
		TestModule tests;

		std::map<std::string, Cleanup> cleanup;

		static std::pair<int, int> RunModule(std::string moduleName, TestSuite& tests)
		{
			int total = 0;
			int success = 0;
			logger::critical("Running {} tests:", moduleName);
			for (auto& test : tests) {
				auto result = test.second();
				logger::critical("\t{}", result);
				if (result.success) {
					success++;
				}
				total++;
			}

			if (const auto& cleanup = GetSingleton()->cleanup.find(moduleName); cleanup != GetSingleton()->cleanup.end()) {
				cleanup->second();
			}

			logger::critical("Completed {}: {}/{} tests passed", moduleName, success, total);
			return { success, total };
		}
	public:
		
		static bool Register(const char* moduleName, const char* testName, Test test)
		{
			auto runner = GetSingleton();
			auto& module = runner->tests[moduleName];
			return module.try_emplace(testName, test).second;
		}

		static bool RegisterCleanup(const char* moduleName, Cleanup cleanup)
		{
			return GetSingleton()->cleanup.try_emplace(moduleName, cleanup).second;
		}

		static void Run() {
			logger::info("{:*^50}", "SELF TESTING");
			std::pair<int, int> counter = { 0, 0 };
			spdlog::set_level(spdlog::level::critical);  // silence all logging coming from the test.
			for (auto& [module, tests] : GetSingleton()->tests) {
				auto res = RunModule(module, tests);
				counter.first += res.first;
				counter.second += res.second;
			}
			spdlog::set_level(spdlog::level::info);  // restore logging level
			if (GetSingleton()->tests.size() > 1) {
				logger::info("Completed all tests: {}/{} tests passed", counter.first, counter.second);
			}

			if (counter.first != counter.second) {
				logger::info("No Skyrim for you! Fix Tests first! 😀");
				abort();
			}
		}
		
	};

	inline void Run() { Runner::Run(); }

	template <typename T>
	inline T* GetForm(RE::FormID a_formID)
	{
		auto form = RE::TESForm::LookupByID<T>(a_formID);
		assert(form);
		return form;
	}
}

#define CLEANUP                                                                                                   \
	inline void cleanupAfterTests();                                                                              \
	static bool cleanupAfterTests_registered = ::Testing::Runner::RegisterCleanup(moduleName, cleanupAfterTests); \
	inline void cleanupAfterTests()

#define TEST(name)                                                                                                     \
	inline ::Testing::TestResult test##name();                                                                         \
	static bool                  test##name##_registered = ::Testing::Runner::Register(moduleName, #name, test##name); \
	inline ::Testing::TestResult test##name##()

#define PASS return ::Testing::TestResult::Success();

#define FAIL(msg) return ::Testing::TestResult::Fail(msg);

#define ASSERT(expr, msg) \
	if (!(expr))          \
		return ::Testing::TestResult::Fail(msg);

#define EXPECT(expr, msg) \
	return (expr) ? ::Testing::TestResult::Success() : ::Testing::TestResult::Fail(msg);
