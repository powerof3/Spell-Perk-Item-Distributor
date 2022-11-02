#include "KeywordDependencies.h"
#include "Defs.h"
#include "LookupForms.h"

/// An object that describes a keyword dependencies.
struct DependencyNode
{
private:
	RE::BGSKeyword* value;

	std::unordered_map<RE::BGSKeyword*, DependencyNode*> children = {};

public:
	DependencyNode(RE::BGSKeyword* val)
	{
		value = val;
		children = {};
	}

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
	bool HasDependencies() const
	{
		return !children.empty();
	}

	/// Returns number of dependencies that this node has.
	size_t DependenciesCount() const
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

/// A map that associates keywords with their dependency nodes, holding all keyword's dependnecies.
inline std::unordered_map<RE::BGSKeyword*, DependencyNode*> keywordDependencies;

/// Finds an existing dependency node for given keyword.
/// If none found, a new node will be created and placed into the map.
DependencyNode* NodeForKeyword(RE::BGSKeyword* keyword)
{
	if (keywordDependencies.contains(keyword)) {
		return keywordDependencies.at(keyword);
	}

	auto node = new DependencyNode(keyword);
	keywordDependencies.try_emplace(keyword, node);
	return node;
}

/// Returns number of dependencies that given `keyword` has.
size_t DependenciesCount(RE::BGSKeyword* keyword)
{
	auto iter = keywordDependencies.find(keyword);
	return iter != keywordDependencies.end() ? iter->second->DependenciesCount() : 0;
}

/// Checks whether `parent` keyword is dependent on `dependency` keyword.
bool IsDepending(RE::BGSKeyword* parent, RE::BGSKeyword* dependency)
{
	auto iter = keywordDependencies.find(parent);
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

    logger::info("{:*^50}", "RESOLVING KEYWORDS");

	// Pre-build a map of all available keywords by names.
	std::unordered_map<std::string, RE::BGSKeyword*> allKeywords{};

	const auto& dataHandler = RE::TESDataHandler::GetSingleton();
	for (const auto& kwd : dataHandler->GetFormArray<RE::BGSKeyword>()) {
		if (kwd) {
			if (const auto edid = kwd->GetFormEditorID(); !string::is_empty(edid)) {
				allKeywords[edid] = kwd;
			} else {
				if (const auto file = kwd->GetFile(0)) {
					logger::error(" WARNING : [0x{:X}~{}] keyword has an empty editorID!", kwd->GetLocalFormID(), file->GetFilename());
				}
			}
		}
	}

	// Fill keywordDependencies based on Keywords found in configs.
	for (auto& formData : Forms::keywords.forms) {
		auto& [keyword, idxOrCount] = std::get<DATA::TYPE::kForm>(formData);
		auto& [strings_ALL, strings_NOT, strings_MATCH, strings_ANY] = std::get<DATA::TYPE::kStrings>(formData);

		const auto findKeyword = [&](const std::string& name) -> RE::BGSKeyword* {
			return allKeywords[name];
		};

		const auto addDependencies = [&](const StringVec& a_strings, std::function<RE::BGSKeyword*(const std::string&)> matchingKeyword) {
			for (const auto& str : a_strings) {
				if (const auto& kwd = matchingKeyword(str); kwd) {
					AddDependency(keyword, kwd);
				}
			}
		};

		addDependencies(strings_ALL, findKeyword);
		addDependencies(strings_NOT, findKeyword);
		addDependencies(strings_MATCH, findKeyword);
		addDependencies(strings_ANY, [&](const std::string& name) -> RE::BGSKeyword* {
			for (const auto& iter : allKeywords) {
				if (string::icontains(iter.first, name)) {
					return iter.second;
				}
			}
			return nullptr;
		});
	}

	// Re-add all keywords back after dependency list has been built.
	auto keywordForms = Forms::keywords.forms;
	Forms::keywords.forms.clear();
	for (const auto& keywordData : keywordForms) {
		Forms::keywords.forms.emplace_back(keywordData);
	}

	logger::info("	Keywords have been sorted: ");
	for (const auto& keywordData : Forms::keywords.forms) {
		const auto& [keyword, idxOrCount] = std::get<DATA::TYPE::kForm>(keywordData);
	    logger::info("		{} [0x{:X}]", keyword->GetFormEditorID(), keyword->GetFormID());
	}
}

/// Comparator that utilizes dependencies map created with Dependencies::ResolveKeywords() to sort keywords.
///
/// Order is determined by the following rules:
/// 1) If A is a dependency of B, then A must always be placed before B
/// 2) If B is a dependency of A, then A must always be placed after B
/// 3) If A has less dependencies than B, then A must be placed before B and vise versa. ("leaf" keywords should be on top)
/// 4) If A and B has the same number of dependencies they should be ordered alphabetically.
bool KeywordDependencySorter::operator()(RE::BGSKeyword* a, RE::BGSKeyword* b) const
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
