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
#include <boost/regex.hpp>

namespace INI
{
	enum TYPE
	{
		kInvalid = 0,
		kUpgrade = 1,
		kDowngrade = 2
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

			size_t pos = 0;
			if ((pos = a_str.find_first_of(a_search, pos)) != std::string::npos) {
				a_str.replace(pos, a_search.length(), a_replace);
			}
		}

		static std::string downgrade(const std::string& a_value)
		{
			auto newValue = a_value;

			replace_all(newValue, "|", " | ");
			replace_all(newValue, ",", " , ");

			//remove excess spaces
			static const boost::regex re_spaces(R"(\B\s+|\s+\B)", boost::regex_constants::optimize);
			newValue = regex_replace(newValue, re_spaces, " ");

			//match NOT hypens
			static const boost::regex re_hypen(R"((, |\| )(-)(\w+|\d+))", boost::regex_constants::optimize);
			newValue = regex_replace(newValue, re_hypen, "$1NOT $3");

			replace_first_instance(newValue, "~", " - ");

			return newValue;
		}

		static std::string upgrade(const std::string& a_value)
		{
			auto newValue = a_value;

		    if (newValue.find_first_of('~') == std::string::npos) {
				replace_first_instance(newValue, " - ", "~");
			}

			static const boost::regex re_spaces(R"((\s+-\s+|\b\s+\b)|\s+)", boost::regex_constants::optimize);
			newValue = regex_replace(newValue, re_spaces, "$1");

		    replace_all(newValue, "NOT ", "-");

		    return newValue;
		}
	};

	bool Format(TYPE a_type);
}
