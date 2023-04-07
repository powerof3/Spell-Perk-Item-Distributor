#pragma once

namespace Expressions
{
	enum class Result
	{
		/// Expression has failed evaluation.
		///
		/// This result is not permanent and the same target might pass future evaluations.
		kFail = 0,

		/// Expression has discarded evaluation.
		///
		///	This result is similar to kPass.
		///	It may or may not be permanent, but some parts of the Expression explicitly requested to permanently discard that entry,
		///	and do not try evaluating it again.
		kDiscard,

		/// Expression has passed evaluation.
		///
		///	This result is not permanent and the same target might fail future evaluations.
		kPass
	};
}

// ---------------- Expressions ----------------
namespace Expressions
{
	/// An abstract filter component that can be evaluated for specified Target.
	template <typename Target>
	struct Evaluatable
	{
		virtual ~Evaluatable() = default;

		/// Evaluates whether specified Target matches conditions defined by this Evaluatable.
		[[nodiscard]] virtual Result evaluate([[maybe_unused]] const Target&) const = 0;

		virtual std::ostringstream& describe(std::ostringstream&) const = 0;

		/// Returns a flag that determines whether given Evaluatable doesn't contribute
		/// to overall composition of Evaluatables.
		/// This is usually true for Evaluatable which always return Result::kPass, or empty Expressions.
		[[nodiscard]] virtual bool isSuperfluous() const = 0;

		[[nodiscard]] virtual bool isValid() const = 0;
	};

	template <typename Target>
	struct Filter : Evaluatable<Target>
	{
		// All Filters by default are not superfluous, unless they explicitly override this method.
		[[nodiscard]] bool isSuperfluous() const override { return false; }

		// All Filters by default are valid, unless they explicitly override this method.
		[[nodiscard]] bool isValid() const override { return true; }

	protected:
		Filter() = default;
	};

	/// Requires template type to be of any Filter sub-types.
	template <typename T, typename Target>
	concept filtertype = std::derived_from<T, Filter<Target>>;

	/// Filter evaluates whether or not given Target matches specific value provided to the filter.
	///	<b>Inherit it to define your own filters.</b>
	template <typename Target, typename Value>
	struct ValueFilter : Filter<Target>
	{
		Value value;

		ValueFilter(const Value& value) :
			Filter<Target>(),
			value(value) {}
		ValueFilter() = default;
	};

	/// An expression is a combination of filters and/or nested expressions.
	///
	/// To determine how entries in the expression are combined use either AndExpression or OrExpression.
	template <typename Target>
	struct Expression : Evaluatable<Target>
	{
		// Expression is considered superfluous if it is empty or contains only superfluous Evaluatables.
		[[nodiscard]] bool isSuperfluous() const override
		{
			if (entries.empty()) {
				return true;
			}

			return std::ranges::all_of(entries.begin(), entries.end(), [](const auto& entry) {
				return entry->isSuperfluous();
			});
		}

		// Expression is considered valid only when all Evaluatables it contains are valid.
		[[nodiscard]] bool isValid() const override
		{
			if (entries.empty()) {
				return true;
			}

			return std::ranges::all_of(entries.begin(), entries.end(), [](const auto& entry) {
				return entry->isValid();
			});
		}

		/// Reduces the expression by removing any contained superfluous Evaluatables.
		void reduce()
		{
			for (auto it = entries.begin(); it != entries.end();) {
				const auto eval = it->get();
				if (const auto expression = dynamic_cast<Expression*>(eval)) {
					expression->reduce();
				}
				if (eval->isSuperfluous()) {
					it = entries.erase(it);
				} else {
					++it;
				}
			}
		}

		void emplace_back(Evaluatable<Target>* ptr)
		{
			if (ptr) {
				entries.emplace_back(ptr);
			}
		}

		template <typename FilterType>
		requires filtertype<FilterType, Target>
		void for_each_filter(std::function<void(const FilterType*)> a_callback) const
		{
			for (const auto& eval : entries) {
				if (const auto expression = dynamic_cast<Expression*>(eval.get())) {
					expression->for_each_filter(a_callback);
				} else if (const auto entry = dynamic_cast<FilterType*>(eval.get())) {
					a_callback(entry);
				}
			}
		}

