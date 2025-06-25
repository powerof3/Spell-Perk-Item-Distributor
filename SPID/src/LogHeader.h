#pragma once

struct LogHeader
{
	std::string value;
};

// Allows custom alignment for log headers :)
// Supported formats:
//
// {:*<100}
// {:*5<100}
// {:*:2<100}
// {:*5:2<100}
template <>
struct fmt::formatter<LogHeader>
{
	enum Align
	{
		None = 0,
		Left,
		Right,
		Center,
	};

	char  fill = 0;
	int   shift = 0;
	int   spacer = 0;
	int   width = 0;
	Align align = Center;

	template <class ParseContext>
	constexpr auto parse(ParseContext& ctx)
	{
		auto it = ctx.begin();
		auto end = ctx.end();

		if (it != end) {
			fill = *it;
			++it;
		} else {
			return it;
		}

		auto getNumber = [&end](auto& it) {
			int num = 0;
			while (it != end && *it >= '0' && *it <= '9') {
				num = num * 10 + (*it - '0');
				++it;
			}
			return num;
		};

		if (it != end) {
			shift = getNumber(it);
			if (it != end) {
				if (*it == ':') {
					++it;
					spacer = getNumber(it);
				}
			}
		}

		if (it != end) {
			switch (*it) {
			case '<':
				align = Left;
				break;
			case '>':
				align = Right;
				break;
			case '^':
				align = Center;
				break;
			default:
				break;
			}
			++it;
		}

		if (it != end) {
			width = getNumber(it);
		}

		if (it != end && *it != '}') {
			throw format_error("Fill format needs char, alignment (<,^,>) and length (20)");
		}
		return it;
	}

	constexpr static int ToFill(int width, Align align, int text, int effectiveSpacer, int spacer, int shift)
	{
		switch (align) {
		default:
		case Center:
			return width - (text + 2 * effectiveSpacer);
		case Left:
			return width - (text + (shift > 0 ? spacer : 0) + effectiveSpacer) - shift;
		case Right:
			return width - (text + (shift > 0 ? spacer : 0) + effectiveSpacer) - shift;
		}
	}

	constexpr static int ToFillLeft(int toFill, char align, int shift)
	{
		switch (align) {
		default:
		case Center:
			return toFill / 2;
		case Left:
			return shift;
		case Right:
			return toFill;
		}
	};

	constexpr static int ToFillRight(int toFill, int fillLeft, char align, int shift)
	{
		switch (align) {
		default:
		case Center:
			return toFill - fillLeft;
		case Left:
			return toFill;
		case Right:
			return shift;
		}
	};

	constexpr static int LeftSpacer(Align align, int effectiveSpacer, int spacer, int shift)
	{
		switch (align) {
		default:
		case Center:
			return effectiveSpacer;
		case Left:
			return shift > 0 ? spacer : 0;
		case Right:
			return effectiveSpacer;
		}
	}

	constexpr static int RightSpacer(char align, int effectiveSpacer, int spacer, int shift)
	{
		switch (align) {
		default:
		case Center:
			return effectiveSpacer;
		case Left:
			return effectiveSpacer;
		case Right:
			return shift > 0 ? spacer : 0;
		}
	}

	// Format the string with custom alignment
	template <typename FormatContext>
	constexpr auto format(const LogHeader& custom, FormatContext& ctx) const
	{
		int textSize = custom.value.size();
		int effectiveSpacer = spacer;
		int shift = this->shift;
		int toFill = ToFill(width, align, textSize, effectiveSpacer, spacer, shift);
		int fillLeft = ToFillLeft(toFill, align, shift);
		int fillRight = ToFillRight(toFill, fillLeft, align, shift);

		while (toFill <= 1 && effectiveSpacer > 0) {
			effectiveSpacer -= 1;
			toFill = ToFill(width, align, textSize, effectiveSpacer, spacer, shift);
			fillLeft = ToFillLeft(toFill, align, shift);
			fillRight = ToFillRight(toFill, fillLeft, align, shift);
		}

		while (toFill <= 0 && (textSize + spacer + shift) > width && shift > 0) {
			shift -= 1;
			toFill = ToFill(width, align, textSize, effectiveSpacer, spacer, shift);
			fillLeft = ToFillLeft(toFill, align, shift);
			fillRight = ToFillRight(toFill, fillLeft, align, shift);
		}

		auto result = std::string(std::max(fillLeft, 0), fill) +
		              std::string(std::max(LeftSpacer(align, effectiveSpacer, spacer, shift), 0), ' ') +
		              custom.value +
		              std::string(std::max(RightSpacer(align, effectiveSpacer, spacer, shift), 0), ' ') +
		              std::string(std::max(fillRight, 0), fill);

		return fmt::format_to(ctx.out(), "{}", result);
	}
};

//#ifndef NDEBUG
//#	define LOG_HEADER(name)                     \
//		{                                        \
//			LogHeader header{ name };            \
//			logger::info("{:#3:1<100}", header); \
//		}
//#else
#define LOG_HEADER(name)                    \
	{                                       \
		LogHeader header{ name };           \
		logger::info("{:#3:1<50}", header); \
	}
//#endif
