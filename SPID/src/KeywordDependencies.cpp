#include "KeywordDependencies.h"
#include "DependencyResolver.h"
#include "FormData.h"

using Keyword = RE::BGSKeyword*;

struct keyword_less
{
	bool operator()(const Keyword& a, const Keyword& b) const
	{
		return a->GetFormEditorID() < b->GetFormEditorID();
	}
};

using Resolver = DependencyResolver<Keyword, keyword_less>;

/// Handy function that will catch and log exceptions related to dependency resolution when trying to add dependencies to resolver.
void AddDependency(Resolver& resolver, const Keyword& lhs, const Keyword& rhs)
{
	try {
		resolver.addDependency(lhs, rhs);
	} catch (Resolver::SelfReferenceDependencyException& e) {
		buffered_logger::warn("\tINFO - {} is referencing itself", describe(e.current));
	} catch (Resolver::CyclicDependencyException& e) {
		std::ostringstream os;
		os << e.path.top();
		auto path = e.path;
		path.pop();
		while (!path.empty()) {
			os << " -> " << path.top();
			path.pop();
		}
		buffered_logger::warn("\tINFO - {} and {} depend on each other. Distribution might not work as expected.\n\t\t\t\t\tFull path: {}", describe(e.first), describe(e.second), os.str());
	} catch (...) {
		// we'll ignore other exceptions
	}
}

void Dependencies::ResolveKeywords()
{
	if (!Forms::keywords) {
		return;
	}

	const auto startTime = std::chrono::steady_clock::now();

	// Pre-build a map of all available keywords by names.
	StringMap<RE::BGSKeyword*> allKeywords{};

	auto& keywordForms = Forms::keywords.GetForms();


	Resolver resolver;

	/// A map that will be used to map back keywords to their data wrappers.
	std::unordered_multimap<RE::BGSKeyword*, Forms::Data<RE::BGSKeyword>> dataKeywords;

	for (auto& formData : keywordForms) {
		dataKeywords.emplace(formData.form, formData);
		resolver.addIsolated(formData.form);
		formData.filters.filters->for_each_filter<filters::SPID::KeywordFilter>([&](const auto* entry) {
			if (const auto& kwd = entry->value) {
				AddDependency(resolver, formData.form, kwd);
			}
		});
	}

	const auto result = resolver.resolve();
	const auto endTime = std::chrono::steady_clock::now();

	keywordForms.clear();
	logger::info("\tSorting keywords :");
	for (const auto& keyword : result) {
		const auto& [begin, end] = dataKeywords.equal_range(keyword);
		if (begin != end) {
			logger::info("\t\t{}", describe(begin->second.form));
		}
		for (auto it = begin; it != end; ++it) {
			keywordForms.push_back(it->second);
		}
	}

	const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
	logger::info("\tKeyword resolution took {}μs / {}ms", duration, duration / 1000.0f);
}
