#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <cassert>
#include <conio.h>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <ClibUtil/distribution.hpp>
#include <ClibUtil/simpleINI.hpp>
#include <ClibUtil/string.hpp>
#include <srell.hpp>

namespace INI
{
	enum TYPE
	{
		kInvalid = 0,
		kUpgrade,
		kDowngrade
	};

	struct detail
	{
		static std::string downgrade(const std::string& a_value)
		{
			auto newValue = a_value;

			clib_util::string::replace_all(newValue, "|", " | ");
			clib_util::string::replace_all(newValue, ",", " , ");

			//strip spaces between " | "
			static const srell::regex re_bar(R"(\s*\|\s*)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_bar, "|");

			//strip spaces between " , "
			static const srell::regex re_comma(R"(\s*,\s*)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_comma, ",");

			//convert 00012345 formIDs to 0x12345
			static const srell::regex re_formID(R"(\b00+([0-9a-fA-F]{1,6})\b)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_formID, "0x$1");

			//strip leading zeros
			static const srell::regex re_zeros(R"((0x00+)([0-9a-fA-F]+))", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_zeros, "0x$2");

			//match NOT hypens
			static const srell::regex re_hypen(R"((, |\| )(-)(\w+|\d+))", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_hypen, "$1NOT $3");

			clib_util::string::replace_first_instance(newValue, "~", " - ");

			return newValue;
		}

		static std::string upgrade(const std::string& a_value)
		{
			auto newValue = a_value;

			if (newValue.find('~') == std::string::npos) {
				clib_util::string::replace_first_instance(newValue, " - ", "~");
			}

			//strip spaces
			static const srell::regex re_spaces(R"((\s+-\s+|\b\s+\b)|\s+)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_spaces, "$1");

			//convert 00012345 formIDs to 0x12345
			static const srell::regex re_formID(R"(\b00+([0-9a-fA-F]{1,6})\b)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_formID, "0x$1");

			//strip leading zeros
			static const srell::regex re_zeros(R"((0x00+)([0-9a-fA-F]+))", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_zeros, "0x$2");

			clib_util::string::replace_all(newValue, "NOT ", "-");

			return newValue;
		}
	};

	bool Format(TYPE a_type);
}