		/// Calls the mapper function on every Evaluatable of specified type FilterType within the Expression (including nested Expressions)
		/// and replaces such Evaluatables with a new Evaluatable returned from mapper.
		template <typename FilterType>
		requires filtertype<FilterType, Target>
		void map(std::function<Evaluatable<Target>*(FilterType*)> mapper)
		{
			for (auto it = entries.begin(); it != entries.end(); ++it) {
				if (const auto expression = dynamic_cast<Expression*>(it->get())) {
					expression->map(mapper);
				} else if (const auto entry = dynamic_cast<FilterType*>(it->get())) {
					if (const auto newFilter = mapper(entry); newFilter) {
						auto newEval = std::unique_ptr<Evaluatable<Target>>(newFilter);
						it->swap(newEval);
					}
				}
			}
		}

		template <typename FilterType>
		requires filtertype<FilterType, Target>
		[[nodiscard]] bool contains(std::function<bool(const FilterType*)> comparator) const
		{
			return std::ranges::any_of(entries, [&](const auto& eval) {
				if (const auto filter = dynamic_cast<Expression*>(eval.get())) {
					return filter->contains(comparator);
				}
				if (const auto entry = dynamic_cast<const FilterType*>(eval.get())) {
					return comparator(entry);
				}
				return false;
			});
		}

		std::ostringstream& describe(std::ostringstream& ss) const override
		{
			return join(", ", ss);
		}

	protected:
		std::vector<std::unique_ptr<Evaluatable<Target>>> entries{};

		std::ostringstream& join(const std::string& separator, std::ostringstream& os) const
		{
			auto       begin = entries.begin();
			const auto end = entries.end();
			const bool isComposite = std::ranges::count_if(entries, [](const auto& entry) {
				return !entry->isSuperfluous();
			}) > 1;

			if (isComposite)
				os << "(";
			if (begin != end) {
				(*begin++)->describe(os);
				for (; begin != end; ++begin) {
					os << separator;
					(*begin)->describe(os);
				}
			}
			if (isComposite)
				os << ")";
			return os;
		}
	};

	/// Negated is an evaluatable wrapper that always inverts the result of the wrapped evaluation.
	/// It corresponds to `-` prefix in a config file (e.g. "-ActorTypeNPC")
	template <typename Target>
	struct Negated : Expression<Target>
	{
		Negated(Evaluatable<Target>* const eval) :
			Expression<Target>()
		{
			this->entries.emplace_back(eval);
		}

		[[nodiscard]] Result evaluate(const Target& target) const override
		{
			if (const auto eval = wrapped_value()) {
				switch (eval->evaluate(target)) {
				case Result::kFail:
				case Result::kDiscard:
					return Result::kPass;
				case Result::kPass:
					return Result::kFail;
				}
			}
			return Result::kFail;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			if (const auto eval = wrapped_value()) {
				os << "NOT ";
				return eval->describe(os);
			}
			return os;
		}

		[[nodiscard]] bool isSuperfluous() const override
		{
			// At the moment we can't detect whether a Negated expression is superfluous even if wrapped Evaluatable is.
			// So Negated expression is only considered superfluous if it doesn't have a wrapped value (which should never be the case btw :) )
			return wrapped_value() == nullptr;
		}

	private:
		Evaluatable<Target>* wrapped_value() const
		{
			return this->entries.front().get();
		}
	};

	/// Entries are combined using AND logic.
	template <typename Target>
	struct AndExpression : Expression<Target>
	{
		[[nodiscard]] Result evaluate(const Target& target) const override
		{
			for (const auto& entry : this->entries) {
				const auto res = entry.get()->evaluate(target);
				if (res != Result::kPass) {
					return res;
				}
			}
			return Result::kPass;
		}

		std::ostringstream& describe(std::ostringstream& os) const override
		{
			return this->join(" AND ", os);
		}
	};

	/// Entries are combined using OR logic.
	template <typename Target>
	struct OrExpression : Expression<Target>
	{
		[[nodiscard]] Result evaluate(const Target& target) const override
		{
			auto failure = Result::kFail;

			for (const auto& entry : this->entries) {
				const auto res = entry.get()->evaluate(target);
				if (res == Result::kPass) {
					return Result::kPass;
				}
				if (res > failure) {
					// we rely on Result cases being ordered in order of priorities.
					// Hence if there was a kDiscard it will always be the result of this evaluation.
					failure = res;
				}
			}
			return this->entries.empty() ? Result::kPass : failure;
		}

		std::ostringstream& describe(std::ostringstream& ss) const override
		{
			return this->join(" OR ", ss);
		}
	};
}
