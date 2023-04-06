#pragma once

#include <functional>

/// DependencyResolver builds a dependency graph for any arbitrary Values
/// and resolves it into a vector of the Values in the order
/// that ensures that dependent Values are placed after those they depend on.
/// <p>
///	<b>Note: If custom Value type does not define overloaded operator `<` you may provide a Comparator functor,
///	that will be used to control the order in which Values will be processed.
///	This Comparator should indicate whether one value is less than the other.</b>
///	</p>
template <typename Value, typename Comparator = std::less<Value>>
class DependencyResolver
{
	/// An object that represents each unique value passed to DependencyResolver
	///	and carries information about it's dependencies.
	struct Node
	{
		const Value value;

		/// Comparator functor that is used to compare Value of two Nodes.
		const Comparator& comparator;

		/// A list of all other nodes that current node depends on.
		///
		/// <p>
		///	<b>Use dependsOn() method to determine whether current node depends on another</b>.
		///	</p>
		Set<Node*> dependencies{};

		/// Flag that is used by DependencyResolver during resolution process
		///	to detect whether given Node was already resolved.
		///
		///	<p>
		///	This helps avoid unnecessary iterations over the same nodes,
		///	which might occur when it is a part of several dependencies lists.
		///	</p>
		bool isResolved;

		Node(Value value, const Comparator& comparator) :
			value(std::move(value)), comparator(comparator), isResolved(false) {}
		~Node() = default;

		bool dependsOn(Node* const node, std::stack<Value>& path) const
		{
			if (dependencies.empty()) {
				return false;
			}

			if (dependencies.contains(node)) {
				path.push(node->value);
				path.push(this->value);
				return true;
			}

			if (auto nextNode = std::find_if(dependencies.begin(), dependencies.end(), [&](const auto& dependency) { return dependency->dependsOn(node, path); }); nextNode != dependencies.end()) {
				path.push(this->value);
				return true;
			}

			return false;
		}

		/// May throw an exception if Directed Acyclic Graph rules will be violated.
		void addDependency(Node* node)
		{
			assert(node != nullptr);

			if (this == node) {
				throw SelfReferenceDependencyException(this->value);
			}

			std::stack<Value> cyclicPath;

			if (node->dependsOn(this, cyclicPath)) {
				cyclicPath.push(this->value);
				throw CyclicDependencyException(this->value, node->value, cyclicPath);
			}

			std::stack<Value> superfluousPath;

			if (this->dependsOn(node, superfluousPath)) {
				throw SuperfluousDependencyException(this->value, node->value, superfluousPath);
			}

			if (!dependencies.emplace(node).second) {
				// this->dependsOn(node) should always detect when duplicated dependency is added, but just to be safe.. :)
				throw SuperfluousDependencyException(this->value, node->value, {});
			}
		}

		bool operator==(const Node& other) const
		{
			return this->value == other.value;
		}

		bool operator<(const Node& other) const
		{
			if (this->dependencies.size() < other.dependencies.size()) {
				return true;
			}
			if (this->dependencies.size() == other.dependencies.size()) {
				return comparator(this->value, other.value);
			}

			return false;
		}
	};

	/// Custom functor that invokes Node's overloaded operator `<`.
	struct node_less
	{
		bool operator()(const Node* lhs, const Node* rhs) const
		{
			return *lhs < *rhs;
		}
	};

	/// A comparator object that will be used to determine whether one value is less than the other.
	///	This comparator is used to determine ordering in which nodes should be processed for the optimal resolution.
	const Comparator comparator;

	/// A container that holds nodes associated with each value that was added to DependencyResolver.
	Map<Value, Node*> nodes{};

	/// Looks up dependencies of a single node and places it into the result vector afterwards.
	void resolveNode(Node* const node, std::vector<Value>& result) const
	{
		if (node->isResolved) {
			return;
		}
		for (const auto& dependency : node->dependencies) {
			resolveNode(dependency, result);
		}
		result.push_back(node->value);
		node->isResolved = true;
	}

public:
	DependencyResolver() = default;

