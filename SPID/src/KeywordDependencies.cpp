#include "KeywordDependencies.h"
#include "FormData.h"

/// An object that describes a keyword dependencies.
struct DependencyNode
{
private:
	RE::BGSKeyword*                                      value;
	std::unordered_map<RE::BGSKeyword*, DependencyNode*> children{};

public:
	explicit DependencyNode(RE::BGSKeyword* val) :
		value(val)
	{}

	~DependencyNode() = default;

	/// Adds given node as a child of the current node.
	/// If the same keyword has already been added, subsequent attempts will be ignored.
	void AddChild(DependencyNode* childNode)
	{
		auto child = childNode->value;
		if (child && child != value) {
			children.try_emplace(child, childNode);
		}
	}

	/// Checks whether current node has any dependencies.
	[[nodiscard]] bool HasDependencies() const
	{
		return !children.empty();
	}

	/// Returns number of dependencies that this node has.
	[[nodiscard]] std::size_t DependenciesCount() const
	{
		return children.size();
	}

	/// Checks if given keyword is either a direct child of the current node or is a child of one of the descendants.
	bool IsNestedChild(RE::BGSKeyword* keyword)
	{
		if (!value || keyword == value || children.empty()) {
			return false;
		}

		if (children.contains(keyword)) {
			return true;
		}

		for (const auto& dep : children | std::views::values) {
			if (dep->IsNestedChild(keyword)) {
				return true;
			}
		}

		return false;
	}
};

/// A map that associates keywords with their dependency nodes, holding all keyword's dependencies.
inline std::unordered_map<RE::BGSKeyword*, std::unique_ptr<DependencyNode>> keywordDependencies;

/// Finds an existing dependency node for given keyword.
/// If none found, a new node will be created and placed into the map.
DependencyNode* NodeForKeyword(RE::BGSKeyword* keyword)
{
	if (keywordDependencies.contains(keyword)) {
		return keywordDependencies[keyword].get();
	}

	const auto [it, result] = keywordDependencies.try_emplace(keyword, std::make_unique<DependencyNode>(keyword));
	return it->second.get();
}

/// Returns number of dependencies that given `keyword` has.
std::size_t DependenciesCount(RE::BGSKeyword* keyword)
{
	const auto iter = keywordDependencies.find(keyword);
	return iter != keywordDependencies.end() ? iter->second->DependenciesCount() : 0;
}

/// Checks whether `parent` keyword is dependent on `dependency` keyword.
bool IsDepending(RE::BGSKeyword* parent, RE::BGSKeyword* dependency)
{
	const auto iter = keywordDependencies.find(parent);
	return iter != keywordDependencies.end() && iter->second->IsNestedChild(dependency);
}

/// Makes `parent` keyword dependent on `dependency` keyword.
/// If `dependency` keyword is already depending on `parent` then no association will be created for `parent` -> `dependency` to avoid circular dependencies.
void AddDependency(RE::BGSKeyword* parent, RE::BGSKeyword* dependency)
{
	if (parent == dependency) {
		logger::warn("		{} SKIP - Keyword referencing itself in its filters.", parent->GetFormEditorID());
		return;
	}

	// If dependency is already present in parent's tree then no need to add it.
	if (IsDepending(parent, dependency)) {
		return;
	}

	// Skip circular dependencies.
	if (IsDepending(dependency, parent)) {
		logger::warn("		{} SKIP - Keywords {} and {} use each other in their filters.", parent->GetFormEditorID(), parent->GetFormEditorID(), dependency->GetFormEditorID(), parent->GetFormEditorID(), dependency->GetFormEditorID());
		return;
	}

	const auto parentNode = NodeForKeyword(parent);
	const auto dependencyNode = NodeForKeyword(dependency);

	parentNode->AddChild(dependencyNode);
}

void Dependencies::ResolveKeywords()
{
	if (!Forms::keywords) {
		return;
	}

	// Pre-build a map of all available keywords by names.
	std::unordered_map<std::string, RE::BGSKeyword*> allKeywords{};

	auto&          keywordForms = Forms::keywords.GetForms();
	const std::set distrKeywords(keywordForms.begin(), keywordForms.end());

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
					logger::error(" WARNING : [0x{:X}~{}{}] keyword has an empty editorID!", formID, modname, mergeDetails);
				}
			}
		}
	}

	// Fill keywordDependencies based on Keywords found in configs.
	for (auto& formData : distrKeywords) {
		const auto findKeyword = [&](const std::string& name) -> RE::BGSKeyword* {
			return allKeywords[name];
		};

		formData.filters.filters.for_each_filter<filters::SPID::MatchFilter>([&](const auto* entry) {
			if (const auto& kwd = allKeywords[entry->value]; kwd) {
				AddDependency(formData.form, kwd);
			}
		});

		formData.filters.filters.for_each_filter<filters::SPID::WildcardFilter>([&](const auto* entry) {
			for (const auto& [keywordName, keyword] : allKeywords) {
				if (string::icontains(keywordName, entry->value)) {
					AddDependency(formData.form, keyword);
				}
			}
		});
	}

	std::sort(keywordForms.begin(), keywordForms.end());

	// Print only unique entries in the log.
	Forms::DataVec<RE::BGSKeyword> resolvedKeywords(keywordForms);
	resolvedKeywords.erase(std::ranges::unique(resolvedKeywords).begin(), resolvedKeywords.end());

	logger::info("\tKeywords have been sorted: ");
	for (const auto& keywordData : resolvedKeywords) {
		logger::info("\t\t{} [0x{:X}]", keywordData.form->GetFormEditorID(), keywordData.form->GetFormID());
	}

	// Clear keyword dependencies
	keywordDependencies.clear();
}

/// Comparator that utilizes dependencies map created with Dependencies::ResolveKeywords() to sort keywords.
///
/// Order is determined by the following rules:
/// 1) If A is a dependency of B, then A must always be placed before B
/// 2) If B is a dependency of A, then A must always be placed after B
/// 3) If A has less dependencies than B, then A must be placed before B and vise versa. ("leaf" keywords should be on top)
/// 4) If A and B has the same number of dependencies they should be ordered alphabetically.
bool Forms::KeywordDependencySorter::sort(RE::BGSKeyword* a, RE::BGSKeyword* b)
{
	if (IsDepending(b, a)) {
		return true;
	} else if (IsDepending(a, b)) {
		return false;
	} else if (DependenciesCount(a) < DependenciesCount(b)) {
		return true;
	} else if (DependenciesCount(a) > DependenciesCount(b)) {
		return false;
	} else {
		return _stricmp(a->GetFormEditorID(), b->GetFormEditorID()) < 0;
	}
}
