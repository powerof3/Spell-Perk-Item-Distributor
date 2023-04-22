#pragma once

/// A template to enable bitmask behavior (e.g. bitwise operators) for a given type E.
///	Typically used for typed enums.
template <typename E>
struct enable_bitmask_operators
{
	static constexpr bool enable = false;
};

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, E> constexpr operator|(E lhs, E rhs)
{
	typedef std::underlying_type_t<E> underlying;
	return static_cast<E>(
		static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, E> constexpr operator&(E lhs, E rhs)
{
	typedef std::underlying_type_t<E> underlying;
	return static_cast<E>(
		static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, E> constexpr operator^(E lhs, E rhs)
{
	typedef std::underlying_type_t<E> underlying;
	return static_cast<E>(
		static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
}

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, E> constexpr operator~(E lhs)
{
	typedef std::underlying_type_t<E> underlying;
	return static_cast<E>(
		~static_cast<underlying>(lhs));
}

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, E&> constexpr operator|=(E& lhs, E rhs)
{
	typedef std::underlying_type_t<E> underlying;
	lhs = static_cast<E>(
		static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
	return lhs;
}

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, E&> constexpr operator&=(E& lhs, E rhs)
{
	typedef std::underlying_type_t<E> underlying;
	lhs = static_cast<E>(
		static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
	return lhs;
}

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, E&> constexpr operator^=(E& lhs, E rhs)
{
	typedef std::underlying_type_t<E> underlying;
	lhs = static_cast<E>(
		static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
	return lhs;
}

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, bool> constexpr has(const E& value, const E& flag)
{
	return (value & flag) == flag;
}

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, E&> constexpr enable(E& value, const E& flag)
{
	value |= flag;
	return value;
}

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, E&> constexpr disable(E& value, const E& flag)
{
	value &= ~flag;
	return value;
}

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, E> constexpr create(const std::underlying_type_t<E>& value)
{
	return static_cast<E>(value);
}

template <typename E>
std::enable_if_t<enable_bitmask_operators<E>::enable, std::underlying_type_t<E>> constexpr rawValue(const E value)
{
	return static_cast<std::underlying_type_t<E>>(value);
}
