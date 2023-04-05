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

/// Handy function that can will catch and log exceptions related to dependency resolution when trying to add dependencies to resolver.
void AddDependency(const Resolver& resolver, const Keyword& lhs, const Keyword& rhs)
{
	try {
		resolver.addDependency(lhs, rhs);
	} catch (Resolver::SelfReferenceDependencyException& e) {
		logger::warn("	SKIP - {} is referencing itself", describe(e.current));
	} catch (Resolver::SuperfluousDependencyException& e) {
		logger::info("	INFO - {} does already implicitly depend on {}. This filter may be omitted.", describe(e.current), describe(e.superfluous));
		auto path = e.path;
		path.pop();
		std::ostringstream os;
		while (!path.empty()) {
			os << " -> " << path.top();
			path.pop();
		}
		logger::info("		Full path: {}", os.str());
	} catch (Resolver::CyclicDependencyException& e) {
		logger::warn("	SKIP - {} and {} use each other in their filters. Distribution might not work as expected.", describe(e.second), describe(e.first), describe(e.second));
		std::ostringstream os;
		os << e.path.top();
		auto path = e.path;
		path.pop();
		while (!path.empty()) {
			os << " -> " << path.top();
			path.pop();
		}
		logger::info("		Full path: {}", os.str());
	} catch (Resolver::UnknownDependencyException& e) {
		logger::error("	FAIL - {} was not present in configs. If you see this report to sasnikol :)", describe(e.value));
	}
}

void Dependencies::ResolveKeywords()
{
	if (!Forms::keywords) {
		return;
	}

	logger::info("{:*^50}", "RESOLVING KEYWORDS");
	const auto startTime = std::chrono::steady_clock::now();
	// Pre-build a map of all available keywords by names.
	std::unordered_map<std::string, RE::BGSKeyword*> allKeywords{};

	auto& keywordForms = Forms::keywords.GetForms();

	const auto dataHandler = RE::TESDataHandler::GetSingleton();
	for (const auto& kwd : dataHandler->GetFormArray<RE::BGSKeyword>()) {
		if (kwd) {
			if (const auto edid = kwd->GetFormEditorID(); !string::is_empty(edid)) {
				allKeywords[edid] = kwd;
			} else {
				if (const auto file = kwd->GetFile(0)) {
					const auto  modname = file->GetFilename();
					const auto  formID = kwd->GetLocalFormID();
					std::string mergeDetails;
					if (g_mergeMapperInterface && g_mergeMapperInterface->isMerge(modname.data())) {
						const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetOriginalFormID(
							modname.data(),
							formID);
						mergeDetails = std::format("->0x{:X}~{}", mergedFormID, mergedModName);
					}
					logger::error(" WARN : [0x{:X}~{}{}] keyword has an empty editorID!", formID, modname, mergeDetails);
				}
			}
		}
	}

	std::vector<Keyword> keywords;

	std::ranges::transform(keywordForms, std::back_inserter(keywords), [](const auto& elem) {
		return elem.form;
	});

	auto resolver = Resolver(keywords);
	/// A map that will be used to map back keywords to their data wrappers.
	std::unordered_multimap<RE::BGSKeyword*, Forms::Data<RE::BGSKeyword>> dataKeywords;

	for (const auto& formData : keywordForms) {
		dataKeywords.emplace(formData.form, formData);

		const auto findKeyword = [&](const std::string& name) -> RE::BGSKeyword* {
			return allKeywords[name];
		};

		const auto addDependencies = [&](const StringVec& a_strings, const std::function<RE::BGSKeyword*(const std::string&)>& matchingKeyword) {
			for (const auto& str : a_strings) {
				if (const auto& kwd = matchingKeyword(str); kwd) {
					AddDependency(resolver, formData.form, kwd);
				}
			}
		};

		const auto& stringFilters = formData.filters.strings;

		addDependencies(stringFilters.ALL, findKeyword);
		addDependencies(stringFilters.NOT, findKeyword);
		addDependencies(stringFilters.MATCH, findKeyword);
		addDependencies(stringFilters.ANY, [&](const std::string& name) -> RE::BGSKeyword* {
			for (const auto& [keywordName, keyword] : allKeywords) {
				if (string::icontains(keywordName, name)) {
					return keyword;
				}
			}
			return nullptr;
		});
	}

	const auto result = resolver.resolve();
	const auto endTime = std::chrono::steady_clock::now();

	keywordForms.clear();
	logger::info("	Keywords have been sorted: ");
	for (const auto& keyword : result) {
		logger::info("		{}", describe(keyword));
		const auto& [begin, end] = dataKeywords.equal_range(keyword);
		for (auto it = begin; it != end; ++it) {
			keywordForms.push_back(it->second);
		}
	}

	const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
	logger::info("Keywords resolution took {}μs / {}ms", duration, duration / 1000.0f);
}
