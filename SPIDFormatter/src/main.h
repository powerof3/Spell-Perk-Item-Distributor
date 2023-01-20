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
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <SimpleIni.h>
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
		static void replace_all(std::string& a_str, std::string_view a_search, std::string_view a_replace)
		{
			if (a_search.empty()) {
				return;
			}

			size_t pos = 0;
			while ((pos = a_str.find(a_search, pos)) != std::string::npos) {
				a_str.replace(pos, a_search.length(), a_replace);
				pos += a_replace.length();
			}
		}

		static void replace_first_instance(std::string& a_str, std::string_view a_search, std::string_view a_replace)
		{
			if (a_search.empty()) {
				return;
			}

			if (auto pos = a_str.find(a_search); pos != std::string::npos) {
				a_str.replace(pos, a_search.length(), a_replace);
			}
		}

		static std::string downgrade(const std::string& a_value)
		{
			auto newValue = a_value;

			replace_all(newValue, "|", " | ");
			replace_all(newValue, ",", " , ");

			//strip spaces between " | "
			static const srell::regex re_bar(R"(\s*\|\s*)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_bar, "|");

			//strip spaces between " , "
			static const srell::regex re_comma(R"(\s*,\s*)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_comma, ",");

			//strip leading zeros
			static const srell::regex re_zeros(R"((0x00+)([0-9a-fA-F]+))", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_zeros, "0x$2");

			//match NOT hypens
			static const srell::regex re_hypen(R"((, |\| )(-)(\w+|\d+))", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_hypen, "$1NOT $3");

			replace_first_instance(newValue, "~", " - ");

			return newValue;
		}

		static std::string upgrade(const std::string& a_value)
		{
			auto newValue = a_value;

			if (newValue.find('~') == std::string::npos) {
				replace_first_instance(newValue, " - ", "~");
			}

			//strip spaces
			static const srell::regex re_spaces(R"((\s+-\s+|\b\s+\b)|\s+)", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_spaces, "$1");

			//strip leading zeros
			static const srell::regex re_zeros(R"((0x00+)([0-9a-fA-F]+))", srell::regex_constants::optimize);
			newValue = srell::regex_replace(newValue, re_zeros, "0x$2");

			replace_all(newValue, "NOT ", "-");

			return newValue;
		}
	};

	bool Format(TYPE a_type);
}