	DependencyResolver(const std::vector<Value>& values) :
		comparator(std::move(Comparator()))
	{
		for (const auto& value : values) {
			nodes.try_emplace(value, new Node(value, comparator));
		}
	}

	~DependencyResolver()
	{
		for (const auto& pair : nodes) {
			delete pair.second;
		}
	}

	/// Attempts to create a dependency rule between `parent` and `dependency` objects.
	///	If either of those objects were not present in the original vector they'll be added in-place.
	///
	/// <p>
	///	<b>May throw one of the following exceptions when Directed Acyclic Graph rules are being violated.</b>
	///	<list>
	///	CyclicDependencyException
	///	SuperfluousDependencyException
	///	SelfReferenceDependencyException
	///	</list>
	///	</p>
	void addDependency(const Value& parent, const Value& dependency)
	{
		Node* parentNode;
		Node* dependencyNode;

		if (const auto it = nodes.find(parent); it != nodes.end()) {
			parentNode = it->second;
		} else {
			parentNode = new Node(parent, comparator);
			nodes.try_emplace(parent, parentNode);
		}

		if (const auto it = nodes.find(dependency); it != nodes.end()) {
			dependencyNode = it->second;
		} else {
			dependencyNode = new Node(dependency, comparator);
			nodes.try_emplace(dependency, dependencyNode);
		}

		if (parentNode && dependencyNode) {
			parentNode->addDependency(dependencyNode);
		}
	}

    /// Add an isolated object to the resolver's graph.
    ///
    /// Isolated object is the one that doesn't have any dependencies on the others.
    /// However, dependencies can be added later using addDependency() method.
    void addIsolated(const Value& value)
	{
		if (!nodes.contains(value)) {
			nodes.try_emplace(value, new Node(value, comparator));
		}
	}

	/// Creates a vector that contains all values sorted topologically according to dependencies provided with addDependency method.
	[[nodiscard]] std::vector<Value> resolve() const
	{
		std::vector<Value> result;

		/// A vector of nodes that are ordered in a way that would make resolution the most efficient
		///	by reducing number of lookups for all nodes to resolved the graph.
		std::vector<Node*> orderedNodes;

		std::ranges::transform(nodes, std::back_inserter(orderedNodes), [](const auto& pair) {
			return pair.second;
		});
		// Sort nodes in correct order of processing.
		std::sort(orderedNodes.begin(), orderedNodes.end(), node_less());

		for (const auto& node : orderedNodes) {
			node->isResolved = false;
		}

		for (const auto& node : orderedNodes) {
			resolveNode(node, result);
		}

		return result;
	}

	/// An exception thrown when DependencyResolver attempts to add current value as a dependency of itself.
	struct SelfReferenceDependencyException : std::exception
	{
		SelfReferenceDependencyException(const Value& current) :
			current(current)
		{}

		const Value& current;
	};

	/// An exception thrown when DependencyResolver attempts to add a dependency that will create a cycle (e.g. A -> B -> A)
	struct CyclicDependencyException : std::exception
	{
		CyclicDependencyException(Value first, Value second, std::stack<Value> path) :
			first(std::move(first)),
			second(std::move(second)),
			path(std::move(path))
		{}

		const Value             first;
		const Value             second;
		const std::stack<Value> path;
	};

	/// An exception thrown when DependencyResolver attempts to add a dependency that can be inferred implicitly.
	///	For example if (A -> B) and (B -> C) then (A -> C) is a superfluous dependency that is inferred from the first two.
	struct SuperfluousDependencyException : std::exception
	{
		SuperfluousDependencyException(Value current, Value superfluous, std::stack<Value> path) :
			current(std::move(current)),
			superfluous(std::move(superfluous)),
			path(std::move(path))
		{}

		const Value             current;
		const Value             superfluous;
		const std::stack<Value> path;
	};
};
