#include "KeywordDependencies.h"
#include "DependencyResolver.h"
#include "FormData.h"

using Keyword = RE::BGSKeyword*;

/// Comparator that preserves relative order at which Keywords appeared in config files.
/// If that order is undefined it falls back to alphabetical order of EditorIDs.
struct keyword_less
{
	using RelativeOrderMap = Map<Keyword, std::int32_t>;

	const RelativeOrderMap relativeOrder;

	bool operator()(const Keyword& a, const Keyword& b) const
	{
		const auto aIdx = getIndex(a);
		const auto bIdx = getIndex(b);
		if (aIdx >= 0 && bIdx >= 0) {
			if (aIdx < bIdx) {
				return true;
			}
			if (aIdx > bIdx) {
				return false;
			}
		}
		return a->GetFormEditorID() < b->GetFormEditorID();
	}

	[[nodiscard]] int getIndex(const Keyword& kwd) const
	{
		if (relativeOrder.contains(kwd)) {
			return relativeOrder.at(kwd);
		}
		return -1;
	}
};

using Resolver = DependencyResolver<Keyword, keyword_less>;

/// Handy function that will catch and log exceptions related to dependency resolution when trying to add dependencies to resolver.
void AddDependency(Resolver& resolver, const Keyword& lhs, const Keyword& rhs)
{
	try {
		resolver.addDependency(lhs, rhs);
	} catch (Resolver::SelfReferenceDependencyException& e) {
		buffered_logger::warn("\t\tINFO - {} is referencing itself.", describe(e.current));
	} catch (Resolver::CyclicDependencyException& e) {
		std::ostringstream os;
		os << e.path.top();
		auto path = e.path;
		path.pop();
		while (!path.empty()) {
			os << " -> " << path.top();
			path.pop();
		}
		buffered_logger::warn("\t\tINFO - {} and {} may depend on each other. Distribution might not work as expected.", describe(e.first), describe(e.second));
		buffered_logger::warn("\t\t\tFull path: {}", os.str());
	} catch (...) {
		// we'll ignore other exceptions
	}
}

void Dependencies::ResolveKeywords()
{
	if (!Forms::keywords) {
		return;
	}

	Timer timer;
	timer.start();

	// Pre-build a map of all available keywords by names.
	StringMap<RE::BGSKeyword*> allKeywords{};

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
					logger::error("\tWARN : [0x{:X}~{}{}] keyword has an empty editorID!", formID, modname, mergeDetails);
				}
			}
		}
	}

	keyword_less::RelativeOrderMap orderMap;

	for (std::int32_t index = 0; index < keywordForms.size(); ++index) {
		orderMap.emplace(keywordForms[index].form, index);
	}

	Resolver resolver{ keyword_less(orderMap) };

	/// A map that will be used to map back keywords to their data wrappers.
	std::unordered_multimap<RE::BGSKeyword*, Forms::Data<RE::BGSKeyword>> dataKeywords;

	logger::info("\tSorting keywords...");
	for (const auto& formData : keywordForms) {
		dataKeywords.emplace(formData.form, formData);
		resolver.addIsolated(formData.form);

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

	timer.end();

	keywordForms.clear();
	logger::info("\tSorted keywords: ");
	for (const auto& keyword : result) {
		const auto& [begin, end] = dataKeywords.equal_range(keyword);
		if (begin != end) {
			logger::info("\t\t{}", describe(begin->second.form));
		}
		for (auto it = begin; it != end; ++it) {
			keywordForms.push_back(it->second);
		}
	}

	logger::info("\tKeyword resolution took {}μs / {}ms", timer.duration_μs(), timer.duration_ms());

	allKeywords.clear();
}
